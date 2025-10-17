#include <modbuscore/transport/iface.h>

mbc_status_t mbc_transport_send(const mbc_transport_iface_t *iface,
                                const uint8_t *buffer,
                                size_t length,
                                mbc_transport_io_t *out)
{
    if (!iface || !iface->send || !buffer) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (length == 0U) {
        if (out) {
            out->processed = 0U;
        }
        return MBC_STATUS_OK;
    }

    return iface->send(iface->ctx, buffer, length, out);
}

mbc_status_t mbc_transport_receive(const mbc_transport_iface_t *iface,
                                   uint8_t *buffer,
                                   size_t capacity,
                                   mbc_transport_io_t *out)
{
    if (!iface || !iface->receive || !buffer || capacity == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    return iface->receive(iface->ctx, buffer, capacity, out);
}

uint64_t mbc_transport_now(const mbc_transport_iface_t *iface)
{
    if (!iface || !iface->now) {
        return 0ULL;
    }

    return iface->now(iface->ctx);
}

void mbc_transport_yield(const mbc_transport_iface_t *iface)
{
    if (!iface || !iface->yield) {
        return;
    }

    iface->yield(iface->ctx);
}
