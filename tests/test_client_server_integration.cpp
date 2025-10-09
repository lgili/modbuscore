#include <array>
#include <cstring>
#include <deque>
#include <vector>

#include <gtest/gtest.h>

extern "C" {
#include <modbus/client.h>
#include <modbus/pdu.h>
#include <modbus/server.h>
#include <modbus/transport_if.h>
}

namespace {

struct LoopBus {
    std::deque<mb_u8> client_to_server;
    std::deque<mb_u8> server_to_client;
    mb_time_ms_t now_ms{0};
};

struct LoopEndpoint {
    LoopBus *bus;
    bool towards_server; // true for client endpoint
};

static mb_err_t loop_send(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out)
{
    if (!ctx || !buf) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    auto *endpoint = static_cast<LoopEndpoint *>(ctx);
    auto &queue = endpoint->towards_server ? endpoint->bus->client_to_server : endpoint->bus->server_to_client;
    for (mb_size_t i = 0; i < len; ++i) {
        queue.push_back(buf[i]);
    }
    if (out) {
        out->processed = len;
    }
    return MB_OK;
}

static mb_err_t loop_recv(void *ctx, mb_u8 *buf, mb_size_t cap, mb_transport_io_result_t *out)
{
    if (!ctx || !buf || cap == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    auto *endpoint = static_cast<LoopEndpoint *>(ctx);
    auto &queue = endpoint->towards_server ? endpoint->bus->server_to_client : endpoint->bus->client_to_server;
    if (queue.empty()) {
        if (out) {
            out->processed = 0U;
        }
        return MB_ERR_TIMEOUT;
    }

    mb_size_t processed = 0U;
    while (!queue.empty() && processed < cap) {
        buf[processed++] = queue.front();
        queue.pop_front();
    }

    if (out) {
        out->processed = processed;
    }
    return MB_OK;
}

static mb_time_ms_t loop_now(void *ctx)
{
    auto *endpoint = static_cast<LoopEndpoint *>(ctx);
    return endpoint ? endpoint->bus->now_ms : 0U;
}

static void loop_yield(void *ctx)
{
    (void)ctx;
}

static void loop_advance(LoopBus &bus, mb_time_ms_t delta)
{
    bus.now_ms += delta;
}

struct CallbackCapture {
    bool invoked{false};
    mb_err_t status{MB_OK};
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
    if (response) {
        capture->response = *response;
    }
}

static void prepare_request(mb_client_request_t &request,
                            uint8_t unit_id,
                            const mb_u8 *pdu,
                            mb_size_t payload_len,
                            CallbackCapture &capture)
{
    std::memset(&request, 0, sizeof(request));
    request.request.unit_id = unit_id;
    request.request.function = pdu[0];
    request.request.payload = (payload_len > 0U) ? &pdu[1] : nullptr;
    request.request.payload_len = payload_len;
    request.timeout_ms = 100U;
    request.retry_backoff_ms = 20U;
    request.max_retries = 0U;
    request.callback = client_callback;
    request.user_ctx = &capture;
}

struct PriorityCapture {
    std::vector<int> order;
    std::vector<mb_err_t> statuses;
};

struct PriorityCallbackCtx {
    PriorityCapture *capture;
    int id;
};

static void priority_callback(mb_client_t *,
                              const mb_client_txn_t *,
                              mb_err_t status,
                              const mb_adu_view_t *,
                              void *user_ctx)
{
    auto *ctx = static_cast<PriorityCallbackCtx *>(user_ctx);
    if (ctx == nullptr || ctx->capture == nullptr) {
        return;
    }
    ctx->capture->order.push_back(ctx->id);
    ctx->capture->statuses.push_back(status);
}

class ClientServerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        client_endpoint_.bus = &bus_;
        client_endpoint_.towards_server = true;
        server_endpoint_.bus = &bus_;
        server_endpoint_.towards_server = false;

        client_iface_.ctx = &client_endpoint_;
        client_iface_.send = loop_send;
        client_iface_.recv = loop_recv;
        client_iface_.now = loop_now;
        client_iface_.yield = loop_yield;

        server_iface_.ctx = &server_endpoint_;
        server_iface_.send = loop_send;
        server_iface_.recv = loop_recv;
        server_iface_.now = loop_now;
        server_iface_.yield = loop_yield;

        ASSERT_EQ(MB_OK, mb_client_init(&client_, &client_iface_, txn_pool_.data(), txn_pool_.size()));
        ASSERT_EQ(MB_OK,
                  mb_server_init(&server_,
                                  &server_iface_,
                                  kUnitId,
                                  regions_.data(),
                                  regions_.size(),
                                  request_pool_.data(),
                                  request_pool_.size()));

        ASSERT_EQ(MB_OK, mb_server_add_storage(&server_, 0x0000U, static_cast<mb_u16>(rw_storage_.size()), false, rw_storage_.data()));
        ASSERT_EQ(MB_OK, mb_server_add_storage(&server_, 0x0100U, static_cast<mb_u16>(ro_storage_.size()), true, ro_storage_.data()));
    }

    void pump(unsigned steps)
    {
        for (unsigned i = 0; i < steps; ++i) {
            loop_advance(bus_, 1U);
            (void)mb_client_poll(&client_);
            (void)mb_server_poll(&server_);
        }
    }

    static constexpr mb_u8 kUnitId = 0x22U;

    LoopBus bus_{};
    LoopEndpoint client_endpoint_{};
    LoopEndpoint server_endpoint_{};
    mb_transport_if_t client_iface_{};
    mb_transport_if_t server_iface_{};

    mb_client_t client_{};
    std::array<mb_client_txn_t, 4> txn_pool_{};

    mb_server_t server_{};
    std::array<mb_server_region_t, 4> regions_{};
    std::array<mb_server_request_t, 4> request_pool_{};
    std::array<mb_u16, 8> rw_storage_{0x1111U, 0x2222U, 0x3333U, 0x4444U, 0x5555U, 0x6666U, 0x7777U, 0x8888U};
    std::array<mb_u16, 4> ro_storage_{0xAAAAU, 0xBBBBU, 0xCCCCU, 0xDDDDU};
};

TEST_F(ClientServerIntegrationTest, ReadHoldingRegistersEndToEnd)
{
    mb_u8 pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_read_holding_request(pdu, sizeof pdu, 0x0002U, 0x0003U));

    CallbackCapture capture{};
    mb_client_request_t request{};
    prepare_request(request, kUnitId, pdu, 4U, capture);

    ASSERT_EQ(MB_OK, mb_client_submit(&client_, &request, nullptr));

    for (int i = 0; i < 50 && !capture.invoked; ++i) {
        pump(1);
    }

    ASSERT_TRUE(capture.invoked);
    EXPECT_EQ(MB_OK, capture.status);
    EXPECT_EQ(kUnitId, capture.response.unit_id);
    EXPECT_EQ(MB_PDU_FC_READ_HOLDING_REGISTERS, capture.response.function);
    ASSERT_EQ(static_cast<mb_size_t>(7U), capture.response.payload_len);
    EXPECT_EQ(6U, capture.response.payload[0]);

    std::array<mb_u8, MB_PDU_MAX> full{};
    full[0] = capture.response.function;
    std::memcpy(&full[1], capture.response.payload, capture.response.payload_len);

    const mb_u8 *data = nullptr;
    mb_u16 reg_count = 0U;
    ASSERT_EQ(MB_OK, mb_pdu_parse_read_holding_response(full.data(), capture.response.payload_len + 1U, &data, &reg_count));
    ASSERT_EQ(3U, reg_count);
    EXPECT_EQ(0x3333U, static_cast<mb_u16>((data[0] << 8) | data[1]));
    EXPECT_EQ(0x4444U, static_cast<mb_u16>((data[2] << 8) | data[3]));
    EXPECT_EQ(0x5555U, static_cast<mb_u16>((data[4] << 8) | data[5]));
}

TEST_F(ClientServerIntegrationTest, WriteSingleRegisterEndToEnd)
{
    mb_u8 pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_write_single_request(pdu, sizeof pdu, 0x0001U, 0xBEEFU));

    CallbackCapture capture{};
    mb_client_request_t request{};
    prepare_request(request, kUnitId, pdu, 4U, capture);

    ASSERT_EQ(MB_OK, mb_client_submit(&client_, &request, nullptr));

    for (int i = 0; i < 50 && !capture.invoked; ++i) {
        pump(1);
    }

    ASSERT_TRUE(capture.invoked);
    EXPECT_EQ(MB_OK, capture.status);
    EXPECT_EQ(MB_PDU_FC_WRITE_SINGLE_REGISTER, capture.response.function);
    EXPECT_EQ(static_cast<mb_size_t>(4U), capture.response.payload_len);
    EXPECT_EQ(0xBEEFU, rw_storage_[1]);
}

TEST_F(ClientServerIntegrationTest, WriteMultipleRegistersEndToEnd)
{
    std::array<mb_u16, 3> values{0x0102U, 0x0304U, 0x0506U};
    mb_u8 pdu[MB_PDU_MAX];
    ASSERT_EQ(MB_OK, mb_pdu_build_write_multiple_request(pdu, sizeof pdu, 0x0003U, values.data(), values.size()));
    const mb_size_t payload_len = 6U + values.size() * 2U;

    CallbackCapture capture{};
    mb_client_request_t request{};
    prepare_request(request, kUnitId, pdu, payload_len - 1U, capture);

    ASSERT_EQ(MB_OK, mb_client_submit(&client_, &request, nullptr));

    for (int i = 0; i < 50 && !capture.invoked; ++i) {
        pump(1);
    }

    ASSERT_TRUE(capture.invoked);
    EXPECT_EQ(MB_OK, capture.status);
    EXPECT_EQ(MB_PDU_FC_WRITE_MULTIPLE_REGISTERS, capture.response.function);
    EXPECT_EQ(static_cast<mb_size_t>(4U), capture.response.payload_len);
    EXPECT_EQ(0x0102U, rw_storage_[3]);
    EXPECT_EQ(0x0304U, rw_storage_[4]);
    EXPECT_EQ(0x0506U, rw_storage_[5]);
}

TEST_F(ClientServerIntegrationTest, OutOfRangeReadReturnsException)
{
    mb_u8 pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_read_holding_request(pdu, sizeof pdu, 0x0200U, 0x0001U));

    CallbackCapture capture{};
    mb_client_request_t request{};
    prepare_request(request, kUnitId, pdu, 4U, capture);

    ASSERT_EQ(MB_OK, mb_client_submit(&client_, &request, nullptr));

    for (int i = 0; i < 50 && !capture.invoked; ++i) {
        pump(1);
    }

    ASSERT_TRUE(capture.invoked);
    EXPECT_EQ(MB_OK, capture.status);
    EXPECT_EQ(static_cast<mb_u8>(MB_PDU_FC_READ_HOLDING_REGISTERS | MB_PDU_EXCEPTION_BIT), capture.response.function);
    ASSERT_EQ(static_cast<mb_size_t>(1U), capture.response.payload_len);
    EXPECT_EQ(MB_EX_ILLEGAL_DATA_ADDRESS, capture.response.payload[0]);
}

TEST_F(ClientServerIntegrationTest, ReadInputRegistersEndToEnd)
{
    mb_u8 pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_read_input_request(pdu, sizeof pdu, 0x0100U, 0x0002U));

    CallbackCapture capture{};
    mb_client_request_t request{};
    prepare_request(request, kUnitId, pdu, 4U, capture);

    ASSERT_EQ(MB_OK, mb_client_submit(&client_, &request, nullptr));

    for (int i = 0; i < 50 && !capture.invoked; ++i) {
        pump(1);
    }

    ASSERT_TRUE(capture.invoked);
    EXPECT_EQ(MB_OK, capture.status);
    EXPECT_EQ(MB_PDU_FC_READ_INPUT_REGISTERS, capture.response.function);
    ASSERT_EQ(static_cast<mb_size_t>(5U), capture.response.payload_len);
    EXPECT_EQ(4U, capture.response.payload[0]);

    std::array<mb_u8, MB_PDU_MAX> full{};
    full[0] = capture.response.function;
    std::memcpy(&full[1], capture.response.payload, capture.response.payload_len);

    const mb_u8 *data = nullptr;
    mb_u16 reg_count = 0U;
    ASSERT_EQ(MB_OK, mb_pdu_parse_read_input_response(full.data(),
                                                      capture.response.payload_len + 1U,
                                                      &data,
                                                      &reg_count));
    ASSERT_EQ(2U, reg_count);
    EXPECT_EQ(ro_storage_[0], static_cast<mb_u16>((data[0] << 8) | data[1]));
    EXPECT_EQ(ro_storage_[1], static_cast<mb_u16>((data[2] << 8) | data[3]));
}

TEST_F(ClientServerIntegrationTest, HighPriorityRequestServedFirst)
{
    mb_u8 slow_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_read_holding_request(slow_pdu, sizeof slow_pdu, 0x0000U, 0x0001U));
    mb_u8 fast_pdu[5];
    ASSERT_EQ(MB_OK, mb_pdu_build_read_holding_request(fast_pdu, sizeof fast_pdu, 0x0004U, 0x0001U));

    CallbackCapture placeholder_slow{};
    CallbackCapture placeholder_fast{};

    mb_client_request_t slow{};
    prepare_request(slow, kUnitId, slow_pdu, 4U, placeholder_slow);
    mb_client_request_t fast{};
    prepare_request(fast, kUnitId, fast_pdu, 4U, placeholder_fast);

    PriorityCapture capture{};
    PriorityCallbackCtx slow_ctx{&capture, 1};
    PriorityCallbackCtx fast_ctx{&capture, 2};

    slow.callback = priority_callback;
    slow.user_ctx = &slow_ctx;

    fast.callback = priority_callback;
    fast.user_ctx = &fast_ctx;
    fast.flags = MB_CLIENT_REQUEST_HIGH_PRIORITY;

    ASSERT_EQ(MB_OK, mb_client_submit(&client_, &slow, nullptr));
    ASSERT_EQ(MB_OK, mb_client_submit(&client_, &fast, nullptr));

    for (int i = 0; i < 100 && capture.order.size() < 2; ++i) {
        pump(1);
    }

    ASSERT_EQ(2U, capture.order.size());
    EXPECT_EQ(2, capture.order[0]) << "High priority request should complete first";
    EXPECT_EQ(1, capture.order[1]);
    ASSERT_EQ(2U, capture.statuses.size());
    EXPECT_EQ(MB_OK, capture.statuses[0]);
    EXPECT_EQ(MB_OK, capture.statuses[1]);
}

} // namespace
