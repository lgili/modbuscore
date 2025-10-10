/**
 * @file transport/rtu.h
 * @brief Minimal RTU transport built on top of the non-blocking transport interface.
 */

#ifndef MODBUS_TRANSPORT_RTU_H
#define MODBUS_TRANSPORT_RTU_H

#include <stdbool.h>

#include <modbus/conf.h>

#if MB_CONF_TRANSPORT_RTU
#include <modbus/frame.h>
#include <modbus/pdu.h>
#include <modbus/transport_if.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_RTU_DEFAULT_SILENCE_TIMEOUT_MS 5U
#define MB_RTU_BUFFER_SIZE (MB_PDU_MAX + 4U)

typedef struct mb_rtu_transport mb_rtu_transport_t;

typedef void (*mb_rtu_frame_callback_t)(mb_rtu_transport_t *rtu,
                                        const mb_adu_view_t *adu,
                                        mb_err_t status,
                                        void *user);

struct mb_rtu_transport {
    const mb_transport_if_t *iface;
    mb_time_ms_t silence_timeout_ms;
    mb_time_ms_t last_activity;
    mb_u8 buffer[MB_RTU_BUFFER_SIZE];
    mb_size_t index;
    bool receiving;
    mb_rtu_frame_callback_t callback;
    void *user_ctx;
};

mb_err_t mb_rtu_init(mb_rtu_transport_t *rtu,
                     const mb_transport_if_t *iface,
                     mb_rtu_frame_callback_t callback,
                     void *user_ctx);

void mb_rtu_reset(mb_rtu_transport_t *rtu);

void mb_rtu_set_silence_timeout(mb_rtu_transport_t *rtu, mb_time_ms_t timeout_ms);

mb_err_t mb_rtu_poll(mb_rtu_transport_t *rtu);

mb_err_t mb_rtu_submit(mb_rtu_transport_t *rtu, const mb_adu_view_t *adu);

#ifdef __cplusplus
}
#endif

#endif /* MB_CONF_TRANSPORT_RTU */

#endif /* MODBUS_TRANSPORT_RTU_H */
