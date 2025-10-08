// SPDX-License-Identifier: Apache-2.0
//
// STM32 LL-based Modbus RTU transport using circular DMA + IDLE detection.
// Hardware-specific glue (timer, DE/RE GPIO, precise delays) is provided via
// the configuration struct so this file stays drop-in friendly.

#include "modbus_stm32_idle.h"

#include <modbus/mb_embed.h>

// Pick the LL headers that match your family (g0xx, f3xx, l4xx, ...).
#include "stm32xx_ll_dma.h"
#include "stm32xx_ll_gpio.h"
#include "stm32xx_ll_rcc.h"
#include "stm32xx_ll_usart.h"

#include <string.h>

typedef struct {
    mb_client_t client;

    USART_TypeDef *uart;
    DMA_TypeDef *dma;
    uint32_t dma_channel;

    size_t rx_head;
    uint8_t rx_buf[MODBUS_STM32_IDLE_RX_SIZE];

    bool dma_pending;
    bool idle_flag;

    uint32_t last_activity_us;
    uint32_t idle_timestamp_us;
    uint32_t char_time_us;
    uint32_t t15_guard_us;
    uint32_t t35_guard_us;

    modbus_stm32_idle_now_fn_t now_us;
    modbus_stm32_idle_delay_fn_t delay_us;
    modbus_stm32_idle_direction_fn_t set_direction;
    void *user_ctx;
} modbus_stm32_idle_ctx_t;

static modbus_stm32_idle_ctx_t g_ctx;

static uint32_t modbus_ticks_now_us(void)
{
    if (g_ctx.now_us == NULL) {
        return 0U;
    }
    return g_ctx.now_us(g_ctx.user_ctx);
}

static void modbus_stm32_idle_wait_until_elapsed(modbus_stm32_idle_ctx_t *ctx,
                                                 uint32_t reference_us,
                                                 uint32_t guard_us)
{
    if (guard_us == 0U || ctx->now_us == NULL) {
        return;
    }

    uint32_t now = ctx->now_us(ctx->user_ctx);
    while ((uint32_t)(now - reference_us) < guard_us) {
        const uint32_t remaining = guard_us - (uint32_t)(now - reference_us);
        if (ctx->delay_us != NULL) {
            const uint32_t slice = (remaining > 64U) ? 64U : remaining;
            ctx->delay_us(slice, ctx->user_ctx);
        }
        now = ctx->now_us(ctx->user_ctx);
    }
}

static void modbus_stm32_idle_set_direction(bool is_tx)
{
    if (g_ctx.set_direction != NULL) {
        g_ctx.set_direction(is_tx, g_ctx.user_ctx);
    }
}

static void modbus_stm32_idle_uart_start_rx(modbus_stm32_idle_ctx_t *ctx)
{
    ctx->rx_head = 0U;
    ctx->idle_flag = false;
    ctx->dma_pending = false;
    ctx->idle_timestamp_us = modbus_ticks_now_us();

    LL_DMA_DisableChannel(ctx->dma, ctx->dma_channel);
    LL_DMA_SetMemoryAddress(ctx->dma, ctx->dma_channel, (uint32_t)ctx->rx_buf);
    LL_DMA_SetPeriphAddress(ctx->dma, ctx->dma_channel, (uint32_t)&ctx->uart->RDR);
    LL_DMA_SetDataLength(ctx->dma, ctx->dma_channel, sizeof(ctx->rx_buf));
    LL_DMA_EnableChannel(ctx->dma, ctx->dma_channel);

    LL_USART_EnableDMAReq_RX(ctx->uart);
    LL_USART_EnableIT_IDLE(ctx->uart);
}

static mb_err_t modbus_stm32_idle_uart_send(void *ctx,
                                            const mb_u8 *data,
                                            mb_size_t len,
                                            mb_transport_io_result_t *out)
{
    modbus_stm32_idle_ctx_t *state = ctx;
    if (state == NULL || data == NULL || len == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    modbus_stm32_idle_wait_until_elapsed(state, state->last_activity_us, state->t35_guard_us);

    modbus_stm32_idle_set_direction(true);

    for (mb_size_t i = 0; i < len; ++i) {
        LL_USART_TransmitData8(state->uart, data[i]);
        while (!LL_USART_IsActiveFlag_TXE(state->uart)) {
        }
    }

    while (!LL_USART_IsActiveFlag_TC(state->uart)) {
    }

    const uint32_t frame_complete_us = modbus_ticks_now_us();

    modbus_stm32_idle_wait_until_elapsed(state, frame_complete_us, state->t35_guard_us);

    modbus_stm32_idle_set_direction(false);
    state->last_activity_us = modbus_ticks_now_us();
    state->dma_pending = true;

    if (out != NULL) {
        out->processed = len;
    }

    return MB_OK;
}

static mb_err_t modbus_stm32_idle_uart_recv(void *ctx,
                                            mb_u8 *data,
                                            mb_size_t cap,
                                            mb_transport_io_result_t *out)
{
    modbus_stm32_idle_ctx_t *state = ctx;
    if (state == NULL || data == NULL || cap == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (!state->idle_flag && !state->dma_pending) {
        if (out != NULL) {
            out->processed = 0U;
        }
        return MB_ERR_TIMEOUT;
    }

    if (state->idle_flag && state->t15_guard_us != 0U) {
        modbus_stm32_idle_wait_until_elapsed(state, state->idle_timestamp_us, state->t15_guard_us);
    }

    // CRITICAL FIX: Atomic snapshot of DMA position to prevent race condition
    // Disable DMA momentarily to ensure consistent read of remaining count
    const bool was_enabled = LL_DMA_IsEnabledChannel(state->dma, state->dma_channel);
    if (was_enabled) {
        LL_DMA_DisableChannel(state->dma, state->dma_channel);
    }
    
    const size_t remaining = LL_DMA_GetDataLength(state->dma, state->dma_channel);
    const size_t dma_head = sizeof(state->rx_buf) - remaining;
    
    if (was_enabled) {
        LL_DMA_EnableChannel(state->dma, state->dma_channel);
    }
    
    const size_t available = (dma_head >= state->rx_head)
                           ? (dma_head - state->rx_head)
                           : ((sizeof(state->rx_buf) - state->rx_head) + dma_head);

    if (available == 0U) {
        state->idle_flag = false;
        state->dma_pending = false;
        if (out != NULL) {
            out->processed = 0U;
        }
        return MB_ERR_TIMEOUT;
    }

    if (available > cap) {
        available = cap;
    }

    size_t copied = 0U;
    size_t first_chunk = sizeof(state->rx_buf) - state->rx_head;
    if (first_chunk > available) {
        first_chunk = available;
    }

    memcpy(&data[copied], &state->rx_buf[state->rx_head], first_chunk);
    copied += first_chunk;
    state->rx_head = (state->rx_head + first_chunk) % sizeof(state->rx_buf);

    while (copied < available) {
        data[copied] = state->rx_buf[state->rx_head];
        state->rx_head = (state->rx_head + 1U) % sizeof(state->rx_buf);
        ++copied;
    }

    if (((size_t)(sizeof(state->rx_buf) - remaining) == state->rx_head) && copied == available) {
        state->idle_flag = false;
        state->dma_pending = false;
    }

    state->last_activity_us = modbus_ticks_now_us();

    if (out != NULL) {
        out->processed = copied;
    }

    return MB_OK;
}

static mb_time_ms_t modbus_stm32_idle_now_ms(void *ctx)
{
    (void)ctx;
    if (g_ctx.now_us == NULL) {
        return 0U;
    }
    return (mb_time_ms_t)(g_ctx.now_us(g_ctx.user_ctx) / 1000U);
}

static void modbus_stm32_idle_yield(void *ctx)
{
    (void)ctx;
    if (g_ctx.delay_us != NULL) {
        g_ctx.delay_us(100U, g_ctx.user_ctx);
    }
}

static const mb_transport_if_t modbus_stm32_idle_if = {
    .ctx = &g_ctx,
    .send = modbus_stm32_idle_uart_send,
    .recv = modbus_stm32_idle_uart_recv,
    .now = modbus_stm32_idle_now_ms,
    .yield = modbus_stm32_idle_yield,
};

mb_err_t modbus_stm32_idle_init(const modbus_stm32_idle_config_t *cfg,
                                mb_client_txn_t *txn_pool,
                                mb_size_t txn_pool_len)
{
    if (cfg == NULL || cfg->uart == NULL || cfg->dma == NULL || txn_pool == NULL || txn_pool_len == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    memset(&g_ctx, 0, sizeof(g_ctx));

    g_ctx.uart = cfg->uart;
    g_ctx.dma = cfg->dma;
    g_ctx.dma_channel = cfg->dma_channel;
    g_ctx.now_us = cfg->now_us;
    g_ctx.delay_us = cfg->delay_us;
    g_ctx.set_direction = cfg->set_direction;
    g_ctx.user_ctx = cfg->user_ctx;

    const uint32_t data_bits = (cfg->data_bits == 0U) ? MODBUS_STM32_IDLE_DEFAULT_DATA_BITS : cfg->data_bits;
    const uint32_t stop_bits = (cfg->stop_bits == 0U) ? MODBUS_STM32_IDLE_DEFAULT_STOP_BITS : cfg->stop_bits;
    const uint32_t parity_bits = cfg->parity_enabled ? 1U : 0U;

    if (cfg->baudrate != 0U) {
        const uint32_t symbol_bits = 1U + data_bits + parity_bits + stop_bits;
        const uint64_t numerator = (uint64_t)symbol_bits * 1000000ULL;
        g_ctx.char_time_us = (uint32_t)((numerator + cfg->baudrate - 1U) / cfg->baudrate);
    } else {
        g_ctx.char_time_us = 0U;
    }

    if (cfg->t35_guard_us != 0U) {
        g_ctx.t35_guard_us = cfg->t35_guard_us;
    } else if (g_ctx.char_time_us != 0U) {
        g_ctx.t35_guard_us = (g_ctx.char_time_us * 7U) / 2U;
    } else {
        g_ctx.t35_guard_us = 0U;
    }

    if (cfg->t15_guard_us != 0U) {
        g_ctx.t15_guard_us = cfg->t15_guard_us;
    } else if (g_ctx.char_time_us != 0U) {
        g_ctx.t15_guard_us = (g_ctx.char_time_us * 3U) / 2U;
    } else {
        g_ctx.t15_guard_us = 0U;
    }

    modbus_stm32_idle_uart_start_rx(&g_ctx);

    mb_err_t err = mb_client_init(&g_ctx.client, &modbus_stm32_idle_if, txn_pool, txn_pool_len);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    g_ctx.last_activity_us = modbus_ticks_now_us();

#if MB_CONF_TRANSPORT_RTU
    if (cfg->silence_timeout_ms != 0U) {
        mb_rtu_set_silence_timeout(&g_ctx.client.rtu, cfg->silence_timeout_ms);
    }
#endif

    return MB_OK;
}

mb_err_t modbus_stm32_idle_poll(void)
{
    return mb_client_poll(&g_ctx.client);
}

mb_client_t *modbus_stm32_idle_client(void)
{
    return &g_ctx.client;
}

mb_err_t modbus_stm32_idle_submit_read_inputs(uint8_t unit_id,
                                              uint16_t addr,
                                              uint16_t count,
                                              const mb_embed_request_opts_t *opts,
                                              mb_client_txn_t **out_txn)
{
    return mb_embed_submit_read_input_registers(&g_ctx.client, unit_id, addr, count, opts, out_txn);
}

mb_err_t modbus_stm32_idle_submit_write_single(uint8_t unit_id,
                                               uint16_t addr,
                                               uint16_t value,
                                               const mb_embed_request_opts_t *opts,
                                               mb_client_txn_t **out_txn)
{
    return mb_embed_submit_write_single_register(&g_ctx.client, unit_id, addr, value, opts, out_txn);
}

void modbus_stm32_idle_usart_isr(void)
{
    if (LL_USART_IsActiveFlag_IDLE(g_ctx.uart)) {
        LL_USART_ClearFlag_IDLE(g_ctx.uart);
        g_ctx.idle_flag = true;
        g_ctx.idle_timestamp_us = modbus_ticks_now_us();
    }
}

void modbus_stm32_idle_dma_isr(void)
{
    g_ctx.dma_pending = true;
    g_ctx.idle_timestamp_us = modbus_ticks_now_us();
}
