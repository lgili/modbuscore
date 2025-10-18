#include <assert.h>
#include <string.h>

#include <modbuscore/transport/rtu_uart.h>

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

static size_t fake_write(void *ctx, const uint8_t *data, size_t length)
{
    fake_uart_t *uart = ctx;
    if (uart->fail_partial_write && length > 0U) {
        uart->fail_partial_write = 0;
        return length - 1U;
    }

    memcpy(&uart->tx_buffer[uart->tx_len], data, length);
    uart->tx_len += length;
    return length;
}

static size_t fake_read(void *ctx, uint8_t *data, size_t capacity)
{
    fake_uart_t *uart = ctx;
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

static void fake_flush(void *ctx)
{
    fake_uart_t *uart = ctx;
    uart->flush_count++;
}

static uint64_t fake_now(void *ctx)
{
    fake_uart_t *uart = ctx;
    uart->current_time += 50U;
    return uart->current_time;
}

static void fake_delay(void *ctx, uint32_t micros)
{
    fake_uart_t *uart = ctx;
    uart->delay_total += micros;
    uart->current_time += micros;
}

static mbc_rtu_uart_ctx_t *create_driver(fake_uart_t *backend,
                                         mbc_transport_iface_t *iface,
                                         uint32_t baud,
                                         uint32_t guard_override,
                                         size_t rx_capacity)
{
    mbc_rtu_uart_config_t cfg = {
        .backend = {
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

    mbc_rtu_uart_ctx_t *ctx = NULL;
    assert(mbc_rtu_uart_create(&cfg, iface, &ctx) == MBC_STATUS_OK);
    return ctx;
}

static void test_guard_time_respected(void)
{
    fake_uart_t backend = {0};
    mbc_transport_iface_t iface;
    mbc_rtu_uart_ctx_t *ctx = create_driver(&backend, &iface, 9600U, 0U, 64U);

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
    mbc_rtu_uart_ctx_t *ctx = create_driver(&backend, &iface, 19200U, 0U, 32U);

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
    mbc_rtu_uart_ctx_t *ctx = create_driver(&backend, &iface, 9600U, 0U, 32U);

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
    mbc_rtu_uart_ctx_t *ctx = create_driver(&backend, &iface, 115200U, 0U, 32U);

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

int main(void)
{
    test_guard_time_respected();
    test_receive_and_flush();
    test_partial_write_error();
    test_receive_partial_chunks();
    return 0;
}
