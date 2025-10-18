#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <modbuscore/protocol/engine.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/mock.h>

#include "engine_test_helpers.h"

static void init_default_env(engine_test_env_t *env)
{
    mbc_mock_transport_config_t cfg = {
        .initial_now_ms = 0U,
        .yield_advance_ms = 1U,
    };
    engine_test_env_init(env, &cfg);
}

static void test_engine_initialisation(void)
{
    engine_test_env_t env;
    init_default_env(&env);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &env.runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .use_override = false,
        .event_cb = engine_test_env_capture_event,
        .event_ctx = &env,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);
    assert(mbc_engine_is_ready(&engine));

    mbc_engine_shutdown(&engine);
    engine_test_env_shutdown(&env);
}

static void test_engine_step_transitions(void)
{
    static const uint8_t request_fc03[] = {0x01U, 0x03U, 0x00U, 0x0AU, 0x00U, 0x02U};

    engine_test_env_t env;
    init_default_env(&env);
    assert(mbc_mock_transport_schedule_rx(env.mock, request_fc03, sizeof(request_fc03), 0U) == MBC_STATUS_OK);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &env.runtime,
        .role = MBC_ENGINE_ROLE_SERVER,
        .use_override = false,
        .event_cb = engine_test_env_capture_event,
        .event_ctx = &env,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);
    assert(engine.state == MBC_ENGINE_STATE_IDLE);

    engine_test_env_clear_events(&env);
    assert(mbc_engine_step(&engine, sizeof(request_fc03)) == MBC_STATUS_OK);
    assert(engine.state == MBC_ENGINE_STATE_IDLE);
    assert(engine_test_env_event_seen(&env, MBC_ENGINE_EVENT_RX_READY));
    assert(engine_test_env_event_seen(&env, MBC_ENGINE_EVENT_PDU_READY));
    assert(mbc_mock_transport_pending_rx(env.mock) == 0U);

    mbc_pdu_t taken;
    assert(mbc_engine_take_pdu(&engine, &taken));
    assert(taken.function == 0x03U);
    assert(taken.payload_length == 4U);
    assert(!mbc_engine_take_pdu(&engine, &taken));

    mbc_engine_shutdown(&engine);
    engine_test_env_shutdown(&env);
}

static void test_engine_submit_request_client(void)
{
    engine_test_env_t env;
    init_default_env(&env);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &env.runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .use_override = false,
        .event_cb = engine_test_env_capture_event,
        .event_ctx = &env,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);

    uint8_t frame[2] = {0x01U, 0x03U};
    engine_test_env_clear_events(&env);
    assert(mbc_engine_submit_request(&engine, frame, sizeof(frame)) == MBC_STATUS_OK);
    engine_test_env_fetch_tx(&env, frame, sizeof(frame));
    assert(engine.state == MBC_ENGINE_STATE_WAIT_RESPONSE);
    assert(engine_test_env_event_seen(&env, MBC_ENGINE_EVENT_TX_SENT));
    assert(engine_test_env_event_seen(&env, MBC_ENGINE_EVENT_STATE_CHANGE));

    /* Nova submissão sem voltar ao estado IDLE deve falhar */
    assert(mbc_engine_submit_request(&engine, frame, sizeof(frame)) == MBC_STATUS_BUSY);

    mbc_engine_shutdown(&engine);
    engine_test_env_shutdown(&env);
}

static void test_engine_client_response_decode(void)
{
    static const uint8_t response_fc03[] = {0x01U, 0x03U, 0x04U, 0xDEU, 0xADU, 0xBEU, 0xEFU};

    engine_test_env_t env;
    init_default_env(&env);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &env.runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .use_override = false,
        .event_cb = engine_test_env_capture_event,
        .event_ctx = &env,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);

    uint8_t frame[2] = {0x01U, 0x03U};
    engine_test_env_clear_events(&env);
    assert(mbc_engine_submit_request(&engine, frame, sizeof(frame)) == MBC_STATUS_OK);
    engine_test_env_fetch_tx(&env, frame, sizeof(frame));
    assert(engine.state == MBC_ENGINE_STATE_WAIT_RESPONSE);

    assert(mbc_mock_transport_schedule_rx(env.mock, response_fc03, sizeof(response_fc03), 0U) == MBC_STATUS_OK);
    engine_test_env_clear_events(&env);

    assert(mbc_engine_step(&engine, sizeof(response_fc03)) == MBC_STATUS_OK);
    assert(engine_test_env_event_seen(&env, MBC_ENGINE_EVENT_PDU_READY));

    mbc_pdu_t pdu;
    assert(mbc_engine_take_pdu(&engine, &pdu));
    assert(pdu.function == 0x03U);
    assert(pdu.payload_length == 5U);
    assert(pdu.payload[0] == 0x04U);
    assert(engine.state == MBC_ENGINE_STATE_IDLE);

    mbc_engine_shutdown(&engine);
    engine_test_env_shutdown(&env);
}

static void test_engine_timeout_client(void)
{
    engine_test_env_t env;
    init_default_env(&env);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &env.runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .use_override = false,
        .event_cb = engine_test_env_capture_event,
        .event_ctx = &env,
        .response_timeout_ms = 3U,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);

    uint8_t frame[2] = {0x01U, 0x05U};
    engine_test_env_clear_events(&env);
    assert(mbc_engine_submit_request(&engine, frame, sizeof(frame)) == MBC_STATUS_OK);
    engine_test_env_fetch_tx(&env, frame, sizeof(frame));
    assert(engine.state == MBC_ENGINE_STATE_WAIT_RESPONSE);

    mbc_mock_transport_advance(env.mock, config.response_timeout_ms + 1U);

    mbc_status_t status = mbc_engine_step(&engine, 1U);
    assert(status == MBC_STATUS_TIMEOUT);
    assert(engine.state == MBC_ENGINE_STATE_IDLE);
    assert(engine_test_env_event_seen(&env, MBC_ENGINE_EVENT_TIMEOUT));

    mbc_engine_shutdown(&engine);
    engine_test_env_shutdown(&env);
}

static void test_engine_invalid_inputs(void)
{
    mbc_engine_t engine;
    assert(mbc_engine_init(NULL, NULL) == MBC_STATUS_INVALID_ARGUMENT);
    assert(mbc_engine_step(NULL, 1U) == MBC_STATUS_NOT_INITIALISED);

    engine_test_env_t env;
    init_default_env(&env);

    mbc_engine_config_t config = {
        .runtime = &env.runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .use_override = false,
        .event_cb = engine_test_env_capture_event,
        .event_ctx = &env,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);
    assert(mbc_engine_step(&engine, 0U) == MBC_STATUS_INVALID_ARGUMENT);

    mbc_engine_shutdown(&engine);
    engine_test_env_shutdown(&env);
}

static void test_engine_send_failure(void)
{
    engine_test_env_t env;
    init_default_env(&env);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &env.runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .use_override = false,
        .event_cb = engine_test_env_capture_event,
        .event_ctx = &env,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);

    uint8_t frame[2] = {0x01U, 0x06U};
    mbc_mock_transport_fail_next_send(env.mock, MBC_STATUS_IO_ERROR);
    engine_test_env_clear_events(&env);
    assert(mbc_engine_submit_request(&engine, frame, sizeof(frame)) == MBC_STATUS_IO_ERROR);
    assert(!engine_test_env_event_seen(&env, MBC_ENGINE_EVENT_TX_SENT));
    assert(engine.state == MBC_ENGINE_STATE_IDLE);

    mbc_engine_shutdown(&engine);
    engine_test_env_shutdown(&env);
}

static void test_engine_receive_failure(void)
{
    engine_test_env_t env;
    init_default_env(&env);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &env.runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .use_override = false,
        .event_cb = engine_test_env_capture_event,
        .event_ctx = &env,
        .response_timeout_ms = 100U,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);

    uint8_t frame[2] = {0x01U, 0x03U};
    engine_test_env_clear_events(&env);
    assert(mbc_engine_submit_request(&engine, frame, sizeof(frame)) == MBC_STATUS_OK);
    engine_test_env_fetch_tx(&env, frame, sizeof(frame));
    assert(engine.state == MBC_ENGINE_STATE_WAIT_RESPONSE);

    mbc_mock_transport_fail_next_receive(env.mock, MBC_STATUS_IO_ERROR);
    engine_test_env_clear_events(&env);
    mbc_status_t status = mbc_engine_step(&engine, 4U);
    assert(status == MBC_STATUS_IO_ERROR);
    assert(engine.state == MBC_ENGINE_STATE_WAIT_RESPONSE);
    assert(env.events_total >= 1U);  /* STEP_BEGIN/END still emitted */

    mbc_engine_shutdown(&engine);
    engine_test_env_shutdown(&env);
}

static void test_engine_disconnect_mid_step(void)
{
    engine_test_env_t env;
    init_default_env(&env);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &env.runtime,
        .role = MBC_ENGINE_ROLE_SERVER,
        .use_override = false,
        .event_cb = engine_test_env_capture_event,
        .event_ctx = &env,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);

    const uint8_t request_fc06[] = {0x01U, 0x06U, 0x00U, 0x02U, 0x00U, 0x63U};
    assert(mbc_mock_transport_schedule_rx(env.mock, request_fc06, sizeof(request_fc06), 0U) == MBC_STATUS_OK);
    mbc_mock_transport_set_connected(env.mock, false);
    engine_test_env_clear_events(&env);
    mbc_status_t status = mbc_engine_step(&engine, sizeof(request_fc06));
    assert(status == MBC_STATUS_IO_ERROR);
    assert(engine_test_env_event_seen(&env, MBC_ENGINE_EVENT_STEP_END));
    assert(mbc_mock_transport_pending_rx(env.mock) == 1U);

    mbc_engine_shutdown(&engine);
    engine_test_env_shutdown(&env);
}

int main(void)
{
    test_engine_initialisation();
    test_engine_step_transitions();
    test_engine_submit_request_client();
    test_engine_client_response_decode();
    test_engine_timeout_client();
    test_engine_invalid_inputs();
    test_engine_send_failure();
    test_engine_receive_failure();
    test_engine_disconnect_mid_step();
    return 0;
}
