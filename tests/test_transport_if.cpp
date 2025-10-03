#include <array>

#include <gtest/gtest.h>

extern "C" {
#include <modbus/frame.h>
#include <modbus/transport_if.h>
#include <modbus/modbus.h>
}

extern "C" {
void modbus_transport_init_mock(modbus_transport_t *transport);
int mock_inject_rx_data(const uint8_t *data, uint16_t length);
uint16_t mock_get_tx_data(uint8_t *data, uint16_t maxlen);
void mock_clear_tx_buffer(void);
void mock_advance_time(uint16_t ms);
const mb_transport_if_t *mock_transport_get_iface(void);
}

namespace {

class TransportIfTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        modbus_transport_init_mock(&legacy_transport_);
        iface_ = mock_transport_get_iface();
        ASSERT_NE(nullptr, iface_);
    }

    modbus_transport_t legacy_transport_{};
    const mb_transport_if_t *iface_ = nullptr;
};

TEST_F(TransportIfTest, SendAndReceiveFrame)
{
    constexpr mb_adu_view_t adu{0x11U, MODBUS_FUNC_READ_HOLDING_REGISTERS, nullptr, 0U};

    std::array<mb_u8, 32> frame{};
    mb_size_t frame_len = 0U;
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_frame_rtu_encode(&adu, frame.data(), frame.size(), &frame_len));

    mb_transport_io_result_t io{};
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_transport_send(iface_, frame.data(), frame_len, &io));
    EXPECT_EQ(frame_len, io.processed);

    std::array<uint8_t, 32> tx_buffer{};
    EXPECT_EQ(frame_len, mock_get_tx_data(tx_buffer.data(), tx_buffer.size()));
    EXPECT_TRUE(std::equal(frame.begin(), frame.begin() + frame_len, tx_buffer.begin()));

    mock_clear_tx_buffer();

    ASSERT_EQ(0, mock_inject_rx_data(frame.data(), static_cast<uint16_t>(frame_len)));
    std::array<uint8_t, 32> rx_buffer{};
    mb_transport_io_result_t rx_io{};
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_transport_recv(iface_, rx_buffer.data(), rx_buffer.size(), &rx_io));
    EXPECT_EQ(frame_len, rx_io.processed);
    EXPECT_TRUE(std::equal(frame.begin(), frame.begin() + frame_len, rx_buffer.begin()));
}

TEST_F(TransportIfTest, ElapsedSince)
{
    const mb_time_ms_t start = mb_transport_now(iface_);
    mock_advance_time(25);
    const mb_time_ms_t elapsed = mb_transport_elapsed_since(iface_, start);
    EXPECT_EQ(25U, elapsed);
}

TEST_F(TransportIfTest, GuardsRejectInvalidParameters)
{
    mb_transport_io_result_t io{};
    std::array<mb_u8, 8> buffer{};

    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, mb_transport_send(nullptr, buffer.data(), buffer.size(), &io));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, mb_transport_send(iface_, nullptr, buffer.size(), &io));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, mb_transport_recv(nullptr, buffer.data(), buffer.size(), &io));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, mb_transport_recv(iface_, nullptr, buffer.size(), &io));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, mb_transport_recv(iface_, buffer.data(), 0U, &io));
}

} // namespace
