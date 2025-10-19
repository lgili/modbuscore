/**
 * @file transport_if.h
 * @brief Minimal, non-blocking transport interface shared by client and server code.
 */

#ifndef MODBUS_TRANSPORT_IF_H
#define MODBUS_TRANSPORT_IF_H

#include <modbus/mb_err.h>
#include <modbus/mb_types.h>
#include <modbus/internal/mb_iovec.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Result metadata for transport I/O operations.
 */
typedef struct {
    mb_size_t processed; /**< Number of bytes sent/received in the operation. */
} mb_transport_io_result_t;

/**
 * @brief Function pointer prototype for non-blocking send operations.
 */
typedef mb_err_t (*mb_transport_send_fn)(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out);

/**
 * @brief Function pointer prototype for non-blocking receive operations.
 */
typedef mb_err_t (*mb_transport_recv_fn)(void *ctx, mb_u8 *buf, mb_size_t cap, mb_transport_io_result_t *out);

/**
 * @brief Function pointer prototype for scatter-gather send operations (zero-copy).
 * 
 * Optional optimization for transports that support vectored IO.
 * If NULL, falls back to regular send with memcpy.
 */
typedef mb_err_t (*mb_transport_sendv_fn)(void *ctx, const mb_iovec_list_t *list, mb_transport_io_result_t *out);

/**
 * @brief Function pointer prototype for scatter-gather receive operations (zero-copy).
 *
 * Optional optimization for transports that can receive directly into fragmented buffers.
 * If NULL, falls back to regular recv with memcpy.
 */
typedef mb_err_t (*mb_transport_recvv_fn)(void *ctx, mb_iovec_list_t *list, mb_transport_io_result_t *out);

/**
 * @brief Function pointer prototype returning a monotonic timestamp in milliseconds.
 */
typedef mb_time_ms_t (*mb_transport_now_fn)(void *ctx);

/**
 * @brief Optional cooperative-yield callback (may be `NULL`).
 */
typedef void (*mb_transport_yield_fn)(void *ctx);

/**
 * @brief Non-blocking transport interface description.
 */
typedef struct {
    void *ctx;                          /**< User-supplied context forwarded to callbacks. */
    mb_transport_send_fn send;          /**< Send callback (required). */
    mb_transport_recv_fn recv;          /**< Receive callback (required). */
    mb_transport_sendv_fn sendv;        /**< Scatter-gather send (optional, may be NULL). */
    mb_transport_recvv_fn recvv;        /**< Scatter-gather receive (optional, may be NULL). */
    mb_transport_now_fn now;            /**< Monotonic time source (required). */
    mb_transport_yield_fn yield;        /**< Optional cooperative-yield hook (may be `NULL`). */
} mb_transport_if_t;

/**
 * @brief Performs a guarded send using the provided transport interface.
 */
static inline mb_err_t mb_transport_send(const mb_transport_if_t *iface,
                                         const mb_u8 *buf,
                                         mb_size_t len,
                                         mb_transport_io_result_t *out)
{
    if (!iface || !iface->send || !buf) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    return iface->send(iface->ctx, buf, len, out);
}

/**
 * @brief Performs a guarded receive using the provided transport interface.
 */
static inline mb_err_t mb_transport_recv(const mb_transport_if_t *iface,
                                         mb_u8 *buf,
                                         mb_size_t cap,
                                         mb_transport_io_result_t *out)
{
    if (!iface || !iface->recv || !buf || cap == 0U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    return iface->recv(iface->ctx, buf, cap, out);
}

/**
 * @brief Fetches the current monotonic timestamp in milliseconds.
 */
static inline mb_time_ms_t mb_transport_now(const mb_transport_if_t *iface)
{
    if (!iface || !iface->now) {
        return 0U;
    }
    return iface->now(iface->ctx);
}

/**
 * @brief Yields cooperatively to the underlying platform when supported.
 */
static inline void mb_transport_yield(const mb_transport_if_t *iface)
{
    if (iface && iface->yield) {
        iface->yield(iface->ctx);
    }
}

/**
 * @brief Computes the elapsed time in milliseconds since @p since.
 *
 * Returns zero when @p iface is NULL or the clock rolled backwards.
 */
static inline mb_time_ms_t mb_transport_elapsed_since(const mb_transport_if_t *iface,
                                                      mb_time_ms_t since)
{
    if (!iface) {
        return 0U;
    }

    const mb_time_ms_t now = mb_transport_now(iface);
    return (now >= since) ? (now - since) : 0U;
}

/**
 * @brief Performs a scatter-gather send using the transport interface.
 *
 * Uses sendv if available (zero-copy), otherwise falls back to copying
 * the iovec list into a temporary buffer and using regular send.
 *
 * @param iface Transport interface
 * @param list IO vector list to send
 * @param out Result metadata (bytes sent)
 * @return MB_OK on success, error code otherwise
 */
static inline mb_err_t mb_transport_sendv(const mb_transport_if_t *iface,
                                          const mb_iovec_list_t *list,
                                          mb_transport_io_result_t *out)
{
    if (!iface || !list) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    
    // Use native scatter-gather if available
    if (iface->sendv != NULL) {
        return iface->sendv(iface->ctx, list, out);
    }
    
    // Fallback: copy to temporary buffer and use regular send
    // Note: This requires scratch memory (not ideal for zero-copy goal)
    mb_u8 temp_buf[256]; // Scratch buffer (should be enough for most PDUs)
    mb_size_t total = mb_iovec_list_total(list);
    
    if (total > sizeof(temp_buf)) {
        return MB_ERR_NO_RESOURCES;
    }
    
    mb_size_t copied = mb_iovec_list_copyout(list, temp_buf, sizeof(temp_buf));
    if (copied != total) {
        return MB_ERR_INVALID_REQUEST;
    }
    
    return iface->send(iface->ctx, temp_buf, copied, out);
}

/**
 * @brief Performs a scatter-gather receive using the transport interface.
 *
 * Uses recvv if available (zero-copy), otherwise receives into a temporary
 * buffer and copies to the iovec list.
 *
 * @param iface Transport interface
 * @param list IO vector list to receive into
 * @param out Result metadata (bytes received)
 * @return MB_OK on success, error code otherwise
 */
static inline mb_err_t mb_transport_recvv(const mb_transport_if_t *iface,
                                          mb_iovec_list_t *list,
                                          mb_transport_io_result_t *out)
{
    if (!iface || !list) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    
    // Use native scatter-gather if available
    if (iface->recvv != NULL) {
        return iface->recvv(iface->ctx, list, out);
    }
    
    // Fallback: receive to temporary buffer and copy to iovec list
    mb_u8 temp_buf[256];
    mb_size_t capacity = mb_iovec_list_total(list);
    
    if (capacity > sizeof(temp_buf)) {
        capacity = sizeof(temp_buf);
    }
    
    mb_transport_io_result_t temp_result;
    mb_err_t err = iface->recv(iface->ctx, temp_buf, capacity, &temp_result);
    if (!mb_err_is_ok(err)) {
        return err;
    }
    
    mb_size_t copied = mb_iovec_list_copyin(list, temp_buf, temp_result.processed);
    if (out != NULL) {
        out->processed = copied;
    }
    
    return MB_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TRANSPORT_IF_H */
