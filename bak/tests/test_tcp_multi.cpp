#include <cstring>
#include <vector>

#include <gtest/gtest.h>

extern "C" {
#include <modbus/internal/pdu.h>
#include <modbus/transport/tcp_multi.h>
}

#include "tcp_test_utils.h"

using modbus::test::TestTcpChannel;
using modbus::test::channel_advance_time;
using modbus::test::channel_push_rx;
using modbus::test::channel_take_tx;
using modbus::test::make_transport;

namespace {

struct MultiCapture {
    struct Event {
        mb_size_t slot{0U};
        mb_u16 tid{0U};
        mb_err_t status{MB_OK};
        mb_u8 unit_id{0U};
        mb_u8 function{0U};
        std::vector<mb_u8> payload;
    };

    std::vector<Event> events;

    void clear() { events.clear(); }
};

void multi_callback(mb_tcp_multi_transport_t *multi,
                    mb_size_t slot_index,
                    const mb_adu_view_t *adu,
                    mb_u16 transaction_id,
                    mb_err_t status,
                    void *user)
{
    (void)multi;
    auto *capture = static_cast<MultiCapture *>(user);
    MultiCapture::Event event{};
    event.slot = slot_index;
    event.tid = transaction_id;
    event.status = status;

    if (adu) {
        event.unit_id = adu->unit_id;
        event.function = adu->function;
        if (adu->payload && adu->payload_len > 0U) {
            event.payload.assign(adu->payload, adu->payload + adu->payload_len);
        }
    }

    capture->events.push_back(std::move(event));
}

std::vector<mb_u8> build_mbap_response(mb_u16 tid,
                                       mb_u8 unit_id,
                                       mb_u8 function,
                                       std::initializer_list<mb_u8> payload)
{
    const mb_u16 length_field = static_cast<mb_u16>(1U + payload.size() + 1U);
    std::vector<mb_u8> frame;
    frame.reserve(7U + payload.size());

    frame.push_back(static_cast<mb_u8>((tid >> 8) & 0xFFU));
    frame.push_back(static_cast<mb_u8>(tid & 0xFFU));
    frame.push_back(0x00U);
    frame.push_back(0x00U);
    frame.push_back(static_cast<mb_u8>((length_field >> 8) & 0xFFU));
    frame.push_back(static_cast<mb_u8>(length_field & 0xFFU));
    frame.push_back(unit_id);
    frame.push_back(function);
    for (mb_u8 value : payload) {
        frame.push_back(value);
    }

    return frame;
}

TEST(MbTcpMultiTransportTest, HandlesMultipleSlotsIndependently)
{
    mb_tcp_multi_transport_t multi;
    MultiCapture capture{};
    ASSERT_EQ(MB_OK, mb_tcp_multi_init(&multi, multi_callback, &capture));

    TestTcpChannel channel_a;
    TestTcpChannel channel_b;
    mb_transport_if_t iface_a = make_transport(channel_a);
    mb_transport_if_t iface_b = make_transport(channel_b);

    mb_size_t slot_a = 0U;
    mb_size_t slot_b = 0U;
    ASSERT_EQ(MB_OK, mb_tcp_multi_add(&multi, &iface_a, &slot_a));
    ASSERT_EQ(MB_OK, mb_tcp_multi_add(&multi, &iface_b, &slot_b));
    EXPECT_NE(slot_a, slot_b);
    EXPECT_EQ(2U, mb_tcp_multi_active_count(&multi));

    const mb_u8 payload[]{0x00U, 0x04U};
    const mb_adu_view_t request{0x01U, MB_PDU_FC_READ_HOLDING_REGISTERS, payload, MB_COUNTOF(payload)};

    ASSERT_EQ(MB_OK, mb_tcp_multi_submit(&multi, slot_a, &request, 0x1001U));
    ASSERT_EQ(MB_OK, mb_tcp_multi_submit(&multi, slot_b, &request, 0x2002U));

    auto sent_a = channel_take_tx(channel_a);
    auto sent_b = channel_take_tx(channel_b);
    EXPECT_FALSE(sent_a.empty());
    EXPECT_FALSE(sent_b.empty());

    const auto response_a = build_mbap_response(0x1001U, 0x01U, MB_PDU_FC_READ_HOLDING_REGISTERS, {0x02U, 0x12U, 0x34U});
    const auto response_b = build_mbap_response(0x2002U, 0x01U, MB_PDU_FC_READ_HOLDING_REGISTERS, {0x02U, 0x56U, 0x78U});

    channel_push_rx(channel_a, response_a);
    channel_push_rx(channel_b, response_b);

    EXPECT_EQ(MB_OK, mb_tcp_multi_poll_all(&multi));

    ASSERT_EQ(2U, capture.events.size());
    EXPECT_EQ(slot_a, capture.events[0].slot);
    EXPECT_EQ(0x1001U, capture.events[0].tid);
    EXPECT_EQ(MB_OK, capture.events[0].status);
    EXPECT_EQ(0x01U, capture.events[0].unit_id);
    EXPECT_EQ(MB_PDU_FC_READ_HOLDING_REGISTERS, capture.events[0].function);
    ASSERT_EQ(3U, capture.events[0].payload.size());
    EXPECT_EQ(0x02U, capture.events[0].payload[0]);
    EXPECT_EQ(0x12U, capture.events[0].payload[1]);
    EXPECT_EQ(0x34U, capture.events[0].payload[2]);

    EXPECT_EQ(slot_b, capture.events[1].slot);
    EXPECT_EQ(0x2002U, capture.events[1].tid);
    EXPECT_EQ(MB_OK, capture.events[1].status);
    EXPECT_EQ(0x01U, capture.events[1].unit_id);
    EXPECT_EQ(MB_PDU_FC_READ_HOLDING_REGISTERS, capture.events[1].function);
    ASSERT_EQ(3U, capture.events[1].payload.size());
    EXPECT_EQ(0x02U, capture.events[1].payload[0]);
    EXPECT_EQ(0x56U, capture.events[1].payload[1]);
    EXPECT_EQ(0x78U, capture.events[1].payload[2]);

    capture.clear();
    channel_push_rx(channel_a, response_a);
    EXPECT_EQ(MB_OK, mb_tcp_multi_poll(&multi, slot_a));
    ASSERT_EQ(1U, capture.events.size());
    EXPECT_EQ(slot_a, capture.events[0].slot);
    EXPECT_EQ(0x1001U, capture.events[0].tid);
    ASSERT_EQ(3U, capture.events[0].payload.size());
    EXPECT_EQ(0x02U, capture.events[0].payload[0]);
    EXPECT_EQ(0x12U, capture.events[0].payload[1]);
    EXPECT_EQ(0x34U, capture.events[0].payload[2]);
}

TEST(MbTcpMultiTransportTest, RejectsUnknownSlotsAndSupportsRemoval)
{
    mb_tcp_multi_transport_t multi;
    ASSERT_EQ(MB_OK, mb_tcp_multi_init(&multi, nullptr, nullptr));

    TestTcpChannel channel;
    mb_transport_if_t iface = make_transport(channel);

    mb_size_t slot = 0U;
    ASSERT_EQ(MB_OK, mb_tcp_multi_add(&multi, &iface, &slot));
    EXPECT_TRUE(mb_tcp_multi_is_active(&multi, slot));

    EXPECT_EQ(MB_OK, mb_tcp_multi_remove(&multi, slot));
    EXPECT_FALSE(mb_tcp_multi_is_active(&multi, slot));
    EXPECT_EQ(0U, mb_tcp_multi_active_count(&multi));

    const mb_u8 payload[]{0x00U, 0x01U};
    const mb_adu_view_t request{0x01U, MB_PDU_FC_READ_HOLDING_REGISTERS, payload, MB_COUNTOF(payload)};

    EXPECT_EQ(MB_ERR_INVALID_ARGUMENT, mb_tcp_multi_submit(&multi, slot, &request, 0x1234U));
    EXPECT_EQ(MB_ERR_INVALID_ARGUMENT, mb_tcp_multi_poll(&multi, slot));
    EXPECT_EQ(MB_ERR_INVALID_ARGUMENT, mb_tcp_multi_remove(&multi, slot));
}

} // namespace
