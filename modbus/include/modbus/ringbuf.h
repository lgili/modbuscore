/**
 * @file ringbuf.h
 * @brief Single-producer / single-consumer ring buffer utilities.
 *
 * The ring buffer offers a lock-free queue for moving bytes between a
 * producer and a consumer running in different scheduling contexts (for
 * example ISR ↔ main loop).  The implementation keeps monotonically
 * increasing write/read cursors so wrap-around is handled via masking.
 *
 * The API is intentionally small: initialise the buffer with external
 * storage, push or pop bytes (optionally in bulk), and query the
 * occupancy.  The caller is responsible for providing any required
 * memory barriers when used across execution contexts.
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
 * The structure keeps monotonically increasing cursors.  The @p mask is
 * derived from @p capacity and is used to wrap the physical index into the
 * provided storage.
 */
typedef struct mb_ringbuf {
    uint8_t *buffer;      /**< Backing storage provided by the caller. */
    size_t   capacity;    /**< Total number of bytes that fit in @ref buffer. */
    size_t   mask;        /**< Cached (capacity - 1) for fast wrap-around. */
    size_t   head;        /**< Read cursor (monotonic). */
    size_t   tail;        /**< Write cursor (monotonic). */
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
