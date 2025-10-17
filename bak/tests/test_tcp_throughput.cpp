#include <vector>

#include <gtest/gtest.h>

extern "C" {
#include <modbus/internal/pdu.h>
#include <modbus/transport/tcp.h>
}

#include "tcp_test_utils.h"

using modbus::test::TestTcpChannel;
using modbus::test::channel_advance_time;
using modbus::test::channel_push_rx;
using modbus::test::channel_reset;
using modbus::test::channel_take_tx;
using modbus::test::make_transport;

namespace {

struct ThroughputCapture {
    mb_u32 success{0U};
    mb_err_t status{MB_OK};
};

static void tcp_callback(mb_tcp_transport_t *tcp,
                         const mb_adu_view_t *adu,
                         mb_u16 transaction_id,
                         mb_err_t status,
                         void *user)
{
    (void)tcp;
    (void)adu;
    (void)transaction_id;
    auto *capture = static_cast<ThroughputCapture *>(user);
    if (!capture) {
        return;
    }

    if (status == MB_OK) {
        capture->success += 1U;
    } else {
        capture->status = status;
    }
}

std::vector<mb_u8> build_read_response(mb_u16 tid,
                                       mb_u8 unit_id,
                                       std::initializer_list<mb_u8> payload)
{
    const mb_u16 length_field = static_cast<mb_u16>(payload.size() + 2U);
    std::vector<mb_u8> frame;
    frame.reserve(7U + payload.size());

    frame.push_back(static_cast<mb_u8>((tid >> 8) & 0xFFU));
    frame.push_back(static_cast<mb_u8>(tid & 0xFFU));
    frame.push_back(0x00U);
    frame.push_back(0x00U);
    frame.push_back(static_cast<mb_u8>((length_field >> 8) & 0xFFU));
    frame.push_back(static_cast<mb_u8>(length_field & 0xFFU));
    frame.push_back(unit_id);
    frame.push_back(MB_PDU_FC_READ_HOLDING_REGISTERS);
    for (mb_u8 value : payload) {
        frame.push_back(value);
    }

    return frame;
}

TEST(MbTcpTransportThroughputTest, ProcessesThousandResponsesWithinSimulatedSecond)
{
    TestTcpChannel channel;
    mb_transport_if_t iface = make_transport(channel);

    mb_tcp_transport_t tcp;
    ThroughputCapture capture{};
    ASSERT_EQ(MB_OK, mb_tcp_init(&tcp, &iface, tcp_callback, &capture));

    constexpr mb_size_t kRequests = 1000U;
    const mb_u8 payload[]{0x00U, 0x02U};
    const mb_adu_view_t request{0x01U, MB_PDU_FC_READ_HOLDING_REGISTERS, payload, MB_COUNTOF(payload)};

    channel_reset(channel);

    for (mb_size_t i = 0U; i < kRequests; ++i) {
        const mb_u16 tid = static_cast<mb_u16>(i + 1U);
        ASSERT_EQ(MB_OK, mb_tcp_submit(&tcp, &request, tid));
        (void)channel_take_tx(channel); // Discard outbound frame for the test harness.

        const auto response = build_read_response(tid, 0x01U, {0x04U, 0x00U, 0x64U, 0x00U, 0x65U});
        channel_push_rx(channel, response);

        EXPECT_EQ(MB_OK, mb_tcp_poll(&tcp));
        channel_advance_time(channel, 1U);
    }

    EXPECT_EQ(kRequests, capture.success);
    EXPECT_EQ(MB_OK, capture.status);
    EXPECT_LE(channel.now, kRequests);
}

} // namespace
