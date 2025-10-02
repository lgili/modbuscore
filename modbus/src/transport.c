#include <limits.h>

#include <modbus/transport.h>

static mb_err_t modbus_transport_legacy_send(void *ctx,
                                             const mb_u8 *buf,
                                             mb_size_t len,
                                             mb_transport_io_result_t *out)
{
    modbus_transport_t *legacy = (modbus_transport_t *)ctx;
    if (!legacy || !buf || !legacy->write) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (len > (mb_size_t)UINT16_MAX) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    const int32_t written = legacy->write(buf, (uint16_t)len);
    if (written < 0) {
        return MODBUS_ERROR_TRANSPORT;
    }

    if (out) {
        out->processed = (mb_size_t)written;
    }

    return (written == (int32_t)len) ? MODBUS_ERROR_NONE : MODBUS_ERROR_TRANSPORT;
}

static mb_err_t modbus_transport_legacy_recv(void *ctx,
                                             mb_u8 *buf,
                                             mb_size_t cap,
                                             mb_transport_io_result_t *out)
{
    modbus_transport_t *legacy = (modbus_transport_t *)ctx;
    if (!legacy || !buf || cap == 0U || !legacy->read) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (cap > (mb_size_t)UINT16_MAX) {
        cap = UINT16_MAX;
    }

    const int32_t bytes = legacy->read(buf, (uint16_t)cap);
    if (bytes < 0) {
        return MODBUS_ERROR_TRANSPORT;
    }

    if (out) {
        out->processed = (mb_size_t)bytes;
    }

    return (bytes > 0) ? MODBUS_ERROR_NONE : MODBUS_ERROR_TIMEOUT;
}

static mb_time_ms_t modbus_transport_legacy_now(void *ctx)
{
    modbus_transport_t *legacy = (modbus_transport_t *)ctx;
    if (!legacy || !legacy->get_reference_msec) {
        return 0U;
    }

    return (mb_time_ms_t)legacy->get_reference_msec();
}

mb_err_t modbus_transport_bind_legacy(mb_transport_if_t *iface, modbus_transport_t *legacy)
{
    if (!iface || !legacy) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!legacy->write || !legacy->read || !legacy->get_reference_msec) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    iface->ctx = legacy;
    iface->send = modbus_transport_legacy_send;
    iface->recv = modbus_transport_legacy_recv;
    iface->now = modbus_transport_legacy_now;
    iface->yield = NULL;

    return MODBUS_ERROR_NONE;
}
