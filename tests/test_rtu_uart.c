#include <assert.h>
#include <modbuscore/protocol/crc.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/rtu_uart.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
    uint8_t tx_buffer[256];
    size_t tx_len;

    uint8_t rx_buffer[256];
    size_t rx_len;

    uint64_t current_time;
    uint32_t delay_total;
    size_t flush_count;

    size_t next_read_chunk;
    int fail_partial_write;
} fake_uart_t;

static size_t fake_write(void* ctx, const uint8_t* data, size_t length)
{
    fake_uart_t* uart = ctx;
    if (uart->fail_partial_write && length > 0U) {
        uart->fail_partial_write = 0;
        return length - 1U;
    }

    memcpy(&uart->tx_buffer[uart->tx_len], data, length);
    uart->tx_len += length;
    return length;
}

static size_t fake_read(void* ctx, uint8_t* data, size_t capacity)
{
    fake_uart_t* uart = ctx;
    size_t to_copy = (uart->rx_len < capacity) ? uart->rx_len : capacity;
    if (uart->next_read_chunk > 0U && uart->next_read_chunk < to_copy) {
        to_copy = uart->next_read_chunk;
    }
    if (to_copy == 0U) {
        return 0U;
    }
    memcpy(data, uart->rx_buffer, to_copy);
    uart->rx_len -= to_copy;
    memmove(uart->rx_buffer, uart->rx_buffer + to_copy, uart->rx_len);
    return to_copy;
}

static void fake_flush(void* ctx)
{
    fake_uart_t* uart = ctx;
    uart->flush_count++;
}

static uint64_t fake_now(void* ctx)
{
    fake_uart_t* uart = ctx;
    uart->current_time += 50U;
    return uart->current_time;
}

static void fake_delay(void* ctx, uint32_t micros)
{
    fake_uart_t* uart = ctx;
    uart->delay_total += micros;
    uart->current_time += micros;
}

static mbc_rtu_uart_ctx_t* create_driver(fake_uart_t* backend, mbc_transport_iface_t* iface,
                                         uint32_t baud, uint32_t guard_override, size_t rx_capacity)
{
    mbc_rtu_uart_config_t cfg = {
        .backend =
            {
                .ctx = backend,
                .write = fake_write,
                .read = fake_read,
                .flush = fake_flush,
                .now_us = fake_now,
                .delay_us = fake_delay,
            },
        .baud_rate = baud,
        .guard_time_us = guard_override,
        .rx_buffer_capacity = rx_capacity,
    };

    mbc_rtu_uart_ctx_t* ctx = NULL;
    assert(mbc_rtu_uart_create(&cfg, iface, &ctx) == MBC_STATUS_OK);
    return ctx;
}

static void test_guard_time_respected(void)
{
    fake_uart_t backend = {0};
    mbc_transport_iface_t iface;
    mbc_rtu_uart_ctx_t* ctx = create_driver(&backend, &iface, 9600U, 0U, 64U);

    uint8_t frame[4] = {0x11, 0x22, 0x33, 0x44};
    mbc_transport_io_t io = {0};

    assert(mbc_transport_send(&iface, frame, sizeof(frame), &io) == MBC_STATUS_OK);
    assert(io.processed == sizeof(frame));
    uint32_t first_delay = backend.delay_total;

    assert(mbc_transport_send(&iface, frame, sizeof(frame), &io) == MBC_STATUS_OK);
    assert(io.processed == sizeof(frame));
    assert(backend.delay_total > first_delay);

    mbc_rtu_uart_destroy(ctx);
}

static void test_receive_and_flush(void)
{
    fake_uart_t backend = {0};
    mbc_transport_iface_t iface;
    mbc_rtu_uart_ctx_t* ctx = create_driver(&backend, &iface, 19200U, 0U, 32U);

    const uint8_t payload[] = {0xAA, 0xBB, 0xCC, 0xDD};
    memcpy(backend.rx_buffer, payload, sizeof(payload));
    backend.rx_len = sizeof(payload);

    uint8_t out[4] = {0};
    mbc_transport_io_t io = {0};
    assert(mbc_transport_receive(&iface, out, sizeof(out), &io) == MBC_STATUS_OK);
    assert(io.processed == sizeof(payload));
    assert(memcmp(out, payload, sizeof(payload)) == 0);

    assert(mbc_transport_receive(&iface, out, sizeof(out), &io) == MBC_STATUS_OK);
    assert(io.processed == 0U);

    uint8_t frame[2] = {0x01, 0x02};
    backend.flush_count = 0U;
    assert(mbc_transport_send(&iface, frame, sizeof(frame), &io) == MBC_STATUS_OK);
    assert(backend.flush_count == 1U);

    mbc_rtu_uart_destroy(ctx);
}

static void test_partial_write_error(void)
{
    fake_uart_t backend = {0};
    mbc_transport_iface_t iface;
    mbc_rtu_uart_ctx_t* ctx = create_driver(&backend, &iface, 9600U, 0U, 32U);

    uint8_t frame[3] = {0x10, 0x20, 0x30};
    backend.fail_partial_write = 1;
    mbc_transport_io_t io = {0};
    assert(mbc_transport_send(&iface, frame, sizeof(frame), &io) == MBC_STATUS_IO_ERROR);

    mbc_rtu_uart_destroy(ctx);
}

static void test_receive_partial_chunks(void)
{
    fake_uart_t backend = {0};
    backend.next_read_chunk = 2U;
    mbc_transport_iface_t iface;
    mbc_rtu_uart_ctx_t* ctx = create_driver(&backend, &iface, 115200U, 0U, 32U);

    const uint8_t payload[] = {0x01, 0x02, 0x03};
    memcpy(backend.rx_buffer, payload, sizeof(payload));
    backend.rx_len = sizeof(payload);

    uint8_t out[3] = {0};
    mbc_transport_io_t io = {0};

    assert(mbc_transport_receive(&iface, out, sizeof(out), &io) == MBC_STATUS_OK);
    assert(io.processed == sizeof(payload));
    assert(memcmp(out, payload, sizeof(payload)) == 0);

    mbc_rtu_uart_destroy(ctx);
}

static void test_rtu_uart_engine_client(void)
{
    fake_uart_t backend = {0};
    backend.current_time = 1000U;

    mbc_transport_iface_t iface;
    mbc_rtu_uart_ctx_t* ctx = create_driver(&backend, &iface, 19200U, 0U, 64U);

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);

    mbc_runtime_t runtime;
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);

    mbc_engine_t engine;
    mbc_engine_config_t cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_RTU,
        .use_override = false,
        .response_timeout_ms = 100U,
    };
    assert(mbc_engine_init(&engine, &cfg) == MBC_STATUS_OK);

    const uint8_t request_frame[] = {0x11, 0x03, 0x00, 0x00, 0x00, 0x01};
    assert(mbc_engine_submit_request(&engine, request_frame, sizeof(request_frame)) ==
           MBC_STATUS_OK);
    assert(backend.tx_len == sizeof(request_frame) + 2U);
    assert(memcmp(backend.tx_buffer, request_frame, sizeof(request_frame)) == 0);
    uint16_t observed_crc =
        (uint16_t)((uint16_t)backend.tx_buffer[sizeof(request_frame)] |
                   ((uint16_t)backend.tx_buffer[sizeof(request_frame) + 1U] << 8));
    uint16_t expected_crc = mbc_crc16(request_frame, sizeof(request_frame));
    assert(observed_crc == expected_crc);

    const uint8_t response_frame[] = {0x11, 0x03, 0x02, 0x00, 0x2A};
    uint8_t response_with_crc[sizeof(response_frame) + 2U];
    memcpy(response_with_crc, response_frame, sizeof(response_frame));
    uint16_t response_crc = mbc_crc16(response_frame, sizeof(response_frame));
    response_with_crc[sizeof(response_frame)] = (uint8_t)(response_crc & 0xFFU);
    response_with_crc[sizeof(response_frame) + 1U] = (uint8_t)((response_crc >> 8) & 0xFFU);
    memcpy(backend.rx_buffer, response_with_crc, sizeof(response_with_crc));
    backend.rx_len = sizeof(response_with_crc);

    mbc_pdu_t response_pdu = {0};
    bool response_ready = false;
    for (int i = 0; i < 5 && !response_ready; ++i) {
        assert(mbc_engine_step(&engine, sizeof(response_with_crc)) == MBC_STATUS_OK);
        response_ready = mbc_engine_take_pdu(&engine, &response_pdu);
    }
    assert(response_ready);

    const uint8_t* register_data = NULL;
    size_t register_count = 0U;
    assert(mbc_pdu_parse_read_holding_response(&response_pdu, &register_data, &register_count) ==
           MBC_STATUS_OK);
    assert(register_count == 1U);
    assert(register_data[0] == 0x00U && register_data[1] == 0x2AU);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_rtu_uart_destroy(ctx);
}

static void test_rtu_uart_engine_server(void)
{
    fake_uart_t backend = {0};
    backend.current_time = 2000U;

    mbc_transport_iface_t iface;
    mbc_rtu_uart_ctx_t* ctx = create_driver(&backend, &iface, 9600U, 0U, 64U);

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);

    mbc_runtime_t runtime;
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);

    mbc_engine_t engine;
    mbc_engine_config_t cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_SERVER,
        .framing = MBC_FRAMING_RTU,
        .use_override = false,
    };
    assert(mbc_engine_init(&engine, &cfg) == MBC_STATUS_OK);

    const uint8_t request_frame[] = {0x11, 0x03, 0x00, 0x00, 0x00, 0x01};
    uint8_t request_with_crc[sizeof(request_frame) + 2U];
    memcpy(request_with_crc, request_frame, sizeof(request_frame));
    uint16_t request_crc = mbc_crc16(request_frame, sizeof(request_frame));
    request_with_crc[sizeof(request_frame)] = (uint8_t)(request_crc & 0xFFU);
    request_with_crc[sizeof(request_frame) + 1U] = (uint8_t)((request_crc >> 8) & 0xFFU);
    memcpy(backend.rx_buffer, request_with_crc, sizeof(request_with_crc));
    backend.rx_len = sizeof(request_with_crc);

    mbc_pdu_t decoded_request = {0};
    bool has_request = false;
    for (int i = 0; i < 5 && !has_request; ++i) {
        assert(mbc_engine_step(&engine, sizeof(request_with_crc)) == MBC_STATUS_OK);
        has_request = mbc_engine_take_pdu(&engine, &decoded_request);
    }
    assert(has_request);
    assert(decoded_request.function == 0x03U);

    const uint8_t response_frame[] = {0x11, 0x03, 0x02, 0x12, 0x34};
    assert(mbc_engine_submit_request(&engine, response_frame, sizeof(response_frame)) ==
           MBC_STATUS_OK);
    assert(backend.tx_len == sizeof(response_frame) + 2U);
    assert(memcmp(backend.tx_buffer, response_frame, sizeof(response_frame)) == 0);
    uint16_t tx_crc = (uint16_t)((uint16_t)backend.tx_buffer[sizeof(response_frame)] |
                                 ((uint16_t)backend.tx_buffer[sizeof(response_frame) + 1U] << 8));
    uint16_t expected_crc = mbc_crc16(response_frame, sizeof(response_frame));
    assert(tx_crc == expected_crc);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_rtu_uart_destroy(ctx);
}

static void test_rtu_uart_engine_client_crc_error(void)
{
    fake_uart_t backend = {0};
    backend.current_time = 3000U;

    mbc_transport_iface_t iface;
    mbc_rtu_uart_ctx_t* ctx = create_driver(&backend, &iface, 19200U, 0U, 64U);

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);

    mbc_runtime_t runtime;
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);

    mbc_engine_t engine;
    mbc_engine_config_t cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_RTU,
        .use_override = false,
        .response_timeout_ms = 50,
    };
    assert(mbc_engine_init(&engine, &cfg) == MBC_STATUS_OK);

    const uint8_t request_frame[] = {0x11, 0x03, 0x00, 0x00, 0x00, 0x01};
    assert(mbc_engine_submit_request(&engine, request_frame, sizeof(request_frame)) ==
           MBC_STATUS_OK);

    const uint8_t response_frame[] = {0x11, 0x03, 0x02, 0x10, 0x20};
    uint16_t crc = mbc_crc16(response_frame, sizeof(response_frame));
    crc ^= 0xFFFFU;
    uint8_t response_bad[sizeof(response_frame) + 2U];
    memcpy(response_bad, response_frame, sizeof(response_frame));
    response_bad[sizeof(response_frame)] = (uint8_t)(crc & 0xFFU);
    response_bad[sizeof(response_frame) + 1U] = (uint8_t)((crc >> 8) & 0xFFU);
    memcpy(backend.rx_buffer, response_bad, sizeof(response_bad));
    backend.rx_len = sizeof(response_bad);

    bool saw_error = false;
    for (int i = 0; i < 10 && !saw_error; ++i) {
        mbc_status_t status = mbc_engine_step(&engine, sizeof(response_bad));
        if (status == MBC_STATUS_DECODING_ERROR) {
            saw_error = true;
            break;
        }
        assert(status == MBC_STATUS_OK);
        mbc_transport_yield(&iface);
    }

    assert(saw_error);
    mbc_pdu_t out = {0};
    assert(!mbc_engine_take_pdu(&engine, &out));

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_rtu_uart_destroy(ctx);
}

static void test_rtu_uart_engine_server_crc_error(void)
{
    fake_uart_t backend = {0};
    backend.current_time = 4000U;

    mbc_transport_iface_t iface;
    mbc_rtu_uart_ctx_t* ctx = create_driver(&backend, &iface, 9600U, 0U, 64U);

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);

    mbc_runtime_t runtime;
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);

    mbc_engine_t engine;
    mbc_engine_config_t cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_SERVER,
        .framing = MBC_FRAMING_RTU,
        .use_override = false,
    };
    assert(mbc_engine_init(&engine, &cfg) == MBC_STATUS_OK);

    const uint8_t request_frame[] = {0x11, 0x06, 0x00, 0x01, 0x00, 0x05};
    uint16_t crc = mbc_crc16(request_frame, sizeof(request_frame));
    crc ^= 0xFFFFU;
    uint8_t request_bad[sizeof(request_frame) + 2U];
    memcpy(request_bad, request_frame, sizeof(request_frame));
    request_bad[sizeof(request_frame)] = (uint8_t)(crc & 0xFFU);
    request_bad[sizeof(request_frame) + 1U] = (uint8_t)((crc >> 8) & 0xFFU);
    memcpy(backend.rx_buffer, request_bad, sizeof(request_bad));
    backend.rx_len = sizeof(request_bad);

    bool saw_error = false;
    for (int i = 0; i < 10 && !saw_error; ++i) {
        mbc_status_t status = mbc_engine_step(&engine, sizeof(request_bad));
        if (status == MBC_STATUS_DECODING_ERROR) {
            saw_error = true;
            break;
        }
        assert(status == MBC_STATUS_OK);
    }

    assert(saw_error);
    mbc_pdu_t out = {0};
    assert(!mbc_engine_take_pdu(&engine, &out));

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_rtu_uart_destroy(ctx);
}

int main(void)
{
    test_guard_time_respected();
    test_receive_and_flush();
    test_partial_write_error();
    test_receive_partial_chunks();
    test_rtu_uart_engine_client();
    test_rtu_uart_engine_server();
    test_rtu_uart_engine_client_crc_error();
    test_rtu_uart_engine_server_crc_error();
    return 0;
}
