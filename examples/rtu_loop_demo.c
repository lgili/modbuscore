#include <modbus/base.h>
#include <modbus/client.h>
#include <modbus/mb_err.h>
#include <modbus/mb_log.h>
#include <modbus/observe.h>
#include <modbus/pdu.h>
#include <modbus/server.h>
#include <modbus/transport_if.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOOP_QUEUE_CAPACITY 512U

typedef struct {
    mb_u8 data[LOOP_QUEUE_CAPACITY];
    mb_size_t head;
    mb_size_t tail;
    mb_size_t size;
} loop_queue_t;

typedef struct {
    loop_queue_t a_to_b;
    loop_queue_t b_to_a;
    mb_time_ms_t clock_ms;
} loop_link_t;

typedef struct {
    loop_queue_t *tx;
    loop_queue_t *rx;
    loop_link_t *link;
} loop_endpoint_t;

static void loop_queue_reset(loop_queue_t *queue)
{
    queue->head = 0U;
    queue->tail = 0U;
    queue->size = 0U;
}

static void loop_link_init(loop_link_t *link)
{
    loop_queue_reset(&link->a_to_b);
    loop_queue_reset(&link->b_to_a);
    link->clock_ms = 0U;
}

static mb_size_t loop_queue_available(const loop_queue_t *queue)
{
    return LOOP_QUEUE_CAPACITY - queue->size;
}

static void loop_queue_push(loop_queue_t *queue, const mb_u8 *data, mb_size_t len)
{
    for (mb_size_t i = 0U; i < len && queue->size < LOOP_QUEUE_CAPACITY; ++i) {
        queue->data[queue->tail] = data[i];
        queue->tail = (queue->tail + 1U) % LOOP_QUEUE_CAPACITY;
        queue->size += 1U;
    }
}

static mb_size_t loop_queue_pop(loop_queue_t *queue, mb_u8 *out, mb_size_t cap)
{
    mb_size_t read = 0U;
    while (read < cap && queue->size > 0U) {
        out[read++] = queue->data[queue->head];
        queue->head = (queue->head + 1U) % LOOP_QUEUE_CAPACITY;
        queue->size -= 1U;
    }
    return read;
}

static mb_err_t loop_send(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out)
{
    loop_endpoint_t *endpoint = (loop_endpoint_t *)ctx;
    if (endpoint == NULL || buf == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_size_t space = loop_queue_available(endpoint->tx);
    const mb_size_t to_copy = (len > space) ? space : len;
    loop_queue_push(endpoint->tx, buf, to_copy);
    if (out) {
        out->processed = to_copy;
    }
    return (to_copy == len) ? MB_OK : MB_ERR_TRANSPORT;
}

static mb_err_t loop_recv(void *ctx, mb_u8 *buf, mb_size_t cap, mb_transport_io_result_t *out)
{
    loop_endpoint_t *endpoint = (loop_endpoint_t *)ctx;
    if (endpoint == NULL || buf == NULL || cap == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_size_t read = loop_queue_pop(endpoint->rx, buf, cap);
    if (read == 0U) {
        if (out) {
            out->processed = 0U;
        }
        return MB_ERR_TIMEOUT;
    }

    if (out) {
        out->processed = read;
    }
    return MB_OK;
}

static mb_time_ms_t loop_now(void *ctx)
{
    loop_endpoint_t *endpoint = (loop_endpoint_t *)ctx;
    return endpoint ? endpoint->link->clock_ms : 0U;
}

static void loop_yield(void *ctx)
{
    (void)ctx;
}

static void loop_bind_iface(loop_endpoint_t *endpoint, mb_transport_if_t *iface)
{
    iface->ctx = endpoint;
    iface->send = loop_send;
    iface->recv = loop_recv;
    iface->now = loop_now;
    iface->yield = loop_yield;
}

static void advance_time(loop_link_t *link, mb_time_ms_t delta_ms)
{
    link->clock_ms += delta_ms;
}

typedef struct {
    bool completed;
    mb_err_t status;
} client_result_t;

static void client_callback(mb_client_t *client,
                            const mb_client_txn_t *txn,
                            mb_err_t status,
                            const mb_adu_view_t *response,
                            void *user_ctx)
{
    (void)client;
    (void)txn;
    client_result_t *result = (client_result_t *)user_ctx;
    result->completed = true;
    result->status = status;

    if (mb_err_is_ok(status) && response && response->payload_len >= 1U) {
        const mb_u8 count = response->payload[0];
        printf("[client] received %u bytes\n", (unsigned)count);
        for (mb_size_t i = 1U; i + 1U < response->payload_len; i += 2U) {
            const uint16_t value = (uint16_t)((response->payload[i] << 8) | response->payload[i + 1U]);
            printf("  register[%u] = 0x%04X (%u)\n",
                   (unsigned)((i - 1U) / 2U),
                   value,
                   value);
        }
    } else {
        printf("[client] request failed: %s\n", mb_err_str(status));
    }
}

static void client_event_sink(const mb_event_t *event, void *user)
{
    (void)user;
    if (event == NULL) {
        return;
    }
    if (event->source == MB_EVENT_SOURCE_CLIENT && event->type == MB_EVENT_CLIENT_STATE_ENTER) {
        printf("[client] state -> %u\n", (unsigned)event->data.client_state.state);
    }
}

static void server_event_sink(const mb_event_t *event, void *user)
{
    (void)user;
    if (event == NULL) {
        return;
    }
    if (event->source == MB_EVENT_SOURCE_SERVER && event->type == MB_EVENT_SERVER_REQUEST_COMPLETE) {
        printf("[server] request fc=%u status=%d\n",
               (unsigned)event->data.server_req.function,
               event->data.server_req.status);
    }
}

int main(void)
{
    mb_log_bootstrap_defaults();
    printf("Modbus RTU loop demo (in-memory transport)\n");

    loop_link_t link;
    loop_link_init(&link);

    loop_endpoint_t client_ep = {
        .tx = &link.a_to_b,
        .rx = &link.b_to_a,
        .link = &link,
    };
    loop_endpoint_t server_ep = {
        .tx = &link.b_to_a,
        .rx = &link.a_to_b,
        .link = &link,
    };

    mb_transport_if_t client_iface;
    mb_transport_if_t server_iface;
    loop_bind_iface(&client_ep, &client_iface);
    loop_bind_iface(&server_ep, &server_iface);

    mb_server_t server;
    mb_server_region_t regions[2];
    mb_server_request_t request_pool[4];
    uint16_t holding_regs[16];
    for (size_t i = 0; i < MB_COUNTOF(holding_regs); ++i) {
        holding_regs[i] = (uint16_t)(0x1000U + i);
    }

    mb_err_t status = mb_server_init(&server,
                                     &server_iface,
                                     0x11U,
                                     regions,
                                     MB_COUNTOF(regions),
                                     request_pool,
                                     MB_COUNTOF(request_pool));
    if (!mb_err_is_ok(status)) {
        printf("Failed to initialise server: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }

    status = mb_server_add_storage(&server,
                                    0x0000U,
                                    (mb_u16)MB_COUNTOF(holding_regs),
                                    false,
                                    holding_regs);
    if (!mb_err_is_ok(status)) {
        printf("Failed to add server storage: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }
    mb_server_set_event_callback(&server, server_event_sink, NULL);
    mb_server_set_trace_hex(&server, true);

    mb_client_t client;
    mb_client_txn_t client_pool[4];
    status = mb_client_init(&client, &client_iface, client_pool, MB_COUNTOF(client_pool));
    if (!mb_err_is_ok(status)) {
        printf("Failed to initialise client: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }
    mb_client_set_event_callback(&client, client_event_sink, NULL);
    mb_client_set_trace_hex(&client, true);

    mb_u8 pdu[5];
    status = mb_pdu_build_read_holding_request(pdu, sizeof(pdu), 0x0000U, 4U);
    if (!mb_err_is_ok(status)) {
        printf("Failed to build PDU: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }

    client_result_t result = {.completed = false, .status = MB_OK};
    mb_client_request_t request = {
        .request = {
            .unit_id = 0x11U,
            .function = MB_PDU_FC_READ_HOLDING_REGISTERS,
            .payload = &pdu[1],
            .payload_len = sizeof(pdu) - 1U,
        },
        .timeout_ms = 500U,
        .max_retries = 1U,
        .callback = client_callback,
        .user_ctx = &result,
    };

    mb_client_txn_t *txn = NULL;
    status = mb_client_submit(&client, &request, &txn);
    if (!mb_err_is_ok(status)) {
        printf("Failed to submit client request: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }

    printf("[demo] request submitted, stepping loop...\n");

    const unsigned max_steps = 2000U;
    for (unsigned step = 0U; step < max_steps && !result.completed; ++step) {
        status = mb_server_poll(&server);
        if (!mb_err_is_ok(status) && status != MB_ERR_TIMEOUT) {
            printf("Server poll error: %s\n", mb_err_str(status));
            break;
        }

        status = mb_client_poll(&client);
        if (!mb_err_is_ok(status) && status != MB_ERR_TIMEOUT) {
            printf("Client poll error: %s\n", mb_err_str(status));
            break;
        }
        advance_time(&link, 1U);
    }

    if (!result.completed) {
        printf("[demo] client did not complete within loop budget.\n");
        return EXIT_FAILURE;
    }

    printf("[demo] completed with status %s\n", mb_err_str(result.status));
    return mb_err_is_ok(result.status) ? EXIT_SUCCESS : EXIT_FAILURE;
}
