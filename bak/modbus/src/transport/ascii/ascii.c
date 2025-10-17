#include <modbus/conf.h>

#if MB_CONF_TRANSPORT_ASCII

#include <modbus/transport/ascii.h>

#include <string.h>

static void mb_ascii_invoke_callback(mb_ascii_transport_t *ascii,
                                      const mb_adu_view_t *adu,
                                      mb_err_t status)
{
    if (ascii->callback) {
        ascii->callback(ascii, adu, status, ascii->user_ctx);
    }
}

static void mb_ascii_finalize_frame(mb_ascii_transport_t *ascii, mb_err_t status)
{
    if (status == MODBUS_ERROR_NONE) {
        mb_adu_view_t view = {0};
        const mb_err_t decode = mb_frame_ascii_decode(ascii->buffer,
                                                      ascii->index,
                                                      &view,
                                                      ascii->payload,
                                                      sizeof ascii->payload);
        if (decode == MODBUS_ERROR_NONE) {
            mb_ascii_invoke_callback(ascii, &view, MODBUS_ERROR_NONE);
        } else {
            mb_ascii_invoke_callback(ascii, NULL, decode);
        }
    } else {
        mb_ascii_invoke_callback(ascii, NULL, status);
    }

    mb_ascii_reset(ascii);
}

static void mb_ascii_start_frame(mb_ascii_transport_t *ascii)
{
    ascii->receiving = true;
    ascii->index = 0U;
    ascii->last_activity = mb_transport_now(ascii->iface);
}

static void mb_ascii_process_byte(mb_ascii_transport_t *ascii, mb_u8 byte)
{
    if (byte == ':') {
        mb_ascii_start_frame(ascii);
        if (ascii->index < MB_ASCII_BUFFER_SIZE) {
            ascii->buffer[ascii->index++] = byte;
        } else {
            mb_ascii_finalize_frame(ascii, MODBUS_ERROR_INVALID_REQUEST);
        }
        return;
    }

    if (!ascii->receiving) {
        return;
    }

    if (ascii->index >= MB_ASCII_BUFFER_SIZE) {
        mb_ascii_finalize_frame(ascii, MODBUS_ERROR_INVALID_REQUEST);
        return;
    }

    ascii->buffer[ascii->index++] = byte;
    ascii->last_activity = mb_transport_now(ascii->iface);

    if (byte == '\n') {
        mb_ascii_finalize_frame(ascii, MODBUS_ERROR_NONE);
    }
}

mb_err_t mb_ascii_init(mb_ascii_transport_t *ascii,
                       const mb_transport_if_t *iface,
                       mb_ascii_frame_callback_t callback,
                       void *user_ctx)
{
    if (!ascii || !iface || !iface->recv || !iface->send || !iface->now) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    memset(ascii, 0, sizeof *ascii);
    ascii->iface = iface;
    ascii->inter_char_timeout_ms = MB_ASCII_DEFAULT_INTER_CHAR_TIMEOUT_MS;
    ascii->callback = callback;
    ascii->user_ctx = user_ctx;

    return MODBUS_ERROR_NONE;
}

void mb_ascii_reset(mb_ascii_transport_t *ascii)
{
    if (!ascii) {
        return;
    }

    ascii->index = 0U;
    ascii->receiving = false;
    ascii->last_activity = 0U;
}

void mb_ascii_set_inter_char_timeout(mb_ascii_transport_t *ascii, mb_time_ms_t timeout_ms)
{
    if (!ascii) {
        return;
    }

    ascii->inter_char_timeout_ms = (timeout_ms == 0U)
        ? MB_ASCII_DEFAULT_INTER_CHAR_TIMEOUT_MS
        : timeout_ms;
}

mb_err_t mb_ascii_poll(mb_ascii_transport_t *ascii)
{
    if (!ascii || !ascii->iface) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_err_t last_status = MODBUS_ERROR_NONE;

    while (true) {
        mb_transport_io_result_t io = {0};
        mb_u8 byte = 0U;
        const mb_err_t status = mb_transport_recv(ascii->iface, &byte, 1U, &io);

        if (status == MODBUS_ERROR_NONE && io.processed > 0U) {
            mb_ascii_process_byte(ascii, byte);
            last_status = MODBUS_ERROR_NONE;
            continue;
        }

        if (status == MODBUS_ERROR_TIMEOUT || (status == MODBUS_ERROR_NONE && io.processed == 0U)) {
            last_status = MODBUS_ERROR_NONE;
        } else if (status != MODBUS_ERROR_NONE) {
            mb_ascii_invoke_callback(ascii, NULL, status);
            last_status = status;
        }
        break;
    }

    if (ascii->receiving) {
        const mb_time_ms_t elapsed = mb_transport_elapsed_since(ascii->iface, ascii->last_activity);
        if (elapsed >= ascii->inter_char_timeout_ms) {
            mb_ascii_finalize_frame(ascii, MODBUS_ERROR_TIMEOUT);
        }
    }

    return last_status;
}

mb_err_t mb_ascii_submit(mb_ascii_transport_t *ascii, const mb_adu_view_t *adu)
{
    if (!ascii || !ascii->iface || !adu) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u8 frame[MB_ASCII_BUFFER_SIZE];
    mb_size_t frame_len = 0U;
    mb_err_t status = mb_frame_ascii_encode(adu, frame, sizeof frame, &frame_len);
    if (status != MODBUS_ERROR_NONE) {
        return status;
    }

    mb_transport_io_result_t io = {0};
    status = mb_transport_send(ascii->iface, frame, frame_len, &io);
    if (status != MODBUS_ERROR_NONE) {
        return status;
    }

    if (io.processed != frame_len) {
        return MODBUS_ERROR_TRANSPORT;
    }

    return MODBUS_ERROR_NONE;
}

#endif /* MB_CONF_TRANSPORT_ASCII */
