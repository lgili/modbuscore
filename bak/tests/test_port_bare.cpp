#include <array>
#include <vector>

#include <gtest/gtest.h>

extern "C" {
#include <modbus/port/bare.h>
}

namespace {

struct FakeDevice {
    std::vector<mb_u8> rx;
    std::vector<mb_u8> tx;
    uint32_t tick = 0U;
};

static mb_err_t fake_send(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out)
{
    auto *dev = static_cast<FakeDevice *>(ctx);
    dev->tx.insert(dev->tx.end(), buf, buf + len);
    if (out) {
        out->processed = len;
    }
    return MB_OK;
}

static mb_err_t fake_recv(void *ctx, mb_u8 *buf, mb_size_t cap, mb_transport_io_result_t *out)
{
    auto *dev = static_cast<FakeDevice *>(ctx);
    if (dev->rx.empty()) {
        if (out) {
            out->processed = 0U;
        }
        return MB_ERR_TIMEOUT;
    }

    const mb_size_t to_copy = std::min(cap, static_cast<mb_size_t>(dev->rx.size()));
    for (mb_size_t i = 0; i < to_copy; ++i) {
        buf[i] = dev->rx[i];
    }
    dev->rx.erase(dev->rx.begin(), dev->rx.begin() + static_cast<std::ptrdiff_t>(to_copy));
    if (out) {
        out->processed = to_copy;
    }
    return MB_OK;
}

static uint32_t fake_ticks(void *ctx)
{
    auto *dev = static_cast<FakeDevice *>(ctx);
    return dev->tick;
}

TEST(BarePortTest, BindsCallbacksAndConvertsTicks)
{
    FakeDevice dev{};
    dev.rx = {0x10, 0x20, 0x30};
    dev.tick = 500U; // 500 ticks at 1 kHz -> 500 ms

    mb_port_bare_transport_t port{};
    ASSERT_EQ(MB_OK, mb_port_bare_transport_init(&port,
                                                 &dev,
                                                 fake_send,
                                                 fake_recv,
                                                 fake_ticks,
                                                 1000U,
                                                 nullptr,
                                                 nullptr));

    const mb_transport_if_t *iface = mb_port_bare_transport_iface(&port);
    ASSERT_NE(nullptr, iface);

    const mb_time_ms_t now_ms = mb_transport_now(iface);
    EXPECT_EQ(500U, now_ms);

    std::array<mb_u8, 2> to_send{0xAB, 0xCD};
    mb_transport_io_result_t io{};
    ASSERT_EQ(MB_OK, mb_transport_send(iface, to_send.data(), to_send.size(), &io));
    EXPECT_EQ(to_send.size(), io.processed);
    EXPECT_EQ(to_send.size(), dev.tx.size());
    EXPECT_EQ(to_send[0], dev.tx[0]);
    EXPECT_EQ(to_send[1], dev.tx[1]);

    std::array<mb_u8, 3> received{};
    io.processed = 0U;
    ASSERT_EQ(MB_OK, mb_transport_recv(iface, received.data(), received.size(), &io));
    EXPECT_EQ(3U, io.processed);
    EXPECT_EQ(0x10U, received[0]);
    EXPECT_TRUE(dev.rx.empty());

    dev.tick = 250U;
    mb_port_bare_transport_update_tick_rate(&port, 2000U); // 2 kHz -> 125 ms
    EXPECT_EQ(125U, mb_transport_now(iface));
}

} // namespace
