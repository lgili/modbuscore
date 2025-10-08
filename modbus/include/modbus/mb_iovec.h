/**
 * @file mb_iovec.h
 * @brief Zero-copy scatter-gather IO primitives for Modbus.
 *
 * This module provides iovec-style descriptors for efficient, zero-copy
 * data transfer between transport layers and the PDU codec. Instead of
 * copying data multiple times, we work with views (windows) into ring
 * buffers or DMA regions.
 *
 * Key benefits:
 * - Reduced RAM usage (no intermediate buffers)
 * - Fewer memcpy operations in hot paths
 * - Direct encoder/decoder access to ring buffer regions
 *
 * @note All structures are designed for embedded use with minimal overhead.
 */

#ifndef MODBUS_MB_IOVEC_H
#define MODBUS_MB_IOVEC_H

#include <modbus/conf.h>
#include <modbus/mb_types.h>
#include <modbus/mb_err.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IO vector descriptor for scatter-gather operations.
 *
 * Represents a contiguous memory region (view) that can be chained
 * with other iovecs for zero-copy operations.
 */
typedef struct {
    const mb_u8 *base;  /**< Base pointer to the memory region (may be NULL if len=0) */
    mb_size_t len;      /**< Length of the region in bytes */
} mb_iovec_t;

/**
 * @brief Collection of IO vectors for scatter-gather operations.
 *
 * Used to describe fragmented data (e.g., ring buffer wrap-around)
 * without requiring a contiguous copy.
 */
typedef struct {
    mb_iovec_t *vectors;     /**< Array of IO vectors */
    mb_size_t count;         /**< Number of vectors in the array */
    mb_size_t total_len;     /**< Total bytes across all vectors */
} mb_iovec_list_t;

/**
 * @brief Initialize an IO vector with base pointer and length.
 *
 * @param iov Pointer to the iovec to initialize
 * @param base Base pointer to the memory region
 * @param len Length of the region in bytes
 */
static inline void mb_iovec_init(mb_iovec_t *iov, const mb_u8 *base, mb_size_t len)
{
    if (iov != NULL) {
        iov->base = base;
        iov->len = len;
    }
}

/**
 * @brief Initialize an empty IO vector list.
 *
 * @param list Pointer to the iovec list to initialize
 * @param vectors Array of iovecs to use
 * @param capacity Maximum number of vectors in the array
 */
static inline void mb_iovec_list_init(mb_iovec_list_t *list, mb_iovec_t *vectors, mb_size_t capacity)
{
    if (list != NULL) {
        list->vectors = vectors;
        list->count = 0;
        list->total_len = 0;
        (void)capacity; // Reserved for future bounds checking
    }
}

/**
 * @brief Add an IO vector to a list.
 *
 * @param list Pointer to the iovec list
 * @param base Base pointer to the memory region
 * @param len Length of the region in bytes
 * @return MB_OK on success, error code otherwise
 */
static inline mb_err_t mb_iovec_list_add(mb_iovec_list_t *list, const mb_u8 *base, mb_size_t len)
{
    if (list == NULL || list->vectors == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    
    if (len == 0) {
        return MB_OK; // Skip empty regions
    }
    
    // Note: No capacity check for now (assumed pre-allocated correctly)
    mb_iovec_t *vec = &list->vectors[list->count];
    vec->base = base;
    vec->len = len;
    list->count++;
    list->total_len += len;
    
    return MB_OK;
}

/**
 * @brief Calculate total bytes across all vectors in a list.
 *
 * @param list Pointer to the iovec list
 * @return Total number of bytes
 */
static inline mb_size_t mb_iovec_list_total(const mb_iovec_list_t *list)
{
    return (list != NULL) ? list->total_len : 0;
}

/**
 * @brief Copy data from an iovec list to a contiguous buffer.
 *
 * This is a fallback for cases where scatter-gather is not supported
 * by the underlying transport. Should be avoided in performance-critical paths.
 *
 * @param list Source iovec list
 * @param dst Destination buffer
 * @param dst_size Size of destination buffer
 * @return Number of bytes copied, or 0 on error
 */
mb_size_t mb_iovec_list_copyout(const mb_iovec_list_t *list, mb_u8 *dst, mb_size_t dst_size);

/**
 * @brief Copy data from a contiguous buffer into an iovec list.
 *
 * This is used when receiving data into a fragmented ring buffer.
 *
 * @param list Destination iovec list
 * @param src Source buffer
 * @param src_len Length of source data
 * @return Number of bytes copied, or 0 on error
 */
mb_size_t mb_iovec_list_copyin(const mb_iovec_list_t *list, const mb_u8 *src, mb_size_t src_len);

/**
 * @brief Create an iovec view from a ring buffer region.
 *
 * Handles ring buffer wrap-around by creating 1 or 2 iovecs as needed.
 *
 * @param list Output iovec list (must have space for 2 vectors)
 * @param ring_base Ring buffer base pointer
 * @param ring_capacity Ring buffer total capacity
 * @param start Start offset in the ring buffer
 * @param len Number of bytes to view
 * @return MB_OK on success, error code otherwise
 */
mb_err_t mb_iovec_from_ring(mb_iovec_list_t *list,
                             const mb_u8 *ring_base,
                             mb_size_t ring_capacity,
                             mb_size_t start,
                             mb_size_t len);

/**
 * @brief Statistics for zero-copy tracking.
 *
 * Used to verify that zero-copy optimizations are working correctly.
 */
typedef struct {
    mb_u32 tx_zero_copy;    /**< Number of zero-copy TX operations */
    mb_u32 tx_memcpy;       /**< Number of TX operations requiring memcpy */
    mb_u32 rx_zero_copy;    /**< Number of zero-copy RX operations */
    mb_u32 rx_memcpy;       /**< Number of RX operations requiring memcpy */
    mb_u32 scratch_bytes;   /**< Peak scratch memory used per transaction */
} mb_iovec_stats_t;

/**
 * @brief Global zero-copy statistics (optional, for testing/debugging).
 *
 * Can be disabled by setting MB_CONF_ENABLE_IOVEC_STATS to 0.
 */
#if MB_CONF_ENABLE_IOVEC_STATS
extern mb_iovec_stats_t g_mb_iovec_stats;
#define MB_IOVEC_STATS_INC(field) (g_mb_iovec_stats.field++)
#define MB_IOVEC_STATS_MAX(field, val) \
    do { \
        if ((val) > g_mb_iovec_stats.field) { \
            g_mb_iovec_stats.field = (val); \
        } \
    } while (0)
#else
#define MB_IOVEC_STATS_INC(field) ((void)0)
#define MB_IOVEC_STATS_MAX(field, val) ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif // MODBUS_MB_IOVEC_H
