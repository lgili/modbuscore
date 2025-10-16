/**
 * @file ringbuf.h
 * @brief Single-producer / single-consumer ring buffer utilities.
 *
 * The ring buffer offers a simple queue for moving bytes between a
 * producer and a consumer running in different scheduling contexts (for
 * example ISR ↔ main loop).  The implementation keeps monotonically
 * increasing write/read cursors so wrap-around is handled via masking.
 *
 * The API is intentionally small: initialise the buffer with external
 * storage, push or pop bytes (optionally in bulk), and query the
 * occupancy.
 *
 * @section Thread Safety
 *
 * **IMPORTANT:** This ring buffer is **NOT thread-safe** without external synchronization.
 * The internal fields (`head`, `tail`, `count`) are **NOT atomic** and concurrent access
 * will cause data races detectable by ThreadSanitizer.
 *
 * @subsection Safe Usage Patterns
 *
 * **Single-threaded:** Direct calls from one execution context:
 * @code
 * void main_loop() {
 *     mb_ringbuf_write(&rb, tx_data, len);
 *     mb_ringbuf_read(&rb, rx_buf, cap);
 * }
 * @endcode
 *
 * **Multi-threaded with mutex:** Wrap all ring buffer operations:
 * @code
 * pthread_mutex_t rb_mutex = PTHREAD_MUTEX_INITIALIZER;
 *
 * void producer_thread() {
 *     pthread_mutex_lock(&rb_mutex);
 *     mb_ringbuf_write(&rb, data, len);
 *     pthread_mutex_unlock(&rb_mutex);
 * }
 *
 * void consumer_thread() {
 *     pthread_mutex_lock(&rb_mutex);
 *     mb_ringbuf_read(&rb, buffer, cap);
 *     pthread_mutex_unlock(&rb_mutex);
 * }
 * @endcode
 *
 * **ISR-safe with deferred operations:** Use flags to defer ring buffer operations to main loop:
 * @code
 * volatile bool data_pending = false;
 * uint8_t isr_cache[64];
 * size_t isr_cache_len = 0;
 *
 * void uart_rx_isr() {
 *     // Cache data in ISR, don't touch ring buffer yet
 *     isr_cache_len = uart_dma_read(isr_cache, sizeof(isr_cache));
 *     data_pending = true;
 * }
 *
 * void main_loop() {
 *     if (data_pending) {
 *         data_pending = false;
 *         // Single-threaded write, safe!
 *         mb_ringbuf_write(&rb, isr_cache, isr_cache_len);
 *     }
 *     mb_ringbuf_read(&rb, buffer, cap);
 * }
 * @endcode
 *
 * @subsection Future Lock-Free Variant
 *
 * Gate 22 roadmap includes `mb_ringbuf_atomic_t` with C11 atomics for true lock-free SPSC.
 * The current implementation prioritizes embedded portability (no C11 requirement) and
 * low overhead in single-threaded scenarios.
 *
 * @note ThreadSanitizer (`-fsanitize=thread`) will report data races if you use this
 *       ring buffer concurrently without proper synchronization. This is expected behavior.
 */

#ifndef MODBUS_RINGBUF_H
#define MODBUS_RINGBUF_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <modbus/mb_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ring buffer descriptor.
 *
 * The structure keeps explicit cursors and the current occupancy.  The @p mask is
 * derived from @p capacity and is used to wrap the physical index into the provided
 * storage.
 */
typedef struct mb_ringbuf {
    uint8_t *buffer;      /**< Backing storage provided by the caller. */
    size_t   capacity;    /**< Total number of bytes that fit in @ref buffer. */
    size_t   mask;        /**< Cached (capacity - 1) for fast wrap-around. */
    size_t   head;        /**< Read cursor. */
    size_t   tail;        /**< Write cursor. */
    size_t   count;       /**< Number of bytes currently stored. */
} mb_ringbuf_t;

mb_err_t mb_ringbuf_init(mb_ringbuf_t *rb, uint8_t *storage, size_t capacity);
void     mb_ringbuf_reset(mb_ringbuf_t *rb);

size_t mb_ringbuf_capacity(const mb_ringbuf_t *rb);
size_t mb_ringbuf_size(const mb_ringbuf_t *rb);
size_t mb_ringbuf_free(const mb_ringbuf_t *rb);

bool mb_ringbuf_is_empty(const mb_ringbuf_t *rb);
bool mb_ringbuf_is_full(const mb_ringbuf_t *rb);

size_t mb_ringbuf_write(mb_ringbuf_t *rb, const uint8_t *data, size_t len);
size_t mb_ringbuf_read(mb_ringbuf_t *rb, uint8_t *out, size_t len);

bool mb_ringbuf_push(mb_ringbuf_t *rb, uint8_t byte);
bool mb_ringbuf_pop(mb_ringbuf_t *rb, uint8_t *out_byte);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_RINGBUF_H */
