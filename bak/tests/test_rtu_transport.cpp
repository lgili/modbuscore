#include <array>
#include <cstring>
#include <limits>

#include <gtest/gtest.h>

extern "C" {
#include <modbus/modbus.h>
#include <modbus/transport/rtu.h>
#include <modbus/internal/transport_core.h>
#include <modbus/internal/core.h>
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

static void rtu_callback(mb_rtu_transport_t *rtu, const mb_adu_view_t *adu, mb_err_t status, void *user)
{
    (void)rtu;
    auto *capture = static_cast<CallbackCapture *>(user);
    capture->invoked = true;
    capture->status = status;
    if (adu != nullptr) {
        capture->view = *adu;
    }
}

class RtuTransportTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        modbus_transport_t legacy_transport{};
        modbus_transport_init_mock(&legacy_transport);
        iface_ = mock_transport_get_iface();
        ASSERT_NE(nullptr, iface_);
        ASSERT_EQ(MODBUS_ERROR_NONE, mb_rtu_init(&rtu_, iface_, rtu_callback, &capture_));
        mb_rtu_set_silence_timeout(&rtu_, 10U);
        mock_clear_tx_buffer();
    }

    void TearDown() override
    {
        mock_clear_tx_buffer();
        capture_ = CallbackCapture{};
        mb_rtu_reset(&rtu_);
    }

    mb_rtu_transport_t rtu_{};
    CallbackCapture capture_{};
    const mb_transport_if_t *iface_ = nullptr;
};

static std::array<mb_u8, MB_RTU_BUFFER_SIZE> encode_frame(const mb_adu_view_t &adu, mb_size_t &out_len)
{
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> frame{};
    out_len = 0U;
    const mb_err_t status = mb_frame_rtu_encode(&adu, frame.data(), frame.size(), &out_len);
    EXPECT_EQ(MODBUS_ERROR_NONE, status);
    if (status != MODBUS_ERROR_NONE) {
        frame.fill(0U);
    }
    return frame;
}

TEST_F(RtuTransportTest, ReceivesCompleteFrame)
{
    const mb_adu_view_t adu{0x11U, MODBUS_FUNC_READ_HOLDING_REGISTERS, nullptr, 0U};
    mb_size_t frame_len = 0U;
    const auto frame = encode_frame(adu, frame_len);

    ASSERT_EQ(0, mock_inject_rx_data(frame.data(), static_cast<uint16_t>(frame_len)));
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_rtu_poll(&rtu_));
    mock_advance_time(15U);
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_rtu_poll(&rtu_));

    ASSERT_TRUE(capture_.invoked);
    EXPECT_EQ(MODBUS_ERROR_NONE, capture_.status);
    EXPECT_EQ(adu.unit_id, capture_.view.unit_id);
    EXPECT_EQ(adu.function, capture_.view.function);
    EXPECT_EQ(0U, capture_.view.payload_len);
}

TEST_F(RtuTransportTest, DetectsCrcError)
{
    const mb_adu_view_t adu{0x22U, MODBUS_FUNC_WRITE_SINGLE_REGISTER, nullptr, 0U};
    mb_size_t frame_len = 0U;
    auto frame = encode_frame(adu, frame_len);

    ASSERT_GE(frame_len, 2U);
    frame[frame_len - 1U] ^= 0xFFU;
    ASSERT_EQ(0, mock_inject_rx_data(frame.data(), static_cast<uint16_t>(frame_len)));
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_rtu_poll(&rtu_));
    mock_advance_time(15U);
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_rtu_poll(&rtu_));

    ASSERT_TRUE(capture_.invoked);
    EXPECT_EQ(MODBUS_ERROR_CRC, capture_.status);
}

TEST_F(RtuTransportTest, SubmitSendsFrame)
{
    const mb_u8 payload_bytes[] = {0x00U, 0x0AU, 0x00U, 0x01U};
    const mb_adu_view_t adu{0x33U, MODBUS_FUNC_READ_HOLDING_REGISTERS, payload_bytes, sizeof payload_bytes};

    ASSERT_EQ(MODBUS_ERROR_NONE, mb_rtu_submit(&rtu_, &adu));

    std::array<uint8_t, MB_RTU_BUFFER_SIZE> buffer{};
    static_assert(buffer.size() <= std::numeric_limits<uint16_t>::max(),
        "RTU buffer capacity must fit into uint16_t");
    const auto written = mock_get_tx_data(buffer.data(), static_cast<uint16_t>(buffer.size()));
    ASSERT_GT(written, 0U);

    mb_adu_view_t decoded{};
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_frame_rtu_decode(buffer.data(), written, &decoded));
    EXPECT_EQ(adu.unit_id, decoded.unit_id);
    EXPECT_EQ(adu.function, decoded.function);
    ASSERT_EQ(sizeof payload_bytes, decoded.payload_len);
    EXPECT_EQ(0, std::memcmp(decoded.payload, payload_bytes, decoded.payload_len));
}

TEST_F(RtuTransportTest, OverLengthFrameTriggersError)
{
    capture_ = CallbackCapture{};
    bool overflow_seen = false;

    for (size_t i = 0; i < MB_RTU_BUFFER_SIZE + 2U; ++i) {
        const mb_u8 byte = static_cast<mb_u8>(i & 0xFFU);
        const int push = mock_inject_rx_data(&byte, 1U);
        if (push < 0) {
            overflow_seen = true;
        }

        (void)mb_rtu_poll(&rtu_);
        if (capture_.invoked) {
            EXPECT_TRUE(capture_.status == MODBUS_ERROR_INVALID_REQUEST ||
                        capture_.status == MODBUS_ERROR_CRC);
            return;
        }

        if (overflow_seen) {
            mock_advance_time(10U);
            (void)mb_rtu_poll(&rtu_);
            EXPECT_TRUE(capture_.invoked);
            EXPECT_TRUE(capture_.status == MODBUS_ERROR_INVALID_REQUEST ||
                        capture_.status == MODBUS_ERROR_CRC);
            return;
        }
    }

    FAIL() << "Overflow did not trigger callback";
}

} // namespace
