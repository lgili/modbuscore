/**
 * @file mb_queue.c
 * @brief Implementation of lock-free SPSC and MPSC queues.
 */

#include <modbus/mb_queue.h>
#include <modbus/mb_types.h>
#include <string.h>

/* ========================================================================== */
/* SPSC Queue Implementation                                                  */
/* ========================================================================== */

mb_err_t mb_queue_spsc_init(mb_queue_spsc_t *queue, void **slots, size_t capacity) {
    if (!queue || !slots || capacity == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    if (!MB_IS_POWER_OF_TWO(capacity)) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    memset(queue, 0, sizeof(*queue));
    queue->slots = slots;
    queue->capacity = capacity;
    queue->mask = capacity - 1;
    
#if MB_QUEUE_HAS_ATOMICS
    atomic_init(&queue->head, 0);
    atomic_init(&queue->tail, 0);
#else
    queue->head = 0;
    queue->tail = 0;
    mb_err_t err = mb_port_mutex_init(&queue->mutex);
    if (err != MB_OK) {
        return err;
    }
#endif
    
    return MB_OK;
}

void mb_queue_spsc_deinit(mb_queue_spsc_t *queue) {
    if (!queue) {
        return;
    }
    
#if !MB_QUEUE_HAS_ATOMICS
    mb_port_mutex_deinit(&queue->mutex);
#endif
}

bool mb_queue_spsc_enqueue(mb_queue_spsc_t *queue, void *elem) {
    if (!queue || !elem) {
        return false;
    }
    
#if MB_QUEUE_HAS_ATOMICS
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    size_t head = atomic_load_explicit(&queue->head, memory_order_acquire);
    
    size_t next = (tail + 1) & queue->mask;
    if (next == (head & queue->mask)) {
        return false; // Queue full
    }
    
    queue->slots[tail & queue->mask] = elem;
    atomic_store_explicit(&queue->tail, tail + 1, memory_order_release);
    
    // Update high water mark
    size_t size = ((tail + 1) - head) & queue->mask;
    if (size > queue->high_water) {
        queue->high_water = size;
    }
#else
    if (mb_port_mutex_lock(&queue->mutex) != MB_OK) {
        return false;
    }
    
    size_t next = (queue->tail + 1) & queue->mask;
    if (next == (queue->head & queue->mask)) {
        mb_port_mutex_unlock(&queue->mutex);
        return false;
    }
    
    queue->slots[queue->tail & queue->mask] = elem;
    queue->tail++;
    
    size_t size = (queue->tail - queue->head) & queue->mask;
    if (size > queue->high_water) {
        queue->high_water = size;
    }
    
    mb_port_mutex_unlock(&queue->mutex);
#endif
    
    return true;
}

bool mb_queue_spsc_dequeue(mb_queue_spsc_t *queue, void **out_elem) {
    if (!queue || !out_elem) {
        return false;
    }
    
#if MB_QUEUE_HAS_ATOMICS
    size_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
    
    if (head == tail) {
        return false; // Queue empty
    }
    
    *out_elem = queue->slots[head & queue->mask];
    atomic_store_explicit(&queue->head, head + 1, memory_order_release);
#else
    if (mb_port_mutex_lock(&queue->mutex) != MB_OK) {
        return false;
    }
    
    if (queue->head == queue->tail) {
        mb_port_mutex_unlock(&queue->mutex);
        return false;
    }
    
    *out_elem = queue->slots[queue->head & queue->mask];
    queue->head++;
    
    mb_port_mutex_unlock(&queue->mutex);
#endif
    
    return true;
}

size_t mb_queue_spsc_size(const mb_queue_spsc_t *queue) {
    if (!queue) {
        return 0;
    }
    
#if MB_QUEUE_HAS_ATOMICS
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
    size_t head = atomic_load_explicit(&queue->head, memory_order_acquire);
    return (tail - head) & queue->mask;
#else
    return (queue->tail - queue->head) & queue->mask;
#endif
}

size_t mb_queue_spsc_capacity(const mb_queue_spsc_t *queue) {
    return queue ? queue->capacity : 0;
}

size_t mb_queue_spsc_high_water(const mb_queue_spsc_t *queue) {
    return queue ? queue->high_water : 0;
}

bool mb_queue_spsc_is_empty(const mb_queue_spsc_t *queue) {
    return mb_queue_spsc_size(queue) == 0;
}

bool mb_queue_spsc_is_full(const mb_queue_spsc_t *queue) {
    if (!queue) {
        return true;
    }
    
#if MB_QUEUE_HAS_ATOMICS
    size_t tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
    size_t head = atomic_load_explicit(&queue->head, memory_order_acquire);
    size_t next = (tail + 1) & queue->mask;
    return next == (head & queue->mask);
#else
    size_t next = (queue->tail + 1) & queue->mask;
    return next == (queue->head & queue->mask);
#endif
}

/* ========================================================================== */
/* MPSC Queue Implementation                                                  */
/* ========================================================================== */

mb_err_t mb_queue_mpsc_init(mb_queue_mpsc_t *queue, void **slots, size_t capacity) {
    if (!queue || !slots || capacity == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    if (!MB_IS_POWER_OF_TWO(capacity)) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    memset(queue, 0, sizeof(*queue));
    queue->slots = slots;
    queue->capacity = capacity;
    queue->mask = capacity - 1;
    
#if MB_QUEUE_HAS_ATOMICS
    atomic_init(&queue->head, 0);
#else
    queue->head = 0;
#endif
    queue->tail = 0;
    
    return mb_port_mutex_init(&queue->mutex);
}

void mb_queue_mpsc_deinit(mb_queue_mpsc_t *queue) {
    if (!queue) {
        return;
    }
    
    mb_port_mutex_deinit(&queue->mutex);
}

bool mb_queue_mpsc_enqueue(mb_queue_mpsc_t *queue, void *elem) {
    if (!queue || !elem) {
        return false;
    }
    
    if (mb_port_mutex_lock(&queue->mutex) != MB_OK) {
        return false;
    }
    
#if MB_QUEUE_HAS_ATOMICS
    size_t head = atomic_load_explicit(&queue->head, memory_order_acquire);
#else
    size_t head = queue->head;
#endif
    
    size_t next = (queue->tail + 1) & queue->mask;
    if (next == (head & queue->mask)) {
        mb_port_mutex_unlock(&queue->mutex);
        return false; // Queue full
    }
    
    queue->slots[queue->tail & queue->mask] = elem;
    queue->tail++;
    
    // Update high water mark
    size_t size = (queue->tail - head) & queue->mask;
    if (size > queue->high_water) {
        queue->high_water = size;
    }
    
    mb_port_mutex_unlock(&queue->mutex);
    return true;
}

bool mb_queue_mpsc_dequeue(mb_queue_mpsc_t *queue, void **out_elem) {
    if (!queue || !out_elem) {
        return false;
    }
    
#if MB_QUEUE_HAS_ATOMICS
    size_t head = atomic_load_explicit(&queue->head, memory_order_relaxed);
#else
    size_t head = queue->head;
#endif
    
    // No lock needed for dequeue - single consumer only
    size_t tail = queue->tail; // Safe to read without lock in SC case
    
    if (head == tail) {
        return false; // Queue empty
    }
    
    *out_elem = queue->slots[head & queue->mask];
    
#if MB_QUEUE_HAS_ATOMICS
    atomic_store_explicit(&queue->head, head + 1, memory_order_release);
#else
    queue->head = head + 1;
#endif
    
    return true;
}

size_t mb_queue_mpsc_size(const mb_queue_mpsc_t *queue) {
    if (!queue) {
        return 0;
    }
    
#if MB_QUEUE_HAS_ATOMICS
    size_t head = atomic_load_explicit(&queue->head, memory_order_acquire);
#else
    size_t head = queue->head;
#endif
    size_t tail = queue->tail;
    
    return (tail - head) & queue->mask;
}

size_t mb_queue_mpsc_capacity(const mb_queue_mpsc_t *queue) {
    return queue ? queue->capacity : 0;
}

size_t mb_queue_mpsc_high_water(const mb_queue_mpsc_t *queue) {
    return queue ? queue->high_water : 0;
}

bool mb_queue_mpsc_is_empty(const mb_queue_mpsc_t *queue) {
    return mb_queue_mpsc_size(queue) == 0;
}

bool mb_queue_mpsc_is_full(const mb_queue_mpsc_t *queue) {
    if (!queue) {
        return true;
    }
    
#if MB_QUEUE_HAS_ATOMICS
    size_t head = atomic_load_explicit(&queue->head, memory_order_acquire);
#else
    size_t head = queue->head;
#endif
    
    size_t next = (queue->tail + 1) & queue->mask;
    return next == (head & queue->mask);
}
