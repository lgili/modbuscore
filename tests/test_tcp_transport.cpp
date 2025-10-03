#include <array>
#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

extern "C" {
#include <modbus/pdu.h>
#include <modbus/transport.h>
#include <modbus/transport/tcp.h>
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

struct TcpCallbackCapture {
    bool invoked{false};
    mb_err_t status{MB_OK};
    mb_u16 tid{0U};
    mb_adu_view_t view{};
    std::array<mb_u8, MB_PDU_MAX> payload{};
};

static void tcp_callback(mb_tcp_transport_t *tcp,
                         const mb_adu_view_t *adu,
                         mb_u16 transaction_id,
                         mb_err_t status,
                         void *user)
{
    (void)tcp;
    auto *capture = static_cast<TcpCallbackCapture *>(user);
    capture->invoked = true;
    capture->status = status;
    capture->tid = transaction_id;

    if (adu != nullptr) {
        capture->view.unit_id = adu->unit_id;
        capture->view.function = adu->function;
        capture->view.payload_len = adu->payload_len;
        capture->view.payload = (adu->payload_len > 0U) ? capture->payload.data() : nullptr;
        if (adu->payload != nullptr && adu->payload_len > 0U) {
            std::memcpy(capture->payload.data(), adu->payload, adu->payload_len);
        }
    } else {
        capture->view = {};
    }
}

class MbTcpTransportTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        modbus_transport_init_mock(&legacy_transport_);
        iface_ = mock_transport_get_iface();
        ASSERT_NE(nullptr, iface_);
        mock_clear_tx_buffer();
    }

    modbus_transport_t legacy_transport_{};
    const mb_transport_if_t *iface_{nullptr};
};

TEST_F(MbTcpTransportTest, BuildsMbapFrame)
{
    mb_tcp_transport_t tcp{};
    ASSERT_EQ(MB_OK, mb_tcp_init(&tcp, iface_, nullptr, nullptr));

    const mb_u8 payload[]{0x00U, 0x2AU, 0x00U, 0x03U};
    const mb_adu_view_t adu{0x11U, MB_PDU_FC_READ_HOLDING_REGISTERS, payload, MB_COUNTOF(payload)};

    mock_clear_tx_buffer();
    ASSERT_EQ(MB_OK, mb_tcp_submit(&tcp, &adu, 0x1234U));

    std::array<mb_u8, MB_TCP_BUFFER_SIZE> frame{};
    const auto len = mock_get_tx_data(frame.data(), static_cast<uint16_t>(frame.size()));
    ASSERT_EQ(12U, len);

    EXPECT_EQ(0x12U, frame[0]);
    EXPECT_EQ(0x34U, frame[1]);
    EXPECT_EQ(0x00U, frame[2]);
    EXPECT_EQ(0x00U, frame[3]);
    EXPECT_EQ(0x00U, frame[4]);
    EXPECT_EQ(0x06U, frame[5]); // unit id + function + payload length
    EXPECT_EQ(0x11U, frame[6]);
    EXPECT_EQ(MB_PDU_FC_READ_HOLDING_REGISTERS, frame[7]);
    ASSERT_EQ(4U, MB_COUNTOF(payload));
    for (size_t i = 0; i < MB_COUNTOF(payload); ++i) {
        EXPECT_EQ(payload[i], frame[8 + i]);
    }
}

TEST_F(MbTcpTransportTest, HandlesFragmentedFrame)
{
    mb_tcp_transport_t tcp{};
    TcpCallbackCapture capture{};
    ASSERT_EQ(MB_OK, mb_tcp_init(&tcp, iface_, tcp_callback, &capture));

    const mb_u8 payload[]{0x02U, 0x12U, 0x34U};
    const mb_u8 response_frame[]{
        0x43U, 0x21U, // TID
        0x00U, 0x00U, // protocol id
        0x00U, 0x05U, // length = unit + function + payload(3)
        0x11U,        // unit id
        MB_PDU_FC_READ_HOLDING_REGISTERS,
        payload[0], payload[1], payload[2]
    };

    ASSERT_EQ(0, mock_inject_rx_data(response_frame, 4U));
    const mb_err_t first_status = mb_tcp_poll(&tcp);
    EXPECT_TRUE(first_status == MB_OK || first_status == MB_ERR_TIMEOUT);
    EXPECT_FALSE(capture.invoked);

    ASSERT_EQ(0, mock_inject_rx_data(&response_frame[4], static_cast<uint16_t>(sizeof response_frame - 4U)));
    EXPECT_EQ(MB_OK, mb_tcp_poll(&tcp));

    EXPECT_TRUE(capture.invoked);
    EXPECT_EQ(MB_OK, capture.status);
    EXPECT_EQ(0x4321U, capture.tid);
    EXPECT_EQ(0x11U, capture.view.unit_id);
    EXPECT_EQ(MB_PDU_FC_READ_HOLDING_REGISTERS, capture.view.function);
    ASSERT_EQ(static_cast<mb_size_t>(MB_COUNTOF(payload)), capture.view.payload_len);
    for (size_t i = 0; i < MB_COUNTOF(payload); ++i) {
        EXPECT_EQ(payload[i], capture.payload[i]);
    }
}

TEST_F(MbTcpTransportTest, RejectsInvalidProtocolId)
{
    mb_tcp_transport_t tcp{};
    TcpCallbackCapture capture{};
    ASSERT_EQ(MB_OK, mb_tcp_init(&tcp, iface_, tcp_callback, &capture));

    const mb_u8 bad_frame[]{
        0x00U, 0x10U, // TID
        0x00U, 0x01U, // invalid protocol ID
        0x00U, 0x03U, // length = unit + function
        0x01U,
        MB_PDU_FC_READ_HOLDING_REGISTERS
    };

    ASSERT_EQ(0, mock_inject_rx_data(bad_frame, static_cast<uint16_t>(sizeof bad_frame)));
    EXPECT_EQ(MB_OK, mb_tcp_poll(&tcp));

    EXPECT_TRUE(capture.invoked);
    EXPECT_EQ(MB_ERR_INVALID_REQUEST, capture.status);
    EXPECT_EQ(0x0010U, capture.tid);
    EXPECT_EQ(0U, capture.view.payload_len);
}

} // namespace
