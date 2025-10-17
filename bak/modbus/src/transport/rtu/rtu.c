#include <modbus/conf.h>

#if MB_CONF_TRANSPORT_RTU

#include <modbus/transport/rtu.h>

#include <string.h>

static void mb_rtu_invoke_callback(mb_rtu_transport_t *rtu,
                                   const mb_adu_view_t *adu,
                                   mb_err_t status)
{
    if (rtu->callback) {
        rtu->callback(rtu, adu, status, rtu->user_ctx);
    }
}

mb_err_t mb_rtu_init(mb_rtu_transport_t *rtu,
                     const mb_transport_if_t *iface,
                     mb_rtu_frame_callback_t callback,
                     void *user_ctx)
{
    if (rtu == NULL || iface == NULL || iface->recv == NULL || iface->send == NULL || iface->now == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    memset(rtu, 0, sizeof(*rtu));
    rtu->iface = iface;
    rtu->silence_timeout_ms = MB_RTU_DEFAULT_SILENCE_TIMEOUT_MS;
    rtu->callback = callback;
    rtu->user_ctx = user_ctx;

    return MODBUS_ERROR_NONE;
}

void mb_rtu_reset(mb_rtu_transport_t *rtu)
{
    if (rtu == NULL) {
        return;
    }

    rtu->index = 0U;
    rtu->receiving = false;
}

void mb_rtu_set_silence_timeout(mb_rtu_transport_t *rtu, mb_time_ms_t timeout_ms)
{
    if (rtu == NULL) {
        return;
    }

    rtu->silence_timeout_ms = (timeout_ms == 0U) ? MB_RTU_DEFAULT_SILENCE_TIMEOUT_MS : timeout_ms;
}

static void mb_rtu_finalize_frame(mb_rtu_transport_t *rtu, mb_err_t status)
{
    if (rtu->index >= 4U && status == MODBUS_ERROR_NONE) {
        mb_adu_view_t view = {0};
        status = mb_frame_rtu_decode(rtu->buffer, rtu->index, &view);
        if (status == MODBUS_ERROR_NONE) {
            mb_rtu_invoke_callback(rtu, &view, status);
        } else {
            mb_rtu_invoke_callback(rtu, NULL, status);
        }
    } else {
        if (status == MODBUS_ERROR_NONE) {
            status = MODBUS_ERROR_INVALID_REQUEST;
        }
        mb_rtu_invoke_callback(rtu, NULL, status);
    }

    mb_rtu_reset(rtu);
}

static void mb_rtu_process_byte(mb_rtu_transport_t *rtu, mb_u8 byte)
{
    if (rtu->index >= MB_RTU_BUFFER_SIZE) {
        mb_rtu_finalize_frame(rtu, MODBUS_ERROR_INVALID_REQUEST);
        return;
    }

    rtu->buffer[rtu->index++] = byte;
    rtu->last_activity = mb_transport_now(rtu->iface);
    rtu->receiving = true;
    if (rtu->index >= MB_RTU_BUFFER_SIZE) {
        mb_rtu_finalize_frame(rtu, MODBUS_ERROR_INVALID_REQUEST);
    }
}

mb_err_t mb_rtu_poll(mb_rtu_transport_t *rtu)
{
    if (rtu == NULL || rtu->iface == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_err_t last_status = MODBUS_ERROR_NONE;

    while (true) {
        mb_transport_io_result_t io = {0};
        mb_u8 byte = 0U;
        mb_err_t status = mb_transport_recv(rtu->iface, &byte, 1U, &io);

        if (status == MODBUS_ERROR_NONE && io.processed > 0U) {
            mb_rtu_process_byte(rtu, byte);
            last_status = MODBUS_ERROR_NONE;
            continue;
        }

        if (status == MODBUS_ERROR_TIMEOUT || (status == MODBUS_ERROR_NONE && io.processed == 0U)) {
            last_status = MODBUS_ERROR_NONE;
        } else if (status != MODBUS_ERROR_NONE) {
            mb_rtu_invoke_callback(rtu, NULL, status);
            last_status = status;
        }
        break;
    }

    if (rtu->receiving) {
        const mb_time_ms_t elapsed = mb_transport_elapsed_since(rtu->iface, rtu->last_activity);
        if (elapsed >= rtu->silence_timeout_ms) {
            mb_rtu_finalize_frame(rtu, MODBUS_ERROR_NONE);
        }
    }

    return last_status;
}

mb_err_t mb_rtu_submit(mb_rtu_transport_t *rtu, const mb_adu_view_t *adu)
{
    if (rtu == NULL || adu == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u8 frame[MB_RTU_BUFFER_SIZE];
    mb_size_t frame_len = 0U;
    mb_err_t status = mb_frame_rtu_encode(adu, frame, sizeof frame, &frame_len);
    if (status != MODBUS_ERROR_NONE) {
        return status;
    }

    mb_transport_io_result_t io = {0};
    status = mb_transport_send(rtu->iface, frame, frame_len, &io);
    if (status != MODBUS_ERROR_NONE) {
        return status;
    }

    if (io.processed != frame_len) {
        return MODBUS_ERROR_TRANSPORT;
    }

    return MODBUS_ERROR_NONE;
}
#endif /* MB_CONF_TRANSPORT_RTU */
