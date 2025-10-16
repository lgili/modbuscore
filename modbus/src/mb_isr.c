/**
 * @file mb_isr.c
 * @brief Implementation of ISR-safe Modbus operations.
 */

#include <modbus/mb_isr.h>
#include <modbus/internal/mb_queue.h>
#include <modbus/conf.h>
#include <string.h>

#if defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_8M_BASE__) || defined(__ARM_ARCH_8M_MAIN__)
#  define MB_HAS_CORTEX_M_IPSR 1
#else
#  define MB_HAS_CORTEX_M_IPSR 0
#endif

#if defined(FREERTOS_CONFIG_H) || defined(INC_FREERTOS_H)
#  include "FreeRTOS.h"
#  include "task.h"
#  define MB_HAS_FREERTOS 1
#else
#  define MB_HAS_FREERTOS 0
#endif

#if defined(CONFIG_ZEPHYR)
#  include <zephyr/kernel.h>
#  define MB_HAS_ZEPHYR 1
#else
#  define MB_HAS_ZEPHYR 0
#endif

/* ==================================================================== */
/* ISR Context Detection                                                  */
/* ========================================================================== */

#if MB_HAS_CORTEX_M_IPSR
static inline uint32_t mb_read_ipsr(void) {
    uint32_t result;
    __asm volatile ("MRS %0, ipsr" : "=r" (result));
    return result;
}
#endif

static bool g_manual_isr_flag = false;

bool mb_in_isr(void) {
#if MB_HAS_CORTEX_M_IPSR
    // ARM Cortex-M: IPSR != 0 means in exception/interrupt
    return mb_read_ipsr() != 0;
#elif MB_HAS_FREERTOS
    // FreeRTOS: Use built-in ISR detection
    return xPortIsInsideInterrupt() != 0;
#elif MB_HAS_ZEPHYR
    // Zephyr RTOS: Use kernel ISR check
    return k_is_in_isr();
#else
    // Fallback: Manual flag (user must call mb_set_isr_context)
    return g_manual_isr_flag;
#endif
}

void mb_set_isr_context(bool in_isr) {
    g_manual_isr_flag = in_isr;
}

/* ========================================================================== */
/* Initialization & Management                                                */
/* ========================================================================== */

mb_err_t mb_isr_ctx_init(mb_isr_ctx_t *ctx, const mb_isr_config_t *config) {
    if (!ctx || !config) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    if (!config->rx_queue_slots || config->rx_queue_capacity == 0 ||
        !config->tx_queue_slots || config->tx_queue_capacity == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    memset(ctx, 0, sizeof(*ctx));
    
    // Initialize RX queue (ISR produces, thread consumes)
    mb_err_t err = mb_queue_spsc_init(&ctx->rx_queue, 
                                      config->rx_queue_slots,
                                      config->rx_queue_capacity);
    if (err != MB_OK) {
        return err;
    }
    
    // Initialize TX queue (thread produces, ISR consumes)
    err = mb_queue_spsc_init(&ctx->tx_queue,
                             config->tx_queue_slots,
                             config->tx_queue_capacity);
    if (err != MB_OK) {
        mb_queue_spsc_deinit(&ctx->rx_queue);
        return err;
    }
    
    ctx->rx_buffer = config->rx_buffer;
    ctx->rx_buffer_size = config->rx_buffer_size;
    ctx->tx_buffer = config->tx_buffer;
    ctx->tx_buffer_size = config->tx_buffer_size;
    
    ctx->enable_logging = config->enable_logging;
    ctx->turnaround_target_us = config->turnaround_target_us;
    
    // Initialize statistics
    ctx->stats.min_turnaround_us = UINT32_MAX;
    ctx->stats.max_turnaround_us = 0;
    ctx->stats.avg_turnaround_us = 0;
    
    return MB_OK;
}

void mb_isr_ctx_deinit(mb_isr_ctx_t *ctx) {
    if (!ctx) {
        return;
    }
    
    mb_queue_spsc_deinit(&ctx->rx_queue);
    mb_queue_spsc_deinit(&ctx->tx_queue);
    
    memset(ctx, 0, sizeof(*ctx));
}

/* ========================================================================== */
/* ISR-Safe RX Path                                                           */
/* ========================================================================== */

mb_err_t mb_on_rx_chunk_from_isr(mb_isr_ctx_t *ctx, const uint8_t *data, size_t len) {
    if (!ctx || !data || len == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    // Quick validation: minimum Modbus frame size (address + FC + CRC)
    if (len < 4) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    // Copy to scratch buffer (avoids DMA buffer being reused)
    if (len > ctx->rx_buffer_size) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    memcpy(ctx->rx_buffer, data, len);
    
    // Enqueue pointer to scratch buffer (lock-free)
    // Note: In production, would allocate from pool or use ring buffer indices
    if (!mb_queue_spsc_enqueue(&ctx->rx_queue, ctx->rx_buffer)) {
        ctx->stats.queue_full_events++;
        return MB_ERR_BUSY;
    }
    
    ctx->stats.rx_chunks_processed++;
    ctx->last_rx_timestamp = 0; // Would use DWT cycle counter or timer in production
    
    return MB_OK;
}

/* ========================================================================== */
/* ISR-Safe TX Path                                                           */
/* ========================================================================== */

bool mb_try_tx_from_isr(mb_isr_ctx_t *ctx) {
    if (!ctx || ctx->tx_in_progress) {
        return false;
    }
    
    // Try to dequeue TX data (lock-free)
    void *tx_data_ptr;
    if (!mb_queue_spsc_dequeue(&ctx->tx_queue, &tx_data_ptr)) {
        return false; // No data ready
    }
    
    ctx->current_tx_data = (const uint8_t*)tx_data_ptr;
    ctx->current_tx_len = ctx->tx_buffer_size; // In production, would store actual length
    ctx->tx_in_progress = true;
    
    ctx->stats.tx_started_from_isr++;
    
    // Calculate turnaround time if we just received data
    if (ctx->last_rx_timestamp != 0) {
        uint32_t turnaround = 0; // Would calculate from timestamps
        
        if (turnaround < ctx->stats.min_turnaround_us) {
            ctx->stats.min_turnaround_us = turnaround;
        }
        if (turnaround > ctx->stats.max_turnaround_us) {
            ctx->stats.max_turnaround_us = turnaround;
        }
        
        // Update running average
        uint32_t total = ctx->stats.tx_started_from_isr;
        ctx->stats.avg_turnaround_us = 
            ((ctx->stats.avg_turnaround_us * (total - 1)) + turnaround) / total;
        
        ctx->stats.fast_turnarounds++;
    }
    
    return true;
}

bool mb_get_tx_buffer_from_isr(mb_isr_ctx_t *ctx, const uint8_t **out_data, size_t *out_len) {
    if (!ctx || !out_data || !out_len) {
        return false;
    }
    
    if (!ctx->tx_in_progress || !ctx->current_tx_data) {
        return false;
    }
    
    *out_data = ctx->current_tx_data;
    *out_len = ctx->current_tx_len;
    
    return true;
}

void mb_tx_complete_from_isr(mb_isr_ctx_t *ctx) {
    if (!ctx) {
        return;
    }
    
    ctx->tx_in_progress = false;
    ctx->current_tx_data = NULL;
    ctx->current_tx_len = 0;
}

/* ========================================================================== */
/* Statistics                                                                 */
/* ========================================================================== */

mb_err_t mb_isr_get_stats(const mb_isr_ctx_t *ctx, mb_isr_stats_t *stats) {
    if (!ctx || !stats) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    *stats = ctx->stats;
    return MB_OK;
}

void mb_isr_reset_stats(mb_isr_ctx_t *ctx) {
    if (!ctx) {
        return;
    }
    
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    ctx->stats.min_turnaround_us = UINT32_MAX;
}
