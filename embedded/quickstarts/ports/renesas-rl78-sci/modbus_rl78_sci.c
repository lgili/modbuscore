// SPDX-License-Identifier: Apache-2.0
//
// Renesas RL78 SCI helper for the Modbus drop-in client.
// Assumes the project uses the Code Generator UART driver.

#include "modbus_rl78_sci.h"

#include "r_cg_macrodriver.h"
#include "r_cg_serial.h"

#include <string.h>

#ifndef MODBUS_RL78_RX_SIZE
#define MODBUS_RL78_RX_SIZE 256U
#endif

#ifndef MODBUS_RL78_UART_CHANNEL
#define MODBUS_RL78_UART_CHANNEL 0
#endif

#if MODBUS_RL78_UART_CHANNEL == 0
#define MODBUS_RL78_UART_START()      R_UART0_Start()
#define MODBUS_RL78_UART_SEND(buf,len) R_UART0_Send((uint8_t *)(buf), (uint16_t)(len))
#define MODBUS_RL78_UART_RECV(buf,len) R_UART0_Receive((uint8_t *)(buf), (uint16_t)(len))
#define MODBUS_RL78_UART_RX_CALLBACK  r_uart0_callback_receiveend
#define MODBUS_RL78_UART_TX_CALLBACK  r_uart0_callback_sendend
#elif MODBUS_RL78_UART_CHANNEL == 1
#define MODBUS_RL78_UART_START()      R_UART1_Start()
#define MODBUS_RL78_UART_SEND(buf,len) R_UART1_Send((uint8_t *)(buf), (uint16_t)(len))
#define MODBUS_RL78_UART_RECV(buf,len) R_UART1_Receive((uint8_t *)(buf), (uint16_t)(len))
#define MODBUS_RL78_UART_RX_CALLBACK  r_uart1_callback_receiveend
#define MODBUS_RL78_UART_TX_CALLBACK  r_uart1_callback_sendend
#else
#error "Unsupported MODBUS_RL78_UART_CHANNEL"
#endif

typedef struct {
    mb_client_t client;

    uint8_t rx_ring[MODBUS_RL78_RX_SIZE];
    volatile size_t rx_head;
    volatile size_t rx_tail;

    volatile bool tx_in_flight;
    volatile uint8_t rx_shadow;

    uint32_t last_rx_tick_us;
    uint32_t frame_timeout_us;

    modbus_rl78_now_fn_t now_us;
    modbus_rl78_delay_fn_t delay_us;
    modbus_rl78_direction_fn_t set_direction;
    void *user_ctx;
} modbus_rl78_sci_ctx_t;

static modbus_rl78_sci_ctx_t g_ctx;

static uint32_t modbus_rl78_ticks_now_us(void)
{
    if (g_ctx.now_us == NULL) {
        return 0U;
    }
    return g_ctx.now_us(g_ctx.user_ctx);
}

static void modbus_rl78_set_direction(bool is_tx)
{
    if (g_ctx.set_direction != NULL) {
        g_ctx.set_direction(is_tx, g_ctx.user_ctx);
    }
}

static void modbus_rl78_on_rx_byte(uint8_t byte)
{
    size_t next_head = (g_ctx.rx_head + 1U) % MODBUS_RL78_RX_SIZE;
    if (next_head == g_ctx.rx_tail) {
        // Drop the oldest byte on overflow to keep the transport alive.
        g_ctx.rx_tail = (g_ctx.rx_tail + 1U) % MODBUS_RL78_RX_SIZE;
    }

    g_ctx.rx_ring[g_ctx.rx_head] = byte;
    g_ctx.rx_head = next_head;
    g_ctx.last_rx_tick_us = modbus_rl78_ticks_now_us();
}

static void modbus_rl78_arm_rx(void)
{
    MODBUS_RL78_UART_RECV(&g_ctx.rx_shadow, 1U);
}

static mb_err_t modbus_rl78_uart_send(void *ctx,
                                      const mb_u8 *data,
                                      mb_size_t len,
                                      mb_transport_io_result_t *out)
{
    (void)ctx;
    if (data == NULL || len == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    modbus_rl78_set_direction(true);
    g_ctx.tx_in_flight = true;
    MODBUS_RL78_UART_SEND(data, (uint16_t)len);

    while (g_ctx.tx_in_flight) {
        if (g_ctx.delay_us != NULL) {
            g_ctx.delay_us(10U, g_ctx.user_ctx);
        }
    }

    modbus_rl78_set_direction(false);

    if (out != NULL) {
        out->processed = len;
    }

    return MB_OK;
}

static mb_err_t modbus_rl78_uart_recv(void *ctx,
                                      mb_u8 *data,
                                      mb_size_t cap,
                                      mb_transport_io_result_t *out)
{
    (void)ctx;
    if (data == NULL || cap == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    size_t available = (g_ctx.rx_head + MODBUS_RL78_RX_SIZE - g_ctx.rx_tail) % MODBUS_RL78_RX_SIZE;
    if (available == 0U) {
        if (out != NULL) {
            out->processed = 0U;
        }
        return MB_ERR_TIMEOUT;
    }

    const uint32_t now = modbus_rl78_ticks_now_us();
    if ((now - g_ctx.last_rx_tick_us) < g_ctx.frame_timeout_us) {
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
        data[copied++] = g_ctx.rx_ring[g_ctx.rx_tail];
        g_ctx.rx_tail = (g_ctx.rx_tail + 1U) % MODBUS_RL78_RX_SIZE;
    }

    if (out != NULL) {
        out->processed = copied;
    }
    return MB_OK;
}

static mb_time_ms_t modbus_rl78_now_ms(void *ctx)
{
    (void)ctx;
    uint32_t us = modbus_rl78_ticks_now_us();
    return (mb_time_ms_t)(us / 1000U);
}

static void modbus_rl78_yield(void *ctx)
{
    (void)ctx;
    if (g_ctx.delay_us != NULL) {
        g_ctx.delay_us(100U, g_ctx.user_ctx);
    }
}

static const mb_transport_if_t modbus_rl78_if = {
    .ctx = &g_ctx,
    .send = modbus_rl78_uart_send,
    .recv = modbus_rl78_uart_recv,
    .now = modbus_rl78_now_ms,
    .yield = modbus_rl78_yield,
};

mb_err_t modbus_rl78_sci_init(const modbus_rl78_sci_config_t *cfg,
                              mb_client_txn_t *txn_pool,
                              mb_size_t txn_pool_len)
{
    if (cfg == NULL || txn_pool == NULL || txn_pool_len == 0U || cfg->now_us == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    memset(&g_ctx, 0, sizeof(g_ctx));

    g_ctx.now_us = cfg->now_us;
    g_ctx.delay_us = cfg->delay_us;
    g_ctx.set_direction = cfg->set_direction;
    g_ctx.user_ctx = cfg->user_ctx;
    g_ctx.last_rx_tick_us = modbus_rl78_ticks_now_us();

    uint32_t bits = 1U + 8U + (cfg->parity_enabled ? 1U : 0U) + (cfg->two_stop_bits ? 2U : 1U);
    uint32_t char_time_us = (uint32_t)(((uint64_t)bits * 1000000ULL + cfg->baudrate - 1U) / cfg->baudrate);
    g_ctx.frame_timeout_us = char_time_us * 4U; // >= T3.5

    MODBUS_RL78_UART_START();
    modbus_rl78_arm_rx();

    mb_err_t err = mb_client_init(&g_ctx.client, &modbus_rl78_if, txn_pool, txn_pool_len);
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

mb_err_t modbus_rl78_sci_poll(void)
{
    return mb_client_poll(&g_ctx.client);
}

mb_client_t *modbus_rl78_sci_client(void)
{
    return &g_ctx.client;
}

mb_err_t modbus_rl78_sci_submit_read_inputs(uint8_t unit_id,
                                            uint16_t addr,
                                            uint16_t count,
                                            const mb_embed_request_opts_t *opts,
                                            mb_client_txn_t **out_txn)
{
    return mb_embed_submit_read_input_registers(&g_ctx.client, unit_id, addr, count, opts, out_txn);
}

mb_err_t modbus_rl78_sci_submit_write_single(uint8_t unit_id,
                                             uint16_t addr,
                                             uint16_t value,
                                             const mb_embed_request_opts_t *opts,
                                             mb_client_txn_t **out_txn)
{
    return mb_embed_submit_write_single_register(&g_ctx.client, unit_id, addr, value, opts, out_txn);
}

void MODBUS_RL78_UART_RX_CALLBACK(void)
{
    modbus_rl78_on_rx_byte(g_ctx.rx_shadow);
    modbus_rl78_arm_rx();
}

void MODBUS_RL78_UART_TX_CALLBACK(void)
{
    g_ctx.tx_in_flight = false;
}
