/**
 * @file mb_txpool.h
 * @brief Fixed-latency transaction pool with freelist for zero-malloc Modbus operation.
 *
 * This module provides a static memory pool for managing Modbus transaction objects
 * without dynamic allocation. Transactions are pre-allocated at initialization and
 * recycled through a freelist, ensuring:
 *
 * - **Fixed latency**: O(1) acquire/release operations
 * - **Zero malloc**: All memory provided upfront by the application
 * - **Leak detection**: High-water-mark tracking and statistics
 * - **Thread-safe**: Optional mutex protection for concurrent access
 *
 * The pool is built on top of the generic `mb_mempool_t` but adds transaction-specific
 * semantics and diagnostics optimized for Modbus use cases.
 *
 * @section Usage Example
 *
 * @code
 * // Define transaction structure (application-specific)
 * typedef struct {
 *     uint8_t  slave_addr;
 *     uint16_t reg_addr;
 *     uint16_t reg_count;
 *     uint8_t  fc;
 *     uint32_t timestamp;
 * } my_transaction_t;
 *
 * // Static storage for 16 transactions
 * static uint8_t tx_storage[16 * sizeof(my_transaction_t)];
 * static mb_txpool_t tx_pool;
 *
 * void init() {
 *     mb_txpool_init(&tx_pool, tx_storage, sizeof(my_transaction_t), 16);
 * }
 *
 * void send_request() {
 *     my_transaction_t *tx = mb_txpool_acquire(&tx_pool);
 *     if (!tx) {
 *         // Pool exhausted - apply backpressure
 *         return;
 *     }
 *
 *     tx->slave_addr = 1;
 *     tx->reg_addr = 100;
 *     tx->reg_count = 10;
 *     tx->fc = 0x03;
 *     tx->timestamp = get_time_ms();
 *
 *     enqueue_for_transmission(tx);
 * }
 *
 * void on_response_complete(my_transaction_t *tx) {
 *     mb_txpool_release(&tx_pool, tx);
 * }
 *
 * void diagnostics() {
 *     mb_txpool_stats_t stats;
 *     mb_txpool_get_stats(&tx_pool, &stats);
 *     printf("Capacity: %zu, In-use: %zu, High-water: %zu, Total acquired: %llu\n",
 *            stats.capacity, stats.in_use, stats.high_water, stats.total_acquired);
 * }
 * @endcode
 *
 * @section Thread Safety
 *
 * The pool itself is **not thread-safe** by default. For concurrent access:
 * - Wrap acquire/release calls with external mutex
 * - Or use separate pools per thread
 * - Statistics counters are not atomic (diagnostic use only)
 */

#ifndef MODBUS_MB_TXPOOL_H
#define MODBUS_MB_TXPOOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <modbus/mb_err.h>
#include <modbus/mempool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Transaction pool statistics for diagnostics and leak detection.
 */
typedef struct mb_txpool_stats {
    size_t   capacity;        /**< Total number of transactions in pool */
    size_t   in_use;          /**< Currently allocated transactions */
    size_t   available;       /**< Currently free transactions */
    size_t   high_water;      /**< Peak concurrent allocations */
    uint64_t total_acquired;  /**< Total acquire() calls since init */
    uint64_t total_released;  /**< Total release() calls since init */
    uint64_t failed_acquires; /**< Number of acquire() failures (pool exhausted) */
} mb_txpool_stats_t;

/**
 * @brief Transaction pool instance.
 *
 * Wraps a generic memory pool with transaction-specific tracking and statistics.
 */
typedef struct mb_txpool {
    mb_mempool_t       pool;            /**< Underlying generic memory pool */
    mb_txpool_stats_t  stats;           /**< Runtime statistics */
    size_t             current_in_use;  /**< Current allocation count */
} mb_txpool_t;

/**
 * @brief Initializes a transaction pool with external storage.
 *
 * @param[out] txpool        Pool instance to initialize
 * @param[in]  storage       Backing memory (must persist, properly aligned)
 * @param[in]  tx_size       Size of each transaction structure in bytes
 * @param[in]  tx_count      Number of transactions to allocate
 *
 * @retval MB_OK                     Pool initialized successfully
 * @retval MB_ERR_INVALID_ARGUMENT   NULL pointer, zero size, or misalignment
 *
 * @note `storage` must remain valid for the pool's lifetime.
 * @note `tx_size` should be aligned (typically sizeof(your_transaction_t)).
 */
mb_err_t mb_txpool_init(mb_txpool_t *txpool,
                        void *storage,
                        size_t tx_size,
                        size_t tx_count);

/**
 * @brief Resets the pool to initial state (all transactions free).
 *
 * @param[in] txpool Pool instance to reset
 *
 * @warning Only call when no transactions are in use! Does not track leaks.
 */
void mb_txpool_reset(mb_txpool_t *txpool);

/**
 * @brief Acquires a transaction from the pool.
 *
 * @param[in] txpool Pool instance
 * @return Pointer to transaction memory, or NULL if pool is exhausted
 *
 * @note Caller must cast to the appropriate transaction type.
 * @note Returns uninitialized memory - caller must initialize fields.
 */
void *mb_txpool_acquire(mb_txpool_t *txpool);

/**
 * @brief Releases a transaction back to the pool.
 *
 * @param[in] txpool Pool instance
 * @param[in] tx     Transaction pointer previously returned by acquire()
 *
 * @retval MB_OK                     Transaction released successfully
 * @retval MB_ERR_INVALID_ARGUMENT   NULL pointer or tx not from this pool
 *
 * @note Releasing the same transaction twice is undefined behavior.
 * @note No validation is performed on the transaction contents.
 */
mb_err_t mb_txpool_release(mb_txpool_t *txpool, void *tx);

/**
 * @brief Retrieves current pool statistics.
 *
 * @param[in]  txpool Pool instance
 * @param[out] stats  Structure to fill with current statistics
 *
 * @retval MB_OK                     Statistics retrieved successfully
 * @retval MB_ERR_INVALID_ARGUMENT   NULL pointer
 *
 * @note Statistics are not atomic - for diagnostic use only.
 */
mb_err_t mb_txpool_get_stats(const mb_txpool_t *txpool, mb_txpool_stats_t *stats);

/**
 * @brief Returns total pool capacity.
 *
 * @param[in] txpool Pool instance
 * @return Number of transactions (0 if NULL)
 */
size_t mb_txpool_capacity(const mb_txpool_t *txpool);

/**
 * @brief Returns number of available (free) transactions.
 *
 * @param[in] txpool Pool instance
 * @return Free transaction count
 */
size_t mb_txpool_available(const mb_txpool_t *txpool);

/**
 * @brief Returns number of currently allocated transactions.
 *
 * @param[in] txpool Pool instance
 * @return In-use transaction count
 */
size_t mb_txpool_in_use(const mb_txpool_t *txpool);

/**
 * @brief Returns peak concurrent allocations.
 *
 * @param[in] txpool Pool instance
 * @return Highest in-use count ever reached
 */
size_t mb_txpool_high_water(const mb_txpool_t *txpool);

/**
 * @brief Checks if pool is exhausted.
 *
 * @param[in] txpool Pool instance
 * @retval true  No transactions available
 * @retval false At least one transaction available
 */
bool mb_txpool_is_empty(const mb_txpool_t *txpool);

/**
 * @brief Checks if pool has potential leaks (diagnostic).
 *
 * @param[in] txpool Pool instance
 * @retval true  Pool likely has leaked transactions (all allocated)
 * @retval false Pool has available transactions
 *
 * @note This is a heuristic - full pool might be legitimate under heavy load.
 */
bool mb_txpool_has_leaks(const mb_txpool_t *txpool);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MB_TXPOOL_H */
