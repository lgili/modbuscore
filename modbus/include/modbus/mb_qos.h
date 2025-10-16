/**
 * @file mb_qos.h
 * @brief Quality of Service (QoS) and backpressure management for Modbus transactions.
 *
 * This module provides priority-aware queue management to prevent head-of-line blocking
 * and ensure critical transactions meet latency targets even under heavy load.
 *
 * ## Key Features
 *
 * - **Two-tier priority system**: High (critical) and Normal (best-effort)
 * - **Backpressure handling**: Early rejection of non-critical requests when queue is full
 * - **Policy-based prioritization**: By function code, deadline, or application tag
 * - **Performance monitoring**: Per-priority latency tracking and queue pressure metrics
 *
 * ## Priority Classes
 *
 * ### High Priority (Critical)
 * - Time-sensitive operations (alarms, emergency shutdowns)
 * - Default FCs: 05 (Write Single Coil), 06 (Write Single Register), 08 (Diagnostics)
 * - Never dropped due to backpressure
 *
 * ### Normal Priority (Best-Effort)
 * - Bulk reads, periodic polling
 * - Default FCs: 01, 02, 03, 04, 15, 16, 23
 * - May be rejected if queue is full
 *
 * ## Usage Example
 *
 * ```c
 * #include <modbus/mb_qos.h>
 *
 * // Initialize QoS context
 * mb_qos_ctx_t qos;
 * mb_qos_config_t config = {
 *     .high_capacity = 8,      // 8 slots for critical
 *     .normal_capacity = 24,   // 24 slots for best-effort
 *     .policy = MB_QOS_POLICY_FC_BASED,
 *     .enable_monitoring = true
 * };
 * mb_qos_ctx_init(&qos, &config);
 *
 * // Enqueue transaction with priority
 * mb_transaction_t tx;
 * tx.function_code = 0x06;  // Write Single Register (high priority)
 * tx.deadline_ms = now_ms() + 100;
 *
 * mb_err_t err = mb_qos_enqueue(&qos, &tx);
 * if (err == MB_ERR_BUSY) {
 *     // Queue full, non-critical rejected
 * }
 *
 * // Process highest priority transaction
 * mb_transaction_t *next = mb_qos_dequeue(&qos);
 * if (next) {
 *     // Handle transaction
 *     mb_qos_complete(&qos, next);
 * }
 *
 * // Monitor QoS metrics
 * mb_qos_stats_t stats;
 * mb_qos_get_stats(&qos, &stats);
 * printf("High priority avg latency: %u ms\n", stats.high_avg_latency_ms);
 * ```
 *
 * ## Configuration
 *
 * Enable in `conf.h`:
 * ```c
 * #define MB_CONF_ENABLE_QOS 1
 * ```
 *
 * @note Requires Gate 22 (lock-free queues) and Gate 23 (ISR-safe mode).
 */

#ifndef MODBUS_MB_QOS_H
#define MODBUS_MB_QOS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <modbus/conf.h>
#include <modbus/mb_err.h>
#include <modbus/mb_types.h>
#include <modbus/internal/mb_queue.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* Priority Classes                                                           */
/* ========================================================================== */

/**
 * @brief Transaction priority levels.
 */
typedef enum mb_qos_priority {
    MB_QOS_PRIORITY_HIGH = 0,    /**< Critical, never dropped */
    MB_QOS_PRIORITY_NORMAL = 1,  /**< Best-effort, may be rejected */
    MB_QOS_PRIORITY_MAX
} mb_qos_priority_t;

/* ========================================================================== */
/* QoS Policies                                                               */
/* ========================================================================== */

/**
 * @brief Policy for determining transaction priority.
 */
typedef enum mb_qos_policy {
    /**
     * @brief Priority based on function code.
     * 
     * High priority FCs: 05, 06, 08 (write single, diagnostics)
     * Normal priority FCs: 01, 02, 03, 04, 15, 16, 23 (reads, bulk writes)
     */
    MB_QOS_POLICY_FC_BASED = 0,
    
    /**
     * @brief Priority based on deadline.
     * 
     * Transactions with deadlines < threshold are high priority.
     */
    MB_QOS_POLICY_DEADLINE_BASED = 1,
    
    /**
     * @brief Priority explicitly set by application.
     * 
     * Application sets `priority` field in transaction.
     */
    MB_QOS_POLICY_APPLICATION = 2,
    
    /**
     * @brief Hybrid: FC-based with deadline override.
     * 
     * Default to FC-based, but promote to high if deadline is urgent.
     */
    MB_QOS_POLICY_HYBRID = 3
} mb_qos_policy_t;

/* ========================================================================== */
/* QoS Statistics                                                             */
/* ========================================================================== */

/**
 * @brief Per-priority QoS statistics.
 */
typedef struct mb_qos_priority_stats {
    uint32_t enqueued;           /**< Total transactions enqueued */
    uint32_t completed;          /**< Total transactions completed */
    uint32_t rejected;           /**< Rejected due to backpressure */
    
    uint32_t min_latency_ms;     /**< Minimum latency (enqueue→complete) */
    uint32_t max_latency_ms;     /**< Maximum latency */
    uint32_t avg_latency_ms;     /**< Average latency */
    
    uint32_t deadline_misses;    /**< Transactions that missed deadline */
} mb_qos_priority_stats_t;

/**
 * @brief Overall QoS statistics.
 */
typedef struct mb_qos_stats {
    mb_qos_priority_stats_t high;    /**< High priority stats */
    mb_qos_priority_stats_t normal;  /**< Normal priority stats */
    
    uint32_t queue_full_events;      /**< Times queue reached capacity */
    uint32_t priority_inversions;    /**< Normal served before high (bug indicator) */
    
    uint32_t current_high_depth;     /**< Current high priority queue depth */
    uint32_t current_normal_depth;   /**< Current normal priority queue depth */
    
    uint32_t high_water_mark_high;   /**< Peak high priority queue depth */
    uint32_t high_water_mark_normal; /**< Peak normal priority queue depth */
} mb_qos_stats_t;

/* ========================================================================== */
/* QoS Context                                                                */
/* ========================================================================== */

/**
 * @brief QoS context structure.
 */
typedef struct mb_qos_ctx {
    mb_queue_spsc_t high_queue;      /**< High priority queue (lock-free) */
    mb_queue_spsc_t normal_queue;    /**< Normal priority queue */
    
    mb_qos_policy_t policy;          /**< Priority policy */
    uint32_t deadline_threshold_ms;  /**< Deadline threshold for policy */
    
    mb_qos_stats_t stats;            /**< Performance statistics */
    
    bool enable_monitoring;          /**< Enable detailed monitoring */
    uint32_t (*now_ms)(void);        /**< Timestamp function pointer */
} mb_qos_ctx_t;

/**
 * @brief QoS configuration.
 */
typedef struct mb_qos_config {
    void          **high_queue_slots;     /**< Storage for high priority queue */
    size_t          high_capacity;        /**< High priority queue capacity */
    
    void          **normal_queue_slots;   /**< Storage for normal priority queue */
    size_t          normal_capacity;      /**< Normal priority queue capacity */
    
    mb_qos_policy_t policy;               /**< Priority policy */
    uint32_t        deadline_threshold_ms;/**< Deadline threshold (policy-dependent) */
    
    bool            enable_monitoring;    /**< Enable latency tracking */
    uint32_t        (*now_ms)(void);      /**< Timestamp function (required if monitoring) */
} mb_qos_config_t;

/* ========================================================================== */
/* Initialization & Management                                                */
/* ========================================================================== */

/**
 * @brief Initializes QoS context with external storage.
 *
 * @param[out] ctx    Context to initialize
 * @param[in]  config Configuration with queue storage and policy
 *
 * @retval MB_OK                     Context initialized successfully
 * @retval MB_ERR_INVALID_ARGUMENT   NULL pointers or invalid config
 */
mb_err_t mb_qos_ctx_init(mb_qos_ctx_t *ctx, const mb_qos_config_t *config);

/**
 * @brief Deinitializes QoS context.
 *
 * @param[in] ctx Context to deinitialize
 */
void mb_qos_ctx_deinit(mb_qos_ctx_t *ctx);

/* ========================================================================== */
/* Queue Operations                                                           */
/* ========================================================================== */

/**
 * @brief Enqueues transaction with priority-based backpressure.
 *
 * Behavior:
 * - High priority: Always enqueued (unless high queue full, which is a system error)
 * - Normal priority: Rejected if normal queue full (backpressure applied)
 *
 * @param[in] ctx Context handle
 * @param[in] tx  Transaction to enqueue (priority determined by policy)
 *
 * @retval MB_OK                 Transaction enqueued successfully
 * @retval MB_ERR_BUSY           Queue full, non-critical transaction rejected
 * @retval MB_ERR_INVALID_ARGUMENT NULL pointer or invalid transaction
 * @retval MB_ERR_NO_RESOURCES   High priority queue full (system overload)
 */
mb_err_t mb_qos_enqueue(mb_qos_ctx_t *ctx, void *tx);

/**
 * @brief Dequeues highest priority transaction.
 *
 * Priority order:
 * 1. High priority queue (always checked first)
 * 2. Normal priority queue (only if high queue empty)
 *
 * @param[in] ctx Context handle
 *
 * @return Pointer to transaction, or NULL if both queues empty
 */
void *mb_qos_dequeue(mb_qos_ctx_t *ctx);

/**
 * @brief Marks transaction as completed and updates statistics.
 *
 * @param[in] ctx Context handle
 * @param[in] tx  Completed transaction
 */
void mb_qos_complete(mb_qos_ctx_t *ctx, void *tx);

/* ========================================================================== */
/* Priority Determination                                                     */
/* ========================================================================== */

/**
 * @brief Determines transaction priority based on configured policy.
 *
 * @param[in] ctx Context handle
 * @param[in] tx  Transaction to evaluate
 *
 * @return Priority level (HIGH or NORMAL)
 */
mb_qos_priority_t mb_qos_get_priority(const mb_qos_ctx_t *ctx, const void *tx);

/**
 * @brief Checks if function code is high priority (FC-based policy).
 *
 * High priority FCs: 05, 06, 08
 *
 * @param[in] function_code Modbus function code
 *
 * @retval true  Function code is high priority
 * @retval false Function code is normal priority
 */
bool mb_qos_is_high_priority_fc(uint8_t function_code);

/* ========================================================================== */
/* Statistics & Monitoring                                                    */
/* ========================================================================== */

/**
 * @brief Retrieves QoS statistics.
 *
 * @param[in]  ctx   Context handle
 * @param[out] stats Structure to fill with statistics
 *
 * @retval MB_OK                     Statistics retrieved successfully
 * @retval MB_ERR_INVALID_ARGUMENT   NULL pointer
 */
mb_err_t mb_qos_get_stats(const mb_qos_ctx_t *ctx, mb_qos_stats_t *stats);

/**
 * @brief Resets QoS statistics counters.
 *
 * @param[in] ctx Context handle
 */
void mb_qos_reset_stats(mb_qos_ctx_t *ctx);

/* ========================================================================== */
/* Macros & Helpers                                                           */
/* ========================================================================== */

/**
 * @def MB_QOS_HIGH_PRIORITY_FCS
 * @brief List of high priority function codes.
 */
#define MB_QOS_HIGH_PRIORITY_FCS \
    0x05, /* Write Single Coil */ \
    0x06, /* Write Single Register */ \
    0x08  /* Diagnostics */

/**
 * @def MB_QOS_DEFAULT_DEADLINE_THRESHOLD_MS
 * @brief Default deadline threshold for deadline-based policy (100 ms).
 */
#define MB_QOS_DEFAULT_DEADLINE_THRESHOLD_MS 100

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MB_QOS_H */
