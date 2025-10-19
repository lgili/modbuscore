/**
 * @file rtu_uart.c
 * @brief Implementation of RTU UART transport with hardware abstraction.
 */

#include <modbuscore/transport/rtu_uart.h>
#include <stdlib.h>
#include <string.h>

#define MBC_DEFAULT_DATA_BITS 8U
#define MBC_DEFAULT_STOP_BITS 1U
#define MBC_DEFAULT_BUFFER 256U

struct mbc_rtu_uart_ctx {
    mbc_rtu_uart_backend_t backend;
    uint32_t guard_time_us;
    uint64_t last_activity_us;

    uint8_t* rx_buffer;
    size_t rx_capacity;
    size_t rx_length;
};

typedef struct mbc_rtu_uart_ctx mbc_rtu_uart_ctx_internal_t;

static uint32_t compute_guard_time_us(const mbc_rtu_uart_config_t* config)
{
    uint32_t baud = config->baud_rate ? config->baud_rate : 9600U;
    uint8_t data_bits = config->data_bits ? config->data_bits : MBC_DEFAULT_DATA_BITS;
    uint8_t stop_bits = config->stop_bits ? config->stop_bits : MBC_DEFAULT_STOP_BITS;
    uint8_t parity_bits = (config->parity_bits > 0U) ? 1U : 0U;

    uint32_t bits_per_char = 1U + data_bits + stop_bits + parity_bits;
    uint64_t char_time_us = ((uint64_t)bits_per_char * 1000000ULL + baud - 1U) / baud;

    return (uint32_t)((char_time_us * 7ULL) / 2ULL);
}

static uint64_t now_us(const mbc_rtu_uart_ctx_internal_t* ctx)
{
    return ctx->backend.now_us ? ctx->backend.now_us(ctx->backend.ctx) : 0ULL;
}

static void wait_guard_time(mbc_rtu_uart_ctx_internal_t* ctx)
{
    if (!ctx->backend.now_us || ctx->guard_time_us == 0U || ctx->last_activity_us == 0ULL) {
        return;
    }

    uint64_t target = ctx->last_activity_us + ctx->guard_time_us;
    uint64_t current = now_us(ctx);
    if (current >= target) {
        return;
    }

    uint32_t remaining = (uint32_t)(target - current);
    if (ctx->backend.delay_us) {
        ctx->backend.delay_us(ctx->backend.ctx, remaining);
    } else {
        while (now_us(ctx) < target) {
            /* busy wait */
        }
    }
}

static void update_last_activity(mbc_rtu_uart_ctx_internal_t* ctx)
{
    ctx->last_activity_us = now_us(ctx);
}

static void refill_rx_buffer(mbc_rtu_uart_ctx_internal_t* ctx)
{
    if (!ctx->backend.read) {
        return;
    }

    while (ctx->rx_length < ctx->rx_capacity) {
        size_t space = ctx->rx_capacity - ctx->rx_length;
        size_t read = ctx->backend.read(ctx->backend.ctx, &ctx->rx_buffer[ctx->rx_length], space);
        if (read == 0U) {
            break;
        }
        ctx->rx_length += read;
        update_last_activity(ctx);
    }
}

static mbc_status_t rtu_uart_send(void* ctx, const uint8_t* buffer, size_t length,
                                  mbc_transport_io_t* out)
{
    mbc_rtu_uart_ctx_internal_t* uart = ctx;

    if (!uart || (!buffer && length > 0U)) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (length == 0U) {
        if (out) {
            out->processed = 0U;
        }
        return MBC_STATUS_OK;
    }

    if (!uart->backend.write) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    wait_guard_time(uart);

    size_t written = uart->backend.write(uart->backend.ctx, buffer, length);
    if (written != length) {
        return MBC_STATUS_IO_ERROR;
    }

    if (uart->backend.flush) {
        uart->backend.flush(uart->backend.ctx);
    }

    update_last_activity(uart);

    if (out) {
        out->processed = length;
    }
    return MBC_STATUS_OK;
}

static mbc_status_t rtu_uart_receive(void* ctx, uint8_t* buffer, size_t capacity,
                                     mbc_transport_io_t* out)
{
    mbc_rtu_uart_ctx_internal_t* uart = ctx;

    if (!uart || !buffer || capacity == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    refill_rx_buffer(uart);

    if (uart->rx_length == 0U) {
        if (out) {
            out->processed = 0U;
        }
        return MBC_STATUS_OK;
    }

    size_t to_copy = (uart->rx_length < capacity) ? uart->rx_length : capacity;
    memcpy(buffer, uart->rx_buffer, to_copy);

    uart->rx_length -= to_copy;
    if (uart->rx_length > 0U) {
        memmove(uart->rx_buffer, uart->rx_buffer + to_copy, uart->rx_length);
    }

    if (out) {
        out->processed = to_copy;
    }
    return MBC_STATUS_OK;
}

static uint64_t rtu_uart_now(void* ctx)
{
    mbc_rtu_uart_ctx_internal_t* uart = ctx;
    return now_us(uart);
}

static void rtu_uart_yield(void* ctx) { (void)ctx; }

mbc_status_t mbc_rtu_uart_create(const mbc_rtu_uart_config_t* config,
                                 mbc_transport_iface_t* out_iface, mbc_rtu_uart_ctx_t** out_ctx)
{
    if (!config || !out_iface || !out_ctx) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!config->backend.write || !config->backend.read || !config->backend.now_us) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    size_t rx_capacity =
        config->rx_buffer_capacity ? config->rx_buffer_capacity : MBC_DEFAULT_BUFFER;

    mbc_rtu_uart_ctx_internal_t* ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return MBC_STATUS_NO_RESOURCES;
    }

    ctx->backend = config->backend;
    ctx->rx_capacity = rx_capacity;
    ctx->rx_buffer = malloc(rx_capacity);
    if (!ctx->rx_buffer) {
        free(ctx);
        return MBC_STATUS_NO_RESOURCES;
    }

    ctx->guard_time_us =
        config->guard_time_us ? config->guard_time_us : compute_guard_time_us(config);
    ctx->last_activity_us = now_us(ctx);

    *out_iface = (mbc_transport_iface_t){
        .ctx = ctx,
        .send = rtu_uart_send,
        .receive = rtu_uart_receive,
        .now = rtu_uart_now,
        .yield = rtu_uart_yield,
    };

    *out_ctx = ctx;
    return MBC_STATUS_OK;
}

void mbc_rtu_uart_destroy(mbc_rtu_uart_ctx_t* ctx)
{
    mbc_rtu_uart_ctx_internal_t* uart = (mbc_rtu_uart_ctx_internal_t*)ctx;
    if (!uart) {
        return;
    }

    free(uart->rx_buffer);
    free(uart);
}

void mbc_rtu_uart_reset(mbc_rtu_uart_ctx_t* ctx)
{
    mbc_rtu_uart_ctx_internal_t* uart = (mbc_rtu_uart_ctx_internal_t*)ctx;
    if (!uart) {
        return;
    }
    uart->rx_length = 0U;
    uart->last_activity_us = now_us(uart);
}
