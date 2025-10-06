#include <modbus/base.h>
#include <modbus/client.h>
#include <modbus/mapping.h>
#include <modbus/mb_err.h>
#include <modbus/mb_log.h>
#include <modbus/pdu.h>
#include <modbus/server.h>
#include <modbus/transport_if.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOOP_CAPACITY 512U
#define DEMO_UNIT_ID  0x11U

typedef struct {
    mb_u8 data[LOOP_CAPACITY];
    mb_size_t head;
    mb_size_t tail;
    mb_size_t size;
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
    queue->tail = 0U;
    queue->size = 0U;
}

static void loop_link_init(loop_link_t *link)
{
    loop_queue_reset(&link->client_to_server);
    loop_queue_reset(&link->server_to_client);
    link->now_ms = 0U;
}

static mb_size_t loop_queue_available(const loop_queue_t *queue)
{
    return LOOP_CAPACITY - queue->size;
}

static void loop_queue_push(loop_queue_t *queue, const mb_u8 *data, mb_size_t len)
{
    for (mb_size_t i = 0U; i < len && queue->size < LOOP_CAPACITY; ++i) {
        queue->data[queue->tail] = data[i];
        queue->tail = (queue->tail + 1U) % LOOP_CAPACITY;
        queue->size += 1U;
    }
}

static mb_size_t loop_queue_pop(loop_queue_t *queue, mb_u8 *out, mb_size_t cap)
{
    mb_size_t count = 0U;
    while (count < cap && queue->size > 0U) {
        out[count++] = queue->data[queue->head];
        queue->head = (queue->head + 1U) % LOOP_CAPACITY;
        queue->size -= 1U;
    }
    return count;
}

static mb_err_t loop_send(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out)
{
    loop_endpoint_t *endpoint = (loop_endpoint_t *)ctx;
    if (endpoint == NULL || buf == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_size_t capacity = loop_queue_available(endpoint->tx);
    const mb_size_t to_copy = (len > capacity) ? capacity : len;
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
    if (out) {
        out->processed = read;
    }
    return (read > 0U) ? MB_OK : MB_ERR_TIMEOUT;
}

static mb_time_ms_t loop_now(void *ctx)
{
    loop_endpoint_t *endpoint = (loop_endpoint_t *)ctx;
    return endpoint ? endpoint->clock->now_ms : 0U;
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
    mb_u8 function;
    mb_u8 payload[MB_PDU_MAX];
    mb_size_t payload_len;
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
    result->function = 0U;
    result->payload_len = 0U;

    if (mb_err_is_ok(status) && response != NULL) {
        result->function = response->function;
        if (response->payload_len > 0U) {
            if (response->payload_len > MB_PDU_MAX) {
                result->payload_len = MB_PDU_MAX;
            } else {
                result->payload_len = response->payload_len;
            }
            memcpy(result->payload, response->payload, result->payload_len);
        }
    }
}

static mb_err_t run_transaction(mb_client_t *client,
                                mb_server_t *server,
                                loop_link_t *link,
                                mb_client_request_t *request,
                                client_result_t *out)
{
    mb_client_txn_t *txn = NULL;
    out->completed = false;
    out->status = MB_OK;
    out->function = 0U;
    out->payload_len = 0U;

    mb_err_t status = mb_client_submit(client, request, &txn);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    const unsigned max_steps = 4096U;
    for (unsigned step = 0U; step < max_steps; ++step) {
        status = mb_client_poll(client);
        if (!mb_err_is_ok(status) && status != MB_ERR_TIMEOUT) {
            return status;
        }

        status = mb_server_poll(server);
        if (!mb_err_is_ok(status) && status != MB_ERR_TIMEOUT) {
            return status;
        }

        if (out->completed) {
            return out->status;
        }

        loop_advance(link, 1U);
    }

    return MB_ERR_TIMEOUT;
}

static void dump_registers(const mb_u16 *storage, mb_size_t count)
{
    printf("[server] holding registers:\n");
    for (mb_size_t i = 0U; i < count; ++i) {
        printf("  %04zu: 0x%04X\n", (size_t)i, storage[i]);
    }
}

int main(void)
{
    mb_log_bootstrap_defaults();
    printf("modbus::unit_test_loop_demo — Gate 15 mapping showcase\n");

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

    mb_server_t server;
    mb_server_region_t regions[4];
    mb_server_request_t request_pool[6];

    mb_u16 holding_rw[8];
    mb_u16 holding_ro[4];
    for (size_t i = 0; i < MB_COUNTOF(holding_rw); ++i) {
        holding_rw[i] = (mb_u16)(0x1000U + i);
    }
    for (size_t i = 0; i < MB_COUNTOF(holding_ro); ++i) {
        holding_ro[i] = (mb_u16)(0x2000U + i);
    }

    const mb_server_mapping_bank_t banks[] = {
        {
            .start = 0x0000U,
            .count = (mb_u16)MB_COUNTOF(holding_rw),
            .storage = holding_rw,
            .read_only = false,
        },
        {
            .start = 0x0020U,
            .count = (mb_u16)MB_COUNTOF(holding_ro),
            .storage = holding_ro,
            .read_only = true,
        },
    };

    const mb_server_mapping_config_t mapping_cfg = {
        .iface = &server_iface,
        .unit_id = DEMO_UNIT_ID,
        .regions = regions,
        .region_capacity = MB_COUNTOF(regions),
        .request_pool = request_pool,
        .request_capacity = MB_COUNTOF(request_pool),
        .banks = banks,
        .bank_count = MB_COUNTOF(banks),
    };

    mb_err_t status = mb_server_mapping_init(&server, &mapping_cfg);
    if (!mb_err_is_ok(status)) {
        printf("Failed to initialise server: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }

    mb_client_t client;
    mb_client_txn_t client_pool[6];
    status = mb_client_init(&client, &client_iface, client_pool, MB_COUNTOF(client_pool));
    if (!mb_err_is_ok(status)) {
        printf("Failed to initialise client: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }

    client_result_t result;

    mb_u8 frame[MB_PDU_MAX];
    mb_size_t frame_len = 0U;

    printf("[demo] Step 1: read holding registers 0x0000..0x0003\n");
    status = mb_pdu_build_read_holding_request(frame, sizeof frame, 0x0000U, 4U);
    if (!mb_err_is_ok(status)) {
        printf("Failed to build read request: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }
    frame_len = 5U;

    mb_client_request_t request = {
        .request = {
            .unit_id = DEMO_UNIT_ID,
            .function = frame[0],
            .payload = &frame[1],
            .payload_len = frame_len - 1U,
        },
        .timeout_ms = 500U,
        .max_retries = 1U,
        .callback = client_callback,
        .user_ctx = &result,
    };

    status = run_transaction(&client, &server, &link, &request, &result);
    if (!mb_err_is_ok(status)) {
        printf("Client request failed: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }
    if ((result.function & MB_PDU_EXCEPTION_BIT) != 0U) {
        printf("Unexpected exception response: 0x%02X\n", result.payload_len ? result.payload[0] : 0U);
        return EXIT_FAILURE;
    }
    dump_registers(holding_rw, MB_COUNTOF(holding_rw));

    printf("[demo] Step 2: write single register 0x0001 <- 0x1234\n");
    status = mb_pdu_build_write_single_request(frame, sizeof frame, 0x0001U, 0x1234U);
    if (!mb_err_is_ok(status)) {
        printf("Failed to build single write: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }
    frame_len = 5U;
    request.request.function = frame[0];
    request.request.payload = &frame[1];
    request.request.payload_len = frame_len - 1U;

    status = run_transaction(&client, &server, &link, &request, &result);
    if (!mb_err_is_ok(status)) {
        printf("Write single failed: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }
    printf("  register[1] is now 0x%04X\n", holding_rw[1]);

    printf("[demo] Step 3: write multiple registers 0x0002..0x0004\n");
    const mb_u16 multi_values[] = {0x0001U, 0x0002U, 0xBEEF};
    status = mb_pdu_build_write_multiple_request(frame, sizeof frame, 0x0002U, multi_values, MB_COUNTOF(multi_values));
    if (!mb_err_is_ok(status)) {
        printf("Failed to build multiple write: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }
    frame_len = (mb_size_t)(6U + MB_COUNTOF(multi_values) * 2U);
    request.request.function = frame[0];
    request.request.payload = &frame[1];
    request.request.payload_len = frame_len - 1U;

    status = run_transaction(&client, &server, &link, &request, &result);
    if (!mb_err_is_ok(status)) {
        printf("Write multiple failed: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }
    dump_registers(holding_rw, MB_COUNTOF(holding_rw));

    printf("[demo] Step 4: read back holding registers 0x0000..0x0005\n");
    status = mb_pdu_build_read_holding_request(frame, sizeof frame, 0x0000U, 6U);
    if (!mb_err_is_ok(status)) {
        printf("Failed to rebuild read request: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }
    frame_len = 5U;
    request.request.function = frame[0];
    request.request.payload = &frame[1];
    request.request.payload_len = frame_len - 1U;

    status = run_transaction(&client, &server, &link, &request, &result);
    if (!mb_err_is_ok(status)) {
        printf("Read back failed: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }

    if ((result.function & MB_PDU_EXCEPTION_BIT) == 0U && result.payload_len >= 13U) {
        printf("  server response byte count: %u\n", result.payload[0]);
    }

    printf("[demo] Step 5: attempt write to read-only bank (expect exception)\n");
    status = mb_pdu_build_write_single_request(frame, sizeof frame, 0x0020U, 0xFFFFU);
    if (!mb_err_is_ok(status)) {
        printf("Failed to build exception probe: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }
    frame_len = 5U;
    request.request.function = frame[0];
    request.request.payload = &frame[1];
    request.request.payload_len = frame_len - 1U;

    status = run_transaction(&client, &server, &link, &request, &result);
    if (!mb_err_is_ok(status)) {
        printf("Exception probe failed: %s\n", mb_err_str(status));
        return EXIT_FAILURE;
    }

    if ((result.function & MB_PDU_EXCEPTION_BIT) == 0U || result.payload_len == 0U) {
        printf("Expected exception but received normal response.\n");
        return EXIT_FAILURE;
    }

    printf("  server rejected write with exception code 0x%02X\n", result.payload[0]);
    printf("[demo] Sequence complete.\n");

    return EXIT_SUCCESS;
}
