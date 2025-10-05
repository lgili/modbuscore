/**
 * @file transport_if.h
 * @brief Minimal, non-blocking transport interface shared by client and server code.
 */

#ifndef MODBUS_TRANSPORT_IF_H
#define MODBUS_TRANSPORT_IF_H

#include <modbus/mb_err.h>
#include <modbus/mb_types.h>

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
    void *ctx;                      /**< User-supplied context forwarded to callbacks. */
    mb_transport_send_fn send;      /**< Send callback (required). */
    mb_transport_recv_fn recv;      /**< Receive callback (required). */
    mb_transport_now_fn now;        /**< Monotonic time source (required). */
    mb_transport_yield_fn yield;    /**< Optional cooperative-yield hook (may be `NULL`). */
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

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TRANSPORT_IF_H */
