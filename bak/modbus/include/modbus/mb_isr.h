/**
 * @file mb_isr.h
 * @brief ISR-safe mode for fast half-duplex Modbus links with minimal turnaround time.
 *
 * This module provides interrupt-safe APIs for Modbus operations that need to run
 * within ISR context, particularly for RTU/ASCII links with tight timing constraints.
 *
 * ## Key Features
 *
 * - **Fast TX-after-RX turnaround**: <100µs on typical MCUs (72 MHz Cortex-M)
 * - **Minimal ISR overhead**: Lightweight operations, heavy work deferred
 * - **Context detection**: Automatic ISR vs thread context detection
 * - **Safe logging**: Heavy logging suppressed in ISR, deferred to thread
 * - **Lock avoidance**: No mutex operations in ISR paths
 *
 * ## Typical Use Case: RTU Half-Duplex with DMA
 *
 * ```c
 * // UART RX DMA complete + IDLE line ISR
 * void uart_rx_isr(void) {
 *     uint8_t *buf;
 *     size_t len = dma_get_rx_data(&buf);
 *     
 *     // Process immediately in ISR for fast turnaround
 *     mb_on_rx_chunk_from_isr(&modbus_ctx, buf, len);
 *     
 *     // If response ready, start TX immediately
 *     if (mb_try_tx_from_isr(&modbus_ctx)) {
 *         uart_start_dma_tx();
 *     }
 * }
 *
 * // Main loop: minimal work, most done in ISR
 * void main_loop(void) {
 *     mb_client_poll(&client);  // Lightweight, ISR did heavy work
 * }
 * ```
 *
 * ## Performance Targets
 *
 * | MCU           | Clock | Turnaround | Notes                    |
 * |---------------|-------|------------|--------------------------|
 * | STM32F1       | 72MHz | <80µs      | Cortex-M3, no cache      |
 * | STM32F4       | 168MHz| <50µs      | Cortex-M4F, cache+FPU    |
 * | ESP32-C3      | 160MHz| <60µs      | RISC-V, single core      |
 * | nRF52840      | 64MHz | <90µs      | Cortex-M4F, BLE overhead |
 *
 * ## Configuration
 *
 * Enable ISR-safe mode in `conf.h`:
 * ```c
 * #define MB_CONF_ENABLE_ISR_MODE 1
 * ```
 *
 * Disable heavy logging in ISR:
 * ```c
 * #define MB_CONF_ISR_SUPPRESS_LOGGING 1
 * ```
 *
 * ## Thread Safety
 *
 * ISR-safe functions use lock-free operations (SPSC queues, atomic flags) to avoid
 * deadlocks. The general pattern:
 *
 * - **ISR → Thread**: Use `mb_queue_spsc_enqueue()` (lock-free producer)
 * - **Thread → ISR**: Use atomic flags or dedicated hardware registers
 * - **Never**: Call mutex lock/unlock from ISR
 *
 * @note Requires C11 atomics for true lock-free operation. Falls back to
 *       disabling interrupts on platforms without atomics.
 */

#ifndef MODBUS_MB_ISR_H
#define MODBUS_MB_ISR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <modbus/conf.h>
#include <modbus/mb_err.h>
#include <modbus/mb_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* ISR Context Detection                                                      */
/* ========================================================================== */

/**
 * @brief Checks if code is currently executing in ISR context.
 *
 * Platform-specific detection:
 * - **ARM Cortex-M**: Reads IPSR (Interrupt Program Status Register)
 * - **FreeRTOS**: Uses `xPortIsInsideInterrupt()`
 * - **Zephyr**: Uses `k_is_in_isr()`
 * - **Fallback**: Thread-local flag (requires manual setting)
 *
 * @retval true  Code is running in ISR/exception handler
 * @retval false Code is running in thread/task context
 *
 * @note This is a lightweight check, typically <10 CPU cycles.
 */
bool mb_in_isr(void);

/**
 * @brief Sets ISR context flag manually (for platforms without auto-detection).
 *
 * Use at ISR entry/exit if automatic detection is unavailable:
 * ```c
 * void my_isr(void) {
 *     mb_set_isr_context(true);
 *     // ... ISR work ...
 *     mb_set_isr_context(false);
 * }
 * ```
 *
 * @param[in] in_isr true if entering ISR, false if leaving
 *
 * @note Only needed on platforms without IPSR/FreeRTOS/Zephyr.
 */
void mb_set_isr_context(bool in_isr);

/* ========================================================================== */
/* ISR-Safe RX Path                                                           */
/* ========================================================================== */

#include <modbus/internal/mb_queue.h>

/**
 * @brief Statistics for ISR-safe operations.
 */
typedef struct mb_isr_stats {
    uint32_t rx_chunks_processed;    /**< Total RX chunks from ISR */
    uint32_t tx_started_from_isr;    /**< Total TX started from ISR */
    uint32_t fast_turnarounds;       /**< RX→TX without thread involvement */
    uint32_t queue_full_events;      /**< Backpressure events (queue full) */
    
    uint32_t min_turnaround_us;      /**< Fastest observed turnaround */
    uint32_t max_turnaround_us;      /**< Slowest observed turnaround */
    uint32_t avg_turnaround_us;      /**< Average turnaround time */
    
    uint32_t isr_overruns;           /**< ISR called while previous not complete */
} mb_isr_stats_t;

/**
 * @brief ISR-safe Modbus context structure.
 *
 * Contains lock-free queues, buffers, and statistics for ISR-safe operation.
 */
typedef struct mb_isr_ctx {
    mb_queue_spsc_t   rx_queue;           /**< Lock-free SPSC RX queue */
    mb_queue_spsc_t   tx_queue;           /**< Lock-free SPSC TX queue */
    
    uint8_t          *rx_buffer;          /**< Scratch buffer for RX */
    size_t            rx_buffer_size;     /**< RX buffer size */
    uint8_t          *tx_buffer;          /**< Scratch buffer for TX */
    size_t            tx_buffer_size;     /**< TX buffer size */
    
    const uint8_t    *current_tx_data;    /**< Current TX data pointer */
    size_t            current_tx_len;     /**< Current TX data length */
    bool              tx_in_progress;     /**< TX in progress flag */
    
    mb_isr_stats_t    stats;              /**< Performance statistics */
    
    uint32_t          turnaround_target_us; /**< Target turnaround time */
    uint32_t          last_rx_timestamp;  /**< Last RX timestamp (µs) */
    
    bool              enable_logging;     /**< Logging enabled flag */
} mb_isr_ctx_t;

/**
 * @brief Processes incoming data chunk from ISR context.
 *
 * Optimized for minimal latency:
 * - Validates frame structure (address, CRC)
 * - Enqueues to lock-free SPSC queue
 * - Prepares response if server mode
 * - No malloc, no heavy logging
 *
 * @param[in] ctx  ISR context handle
 * @param[in] data Pointer to received data (DMA buffer, ring buffer, etc.)
 * @param[in] len  Number of bytes received
 *
 * @retval MB_OK                 Chunk processed successfully
 * @retval MB_ERR_INVALID_ARGUMENT ctx or data is NULL, len is 0
 * @retval MB_ERR_BUSY           Queue full, backpressure applied
 *
 * @note This function MUST be called from ISR context. Typically invoked
 *       from UART RX DMA complete + IDLE line interrupt.
 *
 * @warning Do not call from thread context! Use `mb_on_rx_chunk()` instead.
 */
mb_err_t mb_on_rx_chunk_from_isr(mb_isr_ctx_t *ctx, const uint8_t *data, size_t len);

/* ========================================================================== */
/* ISR-Safe TX Path                                                           */
/* ========================================================================== */

/**
 * @brief Attempts to start TX transmission from ISR context.
 *
 * Checks if response/request is ready and initiates transmission:
 * - Reads from TX queue (lock-free dequeue)
 * - Configures DMA/UART for transmission
 * - Returns immediately, no blocking
 *
 * Typical usage after RX:
 * ```c
 * void uart_rx_complete_isr(void) {
 *     mb_on_rx_chunk_from_isr(&ctx, rx_buf, rx_len);
 *     
 *     // Try immediate TX for fast turnaround
 *     if (mb_try_tx_from_isr(&ctx)) {
 *         uart_start_tx_dma();
 *     }
 * }
 * ```
 *
 * @param[in] ctx ISR context handle
 *
 * @retval true  TX started, data available for transmission
 * @retval false No data to transmit, or TX already in progress
 *
 * @note Returns immediately, does not block waiting for TX complete.
 */
bool mb_try_tx_from_isr(mb_isr_ctx_t *ctx);

/**
 * @brief Gets pointer to TX buffer and length (zero-copy).
 *
 * For DMA-based transmission, get direct pointer to avoid memcpy:
 * ```c
 * const uint8_t *tx_data;
 * size_t tx_len;
 * if (mb_get_tx_buffer_from_isr(&ctx, &tx_data, &tx_len)) {
 *     uart_dma_start_tx(tx_data, tx_len);
 * }
 * ```
 *
 * @param[in]  ctx      ISR context handle
 * @param[out] out_data Pointer to TX buffer (read-only)
 * @param[out] out_len  Length of TX data in bytes
 *
 * @retval true  TX buffer ready
 * @retval false No data to transmit
 *
 * @note Buffer remains valid until `mb_tx_complete_from_isr()` is called.
 */
bool mb_get_tx_buffer_from_isr(mb_isr_ctx_t *ctx, const uint8_t **out_data, size_t *out_len);

/**
 * @brief Notifies that TX transmission completed.
 *
 * Call from TX complete ISR:
 * ```c
 * void uart_tx_complete_isr(void) {
 *     mb_tx_complete_from_isr(&ctx);
 * }
 * ```
 *
 * @param[in] ctx ISR context handle
 *
 * @note Releases TX buffer, allows next transmission.
 */
void mb_tx_complete_from_isr(mb_isr_ctx_t *ctx);

/* ========================================================================== */
/* Context Initialization & Management                                        */
/* ========================================================================== */

/**
 * @brief Configuration for ISR-safe context.
 */
typedef struct mb_isr_config {
    void            **rx_queue_slots;     /**< Storage for RX queue (power of 2) */
    size_t            rx_queue_capacity;  /**< Number of slots in RX queue */
    
    void            **tx_queue_slots;     /**< Storage for TX queue */
    size_t            tx_queue_capacity;  /**< Number of slots in TX queue */
    
    uint8_t          *rx_buffer;          /**< Scratch buffer for RX processing */
    size_t            rx_buffer_size;     /**< Size of RX scratch buffer */
    
    uint8_t          *tx_buffer;          /**< Scratch buffer for TX preparation */
    size_t            tx_buffer_size;     /**< Size of TX scratch buffer */
    
    bool              enable_logging;     /**< Enable lightweight logging in ISR */
    uint32_t          turnaround_target_us; /**< Target turnaround time (diagnostic) */
} mb_isr_config_t;

/**
 * @brief Initializes ISR-safe context with external storage.
 *
 * @param[out] ctx    Context to initialize
 * @param[in]  config Configuration with queue/buffer storage
 *
 * @retval MB_OK                     Context initialized successfully
 * @retval MB_ERR_INVALID_ARGUMENT   NULL pointers or invalid config
 *
 * @note All storage (queues, buffers) must remain valid for context lifetime.
 */
mb_err_t mb_isr_ctx_init(mb_isr_ctx_t *ctx, const mb_isr_config_t *config);

/**
 * @brief Deinitializes ISR-safe context.
 *
 * @param[in] ctx Context to deinitialize
 */
void mb_isr_ctx_deinit(mb_isr_ctx_t *ctx);

/* ========================================================================== */
/* Performance Monitoring                                                     */
/* ========================================================================== */

/**
 * @brief Retrieves ISR operation statistics.
 *
 * @param[in]  ctx   ISR context handle
 * @param[out] stats Structure to fill with statistics
 *
 * @retval MB_OK                     Statistics retrieved successfully
 * @retval MB_ERR_INVALID_ARGUMENT   NULL pointer
 */
mb_err_t mb_isr_get_stats(const mb_isr_ctx_t *ctx, mb_isr_stats_t *stats);

/**
 * @brief Resets ISR statistics counters.
 *
 * @param[in] ctx ISR context handle
 */
void mb_isr_reset_stats(mb_isr_ctx_t *ctx);

/* ========================================================================== */
/* Macros & Helpers                                                           */
/* ========================================================================== */

/**
 * @def MB_IN_ISR()
 * @brief Macro to check if currently in ISR context.
 *
 * Use for conditional code:
 * ```c
 * if (MB_IN_ISR()) {
 *     // Fast ISR path
 * } else {
 *     // Normal thread path
 * }
 * ```
 */
#define MB_IN_ISR() mb_in_isr()

/**
 * @def MB_ISR_SAFE_LOG(level, msg)
 * @brief Lightweight logging for ISR context.
 *
 * Suppresses detailed logging, only critical errors logged.
 * ```c
 * MB_ISR_SAFE_LOG(ERROR, "RX CRC mismatch");
 * ```
 */
#if MB_CONF_ISR_SUPPRESS_LOGGING
#  define MB_ISR_SAFE_LOG(level, msg) ((void)0)
#else
#  define MB_ISR_SAFE_LOG(level, msg) \
    do { if (!MB_IN_ISR()) { MB_LOG_##level(msg); } } while(0)
#endif

/**
 * @def MB_ASSERT_NOT_ISR()
 * @brief Runtime assertion that code is NOT in ISR context.
 *
 * Use for functions that must never be called from ISR:
 * ```c
 * void heavy_operation(void) {
 *     MB_ASSERT_NOT_ISR();
 *     // ... malloc, mutex, heavy logging ...
 * }
 * ```
 */
#ifdef MB_CONF_ENABLE_ASSERTIONS
#  define MB_ASSERT_NOT_ISR() \
    do { if (MB_IN_ISR()) { MB_PANIC("Called from ISR!"); } } while(0)
#else
#  define MB_ASSERT_NOT_ISR() ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MB_ISR_H */
