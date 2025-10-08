/**
 * @file mb_qos.c
 * @brief Implementation of QoS and backpressure management.
 */

#include <modbus/mb_qos.h>
#include <string.h>

/* ========================================================================== */
/* Helper: Transaction Structure Access                                       */
/* ========================================================================== */

/**
 * @brief Minimal transaction structure for QoS operations.
 * 
 * Note: In production, this would match the actual mb_transaction_t from core.h
 */
typedef struct mb_qos_transaction {
    uint8_t  slave_address;
    uint8_t  function_code;
    uint32_t deadline_ms;
    uint32_t enqueue_timestamp;
    mb_qos_priority_t priority;  /**< Explicitly set priority (for APPLICATION policy) */
} mb_qos_transaction_t;

/* ========================================================================== */
/* Priority Determination                                                     */
/* ========================================================================== */

bool mb_qos_is_high_priority_fc(uint8_t function_code) {
    static const uint8_t high_priority_fcs[] = { MB_QOS_HIGH_PRIORITY_FCS };
    
    for (size_t i = 0; i < sizeof(high_priority_fcs); i++) {
        if (function_code == high_priority_fcs[i]) {
            return true;
        }
    }
    
    return false;
}

mb_qos_priority_t mb_qos_get_priority(const mb_qos_ctx_t *ctx, const void *tx) {
    if (!ctx || !tx) {
        return MB_QOS_PRIORITY_NORMAL;
    }
    
    const mb_qos_transaction_t *qtx = (const mb_qos_transaction_t *)tx;
    
    switch (ctx->policy) {
        case MB_QOS_POLICY_FC_BASED:
            return mb_qos_is_high_priority_fc(qtx->function_code) 
                   ? MB_QOS_PRIORITY_HIGH 
                   : MB_QOS_PRIORITY_NORMAL;
        
        case MB_QOS_POLICY_DEADLINE_BASED: {
            if (!ctx->now_ms) {
                return MB_QOS_PRIORITY_NORMAL;
            }
            
            uint32_t now = ctx->now_ms();
            uint32_t time_to_deadline = (qtx->deadline_ms > now) 
                                        ? (qtx->deadline_ms - now) 
                                        : 0;
            
            return (time_to_deadline < ctx->deadline_threshold_ms)
                   ? MB_QOS_PRIORITY_HIGH
                   : MB_QOS_PRIORITY_NORMAL;
        }
        
        case MB_QOS_POLICY_APPLICATION:
            return qtx->priority;
        
        case MB_QOS_POLICY_HYBRID: {
            // Start with FC-based
            bool high_by_fc = mb_qos_is_high_priority_fc(qtx->function_code);
            
            // Check deadline urgency
            if (ctx->now_ms) {
                uint32_t now = ctx->now_ms();
                uint32_t time_to_deadline = (qtx->deadline_ms > now)
                                            ? (qtx->deadline_ms - now)
                                            : 0;
                
                if (time_to_deadline < ctx->deadline_threshold_ms) {
                    return MB_QOS_PRIORITY_HIGH;
                }
            }
            
            return high_by_fc ? MB_QOS_PRIORITY_HIGH : MB_QOS_PRIORITY_NORMAL;
        }
        
        default:
            return MB_QOS_PRIORITY_NORMAL;
    }
}

/* ========================================================================== */
/* Initialization & Management                                                */
/* ========================================================================== */

mb_err_t mb_qos_ctx_init(mb_qos_ctx_t *ctx, const mb_qos_config_t *config) {
    if (!ctx || !config) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    if (!config->high_queue_slots || config->high_capacity == 0 ||
        !config->normal_queue_slots || config->normal_capacity == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    if (config->enable_monitoring && !config->now_ms) {
        return MB_ERR_INVALID_ARGUMENT; // Monitoring requires timestamp function
    }
    
    memset(ctx, 0, sizeof(*ctx));
    
    // Initialize high priority queue
    mb_err_t err = mb_queue_spsc_init(&ctx->high_queue,
                                      config->high_queue_slots,
                                      config->high_capacity);
    if (err != MB_OK) {
        return err;
    }
    
    // Initialize normal priority queue
    err = mb_queue_spsc_init(&ctx->normal_queue,
                             config->normal_queue_slots,
                             config->normal_capacity);
    if (err != MB_OK) {
        mb_queue_spsc_deinit(&ctx->high_queue);
        return err;
    }
    
    ctx->policy = config->policy;
    ctx->deadline_threshold_ms = config->deadline_threshold_ms;
    ctx->enable_monitoring = config->enable_monitoring;
    ctx->now_ms = config->now_ms;
    
    // Initialize statistics
    ctx->stats.high.min_latency_ms = UINT32_MAX;
    ctx->stats.normal.min_latency_ms = UINT32_MAX;
    
    return MB_OK;
}

void mb_qos_ctx_deinit(mb_qos_ctx_t *ctx) {
    if (!ctx) {
        return;
    }
    
    mb_queue_spsc_deinit(&ctx->high_queue);
    mb_queue_spsc_deinit(&ctx->normal_queue);
    
    memset(ctx, 0, sizeof(*ctx));
}

/* ========================================================================== */
/* Queue Operations                                                           */
/* ========================================================================== */

mb_err_t mb_qos_enqueue(mb_qos_ctx_t *ctx, void *tx) {
    if (!ctx || !tx) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    mb_qos_transaction_t *qtx = (mb_qos_transaction_t *)tx;
    
    // Determine priority
    mb_qos_priority_t priority = mb_qos_get_priority(ctx, tx);
    
    // Store enqueue timestamp for latency tracking
    if (ctx->enable_monitoring && ctx->now_ms) {
        qtx->enqueue_timestamp = ctx->now_ms();
    }
    
    // Enqueue to appropriate queue
    bool success;
    if (priority == MB_QOS_PRIORITY_HIGH) {
        success = mb_queue_spsc_enqueue(&ctx->high_queue, tx);
        
        if (success) {
            ctx->stats.high.enqueued++;
            ctx->stats.current_high_depth++;
            
            if (ctx->stats.current_high_depth > ctx->stats.high_water_mark_high) {
                ctx->stats.high_water_mark_high = ctx->stats.current_high_depth;
            }
        } else {
            // High priority queue full - system overload!
            ctx->stats.queue_full_events++;
            return MB_ERR_NO_RESOURCES;
        }
    } else {
        success = mb_queue_spsc_enqueue(&ctx->normal_queue, tx);
        
        if (success) {
            ctx->stats.normal.enqueued++;
            ctx->stats.current_normal_depth++;
            
            if (ctx->stats.current_normal_depth > ctx->stats.high_water_mark_normal) {
                ctx->stats.high_water_mark_normal = ctx->stats.current_normal_depth;
            }
        } else {
            // Normal priority queue full - apply backpressure
            ctx->stats.normal.rejected++;
            ctx->stats.queue_full_events++;
            return MB_ERR_BUSY;
        }
    }
    
    return MB_OK;
}

void *mb_qos_dequeue(mb_qos_ctx_t *ctx) {
    if (!ctx) {
        return NULL;
    }
    
    void *tx = NULL;
    
    // Always check high priority queue first
    if (mb_queue_spsc_dequeue(&ctx->high_queue, &tx)) {
        ctx->stats.current_high_depth--;
        return tx;
    }
    
    // If high queue empty, check normal priority
    if (mb_queue_spsc_dequeue(&ctx->normal_queue, &tx)) {
        ctx->stats.current_normal_depth--;
        
        // Check for priority inversion (should never happen in SPSC)
        if (ctx->stats.current_high_depth > 0) {
            ctx->stats.priority_inversions++;
        }
        
        return tx;
    }
    
    return NULL;
}

void mb_qos_complete(mb_qos_ctx_t *ctx, void *tx) {
    if (!ctx || !tx) {
        return;
    }
    
    if (!ctx->enable_monitoring || !ctx->now_ms) {
        return; // No monitoring enabled
    }
    
    mb_qos_transaction_t *qtx = (mb_qos_transaction_t *)tx;
    
    uint32_t now = ctx->now_ms();
    uint32_t latency = now - qtx->enqueue_timestamp;
    
    // Determine which priority class this was
    mb_qos_priority_t priority = mb_qos_get_priority(ctx, tx);
    mb_qos_priority_stats_t *pstats = (priority == MB_QOS_PRIORITY_HIGH)
                                      ? &ctx->stats.high
                                      : &ctx->stats.normal;
    
    pstats->completed++;
    
    // Update latency statistics
    if (latency < pstats->min_latency_ms) {
        pstats->min_latency_ms = latency;
    }
    
    if (latency > pstats->max_latency_ms) {
        pstats->max_latency_ms = latency;
    }
    
    // Update running average
    uint32_t total = pstats->completed;
    pstats->avg_latency_ms = ((pstats->avg_latency_ms * (total - 1)) + latency) / total;
    
    // Check for deadline miss
    if (qtx->deadline_ms > 0 && now > qtx->deadline_ms) {
        pstats->deadline_misses++;
    }
}

/* ========================================================================== */
/* Statistics & Monitoring                                                    */
/* ========================================================================== */

mb_err_t mb_qos_get_stats(const mb_qos_ctx_t *ctx, mb_qos_stats_t *stats) {
    if (!ctx || !stats) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    *stats = ctx->stats;
    return MB_OK;
}

void mb_qos_reset_stats(mb_qos_ctx_t *ctx) {
    if (!ctx) {
        return;
    }
    
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    ctx->stats.high.min_latency_ms = UINT32_MAX;
    ctx->stats.normal.min_latency_ms = UINT32_MAX;
}
