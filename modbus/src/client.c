#include <modbus/client.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <modbus/mb_log.h>

#define MB_CLIENT_TIMEOUT_DEFAULT MB_CLIENT_DEFAULT_TIMEOUT_MS
#define MB_CLIENT_BACKOFF_DEFAULT MB_CLIENT_DEFAULT_RETRY_BACKOFF_MS

static void mb_client_tcp_callback(mb_tcp_transport_t *tcp,
                                   const mb_adu_view_t *adu,
                                   mb_u16 transaction_id,
                                   mb_err_t status,
                                   void *user);
static mb_err_t client_transport_submit(mb_client_t *client, mb_client_txn_t *txn);
static mb_err_t client_transport_poll(mb_client_t *client);
static mb_client_txn_t *client_find_by_tid(mb_client_t *client, mb_u16 tid);

static mb_time_ms_t client_now(const mb_client_t *client)
{
    return mb_transport_now(client->iface);
}

static mb_size_t client_total_inflight(const mb_client_t *client)
{
    if (client == NULL) {
        return 0U;
    }

    mb_size_t count = 0U;
    if (client->current != NULL && client->current->in_use && !client->current->cancelled) {
        count = 1U;
    }

    const mb_client_txn_t *node = client->pending_head;
    while (node != NULL) {
        if (node->in_use && !node->cancelled) {
            count += 1U;
        }
        node = node->next;
    }

    return count;
}

static void client_trace_hex_buffer(const mb_client_t *client,
                                    const char *label,
                                    const mb_u8 *buffer,
                                    mb_size_t length)
{
#if MB_LOG_ENABLED
    if (client == NULL || !client->trace_hex || buffer == NULL || length == 0U) {
        return;
    }

    char line[16U + (MB_PDU_MAX * 3U)];
    int written = snprintf(line, sizeof(line), "%s:", (label != NULL) ? label : "client");
    if (written < 0) {
        return;
    }

    size_t pos = (size_t)written;
    for (mb_size_t i = 0U; i < length && pos + 4U < sizeof(line); ++i) {
        int rv = snprintf(&line[pos], sizeof(line) - pos, " %02X", buffer[i]);
        if (rv < 0) {
            break;
        }
        pos += (size_t)rv;
    }

    line[sizeof(line) - 1U] = '\0';
    MB_LOG_DEBUG("%s", line);
#else
    (void)client;
    (void)label;
    (void)buffer;
    (void)length;
#endif
}

static void client_trace_request(const mb_client_t *client, const mb_client_txn_t *txn, const char *label)
{
#if MB_LOG_ENABLED
    if (client == NULL || txn == NULL || !client->trace_hex) {
        return;
    }

    mb_u8 scratch[MB_PDU_MAX + 1U];
    scratch[0] = txn->request_view.function;
    mb_size_t len = 1U;
    if (txn->request_view.payload_len > 0U && txn->request_view.payload != NULL) {
        mb_size_t copy_len = txn->request_view.payload_len;
        if (copy_len > MB_PDU_MAX) {
            copy_len = MB_PDU_MAX;
        }
        memcpy(&scratch[1], txn->request_view.payload, copy_len);
        len += copy_len;
    }

    client_trace_hex_buffer(client, (label != NULL) ? label : "client.tx", scratch, len);
#else
    (void)client;
    (void)txn;
    (void)label;
#endif
}

static void client_trace_response(const mb_client_t *client, const mb_adu_view_t *adu, const char *label)
{
#if MB_LOG_ENABLED
    if (client == NULL || adu == NULL || !client->trace_hex) {
        return;
    }

    mb_u8 scratch[MB_PDU_MAX + 1U];
    scratch[0] = adu->function;
    mb_size_t len = 1U;
    if (adu->payload_len > 0U && adu->payload != NULL) {
        mb_size_t copy_len = adu->payload_len;
        if (copy_len > MB_PDU_MAX) {
            copy_len = MB_PDU_MAX;
        }
        memcpy(&scratch[1], adu->payload, copy_len);
        len += copy_len;
    }

    client_trace_hex_buffer(client, (label != NULL) ? label : "client.rx", scratch, len);
#else
    (void)client;
    (void)adu;
    (void)label;
#endif
}

static void client_emit_state_event(mb_client_t *client,
                                    mb_event_type_t type,
                                    mb_client_state_t state)
{
    if (client == NULL || client->observer_cb == NULL) {
        return;
    }

    mb_event_t event = {
        .source = MB_EVENT_SOURCE_CLIENT,
        .type = type,
        .timestamp = client_now(client),
    };
    event.data.client_state.state = (mb_u8)state;
    client->observer_cb(&event, client->observer_user);
}

static void client_emit_tx_event(mb_client_t *client,
                                 mb_event_type_t type,
                                 const mb_client_txn_t *txn,
                                 mb_err_t status)
{
    if (client == NULL || client->observer_cb == NULL || txn == NULL) {
        return;
    }

    mb_event_t event = {
        .source = MB_EVENT_SOURCE_CLIENT,
        .type = type,
        .timestamp = client_now(client),
    };
    event.data.client_txn.function = txn->request_view.function;
    event.data.client_txn.status = status;
    event.data.client_txn.expect_response = txn->expect_response;
    client->observer_cb(&event, client->observer_user);
}

static void client_transition_state(mb_client_t *client, mb_client_state_t next)
{
    if (client == NULL || client->state == next) {
        return;
    }

    const mb_client_state_t previous = client->state;
    client_emit_state_event(client, MB_EVENT_CLIENT_STATE_EXIT, previous);
    client->state = next;
    client_emit_state_event(client, MB_EVENT_CLIENT_STATE_ENTER, next);
}

static mb_time_ms_t client_base_timeout_ms(const mb_client_txn_t *txn)
{
    const mb_time_ms_t base = (txn->base_timeout_ms == 0U) ? MB_CLIENT_TIMEOUT_DEFAULT : txn->base_timeout_ms;
    return (base == 0U) ? MB_CLIENT_TIMEOUT_DEFAULT : base;
}

static mb_time_ms_t client_current_timeout_ms(const mb_client_txn_t *txn)
{
    mb_time_ms_t timeout = client_base_timeout_ms(txn);
    mb_u8 retries = txn->retry_count;
    while (retries > 0U && timeout < MB_CLIENT_MAX_TIMEOUT_MS) {
        if (timeout > (MB_CLIENT_MAX_TIMEOUT_MS / 2U)) {
            timeout = MB_CLIENT_MAX_TIMEOUT_MS;
            break;
        }
        timeout *= 2U;
        retries -= 1U;
    }
    if (timeout > MB_CLIENT_MAX_TIMEOUT_MS) {
        timeout = MB_CLIENT_MAX_TIMEOUT_MS;
    }
    return timeout;
}

static mb_time_ms_t client_base_backoff_ms(const mb_client_txn_t *txn)
{
    const mb_time_ms_t base = (txn->retry_backoff_ms == 0U) ? MB_CLIENT_BACKOFF_DEFAULT : txn->retry_backoff_ms;
    return (base == 0U) ? MB_CLIENT_BACKOFF_DEFAULT : base;
}

static mb_time_ms_t client_retry_backoff_ms(const mb_client_txn_t *txn)
{
    mb_time_ms_t backoff = client_base_backoff_ms(txn);
    if (backoff == 0U) {
        backoff = 1U;
    }

    if (txn->retry_count == 0U) {
        return backoff;
    }

    mb_u8 exponent = txn->retry_count - 1U;
    while (exponent > 0U && backoff < MB_CLIENT_MAX_TIMEOUT_MS) {
        if (backoff > (MB_CLIENT_MAX_TIMEOUT_MS / 2U)) {
            backoff = MB_CLIENT_MAX_TIMEOUT_MS;
            break;
        }
        backoff *= 2U;
        exponent -= 1U;
    }

    if (backoff > MB_CLIENT_MAX_TIMEOUT_MS) {
        backoff = MB_CLIENT_MAX_TIMEOUT_MS;
    }

    return backoff;
}

static mb_time_ms_t client_backoff_with_jitter(const mb_client_txn_t *txn,
                                               mb_time_ms_t base_backoff,
                                               mb_time_ms_t now)
{
    if (base_backoff <= 1U) {
        return 1U;
    }

    mb_time_ms_t spread = base_backoff / 2U;
    if (spread == 0U) {
        spread = 1U;
    }

    const uintptr_t salt = (uintptr_t)txn;
    const mb_time_ms_t pseudo = (now ^ (now >> 7)) ^ ((mb_time_ms_t)(salt >> 3)) ^ ((mb_time_ms_t)txn->retry_count * 131U);
    const mb_time_ms_t offset = pseudo % (spread + 1U);
    mb_time_ms_t delay = (base_backoff - spread) + offset;
    if (delay == 0U) {
        delay = 1U;
    }
    if (delay > MB_CLIENT_MAX_TIMEOUT_MS) {
        delay = MB_CLIENT_MAX_TIMEOUT_MS;
    }
    return delay;
}

static void client_start_next(mb_client_t *client);

static void client_enqueue(mb_client_t *client, mb_client_txn_t *txn)
{
    txn->next = NULL;
    if (txn->high_priority) {
        txn->next = client->pending_head;
        client->pending_head = txn;
        if (client->pending_tail == NULL) {
            client->pending_tail = txn;
        }
    } else {
        if (client->pending_tail) {
            client->pending_tail->next = txn;
        } else {
            client->pending_head = txn;
        }
        client->pending_tail = txn;
    }
    client->pending_count += 1U;
}

static bool client_remove_from_queue(mb_client_t *client, mb_client_txn_t *target)
{
    if (client == NULL || target == NULL) {
        return false;
    }

    mb_client_txn_t *prev = NULL;
    mb_client_txn_t *node = client->pending_head;
    while (node != NULL) {
        if (node == target) {
            if (prev != NULL) {
                prev->next = node->next;
            } else {
                client->pending_head = node->next;
            }

            if (client->pending_tail == node) {
                client->pending_tail = prev;
            }

            node->next = NULL;
            node->queued = false;
            if (client->pending_count > 0U) {
                client->pending_count -= 1U;
            }
            return true;
        }

        prev = node;
        node = node->next;
    }

    return false;
}

static mb_client_txn_t *client_dequeue(mb_client_t *client)
{
    while (client->pending_head && (client->pending_head->cancelled || !client->pending_head->in_use)) {
        mb_client_txn_t *discard = client->pending_head;
        client->pending_head = discard->next;
        if (client->pending_head == NULL) {
            client->pending_tail = NULL;
        }
        discard->next = NULL;
        discard->queued = false;
        discard->in_use = false;
        discard->next_attempt_ms = 0U;
        if (client->pending_count > 0U) {
            client->pending_count -= 1U;
        }
    }

    mb_client_txn_t *txn = client->pending_head;
    if (txn != NULL) {
        client->pending_head = txn->next;
        if (client->pending_head == NULL) {
            client->pending_tail = NULL;
        }
        txn->next = NULL;
        txn->queued = false;
        if (client->pending_count > 0U) {
            client->pending_count -= 1U;
        }
    }
    return txn;
}

static mb_client_txn_t *client_find_by_tid(mb_client_t *client, mb_u16 tid)
{
    if (client == NULL) {
        return NULL;
    }

    for (mb_size_t i = 0U; i < client->pool_size; ++i) {
        mb_client_txn_t *candidate = &client->pool[i];
        if (!candidate->in_use) {
            continue;
        }
        if (candidate->tid == tid) {
            return candidate;
        }
    }

    return NULL;
}

static void client_finalize(mb_client_t *client,
                            mb_client_txn_t *txn,
                            mb_err_t status,
                            const mb_adu_view_t *response)
{
    const mb_time_ms_t now = client_now(client);

    client->metrics.completed += 1U;
    if (status == MB_OK) {
        client->metrics.response_count += 1U;
        if (txn->start_time > 0U && now >= txn->start_time) {
            client->metrics.response_latency_total_ms += (now - txn->start_time);
        }
    } else if (status == MB_ERR_TIMEOUT) {
        client->metrics.timeouts += 1U;
    } else if (status == MB_ERR_CANCELLED) {
        client->metrics.cancelled += 1U;
    } else {
        client->metrics.errors += 1U;
    }

    if (txn->poison) {
        client->metrics.poison_triggers += 1U;
    }

    if (response && response->payload) {
        (void)response;
    }

    txn->status = status;
    txn->completed = true;
    txn->callback_pending = true;

    mb_diag_record_error(&client->diag, status);
    client_emit_tx_event(client, MB_EVENT_CLIENT_TX_COMPLETE, txn, status);

    if (txn->cfg.callback) {
        txn->cfg.callback(client, txn, status, response, txn->cfg.user_ctx);
    }

    txn->callback_pending = false;
    txn->completed = false;
    txn->cancelled = false;
    txn->in_use = false;
    txn->next = NULL;
    txn->queued = false;
    txn->next_attempt_ms = 0U;
    txn->deadline = 0U;
    txn->watchdog_deadline = 0U;
    txn->tid = 0U;
    txn->high_priority = false;
    txn->poison = false;
    txn->start_time = 0U;
}

static void client_prepare_response(mb_client_txn_t *txn, const mb_adu_view_t *adu)
{
    if (!txn->expect_response || adu == NULL) {
        txn->response_view.payload_len = 0U;
        txn->response_view.payload = NULL;
        return;
    }

    txn->response_view.unit_id = adu->unit_id;
    txn->response_view.function = adu->function;
    txn->response_view.payload_len = adu->payload_len;
    if (adu->payload && adu->payload_len > 0U) {
        if (adu->payload_len > MB_PDU_MAX) {
            txn->response_view.payload_len = MB_PDU_MAX;
        }
        memcpy(txn->response_storage, adu->payload, txn->response_view.payload_len);
        txn->response_view.payload = txn->response_storage;
    } else {
        txn->response_view.payload = NULL;
    }
}

static mb_err_t client_transport_submit(mb_client_t *client, mb_client_txn_t *txn)
{
    if (client == NULL || txn == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_err_t status = MB_ERR_INVALID_ARGUMENT;
    mb_size_t frame_len = 0U;

    client_trace_request(client, txn, "client.tx");

    switch (client->transport) {
    case MB_CLIENT_TRANSPORT_RTU:
        frame_len = 1U + 1U + txn->request_view.payload_len + 2U;
        status = mb_rtu_submit(&client->rtu, &txn->request_view);
        break;
    case MB_CLIENT_TRANSPORT_TCP:
        if (txn->tid == 0U) {
            txn->tid = client->next_tid++;
            if (client->next_tid == 0U) {
                client->next_tid = 1U;
            }
        }
        frame_len = MB_TCP_HEADER_SIZE + 1U + txn->request_view.payload_len;
        status = mb_tcp_submit(&client->tcp, &txn->request_view, txn->tid);
        break;
    default:
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (status == MB_OK) {
        client->metrics.bytes_tx += frame_len;
    }

    return status;
}

static mb_err_t client_transport_poll(mb_client_t *client)
{
    if (client == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    switch (client->transport) {
    case MB_CLIENT_TRANSPORT_RTU:
        return mb_rtu_poll(&client->rtu);
    case MB_CLIENT_TRANSPORT_TCP: {
        mb_err_t status = mb_tcp_poll(&client->tcp);
        return (status == MB_ERR_TIMEOUT) ? MB_OK : status;
    }
    default:
        return MB_ERR_INVALID_ARGUMENT;
    }
}

static void mb_client_tcp_callback(mb_tcp_transport_t *tcp,
                                   const mb_adu_view_t *adu,
                                   mb_u16 transaction_id,
                                   mb_err_t status,
                                   void *user)
{
    (void)tcp;
    mb_client_t *client = (mb_client_t *)user;
    if (client == NULL) {
        return;
    }

    mb_client_txn_t *txn = NULL;
    if (transaction_id != 0U) {
        txn = client_find_by_tid(client, transaction_id);
    }
    if (txn == NULL && transaction_id == 0U) {
        txn = client->current;
    }

    if (txn == NULL) {
        return;
    }

    if (txn->queued) {
        client_remove_from_queue(client, txn);
    }

    if (status == MB_OK && adu != NULL) {
        const mb_size_t adu_len = MB_TCP_HEADER_SIZE + 1U + adu->payload_len;
        client->metrics.bytes_rx += adu_len;
        client_trace_response(client, adu, "client.rx");
    }

    if (status == MB_OK) {
        client_prepare_response(txn, adu);
        client_finalize(client, txn, MB_OK, adu ? &txn->response_view : NULL);
    } else {
        client_finalize(client, txn, status, NULL);
    }

    if (client->current == txn) {
        client->current = NULL;
    }

    client_transition_state(client, MB_CLIENT_STATE_IDLE);
    client_start_next(client);
}

static void client_attempt_send(mb_client_t *client, mb_client_txn_t *txn)
{
    if (client == NULL || txn == NULL) {
        return;
    }

    const mb_time_ms_t now = client_now(client);
    txn->timeout_ms = client_current_timeout_ms(txn);
    txn->deadline = now + txn->timeout_ms;
    txn->watchdog_deadline = (client->watchdog_ms > 0U) ? (now + client->watchdog_ms) : 0U;
    txn->next_attempt_ms = 0U;
    txn->start_time = now;

    if (txn->poison) {
        client_finalize(client, txn, MB_ERR_CANCELLED, NULL);
        client->current = NULL;
        client_transition_state(client, MB_CLIENT_STATE_IDLE);
        client_start_next(client);
        return;
    }

    mb_err_t status = client_transport_submit(client, txn);
    if (status != MB_OK) {
        client_finalize(client, txn, status, NULL);
        client->current = NULL;
        client_transition_state(client, MB_CLIENT_STATE_IDLE);
        return;
    }

    if (!txn->expect_response) {
        client_finalize(client, txn, MB_OK, NULL);
        client->current = NULL;
        client_transition_state(client, MB_CLIENT_STATE_IDLE);
        return;
    }

    client_transition_state(client, MB_CLIENT_STATE_WAITING);
}

static void client_start_next(mb_client_t *client)
{
    while (true) {
        client->current = client_dequeue(client);
        if (client->current == NULL) {
            client_transition_state(client, MB_CLIENT_STATE_IDLE);
            return;
        }

        client_attempt_send(client, client->current);
        if (client->state == MB_CLIENT_STATE_WAITING || client->current != NULL) {
            return;
        }
        /* Immediate completion, continue draining the queue. */
    }
}

static void client_retry(mb_client_t *client)
{
    mb_client_txn_t *txn = client->current;
    if (txn == NULL) {
        return;
    }

    if (txn->retry_count >= txn->max_retries) {
        client_finalize(client, txn, MB_ERR_TIMEOUT, NULL);
        client->current = NULL;
        client_start_next(client);
        return;
    }

    txn->retry_count += 1U;
    client->metrics.retries += 1U;
    const mb_time_ms_t now = client_now(client);
    const mb_time_ms_t base_backoff = client_retry_backoff_ms(txn);
    const mb_time_ms_t delay = client_backoff_with_jitter(txn, base_backoff, now);
    txn->next_attempt_ms = now + delay;
    txn->deadline = 0U;
    txn->watchdog_deadline = 0U;

    client_transition_state(client, MB_CLIENT_STATE_BACKOFF);
}

static void mb_client_rtu_callback(mb_rtu_transport_t *rtu,
                                   const mb_adu_view_t *adu,
                                   mb_err_t status,
                                   void *user)
{
    (void)rtu;
    mb_client_t *client = (mb_client_t *)user;
    mb_client_txn_t *txn = client->current;
    if (txn == NULL) {
        return;
    }

    if (status == MB_OK && adu != NULL) {
        const mb_size_t adu_len = 1U + adu->payload_len + 2U;
        client->metrics.bytes_rx += adu_len;
        client_trace_response(client, adu, "client.rx");
    }

    if (status == MB_OK) {
        client_prepare_response(txn, adu);
        client_finalize(client, txn, MB_OK, adu ? &txn->response_view : NULL);
    } else {
        client_finalize(client, txn, status, NULL);
    }

    client->current = NULL;
    client_transition_state(client, MB_CLIENT_STATE_IDLE);
    client_start_next(client);
}

static mb_err_t mb_client_init_common(mb_client_t *client,
                                      const mb_transport_if_t *iface,
                                      mb_client_txn_t *txn_pool,
                                      mb_size_t txn_pool_len)
{
    if (client == NULL || iface == NULL || txn_pool == NULL || txn_pool_len == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (iface->send == NULL || iface->recv == NULL || iface->now == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    memset(client, 0, sizeof(*client));
    client->iface = iface;
    client->pool = txn_pool;
    client->pool_size = txn_pool_len;
    client->state = MB_CLIENT_STATE_IDLE;
    client->watchdog_ms = MB_CLIENT_DEFAULT_WATCHDOG_MS;
    client->next_tid = 1U;
    client->queue_capacity = txn_pool_len;
    client->pending_count = 0U;
    memset(client->fc_timeouts, 0, sizeof(client->fc_timeouts));
    memset(&client->metrics, 0, sizeof(client->metrics));
    mb_diag_reset(&client->diag);

    for (mb_size_t i = 0U; i < txn_pool_len; ++i) {
        memset(&txn_pool[i], 0, sizeof(mb_client_txn_t));
    }

    return MB_OK;
}

mb_err_t mb_client_init(mb_client_t *client,
                        const mb_transport_if_t *iface,
                        mb_client_txn_t *txn_pool,
                        mb_size_t txn_pool_len)
{
    mb_err_t status = mb_client_init_common(client, iface, txn_pool, txn_pool_len);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    client->transport = MB_CLIENT_TRANSPORT_RTU;
    return mb_rtu_init(&client->rtu, iface, mb_client_rtu_callback, client);
}

mb_err_t mb_client_init_tcp(mb_client_t *client,
                            const mb_transport_if_t *iface,
                            mb_client_txn_t *txn_pool,
                            mb_size_t txn_pool_len)
{
    mb_err_t status = mb_client_init_common(client, iface, txn_pool, txn_pool_len);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    client->transport = MB_CLIENT_TRANSPORT_TCP;
    return mb_tcp_init(&client->tcp, iface, mb_client_tcp_callback, client);
}

mb_err_t mb_client_submit(mb_client_t *client,
                          const mb_client_request_t *request,
                          mb_client_txn_t **out_txn)
{
    if (client == NULL || request == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (request->request.payload_len > MB_PDU_MAX) {
        mb_diag_record_error(&client->diag, MB_ERR_INVALID_ARGUMENT);
        return MB_ERR_INVALID_ARGUMENT;
    }

    const bool is_poison = ((request->flags & MB_CLIENT_REQUEST_POISON) != 0U);
    if (!is_poison && client->queue_capacity > 0U) {
        const mb_size_t inflight = client_total_inflight(client);
        if (inflight >= client->queue_capacity) {
            mb_diag_record_error(&client->diag, MB_ERR_NO_RESOURCES);
            return MB_ERR_NO_RESOURCES;
        }
    }

    mb_client_txn_t *txn = NULL;
    for (mb_size_t i = 0U; i < client->pool_size; ++i) {
        if (!client->pool[i].in_use) {
            txn = &client->pool[i];
            break;
        }
    }

    if (txn == NULL) {
        mb_diag_record_error(&client->diag, MB_ERR_NO_RESOURCES);
        return MB_ERR_NO_RESOURCES;
    }

    memset(txn, 0, sizeof(*txn));
    txn->in_use = true;
    txn->queued = true;
    txn->cfg = *request;
    txn->poison = is_poison;
    txn->high_priority = ((request->flags & MB_CLIENT_REQUEST_HIGH_PRIORITY) != 0U) || txn->poison;
    txn->expect_response = ((request->flags & MB_CLIENT_REQUEST_NO_RESPONSE) == 0U) && !txn->poison;

    mb_time_ms_t base_timeout = request->timeout_ms;
    if (base_timeout == 0U) {
        const mb_u8 function = request->request.function;
        const mb_time_ms_t fc_override = client->fc_timeouts[function];
        base_timeout = (fc_override != 0U) ? fc_override : MB_CLIENT_TIMEOUT_DEFAULT;
    }
    txn->timeout_ms = base_timeout;
    txn->base_timeout_ms = base_timeout;
    txn->retry_backoff_ms = (request->retry_backoff_ms == 0U) ? MB_CLIENT_BACKOFF_DEFAULT : request->retry_backoff_ms;
    txn->max_retries = request->max_retries;
    txn->retry_count = 0U;
    txn->status = MB_OK;
    txn->next_attempt_ms = 0U;

    txn->request_view.unit_id = request->request.unit_id;
    txn->request_view.function = request->request.function;
    txn->request_view.payload_len = request->request.payload_len;
    if (request->request.payload_len > 0U && request->request.payload != NULL) {
        memcpy(txn->request_storage, request->request.payload, request->request.payload_len);
        txn->request_view.payload = txn->request_storage;
    } else {
        txn->request_view.payload = NULL;
    }

    if (!txn->poison) {
        mb_diag_record_fc(&client->diag, txn->request_view.function);
    }
    client_emit_tx_event(client, MB_EVENT_CLIENT_TX_SUBMIT, txn, MB_OK);

    client_enqueue(client, txn);
    client->metrics.submitted += 1U;

    if (out_txn) {
        *out_txn = txn;
    }

    if (client->state == MB_CLIENT_STATE_IDLE && client->current == NULL) {
        client_start_next(client);
    }

    return MB_OK;
}

mb_err_t mb_client_submit_poison(mb_client_t *client)
{
    if (client == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_client_request_t request;
    memset(&request, 0, sizeof(request));
    request.flags = MB_CLIENT_REQUEST_POISON | MB_CLIENT_REQUEST_NO_RESPONSE | MB_CLIENT_REQUEST_HIGH_PRIORITY;
    return mb_client_submit(client, &request, NULL);
}

mb_err_t mb_client_cancel(mb_client_t *client, mb_client_txn_t *txn)
{
    if (client == NULL || txn == NULL || !txn->in_use) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (txn == client->current) {
        client_finalize(client, txn, MB_ERR_CANCELLED, NULL);
        client->current = NULL;
        client_start_next(client);
        return MB_OK;
    }

    const bool removed = client_remove_from_queue(client, txn);

    if (!removed && !txn->queued) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    client_finalize(client, txn, MB_ERR_CANCELLED, NULL);

    if (client->state == MB_CLIENT_STATE_IDLE && client->current == NULL) {
        client_start_next(client);
    }

    return MB_OK;
}

mb_err_t mb_client_poll(mb_client_t *client)
{
    if (client == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_err_t status = client_transport_poll(client);

    mb_client_txn_t *txn = client->current;
    if (txn != NULL) {
        const mb_time_ms_t now = client_now(client);

        if (client->state == MB_CLIENT_STATE_BACKOFF) {
            if (now >= txn->next_attempt_ms) {
                client_attempt_send(client, txn);
                txn = client->current;
            }
        }

        if (client->state == MB_CLIENT_STATE_WAITING && txn != NULL) {
            if (txn->deadline > 0U && now >= txn->deadline) {
                client_retry(client);
                return status;
            }

            if (client->watchdog_ms > 0U && txn->watchdog_deadline > 0U && now >= txn->watchdog_deadline) {
                client_finalize(client, txn, MB_ERR_TRANSPORT, NULL);
                client->current = NULL;
                client_start_next(client);
                return status;
            }
        }
    }

    if (client->state == MB_CLIENT_STATE_IDLE && client->current == NULL) {
        client_start_next(client);
    }

    return status;
}

void mb_client_set_watchdog(mb_client_t *client, mb_time_ms_t watchdog_ms)
{
    if (client == NULL) {
        return;
    }
    client->watchdog_ms = watchdog_ms;
}

bool mb_client_is_idle(const mb_client_t *client)
{
    if (client == NULL) {
        return true;
    }
    return (client->state == MB_CLIENT_STATE_IDLE) && (client->pending_head == NULL) && (client->current == NULL);
}

mb_size_t mb_client_pending(const mb_client_t *client)
{
    return client_total_inflight(client);
}

void mb_client_set_queue_capacity(mb_client_t *client, mb_size_t capacity)
{
    if (client == NULL) {
        return;
    }

    if (capacity == 0U || capacity > client->pool_size) {
        client->queue_capacity = client->pool_size;
    } else {
        client->queue_capacity = capacity;
    }
}

mb_size_t mb_client_queue_capacity(const mb_client_t *client)
{
    if (client == NULL) {
        return 0U;
    }
    return client->queue_capacity;
}

void mb_client_set_fc_timeout(mb_client_t *client, mb_u8 function, mb_time_ms_t timeout_ms)
{
    if (client == NULL) {
        return;
    }

    client->fc_timeouts[function] = timeout_ms;
}

void mb_client_get_metrics(const mb_client_t *client, mb_client_metrics_t *out_metrics)
{
    if (client == NULL || out_metrics == NULL) {
        return;
    }

    *out_metrics = client->metrics;
}

void mb_client_reset_metrics(mb_client_t *client)
{
    if (client == NULL) {
        return;
    }

    memset(&client->metrics, 0, sizeof(client->metrics));
}

void mb_client_get_diag(const mb_client_t *client, mb_diag_counters_t *out_diag)
{
    if (client == NULL || out_diag == NULL) {
        return;
    }

    *out_diag = client->diag;
}

void mb_client_reset_diag(mb_client_t *client)
{
    if (client == NULL) {
        return;
    }

    mb_diag_reset(&client->diag);
}

void mb_client_set_event_callback(mb_client_t *client, mb_event_callback_t callback, void *user_ctx)
{
    if (client == NULL) {
        return;
    }

    client->observer_cb = callback;
    client->observer_user = (callback != NULL) ? user_ctx : NULL;

    if (callback != NULL) {
        client_emit_state_event(client, MB_EVENT_CLIENT_STATE_ENTER, client->state);
    }
}

void mb_client_set_trace_hex(mb_client_t *client, bool enable)
{
    if (client == NULL) {
        return;
    }

    client->trace_hex = enable;
}
