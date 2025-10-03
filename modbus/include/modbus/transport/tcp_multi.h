/**
 * @file transport/tcp_multi.h
 * @brief Helper utilities for managing multiple Modbus TCP connections.
 */

#ifndef MODBUS_TRANSPORT_TCP_MULTI_H
#define MODBUS_TRANSPORT_TCP_MULTI_H

#include <stdbool.h>
#include <stddef.h>

#include <modbus/conf.h>
#include <modbus/mb_err.h>
#include <modbus/mb_types.h>
#include <modbus/transport/tcp.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mb_tcp_multi_transport;

/**
 * @brief Callback invoked whenever a frame is processed on any registered connection.
 */
typedef void (*mb_tcp_multi_frame_callback_t)(struct mb_tcp_multi_transport *multi,
                                              mb_size_t slot_index,
                                              const mb_adu_view_t *adu,
                                              mb_u16 transaction_id,
                                              mb_err_t status,
                                              void *user_ctx);

/**
 * @brief Per-connection bookkeeping data (opaque to users).
 */
typedef struct mb_tcp_multi_slot {
    mb_tcp_transport_t tcp;              /**< Underlying transport instance. */
    bool active;                         /**< Slot activation flag. */
    mb_size_t index;                     /**< Stable index inside the pool. */
    struct mb_tcp_multi_transport *owner;/**< Owning multi-transport handle. */
    const mb_transport_if_t *iface;      /**< Transport interface bound to this slot. */
} mb_tcp_multi_slot_t;

/**
 * @brief Multiplexes multiple TCP connections while reusing the single-connection helper.
 */
typedef struct mb_tcp_multi_transport {
    mb_tcp_multi_slot_t slots[MB_TCP_MAX_CONNECTIONS]; /**< Connection slots. */
    mb_size_t active_count;                             /**< Number of active slots. */
    mb_tcp_multi_frame_callback_t callback;             /**< Shared frame callback. */
    void *user_ctx;                                     /**< User context forwarded to callback. */
} mb_tcp_multi_transport_t;

mb_err_t mb_tcp_multi_init(mb_tcp_multi_transport_t *multi,
                           mb_tcp_multi_frame_callback_t callback,
                           void *user_ctx);

mb_err_t mb_tcp_multi_add(mb_tcp_multi_transport_t *multi,
                          const mb_transport_if_t *iface,
                          mb_size_t *out_slot_index);

mb_err_t mb_tcp_multi_remove(mb_tcp_multi_transport_t *multi,
                             mb_size_t slot_index);

mb_err_t mb_tcp_multi_submit(mb_tcp_multi_transport_t *multi,
                             mb_size_t slot_index,
                             const mb_adu_view_t *adu,
                             mb_u16 transaction_id);

mb_err_t mb_tcp_multi_poll(mb_tcp_multi_transport_t *multi,
                           mb_size_t slot_index);

mb_err_t mb_tcp_multi_poll_all(mb_tcp_multi_transport_t *multi);

bool mb_tcp_multi_is_active(const mb_tcp_multi_transport_t *multi,
                            mb_size_t slot_index);

mb_size_t mb_tcp_multi_active_count(const mb_tcp_multi_transport_t *multi);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TRANSPORT_TCP_MULTI_H */
