#include <algorithm>
#include <array>
#include <vector>

#include <gtest/gtest.h>

extern "C" {
#include <modbus/port/freertos.h>
}

namespace {

struct FakeStream {
    std::vector<mb_u8> data;
};

static size_t fake_stream_send(void *stream, const mb_u8 *payload, size_t length, uint32_t ticks_to_wait)
{
    (void)ticks_to_wait;
    auto *fs = static_cast<FakeStream *>(stream);
    fs->data.insert(fs->data.end(), payload, payload + length);
    return length;
}

static size_t fake_stream_recv(void *stream, mb_u8 *buffer, size_t capacity, uint32_t ticks_to_wait)
{
    (void)ticks_to_wait;
    auto *fs = static_cast<FakeStream *>(stream);
    if (fs->data.empty()) {
        return 0U;
    }

    const size_t to_copy = std::min(capacity, fs->data.size());
    std::copy_n(fs->data.begin(), to_copy, buffer);
    fs->data.erase(fs->data.begin(), fs->data.begin() + static_cast<std::ptrdiff_t>(to_copy));
    return to_copy;
}

static uint32_t fake_tick_count = 0U;

static uint32_t fake_tick(void)
{
    return fake_tick_count;
}

static void fake_yield(void)
{
    // no-op for tests
}

TEST(FreeRtosPortTest, WrapsStreamBuffers)
{
    FakeStream tx{};
    FakeStream rx{};
    rx.data = {0x41U, 0x42U, 0x43U};
    fake_tick_count = 1500U; // at 1 kHz -> 1500 ms

    mb_port_freertos_transport_t port{};
    ASSERT_EQ(MB_OK,
              mb_port_freertos_transport_init(&port,
                                               &tx,
                                               &rx,
                                               fake_stream_send,
                                               fake_stream_recv,
                                               fake_tick,
                                               fake_yield,
                                               1000U,
                                               10U));

    const mb_transport_if_t *iface = mb_port_freertos_transport_iface(&port);
    ASSERT_NE(nullptr, iface);

    EXPECT_EQ(1500U, mb_transport_now(iface));

    std::array<mb_u8, 4> rx_buf{};
    mb_transport_io_result_t io{};
    ASSERT_EQ(MB_OK, mb_transport_recv(iface, rx_buf.data(), rx_buf.size(), &io));
    EXPECT_EQ(3U, io.processed);
    EXPECT_EQ(0x41U, rx_buf[0]);

    std::array<mb_u8, 2> tx_payload{0x10U, 0x20U};
    io.processed = 0U;
    ASSERT_EQ(MB_OK, mb_transport_send(iface, tx_payload.data(), tx_payload.size(), &io));
    EXPECT_EQ(tx_payload.size(), io.processed);
    EXPECT_EQ(tx_payload.size(), tx.data.size());

    // No data available -> timeout
    io.processed = 0U;
    EXPECT_EQ(MB_ERR_TIMEOUT, mb_transport_recv(iface, rx_buf.data(), rx_buf.size(), &io));
    EXPECT_EQ(0U, io.processed);

    fake_tick_count = 500U;
    mb_port_freertos_transport_set_tick_rate(&port, 2000U); // 2 kHz -> 250 ms
    EXPECT_EQ(250U, mb_transport_now(iface));
}

} // namespace
