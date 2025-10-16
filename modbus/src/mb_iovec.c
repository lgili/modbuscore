/**
 * @file mb_iovec.c
 * @brief Implementation of zero-copy scatter-gather IO primitives.
 */

#include <modbus/internal/mb_iovec.h>
#include <modbus/mb_err.h>
#include <string.h>
#include <stdint.h>

#if MB_CONF_ENABLE_IOVEC_STATS
mb_iovec_stats_t g_mb_iovec_stats = {0};
#endif

mb_size_t mb_iovec_list_copyout(const mb_iovec_list_t *list, mb_u8 *dst, mb_size_t dst_size)
{
    if (list == NULL || dst == NULL || dst_size == 0) {
        return 0;
    }
    
    MB_IOVEC_STATS_INC(tx_memcpy);
    
    mb_size_t copied = 0;
    for (mb_size_t i = 0; i < list->count && copied < dst_size; i++) {
        const mb_iovec_t *vec = &list->vectors[i];
        if (vec->base == NULL || vec->len == 0) {
            continue;
        }
        
        mb_size_t to_copy = vec->len;
        if (copied + to_copy > dst_size) {
            to_copy = dst_size - copied;
        }
        
        memcpy(dst + copied, vec->base, to_copy);
        copied += to_copy;
    }
    
    return copied;
}

mb_size_t mb_iovec_list_copyin(const mb_iovec_list_t *list, const mb_u8 *src, mb_size_t src_len)
{
    if (list == NULL || src == NULL || src_len == 0) {
        return 0;
    }
    
    MB_IOVEC_STATS_INC(rx_memcpy);
    
    mb_size_t copied = 0;
    for (mb_size_t i = 0; i < list->count && copied < src_len; i++) {
        const mb_iovec_t *vec = &list->vectors[i];
        if (vec->base == NULL || vec->len == 0) {
            continue;
        }
        
        mb_size_t to_copy = vec->len;
        if (copied + to_copy > src_len) {
            to_copy = src_len - copied;
        }
        
        // Note: Casting away const here is safe - this function is explicitly
        // for copying INTO the iovec regions (destination), so base must be writable.
        // The const in mb_iovec_t is for read-only operations like copyout.
        mb_u8 *dest = (mb_u8*)(uintptr_t)vec->base;  // NOLINT(performance-no-int-to-ptr)
        memcpy(dest, src + copied, to_copy);
        copied += to_copy;
    }
    
    return copied;
}

mb_err_t mb_iovec_from_ring(mb_iovec_list_t *list,
                             const mb_u8 *ring_base,
                             mb_size_t ring_capacity,
                             mb_size_t start,
                             mb_size_t len)
{
    if (list == NULL || list->vectors == NULL || ring_base == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    if (len == 0 || ring_capacity == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    if (start >= ring_capacity) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    // Reset the list
    list->count = 0;
    list->total_len = 0;
    
    // Calculate end position
    mb_size_t end = start + len;
    
    if (end <= ring_capacity) {
        // No wrap-around: single contiguous region
        mb_iovec_list_add(list, ring_base + start, len);
        MB_IOVEC_STATS_INC(rx_zero_copy);
    } else {
        // Wrap-around: two regions
        mb_size_t first_len = ring_capacity - start;
        mb_size_t second_len = len - first_len;
        
        mb_iovec_list_add(list, ring_base + start, first_len);
        mb_iovec_list_add(list, ring_base, second_len);
        MB_IOVEC_STATS_INC(rx_zero_copy);
    }
    
    return MB_OK;
}
