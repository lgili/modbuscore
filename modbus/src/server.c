#include <modbus/server.h>

#include <stdio.h>
#include <string.h>

#include <modbus/mb_log.h>

static const mb_u16 kExceptionIllegalFunction = MB_EX_ILLEGAL_FUNCTION;
static const mb_u16 kExceptionIllegalDataAddress = MB_EX_ILLEGAL_DATA_ADDRESS;
static const mb_u16 kExceptionIllegalDataValue = MB_EX_ILLEGAL_DATA_VALUE;
static const mb_u16 kExceptionServerDeviceFailure = MB_EX_SERVER_DEVICE_FAILURE;

static void server_trace_hex_buffer(const mb_server_t *server,
                                    const char *label,
                                    const mb_u8 *buffer,
                                    mb_size_t length)
{
#if MB_LOG_ENABLED
    if (server == NULL || !server->trace_hex || buffer == NULL || length == 0U) {
        return;
    }

    char line[16U + (MB_PDU_MAX * 3U)];
    int written = snprintf(line, sizeof(line), "%s:", (label != NULL) ? label : "server");
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
    (void)server;
    (void)label;
    (void)buffer;
    (void)length;
#endif
}

static void server_reset_request(mb_server_request_t *req)
{
    if (req == NULL) {
        return;
    }
    memset(req, 0, sizeof(*req));
}

static mb_time_ms_t server_now(const mb_server_t *server)
{
    return mb_transport_now(server->iface);
}

static void server_emit_state_event(mb_server_t *server,
                                    mb_event_type_t type,
                                    mb_server_state_t state)
{
    if (server == NULL || server->observer_cb == NULL) {
        return;
    }

    mb_event_t event = {
        .source = MB_EVENT_SOURCE_SERVER,
        .type = type,
        .timestamp = server_now(server),
    };
    event.data.server_state.state = (mb_u8)state;
    server->observer_cb(&event, server->observer_user);
}

static void server_emit_request_event(mb_server_t *server,
                                      mb_event_type_t type,
                                      const mb_server_request_t *req,
                                      mb_err_t status)
{
    if (server == NULL || server->observer_cb == NULL || req == NULL) {
        return;
    }

    mb_event_t event = {
        .source = MB_EVENT_SOURCE_SERVER,
        .type = type,
        .timestamp = server_now(server),
    };
    event.data.server_req.function = req->function;
    event.data.server_req.broadcast = req->broadcast;
    event.data.server_req.status = status;
    server->observer_cb(&event, server->observer_user);
}

static void server_transition_state(mb_server_t *server, mb_server_state_t next)
{
    if (server == NULL || server->state == next) {
        return;
    }

    const mb_server_state_t previous = server->state;
    server_emit_state_event(server, MB_EVENT_SERVER_STATE_EXIT, previous);
    server->state = next;
    server_emit_state_event(server, MB_EVENT_SERVER_STATE_ENTER, next);
}

static mb_time_ms_t server_fc_timeout(const mb_server_t *server, mb_u8 function)
{
    mb_time_ms_t timeout = server->fc_timeouts[function];
    if (timeout == 0U) {
        timeout = MB_SERVER_DEFAULT_TIMEOUT_MS;
    }
    return timeout;
}

static bool server_is_high_priority_fc(mb_u8 function)
{
    return (function == MB_PDU_FC_WRITE_SINGLE_REGISTER) || (function == MB_PDU_FC_WRITE_MULTIPLE_REGISTERS);
}

static mb_size_t server_total_inflight(const mb_server_t *server)
{
    if (server == NULL) {
        return 0U;
    }
    mb_size_t count = server->pending_count;
    if (server->current != NULL && server->current->in_use) {
        count += 1U;
    }
    return count;
}

static mb_server_request_t *server_alloc_request(mb_server_t *server)
{
    if (server == NULL || server->pool == NULL) {
        return NULL;
    }

    for (mb_size_t i = 0U; i < server->pool_size; ++i) {
        mb_server_request_t *req = &server->pool[i];
        if (!req->in_use) {
            server_reset_request(req);
            req->in_use = true;
            return req;
        }
    }

    return NULL;
}

static void server_enqueue(mb_server_t *server, mb_server_request_t *req)
{
    if (server == NULL || req == NULL) {
        return;
    }

    req->next = NULL;
    if (req->high_priority) {
        req->next = server->pending_head;
        server->pending_head = req;
        if (server->pending_tail == NULL) {
            server->pending_tail = req;
        }
    } else {
        if (server->pending_tail != NULL) {
            server->pending_tail->next = req;
        } else {
            server->pending_head = req;
        }
        server->pending_tail = req;
    }
    server->pending_count += 1U;
}

static mb_server_request_t *server_dequeue(mb_server_t *server)
{
    if (server == NULL) {
        return NULL;
    }

    mb_server_request_t *req = server->pending_head;
    if (req != NULL) {
        server->pending_head = req->next;
        if (server->pending_head == NULL) {
            server->pending_tail = NULL;
        }
        req->next = NULL;
        req->queued = false;
        if (server->pending_count > 0U) {
            server->pending_count -= 1U;
        }
    }
    return req;
}

static void server_release_request(mb_server_request_t *req)
{
    server_reset_request(req);
}

static mb_err_t server_send_exception(mb_server_t *server,
                                      const mb_adu_view_t *request,
                                      mb_u8 exception_code)
{
    if (server == NULL || request == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_err_t status = mb_pdu_build_exception(server->tx_buffer,
                                             sizeof(server->tx_buffer),
                                             request->function,
                                             exception_code);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    server_trace_hex_buffer(server, "server.tx", server->tx_buffer, 2U);

    mb_adu_view_t response = {
        .unit_id = server->unit_id,
        .function = server->tx_buffer[0],
        .payload = &server->tx_buffer[1],
        .payload_len = 1U,
    };

    return mb_rtu_submit(&server->rtu, &response);
}

static bool ranges_overlap(mb_u16 a_start, mb_u16 a_count, mb_u16 b_start, mb_u16 b_count)
{
    const mb_u32 a_begin = a_start;
    const mb_u32 a_end = a_begin + a_count;
    const mb_u32 b_begin = b_start;
    const mb_u32 b_end = b_begin + b_count;
    return !(a_end <= b_begin || b_end <= a_begin);
}

static bool region_contains(const mb_server_region_t *region, mb_u16 addr, mb_u16 count)
{
    const mb_u32 begin = region->start;
    const mb_u32 end = begin + region->count;
    const mb_u32 req_begin = addr;
    const mb_u32 req_end = req_begin + count;
    return (req_begin >= begin) && (req_end <= end);
}

static mb_err_t region_read(const mb_server_region_t *region,
                            mb_u16 addr,
                            mb_u16 quantity,
                            mb_u16 *out_values)
{
    if (region->read_cb) {
        return region->read_cb(addr, quantity, out_values, quantity, region->user_ctx);
    }

    if (!region->storage) {
        return kExceptionServerDeviceFailure;
    }

    const mb_u16 offset = (mb_u16)(addr - region->start);
    for (mb_u16 i = 0; i < quantity; ++i) {
        out_values[i] = region->storage[offset + i];
    }

    return MB_OK;
}

static mb_err_t region_write(const mb_server_region_t *region,
                             mb_u16 addr,
                             const mb_u16 *values,
                             mb_u16 quantity)
{
    if (region->read_only) {
        return kExceptionIllegalDataAddress;
    }

    if (region->write_cb) {
        return region->write_cb(addr, values, quantity, region->user_ctx);
    }

    if (!region->storage) {
        return kExceptionServerDeviceFailure;
    }

    const mb_u16 offset = (mb_u16)(addr - region->start);
    for (mb_u16 i = 0; i < quantity; ++i) {
        region->storage[offset + i] = values[i];
    }

    return MB_OK;
}

static const mb_server_region_t *find_region(const mb_server_t *server, mb_u16 start, mb_u16 quantity)
{
    for (mb_size_t i = 0; i < server->region_count; ++i) {
        const mb_server_region_t *region = &server->regions[i];
        if (region_contains(region, start, quantity)) {
            return region;
        }
    }
    return NULL;
}

static mb_err_t handle_fc03(mb_server_t *server,
                            const mb_server_region_t *region,
                            mb_u16 start,
                            mb_u16 quantity,
                            mb_size_t *out_pdu_len)
{
    mb_u16 registers[MB_PDU_FC03_MAX_REGISTERS];
    mb_err_t err = region_read(region, start, quantity, registers);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    mb_err_t build = mb_pdu_build_read_holding_response(server->tx_buffer,
                                                        sizeof(server->tx_buffer),
                                                        registers,
                                                        quantity);
    if (!mb_err_is_ok(build)) {
        return kExceptionServerDeviceFailure;
    }

    *out_pdu_len = 2U + (mb_size_t)quantity * 2U;
    return MB_OK;
}

static mb_err_t handle_fc06(mb_server_t *server,
                            const mb_server_region_t *region,
                            mb_u16 address,
                            mb_u16 value,
                            mb_size_t *out_pdu_len)
{
    mb_err_t err = region_write(region, address, &value, 1U);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    mb_err_t build = mb_pdu_build_write_single_response(server->tx_buffer,
                                                         sizeof(server->tx_buffer),
                                                         address,
                                                         value);
    if (!mb_err_is_ok(build)) {
        return kExceptionServerDeviceFailure;
    }

    *out_pdu_len = 5U;
    return MB_OK;
}

static mb_err_t handle_fc16(mb_server_t *server,
                            const mb_server_region_t *region,
                            mb_u16 start,
                            mb_u16 quantity,
                            const mb_u8 *payload,
                            mb_size_t payload_len,
                            mb_size_t *out_pdu_len)
{
    if (payload_len < (mb_size_t)(6U + quantity * 2U)) {
        return kExceptionIllegalDataValue;
    }

    mb_u16 values[MB_PDU_FC16_MAX_REGISTERS];
    for (mb_u16 i = 0; i < quantity; ++i) {
        values[i] = (mb_u16)((payload[i * 2U] << 8) | payload[i * 2U + 1U]);
    }

    mb_err_t err = region_write(region, start, values, quantity);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    mb_err_t build = mb_pdu_build_write_multiple_response(server->tx_buffer,
                                                           sizeof(server->tx_buffer),
                                                           start,
                                                           quantity);
    if (!mb_err_is_ok(build)) {
        return kExceptionServerDeviceFailure;
    }

    *out_pdu_len = 5U;
    return MB_OK;
}

static mb_err_t map_error_to_exception(mb_err_t err)
{
    if (mb_err_is_ok(err)) {
        return MB_OK;
    }

    if (mb_err_is_exception(err)) {
        return err;
    }

    return kExceptionServerDeviceFailure;
}

static void server_update_latency(mb_server_t *server, const mb_server_request_t *req)
{
    if (server == NULL || req == NULL || req->start_time == 0U) {
        return;
    }

    const mb_time_ms_t now = server_now(server);
    if (now >= req->start_time) {
        server->metrics.latency_total_ms += (now - req->start_time);
    }
}

static void server_handle_poison(mb_server_t *server)
{
    if (server == NULL) {
        return;
    }

    server->metrics.poison_triggers += 1U;

    while (server->pending_head != NULL) {
        mb_server_request_t *pending = server_dequeue(server);
        if (pending == NULL) {
            break;
        }

        if (!pending->broadcast) {
            mb_err_t send_status = server_send_exception(server, &pending->request_view,
                                                         (mb_u8)kExceptionServerDeviceFailure);
            if (mb_err_is_ok(send_status)) {
                server->metrics.exceptions += 1U;
            } else {
                server->metrics.errors += 1U;
                mb_diag_record_error(&server->diag, send_status);
            }
        }

        server->metrics.dropped += 1U;
        mb_diag_record_error(&server->diag, MB_ERR_CANCELLED);
        server_emit_request_event(server, MB_EVENT_SERVER_REQUEST_COMPLETE, pending, MB_ERR_CANCELLED);
        server_release_request(pending);
    }
}

static void server_execute_request(mb_server_t *server, mb_server_request_t *req)
{
    if (server == NULL || req == NULL) {
        return;
    }

    mb_size_t response_pdu_len = 0U;
    mb_err_t err = MB_OK;

    const mb_u8 *pdu = req->storage;
    const mb_size_t pdu_len = req->pdu_len;
    const mb_u8 function = req->function;

    switch (function) {
    case MB_PDU_FC_READ_HOLDING_REGISTERS: {
        mb_u16 start = 0U;
        mb_u16 quantity = 0U;
        err = mb_pdu_parse_read_holding_request(pdu, pdu_len, &start, &quantity);
        if (!mb_err_is_ok(err)) {
            err = kExceptionIllegalDataValue;
            break;
        }

        const mb_server_region_t *region = find_region(server, start, quantity);
        if (!region) {
            err = kExceptionIllegalDataAddress;
            break;
        }

        err = handle_fc03(server, region, start, quantity, &response_pdu_len);
        break;
    }
    case MB_PDU_FC_WRITE_SINGLE_REGISTER: {
        mb_u16 address = 0U;
        mb_u16 value = 0U;
        err = mb_pdu_parse_write_single_request(pdu, pdu_len, &address, &value);
        if (!mb_err_is_ok(err)) {
            err = kExceptionIllegalDataValue;
            break;
        }

        const mb_server_region_t *region = find_region(server, address, 1U);
        if (!region) {
            err = kExceptionIllegalDataAddress;
            break;
        }

        err = handle_fc06(server, region, address, value, &response_pdu_len);
        break;
    }
    case MB_PDU_FC_WRITE_MULTIPLE_REGISTERS: {
        mb_u16 start = 0U;
        mb_u16 quantity = 0U;
        const mb_u8 *value_bytes = NULL;
        err = mb_pdu_parse_write_multiple_request(pdu, pdu_len, &start, &quantity, &value_bytes);
        if (!mb_err_is_ok(err)) {
            err = kExceptionIllegalDataValue;
            break;
        }

        const mb_server_region_t *region = find_region(server, start, quantity);
        if (!region) {
            err = kExceptionIllegalDataAddress;
            break;
        }

        err = handle_fc16(server, region, start, quantity, value_bytes, pdu_len, &response_pdu_len);
        break;
    }
    default:
        err = kExceptionIllegalFunction;
        break;
    }

    err = map_error_to_exception(err);
    mb_err_t completion = err;

    if (req->broadcast) {
        if (!mb_err_is_ok(err)) {
            server->metrics.errors += 1U;
        }
        server_update_latency(server, req);
        mb_diag_record_error(&server->diag, completion);
        server_emit_request_event(server, MB_EVENT_SERVER_REQUEST_COMPLETE, req, completion);
        return;
    }

    if (err == MB_OK) {
        mb_adu_view_t response = {
            .unit_id = server->unit_id,
            .function = server->tx_buffer[0],
            .payload = &server->tx_buffer[1],
            .payload_len = (response_pdu_len > 0U) ? (response_pdu_len - 1U) : 0U,
        };

        server_trace_hex_buffer(server, "server.tx", server->tx_buffer, response_pdu_len);

        mb_err_t tx = mb_rtu_submit(&server->rtu, &response);
        if (mb_err_is_ok(tx)) {
            server->metrics.responded += 1U;
        } else {
            server->metrics.errors += 1U;
            completion = tx;
        }
    } else {
        if (mb_err_is_ok(server_send_exception(server, &req->request_view, (mb_u8)err))) {
            server->metrics.exceptions += 1U;
        } else {
            server->metrics.errors += 1U;
            completion = MB_ERR_TRANSPORT;
        }
    }

    server_update_latency(server, req);
    mb_diag_record_error(&server->diag, completion);
    server_emit_request_event(server, MB_EVENT_SERVER_REQUEST_COMPLETE, req, completion);
}

static void server_process_queue(mb_server_t *server)
{
    if (server == NULL) {
        return;
    }

    const mb_time_ms_t now = server_now(server);

    while (server->pending_head != NULL) {
        mb_server_request_t *candidate = server->pending_head;
        if (candidate->deadline == 0U || now < candidate->deadline) {
            break;
        }

        candidate = server_dequeue(server);
        if (candidate == NULL) {
            break;
        }

        server->metrics.timeouts += 1U;
        server->metrics.dropped += 1U;
        mb_diag_record_error(&server->diag, MB_ERR_TIMEOUT);
        if (!candidate->broadcast) {
            mb_err_t send_status = server_send_exception(server, &candidate->request_view,
                                                         (mb_u8)kExceptionServerDeviceFailure);
            if (mb_err_is_ok(send_status)) {
                server->metrics.exceptions += 1U;
            } else {
                server->metrics.errors += 1U;
                mb_diag_record_error(&server->diag, send_status);
            }
        }
        server_emit_request_event(server, MB_EVENT_SERVER_REQUEST_COMPLETE, candidate, MB_ERR_TIMEOUT);
        server_release_request(candidate);
    }

    mb_server_request_t *req = server_dequeue(server);
    if (req == NULL) {
        server_transition_state(server, MB_SERVER_STATE_IDLE);
        return;
    }

    server->current = req;
    req->start_time = server_now(server);

    if (req->poison) {
        server_transition_state(server, MB_SERVER_STATE_DRAINING);
        server_handle_poison(server);
        server_release_request(req);
        server->current = NULL;
        server_transition_state(server, MB_SERVER_STATE_IDLE);
        return;
    }

    server_transition_state(server, MB_SERVER_STATE_PROCESSING);
    server_execute_request(server, req);
    server_release_request(req);
    server->current = NULL;
    server_transition_state(server, MB_SERVER_STATE_IDLE);
}

static void server_record_drop(mb_server_t *server,
                               const mb_adu_view_t *adu,
                               bool broadcast,
                               mb_err_t exception)
{
    if (server == NULL) {
        return;
    }

    server->metrics.dropped += 1U;
    if (!broadcast && adu != NULL) {
        if (mb_err_is_ok(server_send_exception(server, adu, (mb_u8)exception))) {
            server->metrics.exceptions += 1U;
        } else {
            server->metrics.errors += 1U;
        }
    }
}

static mb_err_t server_accept_adu(mb_server_t *server,
                                  const mb_adu_view_t *adu,
                                  mb_err_t status)
{
    if (server == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (!mb_err_is_ok(status) || adu == NULL) {
        server->metrics.errors += 1U;
        const mb_err_t diag_status = (!mb_err_is_ok(status)) ? status : MB_ERR_INVALID_ARGUMENT;
        mb_diag_record_error(&server->diag, diag_status);
        return status;
    }

    const bool broadcast = (adu->unit_id == 0U);
    if (!broadcast && adu->unit_id != server->unit_id) {
        return MB_OK;
    }

    const mb_size_t pdu_len = adu->payload_len + 1U;
    if (pdu_len > MB_PDU_MAX) {
        mb_diag_record_error(&server->diag, MB_ERR_INVALID_ARGUMENT);
        server_record_drop(server, adu, broadcast, kExceptionIllegalDataValue);
        return MB_ERR_INVALID_ARGUMENT;
    }

    const bool high_priority = server_is_high_priority_fc(adu->function);
    if (!high_priority && !broadcast && server->queue_capacity > 0U &&
        server_total_inflight(server) >= server->queue_capacity) {
        mb_diag_record_error(&server->diag, MB_ERR_NO_RESOURCES);
        server_record_drop(server, adu, broadcast, kExceptionServerDeviceFailure);
        return MB_ERR_NO_RESOURCES;
    }

    mb_server_request_t *req = server_alloc_request(server);
    if (req == NULL) {
        mb_diag_record_error(&server->diag, MB_ERR_NO_RESOURCES);
        server_record_drop(server, adu, broadcast, kExceptionServerDeviceFailure);
        return MB_ERR_NO_RESOURCES;
    }

    req->queued = true;
    req->broadcast = broadcast;
    req->high_priority = high_priority;
    req->poison = false;
    req->function = adu->function;
    req->flags = 0U;
    req->unit_id = adu->unit_id;
    req->pdu_len = pdu_len;

    req->storage[0] = adu->function;
    if (adu->payload_len > 0U) {
        memcpy(&req->storage[1], adu->payload, adu->payload_len);
    }

    req->request_view.unit_id = adu->unit_id;
    req->request_view.function = adu->function;
    req->request_view.payload = (adu->payload_len > 0U) ? &req->storage[1] : NULL;
    req->request_view.payload_len = adu->payload_len;

    const mb_time_ms_t now = server_now(server);
    req->enqueue_time = now;
    req->deadline = now + server_fc_timeout(server, adu->function);

    server_trace_hex_buffer(server, "server.rx", req->storage, req->pdu_len);

    server_enqueue(server, req);
    server->metrics.received += 1U;
    if (broadcast) {
        server->metrics.broadcasts += 1U;
    }

    mb_diag_record_fc(&server->diag, req->function);
    server_emit_request_event(server, MB_EVENT_SERVER_REQUEST_ACCEPT, req, MB_OK);

    return MB_OK;
}

static void mb_server_rtu_callback(mb_rtu_transport_t *rtu,
                                   const mb_adu_view_t *adu,
                                   mb_err_t status,
                                   void *user)
{
    (void)rtu;
    mb_server_t *server = (mb_server_t *)user;
    (void)server_accept_adu(server, adu, status);
}

mb_err_t mb_server_init(mb_server_t *server,
                        const mb_transport_if_t *iface,
                        mb_u8 unit_id,
                        mb_server_region_t *regions,
                        mb_size_t region_capacity,
                        mb_server_request_t *request_pool,
                        mb_size_t request_pool_len)
{
    if (!server || !iface || !regions || region_capacity == 0U || !request_pool || request_pool_len == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    memset(server, 0, sizeof(*server));

    server->iface = iface;
    server->unit_id = unit_id;
    server->regions = regions;
    server->region_cap = region_capacity;
    server->pool = request_pool;
    server->pool_size = request_pool_len;
    server->queue_capacity = request_pool_len;
    server->state = MB_SERVER_STATE_IDLE;

    for (mb_size_t i = 0U; i < request_pool_len; ++i) {
        server_reset_request(&request_pool[i]);
    }
    memset(server->fc_timeouts, 0, sizeof(server->fc_timeouts));
    memset(&server->metrics, 0, sizeof(server->metrics));
    mb_diag_reset(&server->diag);

    mb_err_t err = mb_rtu_init(&server->rtu, iface, mb_server_rtu_callback, server);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    return MB_OK;
}

void mb_server_reset(mb_server_t *server)
{
    if (!server) {
        return;
    }

    server->region_count = 0U;
    server->pending_head = NULL;
    server->pending_tail = NULL;
    server->current = NULL;
    server->pending_count = 0U;

    for (mb_size_t i = 0U; i < server->pool_size; ++i) {
        server_reset_request(&server->pool[i]);
    }

    mb_rtu_reset(&server->rtu);
    mb_diag_reset(&server->diag);
    server_transition_state(server, MB_SERVER_STATE_IDLE);
}

static mb_err_t insert_region(mb_server_t *server, const mb_server_region_t *cfg)
{
    if (server->region_count >= server->region_cap) {
        return MB_ERR_OTHER;
    }

    for (mb_size_t i = 0; i < server->region_count; ++i) {
        const mb_server_region_t *existing = &server->regions[i];
        if (ranges_overlap(existing->start, existing->count, cfg->start, cfg->count)) {
            return MB_ERR_INVALID_ARGUMENT;
        }
    }

    server->regions[server->region_count++] = *cfg;
    return MB_OK;
}

mb_err_t mb_server_add_region(mb_server_t *server,
                              mb_u16 start,
                              mb_u16 count,
                              bool read_only,
                              mb_server_read_fn read_cb,
                              mb_server_write_fn write_cb,
                              void *user_ctx)
{
    if (!server || count == 0U || (!read_cb && !write_cb)) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if ((mb_u32)start + count > 0x10000UL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_server_region_t region = {
        .start = start,
        .count = count,
        .read_only = read_only,
        .read_cb = read_cb,
        .write_cb = write_cb,
        .user_ctx = user_ctx,
        .storage = NULL,
    };

    return insert_region(server, &region);
}

mb_err_t mb_server_add_storage(mb_server_t *server,
                               mb_u16 start,
                               mb_u16 count,
                               bool read_only,
                               mb_u16 *storage)
{
    if (!server || !storage || count == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if ((mb_u32)start + count > 0x10000UL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_server_region_t region = {
        .start = start,
        .count = count,
        .read_only = read_only,
        .read_cb = NULL,
        .write_cb = NULL,
        .user_ctx = NULL,
        .storage = storage,
    };

    return insert_region(server, &region);
}

mb_err_t mb_server_poll(mb_server_t *server)
{
    if (!server) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_err_t status = mb_rtu_poll(&server->rtu);

    server_process_queue(server);

    return status;
}

mb_err_t mb_server_inject_adu(mb_server_t *server, const mb_adu_view_t *adu)
{
    if (server == NULL || adu == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    return server_accept_adu(server, adu, MB_OK);
}

void mb_server_get_diag(const mb_server_t *server, mb_diag_counters_t *out_diag)
{
    if (server == NULL || out_diag == NULL) {
        return;
    }

    *out_diag = server->diag;
}

void mb_server_reset_diag(mb_server_t *server)
{
    if (server == NULL) {
        return;
    }

    mb_diag_reset(&server->diag);
}

void mb_server_set_event_callback(mb_server_t *server, mb_event_callback_t callback, void *user_ctx)
{
    if (server == NULL) {
        return;
    }

    server->observer_cb = callback;
    server->observer_user = (callback != NULL) ? user_ctx : NULL;

    if (callback != NULL) {
        server_emit_state_event(server, MB_EVENT_SERVER_STATE_ENTER, server->state);
    }
}

void mb_server_set_trace_hex(mb_server_t *server, bool enable)
{
    if (server == NULL) {
        return;
    }

    server->trace_hex = enable;
}

mb_size_t mb_server_pending(const mb_server_t *server)
{
    return server_total_inflight(server);
}

bool mb_server_is_idle(const mb_server_t *server)
{
    if (server == NULL) {
        return true;
    }
    return (server->pending_head == NULL) && (server->current == NULL);
}

void mb_server_set_queue_capacity(mb_server_t *server, mb_size_t capacity)
{
    if (server == NULL) {
        return;
    }

    if (capacity == 0U || capacity > server->pool_size) {
        server->queue_capacity = server->pool_size;
    } else {
        server->queue_capacity = capacity;
    }
}

mb_size_t mb_server_queue_capacity(const mb_server_t *server)
{
    if (server == NULL) {
        return 0U;
    }
    return server->queue_capacity;
}

void mb_server_set_fc_timeout(mb_server_t *server, mb_u8 function, mb_time_ms_t timeout_ms)
{
    if (server == NULL) {
        return;
    }
    server->fc_timeouts[function] = timeout_ms;
}

mb_err_t mb_server_submit_poison(mb_server_t *server)
{
    if (server == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_server_request_t *req = server_alloc_request(server);
    if (req == NULL) {
        return MB_ERR_NO_RESOURCES;
    }

    req->queued = true;
    req->broadcast = false;
    req->high_priority = true;
    req->poison = true;
    req->function = 0U;
    req->flags = 0U;
    req->unit_id = server->unit_id;
    req->pdu_len = 0U;
    req->request_view.unit_id = server->unit_id;
    req->request_view.function = 0U;
    req->request_view.payload = NULL;
    req->request_view.payload_len = 0U;

    const mb_time_ms_t now = server_now(server);
    req->enqueue_time = now;
    req->deadline = now + MB_SERVER_MAX_TIMEOUT_MS;

    server_enqueue(server, req);

    return MB_OK;
}

void mb_server_get_metrics(const mb_server_t *server, mb_server_metrics_t *out_metrics)
{
    if (server == NULL || out_metrics == NULL) {
        return;
    }

    *out_metrics = server->metrics;
}

void mb_server_reset_metrics(mb_server_t *server)
{
    if (server == NULL) {
        return;
    }
    memset(&server->metrics, 0, sizeof(server->metrics));
}
