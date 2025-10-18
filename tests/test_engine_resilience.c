#include <assert.h>
#include <string.h>

#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>

#include "engine_test_helpers.h"

static void init_client_env(engine_test_env_t *env, mbc_engine_t *engine)
{
    mbc_mock_transport_config_t cfg = {
        .initial_now_ms = 0U,
        .yield_advance_ms = 1U,
    };
    engine_test_env_init(env, &cfg);

    mbc_engine_config_t config = {
        .runtime = &env->runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .use_override = false,
        .framing = MBC_FRAMING_TCP,
        .event_cb = engine_test_env_capture_event,
        .event_ctx = env,
        .response_timeout_ms = 50U,
    };

    assert(mbc_engine_init(engine, &config) == MBC_STATUS_OK);
}

static void shutdown_client_env(engine_test_env_t *env, mbc_engine_t *engine)
{
    mbc_engine_shutdown(engine);
    engine_test_env_shutdown(env);
}

static uint16_t read_be16(const uint8_t *bytes)
{
    return (uint16_t)((bytes[0] << 8) | bytes[1]);
}

static void build_fc03_response_frame(uint16_t transaction_id,
                                      const uint16_t *registers,
                                      size_t register_count,
                                      uint8_t *out_frame,
                                      size_t *out_len)
{
    mbc_pdu_t response = {
        .unit_id = 1U,
        .function = 0x03U,
        .payload_length = (size_t)(1U + register_count * 2U),
    };

    response.payload[0] = (uint8_t)(register_count * 2U);
    for (size_t i = 0; i < register_count; ++i) {
        response.payload[1U + i * 2U] = (uint8_t)(registers[i] >> 8);
        response.payload[2U + i * 2U] = (uint8_t)(registers[i] & 0xFFU);
    }

    uint8_t pdu_buf[256];
    pdu_buf[0] = response.function;
    memcpy(&pdu_buf[1], response.payload, response.payload_length);
    size_t pdu_len = 1U + response.payload_length;

    mbc_mbap_header_t header = {
        .transaction_id = transaction_id,
        .protocol_id = 0,
        .length = 0,
        .unit_id = response.unit_id,
    };

    assert(mbc_mbap_encode(&header, pdu_buf, pdu_len, out_frame, 256U, out_len) == MBC_STATUS_OK);
}

static void submit_simple_request(engine_test_env_t *env, mbc_engine_t *engine, uint16_t transaction_id)
{
    mbc_pdu_t request_pdu;
    assert(mbc_pdu_build_read_holding_request(&request_pdu, 1U, 0U, 2U) == MBC_STATUS_OK);

    uint8_t pdu_buf[256];
    pdu_buf[0] = request_pdu.function;
    memcpy(&pdu_buf[1], request_pdu.payload, request_pdu.payload_length);
    size_t pdu_len = 1U + request_pdu.payload_length;

    mbc_mbap_header_t header = {
        .transaction_id = transaction_id,
        .protocol_id = 0,
        .length = 0,
        .unit_id = request_pdu.unit_id,
    };

    uint8_t frame[256];
    size_t frame_len = 0U;
    assert(mbc_mbap_encode(&header, pdu_buf, pdu_len, frame, sizeof(frame), &frame_len) == MBC_STATUS_OK);

    engine_test_env_clear_events(env);
    assert(mbc_engine_submit_request(engine, frame, frame_len) == MBC_STATUS_OK);
    engine_test_env_fetch_tx(env, frame, frame_len);
}

static void test_partial_response_delivery(void)
{
    engine_test_env_t env;
    mbc_engine_t engine;
    init_client_env(&env, &engine);

    submit_simple_request(&env, &engine, 1U);
    assert(engine.state == MBC_ENGINE_STATE_WAIT_RESPONSE);

    uint16_t regs[] = {0xDEAD, 0xBEEF};
    uint8_t frame[256];
    size_t frame_len = 0U;
    build_fc03_response_frame(1U, regs, 2U, frame, &frame_len);

    size_t split = 8U; /* MBAP (7) + function byte */
    assert(split < frame_len);
    assert(mbc_mock_transport_schedule_rx(env.mock, frame, split, 0U) == MBC_STATUS_OK);
    assert(mbc_mock_transport_schedule_rx(env.mock, frame + split, frame_len - split, 10U) == MBC_STATUS_OK);

    engine_test_env_clear_events(&env);
    /* Primeiro step: só parte inicial, sem PDU completo */
    assert(mbc_engine_step(&engine, split) == MBC_STATUS_OK);
    assert(!engine_test_env_event_seen(&env, MBC_ENGINE_EVENT_PDU_READY));
    assert(engine.state == MBC_ENGINE_STATE_WAIT_RESPONSE);

    mbc_mock_transport_advance(env.mock, 10U);
    engine_test_env_clear_events(&env);

    /* Segundo step deve fechar a resposta */
    assert(mbc_engine_step(&engine, frame_len - split) == MBC_STATUS_OK);
    assert(engine_test_env_event_seen(&env, MBC_ENGINE_EVENT_PDU_READY));

    mbc_pdu_t pdu;
    assert(mbc_engine_take_pdu(&engine, &pdu));
    assert(pdu.function == 0x03U);

    const uint8_t *register_bytes = NULL;
    size_t register_count = 0U;
    assert(mbc_pdu_parse_read_holding_response(&pdu, &register_bytes, &register_count) == MBC_STATUS_OK);
    assert(register_count == 2U);
    assert(read_be16(&register_bytes[0]) == 0xDEAD);
    assert(read_be16(&register_bytes[2]) == 0xBEEF);

    shutdown_client_env(&env, &engine);
}

static void test_response_drop_then_recover(void)
{
    engine_test_env_t env;
    mbc_engine_t engine;
    init_client_env(&env, &engine);

    submit_simple_request(&env, &engine, 2U);

    uint16_t regs_drop[] = {0xBEEF, 0x0001};
    uint8_t frame[256];
    size_t frame_len = 0U;
    build_fc03_response_frame(2U, regs_drop, 2U, frame, &frame_len);
    assert(mbc_mock_transport_schedule_rx(env.mock, frame, frame_len, 0U) == MBC_STATUS_OK);
    /* Simula perda antes da entrega */
    assert(mbc_mock_transport_drop_next_rx(env.mock) == MBC_STATUS_OK);

    engine_test_env_clear_events(&env);
    assert(mbc_engine_step(&engine, frame_len) == MBC_STATUS_OK);
    assert(!engine_test_env_event_seen(&env, MBC_ENGINE_EVENT_PDU_READY));
    assert(engine.state == MBC_ENGINE_STATE_WAIT_RESPONSE);

    /* Reagendar resposta e verificar recuperação */
    assert(mbc_mock_transport_schedule_rx(env.mock, frame, frame_len, 0U) == MBC_STATUS_OK);
    engine_test_env_clear_events(&env);
    assert(mbc_engine_step(&engine, frame_len) == MBC_STATUS_OK);
    assert(engine_test_env_event_seen(&env, MBC_ENGINE_EVENT_PDU_READY));

    mbc_pdu_t pdu;
    assert(mbc_engine_take_pdu(&engine, &pdu));
    const uint8_t *register_bytes = NULL;
    size_t register_count = 0U;
    assert(mbc_pdu_parse_read_holding_response(&pdu, &register_bytes, &register_count) == MBC_STATUS_OK);
    assert(register_count == 2U);
    assert(read_be16(&register_bytes[0]) == 0xBEEF);
    assert(read_be16(&register_bytes[2]) == 0x0001);

    shutdown_client_env(&env, &engine);
}

static void test_receive_error_then_success(void)
{
    engine_test_env_t env;
    mbc_engine_t engine;
    init_client_env(&env, &engine);

    submit_simple_request(&env, &engine, 3U);

    mbc_mock_transport_fail_next_receive(env.mock, MBC_STATUS_IO_ERROR);
    engine_test_env_clear_events(&env);
    assert(mbc_engine_step(&engine, 4U) == MBC_STATUS_IO_ERROR);
    assert(engine.state == MBC_ENGINE_STATE_WAIT_RESPONSE);

    uint16_t regs_err[] = {0xAABB, 0xCCDD};
    uint8_t frame[256];
    size_t frame_len = 0U;
    build_fc03_response_frame(3U, regs_err, 2U, frame, &frame_len);
    assert(mbc_mock_transport_schedule_rx(env.mock, frame, frame_len, 0U) == MBC_STATUS_OK);
    engine_test_env_clear_events(&env);
    assert(mbc_engine_step(&engine, frame_len) == MBC_STATUS_OK);
    assert(engine_test_env_event_seen(&env, MBC_ENGINE_EVENT_PDU_READY));

    mbc_pdu_t pdu;
    assert(mbc_engine_take_pdu(&engine, &pdu));
    const uint8_t *register_bytes = NULL;
    size_t register_count = 0U;
    assert(mbc_pdu_parse_read_holding_response(&pdu, &register_bytes, &register_count) == MBC_STATUS_OK);
    assert(register_count == 2U);
    assert(read_be16(&register_bytes[0]) == 0xAABB);
    assert(read_be16(&register_bytes[2]) == 0xCCDD);

    shutdown_client_env(&env, &engine);
}

int main(void)
{
    test_partial_response_delivery();
    test_response_drop_then_recover();
    test_receive_error_then_success();
    return 0;
}
