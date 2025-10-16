/**
 * @file mempool.h
 * @brief Fixed-size block memory pool (no malloc required).
 */

#ifndef MODBUS_MEMPOOL_H
#define MODBUS_MEMPOOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <modbus/mb_err.h>
#include <modbus/mb_types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mb_mempool {
    mb_u8     *storage;
    mb_size_t  block_size;
    mb_size_t  block_count;
    mb_size_t  free_count;
    void      *free_list;
} mb_mempool_t;

mb_err_t mb_mempool_init(mb_mempool_t *pool,
                         void *buffer,
                         mb_size_t block_size,
                         mb_size_t block_count);

void mb_mempool_reset(mb_mempool_t *pool);

void *mb_mempool_acquire(mb_mempool_t *pool);
mb_err_t mb_mempool_release(mb_mempool_t *pool, void *block);

mb_size_t mb_mempool_capacity(const mb_mempool_t *pool);
mb_size_t mb_mempool_free_count(const mb_mempool_t *pool);

bool mb_mempool_contains(const mb_mempool_t *pool, const void *block);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MEMPOOL_H */
