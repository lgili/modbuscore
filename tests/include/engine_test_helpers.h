#ifndef ENGINE_TEST_HELPERS_H
#define ENGINE_TEST_HELPERS_H

#include <assert.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/mock.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct engine_test_env {
    mbc_mock_transport_t* mock;
    mbc_transport_iface_t transport;
    mbc_runtime_t runtime;
    mbc_engine_event_type_t events[32];
    size_t event_count;
    size_t events_total;
    mbc_engine_event_type_t last_event;
} engine_test_env_t;

static inline void engine_test_env_init(engine_test_env_t* env,
                                        const mbc_mock_transport_config_t* config)
{
    *env = (engine_test_env_t){0};

    mbc_mock_transport_config_t cfg = {0};
    if (config) {
        cfg = *config;
    }

    mbc_status_t status = mbc_mock_transport_create(&cfg, &env->transport, &env->mock);
    assert(mbc_status_is_ok(status));

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &env->transport);
    status = mbc_runtime_builder_build(&builder, &env->runtime);
    assert(mbc_status_is_ok(status));
}

static inline void engine_test_env_shutdown(engine_test_env_t* env)
{
    mbc_runtime_shutdown(&env->runtime);
    mbc_mock_transport_destroy(env->mock);
    env->mock = NULL;
}

static inline void engine_test_env_clear_events(engine_test_env_t* env)
{
    env->event_count = 0U;
    env->events_total = 0U;
    env->last_event = 0;
}

static inline void engine_test_env_capture_event(void* ctx, const mbc_engine_event_t* event)
{
    engine_test_env_t* env = ctx;
    env->events_total++;
    env->last_event = event->type;
    if (env->event_count < sizeof(env->events) / sizeof(env->events[0])) {
        env->events[env->event_count++] = event->type;
    }
}

static inline bool engine_test_env_event_seen(const engine_test_env_t* env,
                                              mbc_engine_event_type_t type)
{
    for (size_t i = 0; i < env->event_count; ++i) {
        if (env->events[i] == type) {
            return true;
        }
    }
    return false;
}

static inline void engine_test_env_fetch_tx(engine_test_env_t* env, const uint8_t* expected,
                                            size_t expected_len)
{
    uint8_t buffer[256];
    size_t out_len = 0U;
    mbc_status_t status = mbc_mock_transport_fetch_tx(env->mock, buffer, sizeof(buffer), &out_len);
    assert(mbc_status_is_ok(status));
    assert(out_len == expected_len);
    assert(memcmp(buffer, expected, expected_len) == 0);
}

#endif /* ENGINE_TEST_HELPERS_H */
