/**
 * @file mempool.c
 */

#include <modbus/internal/mempool.h>

#include <modbus/mb_types.h>

static void mempool_build_free_list(mb_mempool_t *pool)
{
    pool->free_list = NULL;
    pool->free_count = 0U;

    for (mb_size_t i = 0U; i < pool->block_count; ++i) {
        mb_u8 *block = pool->storage + (i * pool->block_size);
        *(void **)block = pool->free_list;
        pool->free_list = block;
        pool->free_count += 1U;
    }
}

static bool mempool_pointer_belongs(const mb_mempool_t *pool, const void *block)
{
    if (pool == NULL || block == NULL) {
        return false;
    }

    const mb_u8 *byte_ptr = (const mb_u8 *)block;
    const mb_u8 *begin = pool->storage;
    const mb_u8 *end = begin + (pool->block_count * pool->block_size);

    if (byte_ptr < begin || byte_ptr >= end) {
        return false;
    }

    mb_size_t offset = (mb_size_t)(byte_ptr - begin);
    return (offset % pool->block_size) == 0U;
}

mb_err_t mb_mempool_init(mb_mempool_t *pool,
                         void *buffer,
                         mb_size_t block_size,
                         mb_size_t block_count)
{
    if (pool == NULL || buffer == NULL || block_size == 0U || block_count == 0U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (block_size < sizeof(void *)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    pool->storage = (mb_u8 *)buffer;
    pool->block_size = block_size;
    pool->block_count = block_count;

    mempool_build_free_list(pool);
    return MODBUS_ERROR_NONE;
}

void mb_mempool_reset(mb_mempool_t *pool)
{
    if (pool == NULL || pool->storage == NULL) {
        return;
    }

    mempool_build_free_list(pool);
}

void *mb_mempool_acquire(mb_mempool_t *pool)
{
    if (pool == NULL || pool->free_list == NULL) {
        return NULL;
    }

    void *block = pool->free_list;
    pool->free_list = *(void **)pool->free_list;
    pool->free_count -= 1U;
    return block;
}

mb_err_t mb_mempool_release(mb_mempool_t *pool, void *block)
{
    if (pool == NULL || block == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!mempool_pointer_belongs(pool, block)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pool->free_count >= pool->block_count) {
        return MODBUS_ERROR_OTHER;
    }

    *(void **)block = pool->free_list;
    pool->free_list = block;
    pool->free_count += 1U;
    return MODBUS_ERROR_NONE;
}

mb_size_t mb_mempool_capacity(const mb_mempool_t *pool)
{
    if (pool == NULL) {
        return 0U;
    }
    return pool->block_count;
}

mb_size_t mb_mempool_free_count(const mb_mempool_t *pool)
{
    if (pool == NULL) {
        return 0U;
    }
    return pool->free_count;
}

bool mb_mempool_contains(const mb_mempool_t *pool, const void *block)
{
    return mempool_pointer_belongs(pool, block);
}
