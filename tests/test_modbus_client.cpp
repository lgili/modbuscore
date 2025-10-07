#include <algorithm>
#include <array>
#include <vector>

#include <gtest/gtest.h>

extern "C" {
#include <modbus/client.h>
#include <modbus/core.h>
#include <modbus/observe.h>
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

struct ClientEventRecorder {
    std::vector<mb_event_t> events;

    static void Callback(const mb_event_t *event, void *user_ctx)
    {
        if (event == nullptr || user_ctx == nullptr) {
            return;
        }
        auto *self = static_cast<ClientEventRecorder *>(user_ctx);
        self->events.push_back(*event);
    }
};

struct CallbackCapture {
    bool invoked = false;
    mb_err_t status = MODBUS_ERROR_NONE;
    mb_adu_view_t response{};
};

static void client_callback(mb_client_t *client,
                            const mb_client_txn_t *txn,
                            mb_err_t status,
                            const mb_adu_view_t *response,
                            void *user_ctx)
{
    (void)client;
    (void)txn;
    auto *capture = static_cast<CallbackCapture *>(user_ctx);
    capture->invoked = true;
    capture->status = status;
    if (response != nullptr) {
        capture->response = *response;
    }
}

class MbClientTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        modbus_transport_init_mock(&legacy_transport_);
        iface_ = mock_transport_get_iface();
        ASSERT_NE(nullptr, iface_);
        ASSERT_EQ(MODBUS_ERROR_NONE,
                  mb_client_init(&client_, iface_, txn_pool_.data(), txn_pool_.size()));
        mb_client_set_watchdog(&client_, 200U);
        mock_clear_tx_buffer();
    }

    void TearDown() override
    {
        mock_clear_tx_buffer();
    }

    static mb_client_request_t make_request(uint16_t start, uint16_t qty, CallbackCapture *cap)
    {
        current_payload_[0] = static_cast<mb_u8>(start >> 8);
        current_payload_[1] = static_cast<mb_u8>(start & 0xFFU);
        current_payload_[2] = static_cast<mb_u8>(qty >> 8);
        current_payload_[3] = static_cast<mb_u8>(qty & 0xFFU);

        mb_client_request_t req{};
        req.request.unit_id = 0x11U;
        req.request.function = MODBUS_FUNC_READ_HOLDING_REGISTERS;
        req.flags = 0U;
        req.request.payload = current_payload_;
        req.request.payload_len = 4U;
        req.timeout_ms = 50U;
        req.retry_backoff_ms = 25U;
        req.max_retries = 0U;
        req.callback = client_callback;
        req.user_ctx = cap;
        return req;
    }

    static void build_read_response(mb_adu_view_t &adu,
                                    std::array<mb_u8, MB_RTU_BUFFER_SIZE> &frame,
                                    mb_size_t &frame_len,
                                    uint16_t quantity)
    {
        mb_u8 pdu[MB_PDU_MAX];
        const mb_size_t payload_len = (mb_size_t)(quantity * 2U + 1U);
        pdu[0] = static_cast<mb_u8>(quantity * 2U);
        for (uint16_t i = 0; i < quantity; ++i) {
            const uint16_t value = static_cast<uint16_t>(0x0102U + (i * 0x1111U));
            pdu[1 + i * 2U] = static_cast<mb_u8>(value >> 8);
            pdu[2 + i * 2U] = static_cast<mb_u8>(value & 0xFFU);
        }

        adu.unit_id = 0x11U;
        adu.function = MODBUS_FUNC_READ_HOLDING_REGISTERS;
        adu.payload = pdu;
        adu.payload_len = payload_len;
        ASSERT_EQ(MODBUS_ERROR_NONE, mb_frame_rtu_encode(&adu, frame.data(), frame.size(), &frame_len));
    }

    static mb_u8 current_payload_[4];

    mb_client_t client_{};
    std::array<mb_client_txn_t, 4> txn_pool_{};
    modbus_transport_t legacy_transport_{};
    const mb_transport_if_t *iface_ = nullptr;
};

mb_u8 MbClientTest::current_payload_[4] = {0};

TEST_F(MbClientTest, CompletesSingleTransaction)
{
    CallbackCapture capture{};
    mb_client_txn_t *txn = nullptr;
    auto request = make_request(0x0000, 0x0002, &capture);
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_submit(&client_, &request, &txn));

    (void)mb_client_poll(&client_);
    std::array<uint8_t, MB_RTU_BUFFER_SIZE> tx_frame{};
    const auto tx_len = mock_get_tx_data(tx_frame.data(), tx_frame.size());
    ASSERT_GT(tx_len, 0U);

    mb_adu_view_t response_adu{};
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> response_frame{};
    mb_size_t response_len = 0U;
    build_read_response(response_adu, response_frame, response_len, 2U);

    ASSERT_EQ(0, mock_inject_rx_data(response_frame.data(), (uint16_t)response_len));
    for (int i = 0; i < 8 && !capture.invoked; ++i) {
        (void)mb_client_poll(&client_);
        mock_advance_time(5U);
    }

    EXPECT_TRUE(capture.invoked);
    EXPECT_EQ(MB_OK, capture.status);
    ASSERT_EQ(5U, capture.response.payload_len);
    EXPECT_EQ(4U, capture.response.payload[0]);
}

TEST_F(MbClientTest, PollWithBudgetAdvancesMicroSteps)
{
    CallbackCapture capture{};
    mb_client_txn_t *txn = nullptr;
    auto request = make_request(0x0000, 0x0001, &capture);
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_submit(&client_, &request, &txn));

    std::array<mb_u8, MB_RTU_BUFFER_SIZE> frame{};

    mock_advance_time(1U);
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_client_poll_with_budget(&client_, 1U));
    EXPECT_EQ(0U, mock_get_tx_data(frame.data(), frame.size()));

    mock_advance_time(1U);
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_client_poll_with_budget(&client_, 1U));
    EXPECT_EQ(0U, mock_get_tx_data(frame.data(), frame.size()));

    mock_advance_time(1U);
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_client_poll_with_budget(&client_, 1U));
    auto tx_len = mock_get_tx_data(frame.data(), frame.size());
    ASSERT_GT(tx_len, 0U);
    mock_clear_tx_buffer();

    mb_adu_view_t response_adu{};
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> response_frame{};
    mb_size_t response_len = 0U;
    build_read_response(response_adu, response_frame, response_len, 1U);
    ASSERT_EQ(0, mock_inject_rx_data(response_frame.data(), static_cast<uint16_t>(response_len)));

    for (int i = 0; i < 3; ++i) {
        mock_advance_time(1U);
        EXPECT_EQ(MODBUS_ERROR_NONE, mb_client_poll_with_budget(&client_, 1U));
        EXPECT_FALSE(capture.invoked);
    }

    mock_advance_time(1U);
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_client_poll_with_budget(&client_, 1U));
    EXPECT_TRUE(capture.invoked);
    EXPECT_EQ(MODBUS_ERROR_NONE, capture.status);
    ASSERT_EQ(3U, capture.response.payload_len);
    EXPECT_EQ(2U, capture.response.payload[0]);
}

TEST_F(MbClientTest, RetriesAndTimesOut)
{
    CallbackCapture capture{};
    mb_client_txn_t *txn = nullptr;
    auto request = make_request(0x0000, 0x0001, &capture);
    request.timeout_ms = 10U;
    request.retry_backoff_ms = 20U;
    request.max_retries = 1U;
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_submit(&client_, &request, &txn));

    (void)mb_client_poll(&client_);
    std::array<uint8_t, MB_RTU_BUFFER_SIZE> frame{};
    auto first = mock_get_tx_data(frame.data(), frame.size());
    ASSERT_GT(first, 0U);
    mock_clear_tx_buffer();

    mock_advance_time(11U);
    (void)mb_client_poll(&client_);
    EXPECT_EQ(0U, mock_get_tx_data(frame.data(), frame.size()));
    EXPECT_FALSE(capture.invoked);

    mock_advance_time(21U);
    (void)mb_client_poll(&client_);
    auto second = mock_get_tx_data(frame.data(), frame.size());
    ASSERT_GT(second, 0U);
    mock_clear_tx_buffer();

    mock_advance_time(19U);
    (void)mb_client_poll(&client_);
    EXPECT_FALSE(capture.invoked);

    mock_advance_time(2U);
    (void)mb_client_poll(&client_);

    EXPECT_TRUE(capture.invoked);
    EXPECT_EQ(MODBUS_ERROR_TIMEOUT, capture.status);
}

TEST_F(MbClientTest, QueuesMultipleTransactions)
{
    CallbackCapture first{};
    CallbackCapture second{};
    mb_client_txn_t *first_txn = nullptr;
    mb_client_txn_t *second_txn = nullptr;

    auto req1 = make_request(0x0000, 0x0001, &first);
    auto req2 = make_request(0x0004, 0x0001, &second);

    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_submit(&client_, &req1, &first_txn));
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_submit(&client_, &req2, &second_txn));

    (void)mb_client_poll(&client_);

    std::array<mb_u8, MB_RTU_BUFFER_SIZE> frame{};
    auto len = mock_get_tx_data(frame.data(), frame.size());
    ASSERT_GT(len, 0U);

    mb_adu_view_t response_adu{};
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> resp_frame{};
    mb_size_t resp_len = 0U;
    build_read_response(response_adu, resp_frame, resp_len, 1U);
    ASSERT_EQ(0, mock_inject_rx_data(resp_frame.data(), (uint16_t)resp_len));
    for (int i = 0; i < 8 && !first.invoked; ++i) {
        (void)mb_client_poll(&client_);
        mock_advance_time(5U);
    }

    EXPECT_TRUE(first.invoked);
    EXPECT_EQ(MB_OK, first.status);
    EXPECT_EQ(3U, first.response.payload_len);

    len = mock_get_tx_data(frame.data(), frame.size());
    ASSERT_GT(len, 0U);

    build_read_response(response_adu, resp_frame, resp_len, 1U);
    ASSERT_EQ(0, mock_inject_rx_data(resp_frame.data(), (uint16_t)resp_len));
    for (int i = 0; i < 8 && !second.invoked; ++i) {
        (void)mb_client_poll(&client_);
        mock_advance_time(5U);
    }

    EXPECT_TRUE(second.invoked);
    EXPECT_EQ(MB_OK, second.status);
    EXPECT_EQ(3U, second.response.payload_len);
}

TEST_F(MbClientTest, BackpressureLimitsQueue)
{
    mb_client_set_queue_capacity(&client_, 1U);
    EXPECT_EQ(1U, mb_client_queue_capacity(&client_));

    CallbackCapture first{};
    mb_client_txn_t *first_txn = nullptr;
    auto req1 = make_request(0x0000, 0x0001, &first);
    ASSERT_EQ(MB_OK, mb_client_submit(&client_, &req1, &first_txn));
    ASSERT_NE(nullptr, first_txn);
    (void)mb_client_poll(&client_);

    CallbackCapture second{};
    mb_client_txn_t *second_txn = nullptr;
    auto req2 = make_request(0x0002, 0x0001, &second);
    EXPECT_EQ(MB_ERR_NO_RESOURCES, mb_client_submit(&client_, &req2, &second_txn));
    EXPECT_EQ(nullptr, second_txn);

    EXPECT_EQ(1U, mb_client_queue_capacity(&client_));
}

TEST_F(MbClientTest, HighPriorityBypassesQueue)
{
    CallbackCapture first{};
    CallbackCapture second{};
    CallbackCapture high{};
    mb_client_txn_t *first_txn = nullptr;
    auto req1 = make_request(0x0000, 0x0001, &first);
    ASSERT_EQ(MB_OK, mb_client_submit(&client_, &req1, &first_txn));
    ASSERT_NE(nullptr, first_txn);

    (void)mb_client_poll(&client_);
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> frame{};
    ASSERT_GT(mock_get_tx_data(frame.data(), frame.size()), 0U);
    mock_clear_tx_buffer();

    mb_client_txn_t *second_txn = nullptr;
    auto req2 = make_request(0x0010, 0x0001, &second);
    ASSERT_EQ(MB_OK, mb_client_submit(&client_, &req2, &second_txn));
    ASSERT_NE(nullptr, second_txn);

    mb_client_txn_t *high_txn = nullptr;
    auto req3 = make_request(0x0020, 0x0001, &high);
    req3.flags |= MB_CLIENT_REQUEST_HIGH_PRIORITY;
    ASSERT_EQ(MB_OK, mb_client_submit(&client_, &req3, &high_txn));
    ASSERT_NE(nullptr, high_txn);

    mb_adu_view_t response_adu{};
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> resp_frame{};
    mb_size_t resp_len = 0U;
    build_read_response(response_adu, resp_frame, resp_len, 1U);
    ASSERT_EQ(0, mock_inject_rx_data(resp_frame.data(), (uint16_t)resp_len));

    for (int i = 0; i < 10 && !first.invoked; ++i) {
        (void)mb_client_poll(&client_);
        mock_advance_time(2U);
    }
    EXPECT_TRUE(first.invoked);

    std::array<mb_u8, MB_RTU_BUFFER_SIZE> next_frame{};
    auto next_len = mock_get_tx_data(next_frame.data(), next_frame.size());
    ASSERT_GT(next_len, 0U);
    EXPECT_EQ(req3.request.unit_id, next_frame[0]);
    EXPECT_EQ(req3.request.function, next_frame[1]);
    EXPECT_EQ(0x00U, next_frame[2]);
    EXPECT_EQ(0x20U, next_frame[3]);
    mock_clear_tx_buffer();

    build_read_response(response_adu, resp_frame, resp_len, 1U);
    ASSERT_EQ(0, mock_inject_rx_data(resp_frame.data(), (uint16_t)resp_len));
    for (int i = 0; i < 10 && !high.invoked; ++i) {
        (void)mb_client_poll(&client_);
        mock_advance_time(2U);
    }
    EXPECT_TRUE(high.invoked);

    auto final_len = mock_get_tx_data(next_frame.data(), next_frame.size());
    ASSERT_GT(final_len, 0U);
    EXPECT_EQ(req2.request.unit_id, next_frame[0]);
    EXPECT_EQ(req2.request.function, next_frame[1]);
    EXPECT_EQ(0x00U, next_frame[2]);
    EXPECT_EQ(0x10U, next_frame[3]);

    build_read_response(response_adu, resp_frame, resp_len, 1U);
    ASSERT_EQ(0, mock_inject_rx_data(resp_frame.data(), (uint16_t)resp_len));
    for (int i = 0; i < 10 && !second.invoked; ++i) {
        (void)mb_client_poll(&client_);
        mock_advance_time(2U);
    }
    EXPECT_TRUE(second.invoked);
}

TEST_F(MbClientTest, PoisonPillFlushesQueue)
{
    mb_client_reset_metrics(&client_);
    mb_client_set_queue_capacity(&client_, 1U);

    CallbackCapture first{};
    mb_client_txn_t *first_txn = nullptr;
    auto req1 = make_request(0x0000, 0x0001, &first);
    ASSERT_EQ(MB_OK, mb_client_submit(&client_, &req1, &first_txn));
    ASSERT_NE(nullptr, first_txn);

    (void)mb_client_poll(&client_);
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> frame{};
    ASSERT_GT(mock_get_tx_data(frame.data(), frame.size()), 0U);
    mock_clear_tx_buffer();

    CallbackCapture blocked{};
    auto blocked_req = make_request(0x0004, 0x0001, &blocked);
    EXPECT_EQ(MB_ERR_NO_RESOURCES, mb_client_submit(&client_, &blocked_req, nullptr));

    ASSERT_EQ(MB_OK, mb_client_submit_poison(&client_));

    mb_adu_view_t response_adu{};
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> resp_frame{};
    mb_size_t resp_len = 0U;
    build_read_response(response_adu, resp_frame, resp_len, 1U);
    ASSERT_EQ(0, mock_inject_rx_data(resp_frame.data(), (uint16_t)resp_len));

    for (int i = 0; i < 10 && !first.invoked; ++i) {
        (void)mb_client_poll(&client_);
        mock_advance_time(2U);
    }
    EXPECT_TRUE(first.invoked);
    EXPECT_EQ(MB_OK, first.status);

    EXPECT_EQ(0U, mock_get_tx_data(frame.data(), frame.size()));

    for (int i = 0; i < 4; ++i) {
        (void)mb_client_poll(&client_);
        mock_advance_time(1U);
    }

    EXPECT_TRUE(mb_client_is_idle(&client_));
    EXPECT_EQ(0U, mb_client_pending(&client_));

    mb_client_metrics_t metrics{};
    mb_client_get_metrics(&client_, &metrics);
    EXPECT_EQ(2U, metrics.submitted);
    EXPECT_EQ(1U, metrics.poison_triggers);
    EXPECT_GE(metrics.cancelled, 1U);
}

TEST_F(MbClientTest, CancelTransaction)
{
    CallbackCapture capture{};
    mb_client_txn_t *txn = nullptr;
    auto request = make_request(0x0000, 0x0001, &capture);
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_submit(&client_, &request, &txn));

    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_cancel(&client_, txn));
    (void)mb_client_poll(&client_);

    EXPECT_TRUE(capture.invoked);
    EXPECT_EQ(MODBUS_ERROR_CANCELLED, capture.status);
}

TEST_F(MbClientTest, CancelQueuedTransaction)
{
    CallbackCapture first{};
    CallbackCapture second{};
    mb_client_txn_t *first_txn = nullptr;
    mb_client_txn_t *second_txn = nullptr;

    auto req1 = make_request(0x0000, 0x0001, &first);
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_submit(&client_, &req1, &first_txn));
    (void)mb_client_poll(&client_);

    auto req2 = make_request(0x0004, 0x0001, &second);
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_submit(&client_, &req2, &second_txn));
    ASSERT_NE(first_txn, second_txn);

    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_cancel(&client_, second_txn));
    EXPECT_TRUE(second.invoked);
    EXPECT_EQ(MODBUS_ERROR_CANCELLED, second.status);

    std::array<mb_u8, MB_RTU_BUFFER_SIZE> frame{};
    auto tx_len = mock_get_tx_data(frame.data(), frame.size());
    ASSERT_GT(tx_len, 0U);

    mb_adu_view_t response_adu{};
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> resp_frame{};
    mb_size_t resp_len = 0U;
    build_read_response(response_adu, resp_frame, resp_len, 1U);
    ASSERT_EQ(0, mock_inject_rx_data(resp_frame.data(), (uint16_t)resp_len));

    for (int i = 0; i < 8 && !first.invoked; ++i) {
        (void)mb_client_poll(&client_);
        mock_advance_time(5U);
    }

    EXPECT_TRUE(first.invoked);
    EXPECT_EQ(MB_OK, first.status);
}

TEST_F(MbClientTest, RetryBackoffDelaysResend)
{
    CallbackCapture capture{};
    mb_client_txn_t *txn = nullptr;

    auto request = make_request(0x0000, 0x0001, &capture);
    request.timeout_ms = 10U;
    request.retry_backoff_ms = 40U;
    request.max_retries = 1U;

    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_submit(&client_, &request, &txn));

    (void)mb_client_poll(&client_);
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> frame{};
    auto len = mock_get_tx_data(frame.data(), frame.size());
    ASSERT_GT(len, 0U);
    mock_clear_tx_buffer();

    mock_advance_time(11U);
    (void)mb_client_poll(&client_);
    EXPECT_EQ(0U, mock_get_tx_data(frame.data(), frame.size()));
    EXPECT_FALSE(capture.invoked);

    mock_advance_time(19U);
    (void)mb_client_poll(&client_);
    EXPECT_EQ(0U, mock_get_tx_data(frame.data(), frame.size()));

    mock_advance_time(25U);
    (void)mb_client_poll(&client_);
    EXPECT_GT(mock_get_tx_data(frame.data(), frame.size()), 0U);
    mock_clear_tx_buffer();

    mock_advance_time(21U);
    (void)mb_client_poll(&client_);

    EXPECT_TRUE(capture.invoked);
    EXPECT_EQ(MODBUS_ERROR_TIMEOUT, capture.status);
    EXPECT_EQ(0U, mock_get_tx_data(frame.data(), frame.size()));
}

TEST_F(MbClientTest, FcSpecificTimeoutOverridesDefault)
{
    mb_client_set_fc_timeout(&client_, MODBUS_FUNC_READ_HOLDING_REGISTERS, 250U);

    CallbackCapture capture{};
    mb_client_txn_t *txn = nullptr;
    auto request = make_request(0x0000, 0x0001, &capture);
    request.timeout_ms = 0U;

    ASSERT_EQ(MB_OK, mb_client_submit(&client_, &request, &txn));
    ASSERT_NE(nullptr, txn);
    EXPECT_EQ(250U, txn->base_timeout_ms);
    EXPECT_EQ(250U, txn->timeout_ms);

    EXPECT_EQ(MB_OK, mb_client_cancel(&client_, txn));
}

TEST_F(MbClientTest, StressSequentialTransactions)
{
    constexpr int kTotal = 1000;
    std::vector<CallbackCapture> captures(kTotal);

    std::array<mb_u8, MB_RTU_BUFFER_SIZE> tx_frame{};
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> resp_frame{};
    mb_adu_view_t response_adu{};

    for (int i = 0; i < kTotal; ++i) {
        mb_client_txn_t *txn = nullptr;
        auto request = make_request(static_cast<uint16_t>(i & 0x00FF), 0x0001, &captures[i]);
        request.timeout_ms = 20U;
        request.max_retries = 0U;

        ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_submit(&client_, &request, &txn));

        (void)mb_client_poll(&client_);
        auto len = mock_get_tx_data(tx_frame.data(), tx_frame.size());
        ASSERT_GT(len, 0U);
        mock_clear_tx_buffer();

        mb_size_t resp_len = 0U;
        build_read_response(response_adu, resp_frame, resp_len, 1U);
        ASSERT_EQ(0, mock_inject_rx_data(resp_frame.data(), (uint16_t)resp_len));

        int guard = 0;
        while (!captures[i].invoked && guard < 12) {
            (void)mb_client_poll(&client_);
            mock_advance_time(1U);
            ++guard;
        }

        EXPECT_TRUE(captures[i].invoked);
        EXPECT_EQ(MB_OK, captures[i].status);
    }

    EXPECT_TRUE(mb_client_is_idle(&client_));
    EXPECT_EQ(0U, mb_client_pending(&client_));
}

    class MbClientTcpTest : public ::testing::Test {
    protected:
        void SetUp() override
        {
            modbus_transport_init_mock(&legacy_transport_);
            iface_ = mock_transport_get_iface();
            ASSERT_NE(nullptr, iface_);
            ASSERT_EQ(MB_OK, mb_client_init_tcp(&client_, iface_, txn_pool_.data(), txn_pool_.size()));
            mb_client_set_watchdog(&client_, 200U);
            mock_clear_tx_buffer();
        }

        void TearDown() override
        {
            mock_clear_tx_buffer();
        }

        static mb_client_request_t make_request(uint16_t start, uint16_t qty, CallbackCapture *cap)
        {
            request_payload_[0] = static_cast<mb_u8>(start >> 8);
            request_payload_[1] = static_cast<mb_u8>(start & 0xFFU);
            request_payload_[2] = static_cast<mb_u8>(qty >> 8);
            request_payload_[3] = static_cast<mb_u8>(qty & 0xFFU);

            mb_client_request_t req{};
            req.request.unit_id = 0x22U;
            req.request.function = MODBUS_FUNC_READ_HOLDING_REGISTERS;
            req.flags = 0U;
            req.request.payload = request_payload_;
            req.request.payload_len = 4U;
            req.timeout_ms = 40U;
            req.retry_backoff_ms = 10U;
            req.max_retries = 1U;
            req.callback = client_callback;
            req.user_ctx = cap;
            return req;
        }

        static mb_u8 request_payload_[4];

        mb_client_t client_{};
        std::array<mb_client_txn_t, 4> txn_pool_{};
        modbus_transport_t legacy_transport_{};
        const mb_transport_if_t *iface_{nullptr};
    };

    mb_u8 MbClientTcpTest::request_payload_[4] = {0};

    TEST_F(MbClientTcpTest, SendsAndReceivesMbapFrame)
    {
        CallbackCapture capture{};
        mb_client_txn_t *txn = nullptr;
        auto request = make_request(0x0000, 0x0002, &capture);
        ASSERT_EQ(MB_OK, mb_client_submit(&client_, &request, &txn));

        (void)mb_client_poll(&client_);

        std::array<mb_u8, MB_TCP_BUFFER_SIZE> tx_frame{};
        const auto tx_len = mock_get_tx_data(tx_frame.data(), static_cast<uint16_t>(tx_frame.size()));
        ASSERT_EQ(12U, tx_len);

        const mb_u16 tid = static_cast<mb_u16>((tx_frame[0] << 8U) | tx_frame[1]);
        EXPECT_NE(0U, tid);
        EXPECT_EQ(0x00U, tx_frame[2]);
        EXPECT_EQ(0x00U, tx_frame[3]);
        EXPECT_EQ(0x00U, tx_frame[4]);
        EXPECT_EQ(0x06U, tx_frame[5]);
        EXPECT_EQ(request.request.unit_id, tx_frame[6]);
        EXPECT_EQ(request.request.function, tx_frame[7]);
        EXPECT_EQ(tid, txn->tid);

        mock_clear_tx_buffer();

        const mb_u8 response_payload[]{0x04U, 0x00U, 0x64U, 0x00U, 0x65U};
        const mb_u16 length_field = static_cast<mb_u16>(1U + 1U + MB_COUNTOF(response_payload));
        constexpr mb_size_t kResponseSize = 6U + 1U + 1U + MB_COUNTOF(response_payload);
        std::array<mb_u8, kResponseSize> response_frame{};
        response_frame[0] = static_cast<mb_u8>(tid >> 8U);
        response_frame[1] = static_cast<mb_u8>(tid & 0xFFU);
        response_frame[2] = 0x00U;
        response_frame[3] = 0x00U;
        response_frame[4] = static_cast<mb_u8>(length_field >> 8U);
        response_frame[5] = static_cast<mb_u8>(length_field & 0xFFU);
        response_frame[6] = request.request.unit_id;
        response_frame[7] = request.request.function;
        std::memcpy(&response_frame[8], response_payload, MB_COUNTOF(response_payload));

        ASSERT_EQ(0, mock_inject_rx_data(response_frame.data(), static_cast<uint16_t>(response_frame.size())));

        for (int i = 0; i < 10 && !capture.invoked; ++i) {
            (void)mb_client_poll(&client_);
            mock_advance_time(1U);
        }

        EXPECT_TRUE(capture.invoked);
        EXPECT_EQ(MB_OK, capture.status);
        EXPECT_EQ(request.request.unit_id, capture.response.unit_id);
        EXPECT_EQ(request.request.function, capture.response.function);
        ASSERT_EQ(static_cast<mb_size_t>(MB_COUNTOF(response_payload)), capture.response.payload_len);
        for (size_t i = 0; i < MB_COUNTOF(response_payload); ++i) {
            EXPECT_EQ(response_payload[i], capture.response.payload[i]);
        }
    }

    TEST_F(MbClientTcpTest, RetriesPreserveTransactionId)
    {
        CallbackCapture capture{};
        mb_client_txn_t *txn = nullptr;
        auto request = make_request(0x0010, 0x0001, &capture);
        request.timeout_ms = 5U;
        request.retry_backoff_ms = 1U;
        request.max_retries = 1U;

        ASSERT_EQ(MB_OK, mb_client_submit(&client_, &request, &txn));

        (void)mb_client_poll(&client_);
        std::array<mb_u8, MB_TCP_BUFFER_SIZE> frame{};
        auto len = mock_get_tx_data(frame.data(), static_cast<uint16_t>(frame.size()));
        ASSERT_EQ(12U, len);
        const mb_u16 tid = static_cast<mb_u16>((frame[0] << 8U) | frame[1]);
        EXPECT_EQ(tid, txn->tid);
        mock_clear_tx_buffer();

        mock_advance_time(request.timeout_ms + 1U);
        (void)mb_client_poll(&client_);
        EXPECT_EQ(0U, mock_get_tx_data(frame.data(), static_cast<uint16_t>(frame.size())));

        mock_advance_time(1U);
        (void)mb_client_poll(&client_);
        len = mock_get_tx_data(frame.data(), static_cast<uint16_t>(frame.size()));
        ASSERT_EQ(12U, len);
        const mb_u16 retry_tid = static_cast<mb_u16>((frame[0] << 8U) | frame[1]);
        EXPECT_EQ(tid, retry_tid);
        EXPECT_EQ(tid, txn->tid);

        const mb_u8 response_payload[]{0x02U, 0x00U, 0xAAU};
        const mb_u16 length_field = static_cast<mb_u16>(1U + 1U + MB_COUNTOF(response_payload));
        const mb_size_t frame_size = static_cast<mb_size_t>(6U + length_field);
        std::array<mb_u8, 16> response{};
        response[0] = static_cast<mb_u8>(tid >> 8U);
        response[1] = static_cast<mb_u8>(tid & 0xFFU);
        response[2] = 0x00U;
        response[3] = 0x00U;
        response[4] = static_cast<mb_u8>(length_field >> 8U);
        response[5] = static_cast<mb_u8>(length_field & 0xFFU);
        response[6] = request.request.unit_id;
        response[7] = request.request.function;
        std::memcpy(&response[8], response_payload, MB_COUNTOF(response_payload));

        mock_clear_tx_buffer();
        ASSERT_EQ(0, mock_inject_rx_data(response.data(), static_cast<uint16_t>(frame_size)));

        for (int i = 0; i < 10 && !capture.invoked; ++i) {
            (void)mb_client_poll(&client_);
            mock_advance_time(1U);
        }

        EXPECT_TRUE(capture.invoked);
        EXPECT_EQ(MB_OK, capture.status);
    }

TEST_F(MbClientTest, RecoversFromSinglePacketLoss)
{
    CallbackCapture capture{};
    mb_client_txn_t *txn = nullptr;
    auto request = make_request(0x0000, 0x0001, &capture);
    request.timeout_ms = 15U;
    request.retry_backoff_ms = 30U;
    request.max_retries = 2U;

    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_submit(&client_, &request, &txn));

    std::array<mb_u8, MB_RTU_BUFFER_SIZE> frame{};
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> resp_frame{};
    mb_adu_view_t response_adu{};

    (void)mb_client_poll(&client_);
    auto first_len = mock_get_tx_data(frame.data(), frame.size());
    ASSERT_GT(first_len, 0U);
    mock_clear_tx_buffer();

    mock_advance_time(16U);
    (void)mb_client_poll(&client_);

    bool resent = false;
    for (int step = 0; step < 24 && !resent; ++step) {
        mock_advance_time(5U);
        (void)mb_client_poll(&client_);
        auto resend_len = mock_get_tx_data(frame.data(), frame.size());
        if (resend_len > 0U) {
            resent = true;
            mock_clear_tx_buffer();

            mb_size_t resp_len = 0U;
            build_read_response(response_adu, resp_frame, resp_len, 1U);
            ASSERT_EQ(0, mock_inject_rx_data(resp_frame.data(), (uint16_t)resp_len));
        }
    }

    ASSERT_TRUE(resent);

    for (int i = 0; i < 16 && !capture.invoked; ++i) {
        (void)mb_client_poll(&client_);
        mock_advance_time(2U);
    }

    EXPECT_TRUE(capture.invoked);
    EXPECT_EQ(MB_OK, capture.status);
}

TEST_F(MbClientTest, MetricsResetClearsCounters)
{
    mb_client_reset_metrics(&client_);

    CallbackCapture capture{};
    mb_client_txn_t *txn = nullptr;
    auto request = make_request(0x0000, 0x0001, &capture);
    ASSERT_EQ(MB_OK, mb_client_submit(&client_, &request, &txn));
    ASSERT_NE(nullptr, txn);

    (void)mb_client_poll(&client_);
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> frame{};
    ASSERT_GT(mock_get_tx_data(frame.data(), frame.size()), 0U);
    mock_clear_tx_buffer();

    mb_adu_view_t response_adu{};
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> resp_frame{};
    mb_size_t resp_len = 0U;
    build_read_response(response_adu, resp_frame, resp_len, 1U);
    ASSERT_EQ(0, mock_inject_rx_data(resp_frame.data(), (uint16_t)resp_len));

    for (int i = 0; i < 10 && !capture.invoked; ++i) {
        (void)mb_client_poll(&client_);
        mock_advance_time(1U);
    }

    EXPECT_TRUE(capture.invoked);
    EXPECT_EQ(MB_OK, capture.status);

    mb_client_metrics_t metrics{};
    mb_client_get_metrics(&client_, &metrics);
    EXPECT_EQ(1U, metrics.submitted);
    EXPECT_EQ(1U, metrics.completed);
    EXPECT_EQ(1U, metrics.response_count);

    mb_client_reset_metrics(&client_);
    mb_client_get_metrics(&client_, &metrics);
    EXPECT_EQ(0U, metrics.submitted);
    EXPECT_EQ(0U, metrics.completed);
    EXPECT_EQ(0U, metrics.response_count);
}

TEST_F(MbClientTest, DiagnosticsReflectCompletionStatuses)
{
    CallbackCapture capture{};
    mb_client_txn_t *txn = nullptr;
    auto request = make_request(0x0000, 0x0001, &capture);
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_submit(&client_, &request, &txn));

    (void)mb_client_poll(&client_);
    std::array<uint8_t, MB_RTU_BUFFER_SIZE> tx_frame{};
    auto tx_len = mock_get_tx_data(tx_frame.data(), tx_frame.size());
    ASSERT_GT(tx_len, 0U);

    mb_adu_view_t response_adu{};
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> response_frame{};
    mb_size_t response_len = 0U;
    build_read_response(response_adu, response_frame, response_len, 1U);
    ASSERT_EQ(0, mock_inject_rx_data(response_frame.data(), static_cast<uint16_t>(response_len)));

    for (int i = 0; i < 8 && !capture.invoked; ++i) {
        (void)mb_client_poll(&client_);
        mock_advance_time(5U);
    }
    ASSERT_TRUE(capture.invoked);
    mock_clear_tx_buffer();

    mb_diag_counters_t diag{};
    mb_client_get_diag(&client_, &diag);
    EXPECT_EQ(1U, diag.function[MODBUS_FUNC_READ_HOLDING_REGISTERS]);
    EXPECT_EQ(1U, diag.error[MB_DIAG_ERR_SLOT_OK]);

    mb_client_reset_diag(&client_);
    mb_client_get_diag(&client_, &diag);
    EXPECT_EQ(0U, diag.function[MODBUS_FUNC_READ_HOLDING_REGISTERS]);
    EXPECT_EQ(0U, diag.error[MB_DIAG_ERR_SLOT_OK]);

    CallbackCapture timeout_capture{};
    txn = nullptr;
    auto timeout_request = make_request(0x0004, 0x0001, &timeout_capture);
    timeout_request.timeout_ms = 5U;
    timeout_request.retry_backoff_ms = 5U;
    timeout_request.max_retries = 0U;
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_submit(&client_, &timeout_request, &txn));

    for (int step = 0; step < 16 && !timeout_capture.invoked; ++step) {
        mock_advance_time(6U);
        (void)mb_client_poll(&client_);
    }
    ASSERT_TRUE(timeout_capture.invoked);
    EXPECT_EQ(MB_ERR_TIMEOUT, timeout_capture.status);

    mb_client_get_diag(&client_, &diag);
    EXPECT_EQ(1U, diag.function[MODBUS_FUNC_READ_HOLDING_REGISTERS]);
    EXPECT_EQ(1U, diag.error[MB_DIAG_ERR_SLOT_TIMEOUT]);
}

TEST_F(MbClientTest, EmitsObservabilityEvents)
{
    ClientEventRecorder recorder;
    mb_client_set_event_callback(&client_, ClientEventRecorder::Callback, &recorder);

    CallbackCapture capture{};
    mb_client_txn_t *txn = nullptr;
    auto request = make_request(0x0000, 0x0001, &capture);
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_client_submit(&client_, &request, &txn));

    (void)mb_client_poll(&client_);
    std::array<uint8_t, MB_RTU_BUFFER_SIZE> tx_frame{};
    auto tx_len = mock_get_tx_data(tx_frame.data(), tx_frame.size());
    ASSERT_GT(tx_len, 0U);

    mb_adu_view_t response_adu{};
    std::array<mb_u8, MB_RTU_BUFFER_SIZE> response_frame{};
    mb_size_t response_len = 0U;
    build_read_response(response_adu, response_frame, response_len, 1U);
    ASSERT_EQ(0, mock_inject_rx_data(response_frame.data(), static_cast<uint16_t>(response_len)));

    for (int i = 0; i < 8 && !capture.invoked; ++i) {
        (void)mb_client_poll(&client_);
        mock_advance_time(5U);
    }
    ASSERT_TRUE(capture.invoked);

    ASSERT_GE(recorder.events.size(), static_cast<size_t>(6));
    EXPECT_EQ(MB_EVENT_SOURCE_CLIENT, recorder.events.front().source);
    EXPECT_EQ(MB_EVENT_CLIENT_STATE_ENTER, recorder.events.front().type);
    EXPECT_EQ(MB_CLIENT_STATE_IDLE, recorder.events.front().data.client_state.state);

    const auto submit_it = std::find_if(recorder.events.begin(), recorder.events.end(), [](const mb_event_t &evt) {
        return evt.type == MB_EVENT_CLIENT_TX_SUBMIT;
    });
    ASSERT_NE(recorder.events.end(), submit_it);
    EXPECT_EQ(MODBUS_FUNC_READ_HOLDING_REGISTERS, submit_it->data.client_txn.function);
    EXPECT_TRUE(submit_it->data.client_txn.expect_response);

    const auto complete_it = std::find_if(recorder.events.begin(), recorder.events.end(), [](const mb_event_t &evt) {
        return evt.type == MB_EVENT_CLIENT_TX_COMPLETE;
    });
    ASSERT_NE(recorder.events.end(), complete_it);
    EXPECT_EQ(MB_OK, complete_it->data.client_txn.status);

    bool saw_waiting_enter = false;
    bool saw_waiting_exit = false;
    bool saw_idle_after = false;
    for (size_t i = 0; i < recorder.events.size(); ++i) {
        const mb_event_t &evt = recorder.events[i];
        if (evt.type == MB_EVENT_CLIENT_STATE_ENTER && evt.data.client_state.state == MB_CLIENT_STATE_WAITING) {
            saw_waiting_enter = true;
        } else if (evt.type == MB_EVENT_CLIENT_STATE_EXIT && evt.data.client_state.state == MB_CLIENT_STATE_WAITING) {
            saw_waiting_exit = true;
        } else if (evt.type == MB_EVENT_CLIENT_STATE_ENTER && evt.data.client_state.state == MB_CLIENT_STATE_IDLE && i != 0U) {
            saw_idle_after = true;
        }
    }

    EXPECT_TRUE(saw_waiting_enter);
    EXPECT_TRUE(saw_waiting_exit);
    EXPECT_TRUE(saw_idle_after);
}

} // namespace
