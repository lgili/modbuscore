#include <modbus/base.h>
#include <modbus/client.h>
#include <modbus/mapping.h>
#include <modbus/mb_err.h>
#include <modbus/pdu.h>
#include <modbus/server.h>
#include <modbus/transport_if.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define LOOP_CAPACITY 256U
#define DEMO_UNIT_ID  0x20U

typedef struct {
    mb_u8 data[LOOP_CAPACITY];
    mb_size_t head;
    mb_size_t used;
} loop_queue_t;

typedef struct {
    loop_queue_t client_to_server;
    loop_queue_t server_to_client;
    mb_time_ms_t now_ms;
} loop_link_t;

typedef struct {
    loop_queue_t *tx;
    loop_queue_t *rx;
    loop_link_t *clock;
} loop_endpoint_t;

static void loop_queue_reset(loop_queue_t *queue)
{
    queue->head = 0U;
    queue->used = 0U;
}

static void loop_link_init(loop_link_t *link)
{
    loop_queue_reset(&link->client_to_server);
    loop_queue_reset(&link->server_to_client);
    link->now_ms = 0U;
}

static mb_size_t loop_queue_available(const loop_queue_t *queue)
{
    return LOOP_CAPACITY - queue->used;
}

static void loop_queue_push(loop_queue_t *queue, const mb_u8 *data, mb_size_t len)
{
    mb_size_t tail = (queue->head + queue->used) % LOOP_CAPACITY;
    for (mb_size_t i = 0U; i < len && queue->used < LOOP_CAPACITY; ++i) {
        queue->data[tail] = data[i];
        tail = (tail + 1U) % LOOP_CAPACITY;
        queue->used += 1U;
    }
}

static mb_size_t loop_queue_pop(loop_queue_t *queue, mb_u8 *out, mb_size_t cap)
{
    mb_size_t count = 0U;
    while (count < cap && queue->used > 0U) {
        out[count++] = queue->data[queue->head];
        queue->head = (queue->head + 1U) % LOOP_CAPACITY;
        queue->used -= 1U;
    }
    return count;
}

static mb_err_t loop_send(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out)
{
    loop_endpoint_t *endpoint = (loop_endpoint_t *)ctx;
    if (endpoint == NULL || buf == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_size_t available = loop_queue_available(endpoint->tx);
    const mb_size_t to_copy = (len > available) ? available : len;
    loop_queue_push(endpoint->tx, buf, to_copy);
    if (out != NULL) {
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
    if (out != NULL) {
        out->processed = read;
    }
    return (read > 0U) ? MB_OK : MB_ERR_TIMEOUT;
}

static mb_time_ms_t loop_now(void *ctx)
{
    loop_endpoint_t *endpoint = (loop_endpoint_t *)ctx;
    return (endpoint != NULL && endpoint->clock != NULL) ? endpoint->clock->now_ms : 0U;
}

static void loop_yield(void *ctx)
{
    (void)ctx;
}

static void loop_bind(loop_endpoint_t *endpoint, mb_transport_if_t *iface)
{
    iface->ctx = endpoint;
    iface->send = loop_send;
    iface->recv = loop_recv;
    iface->now = loop_now;
    iface->yield = loop_yield;
}

static void loop_advance(loop_link_t *link, mb_time_ms_t delta_ms)
{
    link->now_ms += delta_ms;
}

typedef struct {
    bool completed;
    mb_err_t status;
    mb_adu_view_t response;
} demo_capture_t;

static void demo_callback(mb_client_t *client,
                          const mb_client_txn_t *txn,
                          mb_err_t status,
                          const mb_adu_view_t *response,
                          void *user_ctx)
{
    (void)client;
    (void)txn;
    demo_capture_t *capture = (demo_capture_t *)user_ctx;
    if (capture == NULL) {
        return;
    }

    capture->completed = true;
    capture->status = status;
    if (response != NULL) {
        capture->response = *response;
    }
}

static mb_err_t run_request(mb_client_t *client,
                            mb_server_t *server,
                            loop_link_t *link,
                            mb_client_request_t *request,
                            demo_capture_t *capture)
{
    capture->completed = false;
    capture->status = MB_OK;

    mb_err_t status = mb_client_submit(client, request, NULL);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    for (unsigned step = 0U; step < 2048U; ++step) {
        (void)mb_client_poll(client);
        (void)mb_server_poll(server);
        if (capture->completed) {
            return capture->status;
        }
        loop_advance(link, 1U);
    }

    return MB_ERR_TIMEOUT;
}

static void print_registers(const mb_u8 *payload, mb_size_t byte_count)
{
    const mb_size_t reg_count = byte_count / 2U;
    for (mb_size_t i = 0U; i < reg_count; ++i) {
        const mb_u16 value = (mb_u16)((payload[i * 2U] << 8) | payload[i * 2U + 1U]);
        printf("    [%02zu] 0x%04X\n", (size_t)i, value);
    }
}

int main(void)
{
    loop_link_t link;
    loop_link_init(&link);

    loop_endpoint_t client_ep = {
        .tx = &link.client_to_server,
        .rx = &link.server_to_client,
        .clock = &link,
    };
    loop_endpoint_t server_ep = {
        .tx = &link.server_to_client,
        .rx = &link.client_to_server,
        .clock = &link,
    };

    mb_transport_if_t client_iface;
    mb_transport_if_t server_iface;
    loop_bind(&client_ep, &client_iface);
    loop_bind(&server_ep, &server_iface);

    mb_client_t client;
    mb_client_txn_t txn_pool[4];
    mb_server_t server;
    mb_server_region_t regions[4];
    mb_server_request_t request_pool[4];

    mb_u16 holding_rw[8] = {0x1000U, 0x1100U, 0x1200U, 0x1300U, 0x1400U, 0x1500U, 0x1600U, 0x1700U};
    mb_u16 holding_ro[4] = {0x9000U, 0x9001U, 0x9002U, 0x9003U};

    if (!mb_err_is_ok(mb_client_init(&client, &client_iface, txn_pool, MB_COUNTOF(txn_pool)))) {
        fprintf(stderr, "Failed to initialise client core\n");
        return 1;
    }

    if (!mb_err_is_ok(mb_server_init(&server,
                                     &server_iface,
                                     DEMO_UNIT_ID,
                                     regions,
                                     MB_COUNTOF(regions),
                                     request_pool,
                                     MB_COUNTOF(request_pool)))) {
        fprintf(stderr, "Failed to initialise server core\n");
        return 1;
    }

    if (!mb_err_is_ok(mb_server_add_storage(&server, 0x0000U, MB_COUNTOF(holding_rw), false, holding_rw))) {
        fprintf(stderr, "Failed to register RW holding registers\n");
        return 1;
    }
    if (!mb_err_is_ok(mb_server_add_storage(&server, 0x0100U, MB_COUNTOF(holding_ro), true, holding_ro))) {
        fprintf(stderr, "Failed to register RO holding registers\n");
        return 1;
    }

    demo_capture_t capture;
    mb_client_request_t request;
    mb_u8 pdu[MB_PDU_MAX];

    printf("== Modbus loopback demo ==\n");

    // Read the first four holding registers.
    if (!mb_err_is_ok(mb_pdu_build_read_holding_request(pdu, sizeof pdu, 0x0000U, 4U))) {
        fprintf(stderr, "Failed to build read request PDU\n");
        return 1;
    }

    memset(&request, 0, sizeof(request));
    request.request.unit_id = DEMO_UNIT_ID;
    request.request.function = pdu[0];
    request.request.payload = &pdu[1];
    request.request.payload_len = 4U;
    request.timeout_ms = 250U;
    request.retry_backoff_ms = 25U;
    request.callback = demo_callback;
    request.user_ctx = &capture;

    mb_err_t status = run_request(&client, &server, &link, &request, &capture);
    if (!mb_err_is_ok(status)) {
        fprintf(stderr, "Initial read failed: %s\n", mb_err_str(status));
        return 1;
    }

    printf("Initial holding registers:\n");
    print_registers(capture.response.payload, capture.response.payload_len);

    // Update two registers via FC 0x10 (write multiple).
    mb_u16 new_values[2] = {0xA5A5U, 0x5A5AU};
    if (!mb_err_is_ok(mb_pdu_build_write_multiple_request(pdu,
                                                          sizeof pdu,
                                                          0x0002U,
                                                          new_values,
                                                          MB_COUNTOF(new_values)))) {
        fprintf(stderr, "Failed to build write request PDU\n");
        return 1;
    }

    memset(&request, 0, sizeof(request));
    request.request.unit_id = DEMO_UNIT_ID;
    request.request.function = pdu[0];
    request.request.payload = &pdu[1];
    request.request.payload_len = 5U + MB_COUNTOF(new_values) * 2U;
    request.timeout_ms = 250U;
    request.retry_backoff_ms = 25U;
    request.callback = demo_callback;
    request.user_ctx = &capture;

    status = run_request(&client, &server, &link, &request, &capture);
    if (!mb_err_is_ok(status)) {
        fprintf(stderr, "Write request failed: %s\n", mb_err_str(status));
        return 1;
    }

    // Re-read to confirm the update.
    if (!mb_err_is_ok(mb_pdu_build_read_holding_request(pdu, sizeof pdu, 0x0000U, 4U))) {
        fprintf(stderr, "Failed to rebuild read request PDU\n");
        return 1;
    }

    memset(&request, 0, sizeof(request));
    request.request.unit_id = DEMO_UNIT_ID;
    request.request.function = pdu[0];
    request.request.payload = &pdu[1];
    request.request.payload_len = 4U;
    request.timeout_ms = 250U;
    request.retry_backoff_ms = 25U;
    request.callback = demo_callback;
    request.user_ctx = &capture;

    status = run_request(&client, &server, &link, &request, &capture);
    if (!mb_err_is_ok(status)) {
        fprintf(stderr, "Second read failed: %s\n", mb_err_str(status));
        return 1;
    }

    printf("Holding registers after write:\n");
    print_registers(capture.response.payload, capture.response.payload_len);

    return 0;
}
