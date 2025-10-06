/**
 * @file transport/ascii.h
 * @brief Minimal ASCII transport built on top of the non-blocking transport interface.
 */

#ifndef MODBUS_TRANSPORT_ASCII_H
#define MODBUS_TRANSPORT_ASCII_H

#include <stdbool.h>

#include <modbus/frame.h>
#include <modbus/pdu.h>
#include <modbus/transport_if.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_ASCII_DEFAULT_INTER_CHAR_TIMEOUT_MS 1000U
#define MB_ASCII_BUFFER_SIZE (((MB_PDU_MAX + 3U) * 2U) + 4U)

typedef struct mb_ascii_transport mb_ascii_transport_t;

typedef void (*mb_ascii_frame_callback_t)(mb_ascii_transport_t *ascii,
                                           const mb_adu_view_t *adu,
                                           mb_err_t status,
                                           void *user);

struct mb_ascii_transport {
    const mb_transport_if_t *iface;
    mb_time_ms_t inter_char_timeout_ms;
    mb_time_ms_t last_activity;
    mb_u8 buffer[MB_ASCII_BUFFER_SIZE];
    mb_size_t index;
    bool receiving;
    mb_ascii_frame_callback_t callback;
    void *user_ctx;
    mb_u8 payload[MB_PDU_MAX];
};

mb_err_t mb_ascii_init(mb_ascii_transport_t *ascii,
                       const mb_transport_if_t *iface,
                       mb_ascii_frame_callback_t callback,
                       void *user_ctx);

void mb_ascii_reset(mb_ascii_transport_t *ascii);

void mb_ascii_set_inter_char_timeout(mb_ascii_transport_t *ascii, mb_time_ms_t timeout_ms);

mb_err_t mb_ascii_poll(mb_ascii_transport_t *ascii);

mb_err_t mb_ascii_submit(mb_ascii_transport_t *ascii, const mb_adu_view_t *adu);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TRANSPORT_ASCII_H */
