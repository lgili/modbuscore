#pragma once

#include <cstring>
#include <vector>

#include <modbus/transport_if.h>

namespace modbus::test {

struct TestTcpChannel {
    std::vector<mb_u8> rx;
    std::vector<mb_u8> tx;
    mb_size_t cursor{0U};
    mb_time_ms_t now{0U};
};

static inline mb_err_t channel_send(void *ctx,
                                    const mb_u8 *buf,
                                    mb_size_t len,
                                    mb_transport_io_result_t *out)
{
    auto *channel = static_cast<TestTcpChannel *>(ctx);
    if (!channel || !buf) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    channel->tx.insert(channel->tx.end(), buf, buf + len);
    if (out) {
        out->processed = len;
    }
    return MB_OK;
}

static inline mb_err_t channel_recv(void *ctx,
                                    mb_u8 *buf,
                                    mb_size_t cap,
                                    mb_transport_io_result_t *out)
{
    auto *channel = static_cast<TestTcpChannel *>(ctx);
    if (!channel || !buf || cap == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_size_t available = (channel->cursor < channel->rx.size())
                                    ? (channel->rx.size() - channel->cursor)
                                    : 0U;
    if (available == 0U) {
        if (out) {
            out->processed = 0U;
        }
        return MB_ERR_TIMEOUT;
    }

    const mb_size_t to_copy = (cap < available) ? cap : available;
    std::memcpy(buf, channel->rx.data() + channel->cursor, to_copy);
    channel->cursor += to_copy;

    if (channel->cursor >= channel->rx.size()) {
        channel->rx.clear();
        channel->cursor = 0U;
    }

    if (out) {
        out->processed = to_copy;
    }
    return MB_OK;
}

static inline mb_time_ms_t channel_now(void *ctx)
{
    auto *channel = static_cast<TestTcpChannel *>(ctx);
    return channel ? channel->now : 0U;
}

static inline mb_transport_if_t make_transport(TestTcpChannel &channel)
{
    mb_transport_if_t iface{};
    iface.ctx = &channel;
    iface.send = channel_send;
    iface.recv = channel_recv;
    iface.now = channel_now;
    iface.yield = NULL;
    return iface;
}

static inline void channel_push_rx(TestTcpChannel &channel,
                                   const mb_u8 *data,
                                   mb_size_t len)
{
    channel.rx.insert(channel.rx.end(), data, data + len);
}

static inline void channel_push_rx(TestTcpChannel &channel,
                                   const std::vector<mb_u8> &data)
{
    channel.rx.insert(channel.rx.end(), data.begin(), data.end());
}

static inline std::vector<mb_u8> channel_take_tx(TestTcpChannel &channel)
{
    std::vector<mb_u8> copy = channel.tx;
    channel.tx.clear();
    return copy;
}

static inline void channel_reset(TestTcpChannel &channel)
{
    channel.rx.clear();
    channel.tx.clear();
    channel.cursor = 0U;
    channel.now = 0U;
}

static inline void channel_advance_time(TestTcpChannel &channel,
                                        mb_time_ms_t delta)
{
    channel.now += delta;
}

} // namespace modbus::test
