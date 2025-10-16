/**
 * @file mb_queue.h
 * @brief Lock-free SPSC and MPSC queues for fixed-latency transaction management.
 *
 * This module provides two queue variants optimized for embedded Modbus applications:
 * 
 * - **SPSC (Single-Producer/Single-Consumer)**: Lock-free queue suitable for ISR → thread
 *   communication with zero blocking and predictable latency.
 * 
 * - **MPSC (Multi-Producer/Single-Consumer)**: Queue with minimal critical sections for
 *   multiple producers (e.g., multiple clients or tasks) with a single consumer thread.
 *
 * Both queues operate on fixed-size slots and work with external storage (no malloc).
 * High-water-mark tracking helps detect queue pressure during development.
 *
 * @section Performance Characteristics
 *
 * - **SPSC**: O(1) enqueue/dequeue, zero locks, ISR-safe with C11 atomics
 * - **MPSC**: O(1) enqueue with short critical section (~10 CPU cycles), lock-free dequeue
 * - **Memory**: ~32 bytes overhead per queue + slot storage
 * - **Latency**: Deterministic, no allocation, no blocking in SPSC
 *
 * @section Usage Example - SPSC (ISR to Main Loop)
 *
 * @code
 * // Storage for transaction pointers (power-of-2 capacity required)
 * static void *tx_slots[16];
 * static mb_queue_spsc_t tx_queue;
 *
 * void init() {
 *     mb_queue_spsc_init(&tx_queue, tx_slots, 16);
 * }
 *
 * void uart_rx_isr() {
 *     mb_transaction_t *tx = parse_incoming();
 *     if (!mb_queue_spsc_enqueue(&tx_queue, tx)) {
 *         // Queue full - apply backpressure
 *     }
 * }
 *
 * void main_loop() {
 *     mb_transaction_t *tx;
 *     while (mb_queue_spsc_dequeue(&tx_queue, (void**)&tx)) {
 *         process_transaction(tx);
 *     }
 * }
 * @endcode
 *
 * @section Usage Example - MPSC (Multiple Clients)
 *
 * @code
 * static void *request_slots[32];
 * static mb_queue_mpsc_t request_queue;
 * static mb_port_mutex_t queue_mutex;
 *
 * void init() {
 *     mb_queue_mpsc_init(&request_queue, request_slots, 32);
 *     mb_port_mutex_init(&queue_mutex);
 * }
 *
 * void client_thread_a() {
 *     mb_request_t *req = build_request();
 *     mb_queue_mpsc_enqueue(&request_queue, &queue_mutex, req);
 * }
 *
 * void dispatcher_thread() {
 *     mb_request_t *req;
 *     while (mb_queue_mpsc_dequeue(&request_queue, (void**)&req)) {
 *         dispatch(req);
 *     }
 * }
 * @endcode
 *
 * @note For SPSC to be truly lock-free, compile with C11 atomics support.
 *       Fallback uses mutex for environments without stdatomic.h.
 */

#ifndef MODBUS_MB_QUEUE_H
#define MODBUS_MB_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) && !defined(__STDC_NO_ATOMICS__)
#  include <stdatomic.h>
#  define MB_QUEUE_HAS_ATOMICS 1
   typedef _Atomic(size_t) mb_atomic_size_t;
#else
#  define MB_QUEUE_HAS_ATOMICS 0
   typedef size_t mb_atomic_size_t;
#endif

#include <modbus/mb_err.h>
#include <modbus/port/mutex.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/* SPSC Queue - Lock-free Single-Producer/Single-Consumer                    */
/* ========================================================================== */

/**
 * @brief Lock-free SPSC queue for ISR-to-thread communication.
 *
 * Uses C11 atomics when available for true lock-free operation. Falls back
 * to mutex-based implementation on platforms without atomic support.
 *
 * @note Capacity must be a power of 2 for efficient masking.
 */
typedef struct mb_queue_spsc {
    void             **slots;      /**< External storage for element pointers */
    size_t             capacity;   /**< Total slots (must be power of 2) */
    size_t             mask;       /**< Cached (capacity - 1) for wrapping */
    
    mb_atomic_size_t   head;       /**< Consumer index (atomic for lock-free) */
    mb_atomic_size_t   tail;       /**< Producer index (atomic for lock-free) */
    
    size_t             high_water; /**< Peak occupancy for diagnostics */
    
#if !MB_QUEUE_HAS_ATOMICS
    mb_port_mutex_t    mutex;      /**< Fallback mutex when atomics unavailable */
#endif
} mb_queue_spsc_t;

/**
 * @brief Initializes an SPSC queue with external storage.
 *
 * @param[out] queue    Queue structure to initialize
 * @param[in]  slots    Storage for element pointers (must persist)
 * @param[in]  capacity Number of slots (must be power of 2)
 *
 * @retval MB_OK                     Queue initialized successfully
 * @retval MB_ERR_INVALID_ARGUMENT   NULL pointer or capacity not power-of-2
 */
mb_err_t mb_queue_spsc_init(mb_queue_spsc_t *queue, void **slots, size_t capacity);

/**
 * @brief Deinitializes SPSC queue and releases resources.
 *
 * Only needed when using mutex fallback. No-op with true atomics.
 *
 * @param[in] queue Queue to deinitialize
 */
void mb_queue_spsc_deinit(mb_queue_spsc_t *queue);

/**
 * @brief Enqueues an element (producer side).
 *
 * @param[in] queue Queue instance
 * @param[in] elem  Element pointer to enqueue (must not be NULL)
 *
 * @retval true   Element enqueued successfully
 * @retval false  Queue is full (backpressure required)
 *
 * @note ISR-safe when compiled with C11 atomics. Use from producer only.
 */
bool mb_queue_spsc_enqueue(mb_queue_spsc_t *queue, void *elem);

/**
 * @brief Dequeues an element (consumer side).
 *
 * @param[in]  queue    Queue instance
 * @param[out] out_elem Pointer to store dequeued element
 *
 * @retval true   Element dequeued successfully
 * @retval false  Queue is empty
 *
 * @note Use from consumer only. Non-blocking.
 */
bool mb_queue_spsc_dequeue(mb_queue_spsc_t *queue, void **out_elem);

/**
 * @brief Returns current queue occupancy.
 *
 * @param[in] queue Queue instance
 * @return Number of elements currently in queue
 *
 * @note Result is approximate in concurrent scenarios due to race conditions.
 */
size_t mb_queue_spsc_size(const mb_queue_spsc_t *queue);

/**
 * @brief Returns queue capacity.
 *
 * @param[in] queue Queue instance
 * @return Total number of slots
 */
size_t mb_queue_spsc_capacity(const mb_queue_spsc_t *queue);

/**
 * @brief Returns peak occupancy since initialization.
 *
 * @param[in] queue Queue instance
 * @return Highest number of elements ever present simultaneously
 */
size_t mb_queue_spsc_high_water(const mb_queue_spsc_t *queue);

/**
 * @brief Checks if queue is empty.
 *
 * @param[in] queue Queue instance
 * @retval true  Queue has no elements
 * @retval false Queue has at least one element
 */
bool mb_queue_spsc_is_empty(const mb_queue_spsc_t *queue);

/**
 * @brief Checks if queue is full.
 *
 * @param[in] queue Queue instance
 * @retval true  Queue cannot accept more elements
 * @retval false Queue has available space
 */
bool mb_queue_spsc_is_full(const mb_queue_spsc_t *queue);

/* ========================================================================== */
/* MPSC Queue - Multi-Producer/Single-Consumer with Short Critical Sections  */
/* ========================================================================== */

/**
 * @brief MPSC queue for multiple producers with single consumer.
 *
 * Uses a mutex to protect enqueue operations. Dequeue is lock-free from
 * consumer side. Optimized for scenarios where multiple threads need to
 * submit work to a single dispatcher.
 *
 * @note Capacity must be a power of 2.
 */
typedef struct mb_queue_mpsc {
    void             **slots;      /**< External storage for element pointers */
    size_t             capacity;   /**< Total slots (must be power of 2) */
    size_t             mask;       /**< Cached (capacity - 1) for wrapping */
    
    mb_atomic_size_t   head;       /**< Consumer index (atomic, no lock needed) */
    size_t             tail;       /**< Producer index (protected by mutex) */
    
    size_t             high_water; /**< Peak occupancy for diagnostics */
    
    mb_port_mutex_t    mutex;      /**< Protects enqueue operations only */
} mb_queue_mpsc_t;

/**
 * @brief Initializes an MPSC queue with external storage.
 *
 * @param[out] queue    Queue structure to initialize
 * @param[in]  slots    Storage for element pointers (must persist)
 * @param[in]  capacity Number of slots (must be power of 2)
 *
 * @retval MB_OK                     Queue initialized successfully
 * @retval MB_ERR_INVALID_ARGUMENT   NULL pointer or capacity not power-of-2
 */
mb_err_t mb_queue_mpsc_init(mb_queue_mpsc_t *queue, void **slots, size_t capacity);

/**
 * @brief Deinitializes MPSC queue and releases mutex.
 *
 * @param[in] queue Queue to deinitialize
 */
void mb_queue_mpsc_deinit(mb_queue_mpsc_t *queue);

/**
 * @brief Enqueues an element from any producer thread.
 *
 * @param[in] queue Queue instance
 * @param[in] elem  Element pointer to enqueue (must not be NULL)
 *
 * @retval true   Element enqueued successfully
 * @retval false  Queue is full
 *
 * @note Thread-safe for multiple producers. Short critical section (~10 cycles).
 */
bool mb_queue_mpsc_enqueue(mb_queue_mpsc_t *queue, void *elem);

/**
 * @brief Dequeues an element (consumer side).
 *
 * @param[in]  queue    Queue instance
 * @param[out] out_elem Pointer to store dequeued element
 *
 * @retval true   Element dequeued successfully
 * @retval false  Queue is empty
 *
 * @note Lock-free from consumer side. Use from single consumer only.
 */
bool mb_queue_mpsc_dequeue(mb_queue_mpsc_t *queue, void **out_elem);

/**
 * @brief Returns current queue occupancy.
 *
 * @param[in] queue Queue instance
 * @return Number of elements currently in queue (approximate)
 */
size_t mb_queue_mpsc_size(const mb_queue_mpsc_t *queue);

/**
 * @brief Returns queue capacity.
 *
 * @param[in] queue Queue instance
 * @return Total number of slots
 */
size_t mb_queue_mpsc_capacity(const mb_queue_mpsc_t *queue);

/**
 * @brief Returns peak occupancy since initialization.
 *
 * @param[in] queue Queue instance
 * @return Highest number of elements ever present simultaneously
 */
size_t mb_queue_mpsc_high_water(const mb_queue_mpsc_t *queue);

/**
 * @brief Checks if queue is empty.
 *
 * @param[in] queue Queue instance
 * @retval true  Queue has no elements
 * @retval false Queue has at least one element
 */
bool mb_queue_mpsc_is_empty(const mb_queue_mpsc_t *queue);

/**
 * @brief Checks if queue is full.
 *
 * @param[in] queue Queue instance
 * @retval true  Queue cannot accept more elements
 * @retval false Queue has available space
 */
bool mb_queue_mpsc_is_full(const mb_queue_mpsc_t *queue);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MB_QUEUE_H */
