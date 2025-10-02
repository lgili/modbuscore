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
    rb->count = 0U;

    return MODBUS_ERROR_NONE;
}

void mb_ringbuf_reset(mb_ringbuf_t *rb)
{
    if (rb == NULL) {
        return;
    }

    rb->head = 0U;
    rb->tail = 0U;
    rb->count = 0U;
}

size_t mb_ringbuf_capacity(const mb_ringbuf_t *rb)
{
    return (rb == NULL) ? 0U : rb->capacity;
}

size_t mb_ringbuf_size(const mb_ringbuf_t *rb)
{
    return (rb == NULL) ? 0U : rb->count;
}

size_t mb_ringbuf_free(const mb_ringbuf_t *rb)
{
    return (rb == NULL) ? 0U : (rb->capacity - rb->count);
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

    return rb->count == rb->capacity;
}

size_t mb_ringbuf_write(mb_ringbuf_t *rb, const uint8_t *data, size_t len)
{
    if (rb == NULL || data == NULL || len == 0U) {
        return 0U;
    }

    const size_t available = rb->capacity - rb->count;
    const size_t to_write = (len < available) ? len : available;

    for (size_t i = 0U; i < to_write; ++i) {
        rb->buffer[rb->tail] = data[i];
        rb->tail = (rb->tail + 1U) & rb->mask;
    }

    rb->count += to_write;
    return to_write;
}

size_t mb_ringbuf_read(mb_ringbuf_t *rb, uint8_t *out, size_t len)
{
    if (rb == NULL || out == NULL || len == 0U) {
        return 0U;
    }

    const size_t to_read = (len < rb->count) ? len : rb->count;

    for (size_t i = 0U; i < to_read; ++i) {
        out[i] = rb->buffer[rb->head];
        rb->head = (rb->head + 1U) & rb->mask;
    }

    rb->count -= to_read;
    return to_read;
}

bool mb_ringbuf_push(mb_ringbuf_t *rb, uint8_t byte)
{
    if (rb == NULL || rb->count == rb->capacity) {
        return false;
    }

    rb->buffer[rb->tail] = byte;
    rb->tail = (rb->tail + 1U) & rb->mask;
    rb->count += 1U;
    return true;
}

bool mb_ringbuf_pop(mb_ringbuf_t *rb, uint8_t *out_byte)
{
    if (rb == NULL || out_byte == NULL || rb->count == 0U) {
        return false;
    }

    *out_byte = rb->buffer[rb->head];
    rb->head = (rb->head + 1U) & rb->mask;
    rb->count -= 1U;
    return true;
}
