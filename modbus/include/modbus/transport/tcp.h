/**
 * @file transport/tcp.h
 * @brief Minimal Modbus TCP (MBAP) transport built on top of the non-blocking transport interface.
 */

#ifndef MODBUS_TRANSPORT_TCP_H
#define MODBUS_TRANSPORT_TCP_H

#include <modbus/conf.h>

#if MB_CONF_TRANSPORT_TCP
#include <modbus/internal/frame.h>
#include <modbus/internal/pdu.h>
#include <modbus/internal/transport_core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_TCP_HEADER_SIZE 7U
#define MB_TCP_BUFFER_SIZE (MB_PDU_MAX + MB_TCP_HEADER_SIZE)

struct mb_tcp_transport;

typedef void (*mb_tcp_frame_callback_t)(struct mb_tcp_transport *tcp,
                                        const mb_adu_view_t *adu,
                                        mb_u16 transaction_id,
                                        mb_err_t status,
                                        void *user_ctx);

typedef struct mb_tcp_transport {
    const mb_transport_if_t *iface;
    mb_tcp_frame_callback_t callback;
    void *user_ctx;
    mb_u8 rx_buffer[MB_TCP_BUFFER_SIZE];
    mb_size_t rx_len;
    mb_u8 payload_buffer[MB_PDU_MAX];
} mb_tcp_transport_t;

mb_err_t mb_tcp_init(mb_tcp_transport_t *tcp,
                     const mb_transport_if_t *iface,
                     mb_tcp_frame_callback_t callback,
                     void *user_ctx);

void mb_tcp_reset(mb_tcp_transport_t *tcp);

mb_err_t mb_tcp_poll(mb_tcp_transport_t *tcp);

mb_err_t mb_tcp_submit(mb_tcp_transport_t *tcp,
                       const mb_adu_view_t *adu,
                       mb_u16 transaction_id);

#ifdef __cplusplus
}
#endif

#endif /* MB_CONF_TRANSPORT_TCP */

#endif /* MODBUS_TRANSPORT_TCP_H */
