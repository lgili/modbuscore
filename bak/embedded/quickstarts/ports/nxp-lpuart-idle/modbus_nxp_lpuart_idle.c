// SPDX-License-Identifier: Apache-2.0
//
// NXP LPUART Modbus RTU transport with IDLE detection and interrupt-driven RX.

#include "modbus_amalgamated.h"

#include <modbus/mb_embed.h>

#include "fsl_clock.h"
#include "fsl_lpuart.h"

#include <string.h>

typedef uint32_t (*modbus_nxp_now_fn_t)(void *user_ctx);
typedef void (*modbus_nxp_delay_fn_t)(uint32_t usec, void *user_ctx);
typedef void (*modbus_nxp_direction_fn_t)(bool is_tx, void *user_ctx);

typedef struct {
    LPUART_Type *uart;
    uint32_t src_clock_hz;
    uint32_t baudrate;
    lpuart_parity_mode_t parity;
    lpuart_stop_bit_count_t stop_bits;
    uint32_t silence_timeout_ms;
    modbus_nxp_now_fn_t now_us;
    modbus_nxp_delay_fn_t delay_us;
    modbus_nxp_direction_fn_t set_direction;
    void *user_ctx;
} modbus_nxp_lpuart_idle_config_t;

typedef struct {
    mb_client_t client;

    LPUART_Type *uart;

    size_t rx_head;
    size_t rx_tail;
    uint8_t rx_buf[MODBUS_NXP_IDLE_RX_SIZE];
    bool idle_flag;

    modbus_nxp_now_fn_t now_us;
    modbus_nxp_delay_fn_t delay_us;
    modbus_nxp_direction_fn_t set_direction;
    void *user_ctx;
} modbus_nxp_lpuart_idle_ctx_t;

static modbus_nxp_lpuart_idle_ctx_t g_ctx;

static uint32_t modbus_ticks_now_us(void)
{
    if (g_ctx.now_us == NULL) {
        return 0U;
    }
    return g_ctx.now_us(g_ctx.user_ctx);
}

static void modbus_nxp_lpuart_set_direction(bool is_tx)
{
    if (g_ctx.set_direction != NULL) {
        g_ctx.set_direction(is_tx, g_ctx.user_ctx);
    }
}

static void modbus_nxp_lpuart_idle_prime_rx(modbus_nxp_lpuart_idle_ctx_t *ctx)
{
    ctx->rx_head = 0U;
    ctx->rx_tail = 0U;
    ctx->idle_flag = false;

    LPUART_ClearStatusFlags(ctx->uart, kLPUART_IdleLineFlag | kLPUART_RxDataRegFullFlag);
    LPUART_EnableInterrupts(ctx->uart, kLPUART_RxDataRegFullInterruptEnable | kLPUART_IdleLineInterruptEnable);
}

static mb_err_t modbus_nxp_lpuart_uart_send(void *ctx,
                                            const mb_u8 *data,
                                            mb_size_t len,
                                            mb_transport_io_result_t *out)
{
    modbus_nxp_lpuart_idle_ctx_t *state = ctx;
    if (state == NULL || data == NULL || len == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    modbus_nxp_lpuart_set_direction(true);
    LPUART_WriteBlocking(state->uart, data, len);
    while ((state->uart->STAT & LPUART_STAT_TC_MASK) == 0U) {
    }
    modbus_nxp_lpuart_set_direction(false);

    if (out != NULL) {
        out->processed = len;
    }
    return MB_OK;
}

static mb_err_t modbus_nxp_lpuart_uart_recv(void *ctx,
                                            mb_u8 *data,
                                            mb_size_t cap,
                                            mb_transport_io_result_t *out)
{
    modbus_nxp_lpuart_idle_ctx_t *state = ctx;
    if (state == NULL || data == NULL || cap == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (!state->idle_flag) {
        if (out != NULL) {
            out->processed = 0U;
        }
        return MB_ERR_TIMEOUT;
    }

    size_t available = (state->rx_head + MODBUS_NXP_IDLE_RX_SIZE - state->rx_tail) % MODBUS_NXP_IDLE_RX_SIZE;
    if (available == 0U) {
        state->idle_flag = false;
        if (out != NULL) {
            out->processed = 0U;
        }
        return MB_ERR_TIMEOUT;
    }

    if (available > cap) {
        available = cap;
    }

    size_t copied = 0U;
    while (copied < available) {
        data[copied++] = state->rx_buf[state->rx_tail];
        state->rx_tail = (state->rx_tail + 1U) % MODBUS_NXP_IDLE_RX_SIZE;
    }

    if (((state->rx_head + MODBUS_NXP_IDLE_RX_SIZE - state->rx_tail) % MODBUS_NXP_IDLE_RX_SIZE) == 0U) {
        state->idle_flag = false;
    }

    if (out != NULL) {
        out->processed = copied;
    }
    return MB_OK;
}

static mb_time_ms_t modbus_nxp_lpuart_now_ms(void *ctx)
{
    (void)ctx;
    if (g_ctx.now_us == NULL) {
        return 0U;
    }
    return (mb_time_ms_t)(g_ctx.now_us(g_ctx.user_ctx) / 1000U);
}

static void modbus_nxp_lpuart_yield(void *ctx)
{
    (void)ctx;
    if (g_ctx.delay_us != NULL) {
        g_ctx.delay_us(100U, g_ctx.user_ctx);
    }
}

static const mb_transport_if_t modbus_nxp_lpuart_if = {
    .ctx = &g_ctx,
    .send = modbus_nxp_lpuart_uart_send,
    .recv = modbus_nxp_lpuart_uart_recv,
    .now = modbus_nxp_lpuart_now_ms,
    .yield = modbus_nxp_lpuart_yield,
};

mb_err_t modbus_nxp_lpuart_idle_init(const modbus_nxp_lpuart_idle_config_t *cfg,
                                     mb_client_txn_t *txn_pool,
                                     mb_size_t txn_pool_len)
{
    if (cfg == NULL || cfg->uart == NULL || txn_pool == NULL || txn_pool_len == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    memset(&g_ctx, 0, sizeof(g_ctx));

    g_ctx.uart = cfg->uart;
    g_ctx.now_us = cfg->now_us;
    g_ctx.delay_us = cfg->delay_us;
    g_ctx.set_direction = cfg->set_direction;
    g_ctx.user_ctx = cfg->user_ctx;

    lpuart_config_t uart_cfg;
    LPUART_GetDefaultConfig(&uart_cfg);
    uart_cfg.baudRate_Bps = cfg->baudrate;
    uart_cfg.parityMode = cfg->parity;
    uart_cfg.stopBitCount = cfg->stop_bits;
    uart_cfg.enableRx = true;
    uart_cfg.enableTx = true;

    LPUART_Init(g_ctx.uart, &uart_cfg, cfg->src_clock_hz);

    modbus_nxp_lpuart_idle_prime_rx(&g_ctx);

    mb_err_t err = mb_client_init(&g_ctx.client, &modbus_nxp_lpuart_if, txn_pool, txn_pool_len);
    if (!mb_err_is_ok(err)) {
        return err;
    }

#if MB_CONF_TRANSPORT_RTU
    if (cfg->silence_timeout_ms != 0U) {
        mb_rtu_set_silence_timeout(&g_ctx.client.rtu, cfg->silence_timeout_ms);
    }
#endif

    return MB_OK;
}

mb_err_t modbus_nxp_lpuart_idle_poll(void)
{
    return mb_client_poll(&g_ctx.client);
}

mb_client_t *modbus_nxp_lpuart_idle_client(void)
{
    return &g_ctx.client;
}

mb_err_t modbus_nxp_lpuart_idle_submit_read_inputs(uint8_t unit_id,
                                                   uint16_t addr,
                                                   uint16_t count,
                                                   const mb_embed_request_opts_t *opts,
                                                   mb_client_txn_t **out_txn)
{
    return mb_embed_submit_read_input_registers(&g_ctx.client, unit_id, addr, count, opts, out_txn);
}

mb_err_t modbus_nxp_lpuart_idle_submit_write_single(uint8_t unit_id,
                                                    uint16_t addr,
                                                    uint16_t value,
                                                    const mb_embed_request_opts_t *opts,
                                                    mb_client_txn_t **out_txn)
{
    return mb_embed_submit_write_single_register(&g_ctx.client, unit_id, addr, value, opts, out_txn);
}

void modbus_nxp_lpuart_idle_isr(void)
{
    uint32_t status = LPUART_GetStatusFlags(g_ctx.uart);

    if ((status & kLPUART_RxDataRegFullFlag) != 0U) {
        uint8_t byte = LPUART_ReadByte(g_ctx.uart);
        size_t next_head = (g_ctx.rx_head + 1U) % MODBUS_NXP_IDLE_RX_SIZE;
        if (next_head != g_ctx.rx_tail) {
            g_ctx.rx_buf[g_ctx.rx_head] = byte;
            g_ctx.rx_head = next_head;
        }
    }

    if ((status & kLPUART_IdleLineFlag) != 0U) {
        LPUART_ClearStatusFlags(g_ctx.uart, kLPUART_IdleLineFlag);
        g_ctx.idle_flag = true;
    }
}
