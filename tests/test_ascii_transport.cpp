#include <array>
#include <cstring>

#include <gtest/gtest.h>

extern "C" {
#include <modbus/modbus.h>
#include <modbus/transport/ascii.h>
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

struct CallbackCapture {
    bool invoked = false;
    mb_err_t status = MODBUS_ERROR_NONE;
    mb_adu_view_t view{};
};

static void ascii_callback(mb_ascii_transport_t *ascii, const mb_adu_view_t *adu, mb_err_t status, void *user)
{
    (void)ascii;
    auto *capture = static_cast<CallbackCapture *>(user);
    capture->invoked = true;
    capture->status = status;
    if (adu != nullptr) {
        capture->view = *adu;
    }
}

class AsciiTransportTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        modbus_transport_t legacy_transport{};
        modbus_transport_init_mock(&legacy_transport);
        iface_ = mock_transport_get_iface();
        ASSERT_NE(nullptr, iface_);
        ASSERT_EQ(MODBUS_ERROR_NONE, mb_ascii_init(&ascii_, iface_, ascii_callback, &capture_));
        mb_ascii_set_inter_char_timeout(&ascii_, 20U);
        mock_clear_tx_buffer();
    }

    void TearDown() override
    {
        mock_clear_tx_buffer();
        capture_ = CallbackCapture{};
        mb_ascii_reset(&ascii_);
    }

    mb_ascii_transport_t ascii_{};
    CallbackCapture capture_{};
    const mb_transport_if_t *iface_ = nullptr;
};

static std::array<mb_u8, MB_ASCII_BUFFER_SIZE> encode_ascii_frame(const mb_adu_view_t &adu, mb_size_t &out_len)
{
    std::array<mb_u8, MB_ASCII_BUFFER_SIZE> frame{};
    out_len = 0U;
    const mb_err_t status = mb_frame_ascii_encode(&adu, frame.data(), frame.size(), &out_len);
    EXPECT_EQ(MODBUS_ERROR_NONE, status);
    if (status != MODBUS_ERROR_NONE) {
        frame.fill(0U);
    }
    return frame;
}

TEST_F(AsciiTransportTest, ReceivesCompleteFrame)
{
    const mb_adu_view_t adu{0x0AU, MODBUS_FUNC_READ_HOLDING_REGISTERS, nullptr, 0U};
    mb_size_t frame_len = 0U;
    const auto frame = encode_ascii_frame(adu, frame_len);

    ASSERT_EQ(0, mock_inject_rx_data(frame.data(), static_cast<uint16_t>(frame_len)));
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_ascii_poll(&ascii_));

    ASSERT_TRUE(capture_.invoked);
    EXPECT_EQ(MODBUS_ERROR_NONE, capture_.status);
    EXPECT_EQ(adu.unit_id, capture_.view.unit_id);
    EXPECT_EQ(adu.function, capture_.view.function);
    EXPECT_EQ(0U, capture_.view.payload_len);
}

TEST_F(AsciiTransportTest, DetectsLrcError)
{
    const mb_adu_view_t adu{0x11U, MODBUS_FUNC_WRITE_SINGLE_REGISTER, nullptr, 0U};
    mb_size_t frame_len = 0U;
    auto frame = encode_ascii_frame(adu, frame_len);

    ASSERT_GE(frame_len, 4U);
    frame[frame_len - 4U] ^= 0x01U; /* flip high nibble of LRC */

    ASSERT_EQ(0, mock_inject_rx_data(frame.data(), static_cast<uint16_t>(frame_len)));
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_ascii_poll(&ascii_));

    ASSERT_TRUE(capture_.invoked);
    EXPECT_EQ(MODBUS_ERROR_CRC, capture_.status);
}

TEST_F(AsciiTransportTest, SubmitSendsFrame)
{
    const mb_u8 payload_bytes[] = {0x00U, 0x0AU, 0x00U, 0x03U};
    const mb_adu_view_t adu{0x01U, MODBUS_FUNC_READ_HOLDING_REGISTERS, payload_bytes, sizeof payload_bytes};

    ASSERT_EQ(MODBUS_ERROR_NONE, mb_ascii_submit(&ascii_, &adu));

    std::array<uint8_t, MB_ASCII_BUFFER_SIZE> buffer{};
    const auto written = mock_get_tx_data(buffer.data(), static_cast<uint16_t>(buffer.size()));
    ASSERT_GT(written, 0U);

    std::array<mb_u8, MB_PDU_MAX> scratch{};
    mb_adu_view_t decoded{};
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_frame_ascii_decode(buffer.data(), written, &decoded, scratch.data(), scratch.size()));
    EXPECT_EQ(adu.unit_id, decoded.unit_id);
    EXPECT_EQ(adu.function, decoded.function);
    ASSERT_EQ(sizeof payload_bytes, decoded.payload_len);
    EXPECT_EQ(0, std::memcmp(decoded.payload, payload_bytes, decoded.payload_len));
}

TEST_F(AsciiTransportTest, TimeoutFlushesPartialFrame)
{
    const char *partial = ":0103";
    ASSERT_EQ(0, mock_inject_rx_data(reinterpret_cast<const uint8_t *>(partial), static_cast<uint16_t>(std::strlen(partial))));
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_ascii_poll(&ascii_));
    mock_advance_time(25U);
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_ascii_poll(&ascii_));

    ASSERT_TRUE(capture_.invoked);
    EXPECT_EQ(MODBUS_ERROR_TIMEOUT, capture_.status);
}

} // namespace
