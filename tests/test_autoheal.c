#include <assert.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/mbap.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/runtime/autoheal.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/runtime/runtime.h>
#include <modbuscore/transport/mock.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct clock_ctx {
    const mbc_transport_iface_t* transport;
} clock_ctx_t;

static uint64_t mock_clock_now(void* ctx)
{
    const clock_ctx_t* clock_ctx = ctx;
    return mbc_transport_now(clock_ctx->transport);
}

static void build_fc03_request(uint8_t unit_id, uint16_t address, uint16_t quantity,
                               uint16_t transaction_id, uint8_t* out_frame, size_t* out_len)
{
    mbc_pdu_t request = {0};
    assert(mbc_pdu_build_read_holding_request(&request, unit_id, address, quantity) ==
           MBC_STATUS_OK);

    uint8_t pdu_buf[256];
    pdu_buf[0] = request.function;
    memcpy(&pdu_buf[1], request.payload, request.payload_length);
    size_t pdu_len = 1U + request.payload_length;

    mbc_mbap_header_t header = {
        .transaction_id = transaction_id,
        .protocol_id = 0U,
        .length = (uint16_t)(pdu_len + 1U),
        .unit_id = request.unit_id,
    };

    assert(mbc_mbap_encode(&header, pdu_buf, pdu_len, out_frame, 256U, out_len) == MBC_STATUS_OK);
}

static void build_fc03_response(uint8_t unit_id, uint16_t transaction_id, const uint16_t* values,
                                size_t count, uint8_t* out_frame, size_t* out_len)
{
    mbc_pdu_t response = {
        .unit_id = unit_id,
        .function = 0x03U,
        .payload_length = (size_t)(1U + count * 2U),
    };

    response.payload[0] = (uint8_t)(count * 2U);
    for (size_t i = 0; i < count; ++i) {
        response.payload[1U + i * 2U] = (uint8_t)(values[i] >> 8);
        response.payload[2U + i * 2U] = (uint8_t)(values[i] & 0xFFU);
    }

    uint8_t pdu_buf[256];
    pdu_buf[0] = response.function;
    memcpy(&pdu_buf[1], response.payload, response.payload_length);
    size_t pdu_len = 1U + response.payload_length;

    mbc_mbap_header_t header = {
        .transaction_id = transaction_id,
        .protocol_id = 0U,
        .length = (uint16_t)(pdu_len + 1U),
        .unit_id = response.unit_id,
    };

    assert(mbc_mbap_encode(&header, pdu_buf, pdu_len, out_frame, 256U, out_len) == MBC_STATUS_OK);
}

static void setup_environment(mbc_transport_iface_t* transport, mbc_mock_transport_t** mock,
                              clock_ctx_t* clock_ctx, mbc_runtime_t* runtime)
{
    mbc_mock_transport_config_t mock_cfg = {
        .initial_now_ms = 0U,
        .yield_advance_ms = 1U,
    };

    assert(mbc_mock_transport_create(&mock_cfg, transport, mock) == MBC_STATUS_OK);

    clock_ctx->transport = transport;
    mbc_clock_iface_t clock_iface = {
        .ctx = clock_ctx,
        .now_ms = mock_clock_now,
    };

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, transport);
    mbc_runtime_builder_with_clock(&builder, &clock_iface);

    assert(mbc_runtime_builder_build(&builder, runtime) == MBC_STATUS_OK);
}

static void teardown_environment(mbc_runtime_t* runtime, mbc_mock_transport_t* mock)
{
    mbc_runtime_shutdown(runtime);
    mbc_mock_transport_destroy(mock);
}

static void test_autoheal_retry_until_success(void)
{
    mbc_transport_iface_t transport = {0};
    mbc_mock_transport_t* mock = NULL;
    clock_ctx_t clock_ctx = {0};
    mbc_runtime_t runtime = {0};
    setup_environment(&transport, &mock, &clock_ctx, &runtime);

    mbc_engine_t engine = {0};
    mbc_engine_config_t engine_cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,
        .response_timeout_ms = 20U,
    };
    assert(mbc_engine_init(&engine, &engine_cfg) == MBC_STATUS_OK);

    mbc_autoheal_config_t heal_cfg = {
        .runtime = &runtime,
        .max_retries = 3U,
        .initial_backoff_ms = 5U,
        .max_backoff_ms = 20U,
        .cooldown_ms = 100U,
        .request_capacity = 256U,
        .observer = NULL,
        .observer_ctx = NULL,
    };

    mbc_autoheal_supervisor_t supervisor;
    assert(mbc_autoheal_init(&supervisor, &heal_cfg, &engine) == MBC_STATUS_OK);

    uint8_t request_frame[256];
    size_t request_len = 0U;
    build_fc03_request(1U, 0U, 2U, 1U, request_frame, &request_len);

    assert(mbc_autoheal_submit(&supervisor, request_frame, request_len) == MBC_STATUS_OK);
    assert(mbc_autoheal_state(&supervisor) == MBC_AUTOHEAL_STATE_WAITING);

    mbc_mock_transport_fail_next_receive(mock, MBC_STATUS_IO_ERROR);
    assert(mbc_autoheal_step(&supervisor, 128U) == MBC_STATUS_IO_ERROR);
    assert(mbc_autoheal_retry_count(&supervisor) == 1U);
    assert(mbc_autoheal_state(&supervisor) == MBC_AUTOHEAL_STATE_SCHEDULED);
    assert(engine.state == MBC_ENGINE_STATE_IDLE);

    mbc_mock_transport_advance(mock, 5U);
    assert(mbc_autoheal_step(&supervisor, 128U) == MBC_STATUS_OK);
    int guard = 0;
    while (mbc_autoheal_state(&supervisor) == MBC_AUTOHEAL_STATE_SCHEDULED && guard++ < 8) {
        mbc_mock_transport_advance(
            mock, supervisor.current_backoff_ms > 0U ? supervisor.current_backoff_ms : 1U);
        mbc_autoheal_step(&supervisor, 128U);
    }
    assert(mbc_autoheal_retry_count(&supervisor) == 1U);
    assert(mbc_autoheal_state(&supervisor) == MBC_AUTOHEAL_STATE_WAITING);

    uint16_t values[] = {0x1234, 0x5678};
    uint8_t response_frame[256];
    size_t response_len = 0U;
    build_fc03_response(1U, 1U, values, 2U, response_frame, &response_len);
    assert(mbc_mock_transport_schedule_rx(mock, response_frame, response_len, 0U) == MBC_STATUS_OK);

    assert(mbc_autoheal_step(&supervisor, 256U) == MBC_STATUS_OK);

    mbc_pdu_t pdu = {0};
    assert(mbc_autoheal_take_pdu(&supervisor, &pdu));
    const uint8_t* payload = NULL;
    size_t count = 0U;
    assert(mbc_pdu_parse_read_holding_response(&pdu, &payload, &count) == MBC_STATUS_OK);
    assert(count == 2U);
    assert(((payload[0] << 8) | payload[1]) == 0x1234);
    assert(((payload[2] << 8) | payload[3]) == 0x5678);

    assert(mbc_autoheal_retry_count(&supervisor) == 0U);
    assert(mbc_autoheal_state(&supervisor) == MBC_AUTOHEAL_STATE_IDLE);

    mbc_autoheal_shutdown(&supervisor);
    mbc_engine_shutdown(&engine);
    teardown_environment(&runtime, mock);
}

static void test_autoheal_circuit_open(void)
{
    mbc_transport_iface_t transport = {0};
    mbc_mock_transport_t* mock = NULL;
    clock_ctx_t clock_ctx = {0};
    mbc_runtime_t runtime = {0};
    setup_environment(&transport, &mock, &clock_ctx, &runtime);

    mbc_engine_t engine = {0};
    mbc_engine_config_t engine_cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,
        .response_timeout_ms = 10U,
    };
    assert(mbc_engine_init(&engine, &engine_cfg) == MBC_STATUS_OK);

    mbc_autoheal_config_t heal_cfg = {
        .runtime = &runtime,
        .max_retries = 2U,
        .initial_backoff_ms = 2U,
        .max_backoff_ms = 8U,
        .cooldown_ms = 15U,
        .request_capacity = 256U,
    };

    mbc_autoheal_supervisor_t supervisor;
    assert(mbc_autoheal_init(&supervisor, &heal_cfg, &engine) == MBC_STATUS_OK);

    uint8_t request_frame[256];
    size_t request_len = 0U;
    build_fc03_request(1U, 0U, 1U, 2U, request_frame, &request_len);

    assert(mbc_autoheal_submit(&supervisor, request_frame, request_len) == MBC_STATUS_OK);

    int guard = 0;
    while (!mbc_autoheal_is_circuit_open(&supervisor) && guard++ < 10) {
        mbc_mock_transport_fail_next_receive(mock, MBC_STATUS_TIMEOUT);
        mbc_autoheal_step(&supervisor, 128U);
        if (mbc_autoheal_is_circuit_open(&supervisor)) {
            break;
        }
        mbc_mock_transport_advance(mock, supervisor.current_backoff_ms > 0U
                                             ? supervisor.current_backoff_ms
                                             : heal_cfg.initial_backoff_ms);
        mbc_autoheal_step(&supervisor, 128U);
    }

    assert(mbc_autoheal_is_circuit_open(&supervisor));
    assert(mbc_autoheal_submit(&supervisor, request_frame, request_len) == MBC_STATUS_BUSY);

    mbc_mock_transport_advance(mock, 20U);
    mbc_autoheal_step(&supervisor, 128U);
    assert(!mbc_autoheal_is_circuit_open(&supervisor));

    assert(mbc_autoheal_submit(&supervisor, request_frame, request_len) == MBC_STATUS_OK);

    mbc_autoheal_shutdown(&supervisor);
    mbc_engine_shutdown(&engine);
    teardown_environment(&runtime, mock);
}

int main(void)
{
    printf("=== AutoHeal Tests Starting ===\n");
    test_autoheal_retry_until_success();
    test_autoheal_circuit_open();
    printf("=== All AutoHeal Tests Passed ===\n");
    return 0;
}
