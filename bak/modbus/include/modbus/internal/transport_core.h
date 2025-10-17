#ifndef MODBUS_INTERNAL_TRANSPORT_CORE_H
#define MODBUS_INTERNAL_TRANSPORT_CORE_H

/**
 * @file transport_core.h
 * @brief Shared transport abstractions (legacy + non-blocking façade).
 *
 * Consolidates the legacy `modbus_transport_t` descriptor with the lightweight
 * `mb_transport_if_t` interface used by the modern client/server paths.
 */

#include <stddef.h>
#include <stdint.h>

#include <modbus/mb_err.h>
#include <modbus/mb_types.h>
#include <modbus/internal/mb_iovec.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Supported transport backends.
 */
typedef enum {
    MODBUS_TRANSPORT_RTU = 1,
    MODBUS_TRANSPORT_TCP = 2,
    MODBUS_TRANSPORT_ASCII = 3
} modbus_transport_type_t;

/**
 * @brief Legacy transport descriptor used by the classic API.
 *
 * The callbacks may block and are typically wired directly to the platform
 * drivers (UART/TCP). New code should rely on ::mb_transport_if_t but we keep
 * this structure while the legacy API remains available.
 */
typedef struct {
    modbus_transport_type_t transport;
    int32_t (*read)(mb_u8 *buf, mb_u16 count);
    int32_t (*write)(const mb_u8 *buf, mb_u16 count);
    mb_u16 (*get_reference_msec)(void);
    mb_u16 (*measure_time_msec)(mb_u16 ref);
    mb_u16 (*change_baudrate)(mb_u16 baudrate);
    void (*restart_uart)(void);
    mb_u8 (*write_gpio)(mb_u8 gpio, mb_u8 value);
    mb_u8 (*parse_bootloader_request)(mb_u8 *buffer, mb_u16 *buffer_size);
    void *arg;
} modbus_transport_t;

/**
 * @brief Result metadata for transport I/O operations.
 */
typedef struct {
    mb_size_t processed;
} mb_transport_io_result_t;

typedef mb_err_t (*mb_transport_send_fn)(void *ctx,
                                         const mb_u8 *buf,
                                         mb_size_t len,
                                         mb_transport_io_result_t *out);

typedef mb_err_t (*mb_transport_recv_fn)(void *ctx,
                                         mb_u8 *buf,
                                         mb_size_t cap,
                                         mb_transport_io_result_t *out);

typedef mb_err_t (*mb_transport_sendv_fn)(void *ctx,
                                          const mb_iovec_list_t *list,
                                          mb_transport_io_result_t *out);

typedef mb_err_t (*mb_transport_recvv_fn)(void *ctx,
                                          mb_iovec_list_t *list,
                                          mb_transport_io_result_t *out);

typedef mb_time_ms_t (*mb_transport_now_fn)(void *ctx);
typedef void (*mb_transport_yield_fn)(void *ctx);

/**
 * @brief Non-blocking façade used by the modern client/server stack.
 */
typedef struct {
    void *ctx;
    mb_transport_send_fn send;
    mb_transport_recv_fn recv;
    mb_transport_sendv_fn sendv;
    mb_transport_recvv_fn recvv;
    mb_transport_now_fn now;
    mb_transport_yield_fn yield;
} mb_transport_if_t;

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

static inline mb_time_ms_t mb_transport_now(const mb_transport_if_t *iface)
{
    if (!iface || !iface->now) {
        return 0U;
    }
    return iface->now(iface->ctx);
}

static inline void mb_transport_yield(const mb_transport_if_t *iface)
{
    if (iface && iface->yield) {
        iface->yield(iface->ctx);
    }
}

static inline mb_time_ms_t mb_transport_elapsed_since(const mb_transport_if_t *iface,
                                                      mb_time_ms_t since)
{
    if (!iface) {
        return 0U;
    }

    const mb_time_ms_t now = mb_transport_now(iface);
    return (now >= since) ? (now - since) : 0U;
}

static inline mb_err_t mb_transport_sendv(const mb_transport_if_t *iface,
                                          const mb_iovec_list_t *list,
                                          mb_transport_io_result_t *out)
{
    if (!iface || !list) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (iface->sendv != NULL) {
        return iface->sendv(iface->ctx, list, out);
    }

    mb_u8 temp_buf[256];
    const mb_size_t total = mb_iovec_list_total(list);
    if (total > sizeof(temp_buf)) {
        return MB_ERR_NO_RESOURCES;
    }

    const mb_size_t copied = mb_iovec_list_copyout(list, temp_buf, sizeof(temp_buf));
    if (copied != total) {
        return MB_ERR_INVALID_REQUEST;
    }

    return iface->send(iface->ctx, temp_buf, copied, out);
}

static inline mb_err_t mb_transport_recvv(const mb_transport_if_t *iface,
                                          mb_iovec_list_t *list,
                                          mb_transport_io_result_t *out)
{
    if (!iface || !list) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (iface->recvv != NULL) {
        return iface->recvv(iface->ctx, list, out);
    }

    mb_u8 temp_buf[256];
    const mb_size_t total = mb_iovec_list_total(list);
    if (total > sizeof(temp_buf)) {
        return MB_ERR_NO_RESOURCES;
    }

    if (!iface->recv) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_transport_io_result_t tmp = {0};
    mb_err_t err = iface->recv(iface->ctx, temp_buf, total, &tmp);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    if (mb_iovec_list_copyin(list, temp_buf, tmp.processed) != tmp.processed) {
        return MB_ERR_INVALID_REQUEST;
    }

    if (out) {
        out->processed = tmp.processed;
    }
    return MB_OK;
}

/**
 * @brief Bridges the legacy descriptor into the non-blocking façade.
 */
mb_err_t modbus_transport_bind_legacy(mb_transport_if_t *iface, modbus_transport_t *legacy);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_INTERNAL_TRANSPORT_CORE_H */

