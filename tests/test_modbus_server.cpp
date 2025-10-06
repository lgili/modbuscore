#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

#include <gtest/gtest.h>

extern "C" {
#include <modbus/frame.h>
#include <modbus/observe.h>
#include <modbus/pdu.h>
#include <modbus/server.h>
#include <modbus/mapping.h>
#include <modbus/transport.h>
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

TEST(ServerMapping, NullArguments)
{
    mb_server_mapping_bank_t bank = {
        .start = 0U,
        .count = 1U,
        .storage = nullptr,
        .read_only = false,
    };

    EXPECT_EQ(MB_ERR_INVALID_ARGUMENT, mb_server_mapping_apply(nullptr, &bank, 1U));

    mb_server_t server{};
    EXPECT_EQ(MB_ERR_INVALID_ARGUMENT, mb_server_mapping_init(&server, nullptr));
}

constexpr mb_u8 kUnitId = 0x11U;

struct ServerEventRecorder {
    std::vector<mb_event_t> events;

    static void Callback(const mb_event_t *event, void *user_ctx)
    {
        if (event == nullptr || user_ctx == nullptr) {
            return;
        }
        auto *self = static_cast<ServerEventRecorder *>(user_ctx);
        self->events.push_back(*event);
    }
};

class MbServerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        modbus_transport_init_mock(&legacy_transport_);
        iface_ = mock_transport_get_iface();
        ASSERT_NE(nullptr, iface_);

        // Map holding registers 0x0010..0x0014 (5 registers) as read/write storage.
        storage_rw_ = {0x1111, 0x2222, 0x3333, 0x4444, 0x5555};

        // Map read-only block 0x0020..0x0021 (2 registers).
        storage_ro_ = {0xAAAA, 0xBBBB};

        const std::array<mb_server_mapping_bank_t, 2> banks = {{{
                                                                    .start = 0x0010U,
                                                                    .count = static_cast<mb_u16>(storage_rw_.size()),
                                                                    .storage = storage_rw_.data(),
                                                                    .read_only = false,
                                                                },
                                                                {
                                                                    .start = 0x0020U,
                                                                    .count = static_cast<mb_u16>(storage_ro_.size()),
                                                                    .storage = storage_ro_.data(),
                                                                    .read_only = true,
                                                                }}};

        const mb_server_mapping_config_t cfg = {
            .iface = iface_,
            .unit_id = kUnitId,
            .regions = regions_.data(),
            .region_capacity = regions_.size(),
            .request_pool = request_pool_.data(),
            .request_capacity = request_pool_.size(),
            .banks = banks.data(),
            .bank_count = banks.size(),
        };

        const mb_err_t err = mb_server_mapping_init(&server_, &cfg);
        ASSERT_EQ(MB_OK, err);

        mock_clear_tx_buffer();
    }

    void TearDown() override
    {
        mock_clear_tx_buffer();
        mb_server_reset(&server_);
    }

    void sendPdu(const mb_u8 *pdu, mb_size_t pdu_len, mb_u8 unit_id)
    {
        ASSERT_NE(nullptr, pdu);
        ASSERT_GT(pdu_len, 0U);

        mb_adu_view_t adu{};
        adu.unit_id = unit_id;
        adu.function = pdu[0];
        adu.payload = (pdu_len > 1U) ? &pdu[1] : nullptr;
        adu.payload_len = pdu_len - 1U;

        std::array<mb_u8, MB_RTU_BUFFER_SIZE> frame{};
        mb_size_t frame_len = 0U;
        ASSERT_EQ(MB_OK, mb_frame_rtu_encode(&adu, frame.data(), frame.size(), &frame_len));
        ASSERT_EQ(0, mock_inject_rx_data(frame.data(), static_cast<uint16_t>(frame_len)));
    }

    mb_err_t injectAdu(const mb_u8 *pdu, mb_size_t pdu_len, mb_u8 unit_id)
    {
        if (pdu == nullptr || pdu_len == 0U) {
            return MB_ERR_INVALID_ARGUMENT;
        }
        mb_adu_view_t adu{};
        adu.unit_id = unit_id;
        adu.function = pdu[0];
        adu.payload = (pdu_len > 1U) ? &pdu[1] : nullptr;
        adu.payload_len = (pdu_len > 0U) ? (pdu_len - 1U) : 0U;
        return mb_server_inject_adu(&server_, &adu);
    }

    void pump(unsigned iterations = 8)
    {
        for (unsigned i = 0; i < iterations; ++i) {
            mock_advance_time(1U);
            (void)mb_server_poll(&server_);
        }
    }

    void step()
    {
        mock_advance_time(1U);
        (void)mb_server_poll(&server_);
    }

    bool fetchResponse(mb_adu_view_t &out)
    {
        last_frame_len_ = mock_get_tx_data(last_frame_.data(), last_frame_.size());
        if (last_frame_len_ == 0U) {
            return false;
        }
        const bool ok = mb_err_is_ok(mb_frame_rtu_decode(last_frame_.data(), last_frame_len_, &out));
        mock_clear_tx_buffer();
        return ok;
    }

    mb_server_t server_{};
    std::array<mb_server_region_t, 8> regions_{};
    std::array<mb_server_request_t, 4> request_pool_{};
    std::array<mb_u16, 5> storage_rw_{};
    std::array<mb_u16, 2> storage_ro_{};
    const mb_transport_if_t *iface_ = nullptr;
    modbus_transport_t legacy_transport_{};
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> last_frame_{};
    uint16_t last_frame_len_ = 0U;
};

TEST_F(MbServerTest, ServesReadHoldingRegisters)
{
    mb_u8 req_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_read_holding_request(req_pdu, sizeof req_pdu, 0x0011U, 0x0003U));

    ASSERT_EQ(storage_rw_[3], static_cast<mb_u16>(0x4444U));

    sendPdu(req_pdu, 5U, kUnitId);
    pump(16);

    EXPECT_EQ(storage_rw_[3], static_cast<mb_u16>(0x4444U));

    mb_adu_view_t response{};
    ASSERT_TRUE(fetchResponse(response));
    EXPECT_EQ(response.unit_id, kUnitId);
    EXPECT_EQ(response.function, MB_PDU_FC_READ_HOLDING_REGISTERS);
    ASSERT_EQ(response.payload_len, static_cast<mb_size_t>(1U + 3U * 2U));
    EXPECT_EQ(response.payload[0], 6U);
    ASSERT_GE(last_frame_len_, static_cast<uint16_t>(9U));
    EXPECT_EQ(last_frame_[0], kUnitId);
    EXPECT_EQ(last_frame_[1], MB_PDU_FC_READ_HOLDING_REGISTERS);
    EXPECT_EQ(last_frame_[2], 6U);
    EXPECT_EQ(last_frame_[3], 0x22U);
    EXPECT_EQ(last_frame_[4], 0x22U);
    EXPECT_EQ(last_frame_[5], 0x33U);
    EXPECT_EQ(last_frame_[6], 0x33U);
    EXPECT_EQ(last_frame_[7], 0x44U);
    EXPECT_EQ(last_frame_[8], 0x44U);
    EXPECT_EQ((response.payload[1] << 8) | response.payload[2], 0x2222U);
    EXPECT_EQ((response.payload[3] << 8) | response.payload[4], 0x3333U);
    EXPECT_EQ((response.payload[5] << 8) | response.payload[6], 0x4444U);
}

TEST_F(MbServerTest, WritesSingleRegister)
{
    mb_u8 req_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_write_single_request(req_pdu, sizeof req_pdu, 0x0010U, 0xABCDU));

    sendPdu(req_pdu, 5U, kUnitId);
    pump(16);

    mb_adu_view_t response{};
    ASSERT_TRUE(fetchResponse(response));
    EXPECT_EQ(response.function, MB_PDU_FC_WRITE_SINGLE_REGISTER);
    ASSERT_EQ(response.payload_len, 4U);
    EXPECT_EQ(storage_rw_[0], 0xABCDU);
}

TEST_F(MbServerTest, RejectsWriteToReadOnlyRegion)
{
    mb_u8 req_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_write_single_request(req_pdu, sizeof req_pdu, 0x0020U, 0x0F0FU));

    sendPdu(req_pdu, 5U, kUnitId);
    pump(16);

    mb_adu_view_t response{};
    ASSERT_TRUE(fetchResponse(response));
    EXPECT_EQ(response.function, MB_PDU_FC_WRITE_SINGLE_REGISTER | MB_PDU_EXCEPTION_BIT);
    ASSERT_EQ(response.payload_len, 1U);
    EXPECT_EQ(response.payload[0], MB_EX_ILLEGAL_DATA_ADDRESS);
    EXPECT_EQ(storage_ro_[0], 0xAAAAU);
}

TEST_F(MbServerTest, WritesMultipleRegisters)
{
    std::array<mb_u16, 3> new_values{0x0102U, 0x0304U, 0x0506U};

    mb_u8 req_pdu[MB_PDU_MAX];
    ASSERT_EQ(MB_OK, mb_pdu_build_write_multiple_request(req_pdu, sizeof req_pdu, 0x0011U, new_values.data(), new_values.size()));

    const mb_size_t pdu_len = 6U + new_values.size() * 2U;
    sendPdu(req_pdu, pdu_len, kUnitId);
    pump(16);

    mb_adu_view_t response{};
    ASSERT_TRUE(fetchResponse(response));
    EXPECT_EQ(response.function, MB_PDU_FC_WRITE_MULTIPLE_REGISTERS);
    ASSERT_EQ(response.payload_len, 4U);
    EXPECT_EQ(storage_rw_[1], 0x0102U);
    EXPECT_EQ(storage_rw_[2], 0x0304U);
    EXPECT_EQ(storage_rw_[3], 0x0506U);
}

TEST_F(MbServerTest, BroadcastWriteDoesNotRespond)
{
    mb_u8 req_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_write_single_request(req_pdu, sizeof req_pdu, 0x0010U, 0x9999U));

    sendPdu(req_pdu, 5U, 0U);
    pump(16);

    mb_adu_view_t response{};
    EXPECT_FALSE(fetchResponse(response));
    EXPECT_EQ(storage_rw_[0], 0x9999U);
}

TEST_F(MbServerTest, IgnoresRequestsForDifferentUnit)
{
    mb_u8 req_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_read_holding_request(req_pdu, sizeof req_pdu, 0x0010U, 1U));

    sendPdu(req_pdu, 5U, 0x22U);
    pump(16);

    mb_adu_view_t response{};
    EXPECT_FALSE(fetchResponse(response));
}

TEST_F(MbServerTest, UnsupportedFunctionRaisesException)
{
    mb_u8 dummy_pdu[3] = {0x45U, 0x00U, 0x01U};
    sendPdu(dummy_pdu, 3U, kUnitId);
    pump();

    mb_adu_view_t response{};
    ASSERT_TRUE(fetchResponse(response));
    EXPECT_EQ(response.function, 0x45U | MB_PDU_EXCEPTION_BIT);
    ASSERT_EQ(response.payload_len, 1U);
    EXPECT_EQ(response.payload[0], MB_EX_ILLEGAL_FUNCTION);
}

TEST_F(MbServerTest, BackpressureLimitsQueue)
{
    mb_server_reset_metrics(&server_);
    mb_server_set_queue_capacity(&server_, 1U);

    mb_u8 req_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_read_holding_request(req_pdu, sizeof req_pdu, 0x0010U, 0x0001U));

    EXPECT_EQ(0U, mb_server_pending(&server_));
    EXPECT_EQ(0U, server_.pending_count);
    EXPECT_EQ(nullptr, server_.current);
    EXPECT_EQ(MB_OK, injectAdu(req_pdu, 5U, kUnitId));
    EXPECT_EQ(MB_ERR_NO_RESOURCES, injectAdu(req_pdu, 5U, kUnitId));

    step();

    mb_server_metrics_t metrics{};
    mb_server_get_metrics(&server_, &metrics);
    EXPECT_EQ(1U, metrics.received);
    EXPECT_EQ(1U, metrics.responded);
    EXPECT_EQ(1U, metrics.dropped);
    EXPECT_GE(metrics.exceptions, 1U);
}

TEST_F(MbServerTest, HighPriorityWriteBypassesReads)
{
    mb_server_reset_metrics(&server_);

    mb_u8 read_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_read_holding_request(read_pdu, sizeof read_pdu, 0x0010U, 0x0001U));

    mb_u8 write_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_write_single_request(write_pdu, sizeof write_pdu, 0x0010U, 0x1234U));

    EXPECT_EQ(0U, mb_server_pending(&server_));
    EXPECT_EQ(0U, server_.pending_count);
    EXPECT_EQ(nullptr, server_.current);
    EXPECT_EQ(MB_OK, injectAdu(read_pdu, 5U, kUnitId));
    EXPECT_EQ(MB_OK, injectAdu(write_pdu, 5U, kUnitId));

    step();
    step();

    EXPECT_EQ(static_cast<mb_u16>(0x1234U), storage_rw_[0]);

    mb_server_metrics_t metrics{};
    mb_server_get_metrics(&server_, &metrics);
    EXPECT_EQ(2U, metrics.received);
    EXPECT_EQ(2U, metrics.responded);
    EXPECT_EQ(0U, metrics.dropped);
}

TEST_F(MbServerTest, FcTimeoutDropsStaleRequests)
{
    mb_server_reset_metrics(&server_);
    mb_server_set_fc_timeout(&server_, MB_PDU_FC_READ_HOLDING_REGISTERS, 2U);

    mb_u8 req_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_read_holding_request(req_pdu, sizeof req_pdu, 0x0010U, 0x0001U));

    EXPECT_EQ(0U, mb_server_pending(&server_));
    EXPECT_EQ(0U, server_.pending_count);
    EXPECT_EQ(nullptr, server_.current);
    EXPECT_EQ(MB_OK, injectAdu(req_pdu, 5U, kUnitId));
    EXPECT_EQ(MB_OK, injectAdu(req_pdu, 5U, kUnitId));

    step();
    mock_advance_time(10U);
    step();

    mb_server_metrics_t metrics{};
    mb_server_get_metrics(&server_, &metrics);
    EXPECT_EQ(2U, metrics.received);
    EXPECT_EQ(1U, metrics.responded);
    EXPECT_EQ(1U, metrics.timeouts);
    EXPECT_EQ(1U, metrics.dropped);
    EXPECT_GE(metrics.exceptions, 1U);
}

TEST_F(MbServerTest, PoisonFlushesQueue)
{
    mb_server_reset_metrics(&server_);

    mb_u8 req_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_read_holding_request(req_pdu, sizeof req_pdu, 0x0010U, 0x0001U));

    EXPECT_EQ(0U, mb_server_pending(&server_));
    EXPECT_EQ(0U, server_.pending_count);
    EXPECT_EQ(nullptr, server_.current);
    EXPECT_EQ(MB_OK, injectAdu(req_pdu, 5U, kUnitId));
    EXPECT_EQ(MB_OK, injectAdu(req_pdu, 5U, kUnitId));

    step();
    ASSERT_EQ(MB_OK, mb_server_submit_poison(&server_));
    step();

    mb_server_metrics_t metrics{};
    mb_server_get_metrics(&server_, &metrics);
    EXPECT_EQ(2U, metrics.received);
    EXPECT_EQ(1U, metrics.responded);
    EXPECT_EQ(1U, metrics.poison_triggers);
    EXPECT_GE(metrics.exceptions, 1U);
    EXPECT_TRUE(mb_server_is_idle(&server_));
}

TEST_F(MbServerTest, MetricsResetClearsCounters)
{
    mb_server_reset_metrics(&server_);

    mb_u8 req_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_read_holding_request(req_pdu, sizeof req_pdu, 0x0010U, 0x0001U));

    sendPdu(req_pdu, 5U, kUnitId);
    pump();

    mb_server_metrics_t metrics{};
    mb_server_get_metrics(&server_, &metrics);
    EXPECT_EQ(1U, metrics.received);
    EXPECT_EQ(1U, metrics.responded);

    mb_server_reset_metrics(&server_);
    mb_server_get_metrics(&server_, &metrics);
    EXPECT_EQ(0U, metrics.received);
    EXPECT_EQ(0U, metrics.responded);
    EXPECT_EQ(0U, metrics.exceptions);
}

TEST_F(MbServerTest, DiagnosticsAccumulateCounts)
{
    mb_diag_counters_t diag{};
    mb_server_get_diag(&server_, &diag);
    EXPECT_EQ(0U, diag.function[MB_PDU_FC_READ_HOLDING_REGISTERS]);
    EXPECT_EQ(0U, diag.error[MB_DIAG_ERR_SLOT_OK]);

    mb_u8 req_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_read_holding_request(req_pdu, sizeof req_pdu, 0x0010U, 0x0001U));
    sendPdu(req_pdu, 5U, kUnitId);
    pump(8);

    mb_server_get_diag(&server_, &diag);
    EXPECT_EQ(1U, diag.function[MB_PDU_FC_READ_HOLDING_REGISTERS]);
    EXPECT_EQ(1U, diag.error[MB_DIAG_ERR_SLOT_OK]);

    ASSERT_EQ(MB_OK, mb_pdu_build_read_holding_request(req_pdu, sizeof req_pdu, 0x00F0U, 0x0001U));
    sendPdu(req_pdu, 5U, kUnitId);
    pump(8);

    mb_server_get_diag(&server_, &diag);
    EXPECT_EQ(2U, diag.function[MB_PDU_FC_READ_HOLDING_REGISTERS]);
    EXPECT_EQ(1U, diag.error[MB_DIAG_ERR_SLOT_OK]);
    EXPECT_EQ(1U, diag.error[MB_DIAG_ERR_SLOT_EXCEPTION_ILLEGAL_DATA_ADDRESS]);

    mb_server_reset_diag(&server_);
    mb_server_get_diag(&server_, &diag);
    EXPECT_EQ(0U, diag.function[MB_PDU_FC_READ_HOLDING_REGISTERS]);
    EXPECT_EQ(0U, diag.error[MB_DIAG_ERR_SLOT_OK]);
    EXPECT_EQ(0U, diag.error[MB_DIAG_ERR_SLOT_EXCEPTION_ILLEGAL_DATA_ADDRESS]);
}

TEST_F(MbServerTest, EmitsObservabilityEvents)
{
    ServerEventRecorder recorder;
    mb_server_set_event_callback(&server_, ServerEventRecorder::Callback, &recorder);

    mb_u8 req_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_read_holding_request(req_pdu, sizeof req_pdu, 0x0010U, 0x0001U));
    sendPdu(req_pdu, 5U, kUnitId);
    pump(8);

    ASSERT_GE(recorder.events.size(), static_cast<size_t>(6));
    EXPECT_EQ(MB_EVENT_SOURCE_SERVER, recorder.events.front().source);
    EXPECT_EQ(MB_EVENT_SERVER_STATE_ENTER, recorder.events.front().type);
    EXPECT_EQ(MB_SERVER_STATE_IDLE, recorder.events.front().data.server_state.state);

    const auto accept_it = std::find_if(recorder.events.begin(), recorder.events.end(), [](const mb_event_t &evt) {
        return evt.type == MB_EVENT_SERVER_REQUEST_ACCEPT;
    });
    ASSERT_NE(recorder.events.end(), accept_it);
    EXPECT_EQ(MB_PDU_FC_READ_HOLDING_REGISTERS, accept_it->data.server_req.function);
    EXPECT_FALSE(accept_it->data.server_req.broadcast);
    EXPECT_EQ(MB_OK, accept_it->data.server_req.status);

    const auto complete_it = std::find_if(recorder.events.begin(), recorder.events.end(), [](const mb_event_t &evt) {
        return evt.type == MB_EVENT_SERVER_REQUEST_COMPLETE;
    });
    ASSERT_NE(recorder.events.end(), complete_it);
    EXPECT_EQ(MB_OK, complete_it->data.server_req.status);

    bool saw_processing_enter = false;
    bool saw_processing_exit = false;
    bool saw_idle_enter_after = false;
    for (size_t i = 0; i < recorder.events.size(); ++i) {
        const mb_event_t &evt = recorder.events[i];
        if (evt.type == MB_EVENT_SERVER_STATE_ENTER && evt.data.server_state.state == MB_SERVER_STATE_PROCESSING) {
            saw_processing_enter = true;
        } else if (evt.type == MB_EVENT_SERVER_STATE_EXIT && evt.data.server_state.state == MB_SERVER_STATE_PROCESSING) {
            saw_processing_exit = true;
        } else if (evt.type == MB_EVENT_SERVER_STATE_ENTER && evt.data.server_state.state == MB_SERVER_STATE_IDLE && i != 0U) {
            saw_idle_enter_after = true;
        }
    }

    EXPECT_TRUE(saw_processing_enter);
    EXPECT_TRUE(saw_processing_exit);
    EXPECT_TRUE(saw_idle_enter_after);
}

} // namespace
