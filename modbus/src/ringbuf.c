/**
 * @file ringbuf.c
 */

#include <modbus/ringbuf.h>

#include <stddef.h>

static bool is_power_of_two(size_t value)
{
    return value != 0U && (value & (value - 1U)) == 0U;
}

mb_err_t mb_ringbuf_init(mb_ringbuf_t *rb, uint8_t *storage, size_t capacity)
{
    if (rb == NULL || storage == NULL || capacity == 0U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!is_power_of_two(capacity)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    rb->buffer = storage;
    rb->capacity = capacity;
    rb->mask = capacity - 1U;
    rb->head = 0U;
    rb->tail = 0U;

    return MODBUS_ERROR_NONE;
}

void mb_ringbuf_reset(mb_ringbuf_t *rb)
{
    if (rb == NULL) {
        return;
    }

    rb->head = 0U;
    rb->tail = 0U;
}

size_t mb_ringbuf_capacity(const mb_ringbuf_t *rb)
{
    if (rb == NULL) {
        return 0U;
    }

    return rb->capacity;
}

size_t mb_ringbuf_size(const mb_ringbuf_t *rb)
{
    if (rb == NULL) {
        return 0U;
    }

    return rb->tail - rb->head;
}

size_t mb_ringbuf_free(const mb_ringbuf_t *rb)
{
    if (rb == NULL) {
        return 0U;
    }

    return rb->capacity - mb_ringbuf_size(rb);
}

bool mb_ringbuf_is_empty(const mb_ringbuf_t *rb)
{
    return mb_ringbuf_size(rb) == 0U;
}

bool mb_ringbuf_is_full(const mb_ringbuf_t *rb)
{
    if (rb == NULL) {
        return false;
    }

    return mb_ringbuf_size(rb) == rb->capacity;
}

size_t mb_ringbuf_write(mb_ringbuf_t *rb, const uint8_t *data, size_t len)
{
    if (rb == NULL || data == NULL || len == 0U) {
        return 0U;
    }

    size_t available = mb_ringbuf_free(rb);
    size_t to_write = (len < available) ? len : available;

    for (size_t i = 0U; i < to_write; ++i) {
        size_t idx = (rb->tail + i) & rb->mask;
        rb->buffer[idx] = data[i];
    }

    rb->tail += to_write;
    return to_write;
}

size_t mb_ringbuf_read(mb_ringbuf_t *rb, uint8_t *out, size_t len)
{
    if (rb == NULL || out == NULL || len == 0U) {
        return 0U;
    }

    size_t available = mb_ringbuf_size(rb);
    size_t to_read = (len < available) ? len : available;

    for (size_t i = 0U; i < to_read; ++i) {
        size_t idx = (rb->head + i) & rb->mask;
        out[i] = rb->buffer[idx];
    }

    rb->head += to_read;
    return to_read;
}

bool mb_ringbuf_push(mb_ringbuf_t *rb, uint8_t byte)
{
    if (rb == NULL) {
        return false;
    }

    if (mb_ringbuf_is_full(rb)) {
        return false;
    }

    rb->buffer[rb->tail & rb->mask] = byte;
    rb->tail += 1U;
    return true;
}

bool mb_ringbuf_pop(mb_ringbuf_t *rb, uint8_t *out_byte)
{
    if (rb == NULL || out_byte == NULL) {
        return false;
    }

    if (mb_ringbuf_is_empty(rb)) {
        return false;
    }

    *out_byte = rb->buffer[rb->head & rb->mask];
    rb->head += 1U;
    return true;
}
