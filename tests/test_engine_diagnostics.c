#include "engine_test_helpers.h"

#include <assert.h>
#include <modbuscore/protocol/mbap.h>
#include <modbuscore/protocol/pdu.h>
#include <stdio.h>
#include <string.h>

typedef struct diag_capture_entry {
    mbc_diag_severity_t severity;
    uint32_t code;
    char message[32];
    char status[32];
    char state[32];
    char event_name[32];
    char transition_from[32];
    char transition_to[32];
    char role[16];
    char framing[16];
    char timeout_ms[16];
} diag_capture_entry_t;

typedef struct diag_capture {
    diag_capture_entry_t entries[64];
    size_t count;
} diag_capture_t;

static void diag_capture_clear(diag_capture_t* capture) { capture->count = 0U; }

static void diag_capture_copy(char* dest, size_t capacity, const char* src)
{
    if (!dest || capacity == 0U) {
        return;
    }
    if (!src) {
        dest[0] = '\0';
        return;
    }
    strncpy(dest, src, capacity - 1U);
    dest[capacity - 1U] = '\0';
}

static void diag_capture_sink(void* ctx, const mbc_diag_event_t* event)
{
    diag_capture_t* capture = (diag_capture_t*)ctx;
    if (capture->count >= sizeof(capture->entries) / sizeof(capture->entries[0])) {
        return;
    }

    diag_capture_entry_t* entry = &capture->entries[capture->count++];
    memset(entry, 0, sizeof(*entry));

    entry->severity = event->severity;
    entry->code = event->code;
    diag_capture_copy(entry->message, sizeof(entry->message), event->message);

    for (size_t i = 0; i < event->field_count; ++i) {
        const mbc_diag_kv_t* field = &event->fields[i];
        if (!field->key || !field->value) {
            continue;
        }

        if (strcmp(field->key, "status") == 0) {
            diag_capture_copy(entry->status, sizeof(entry->status), field->value);
        } else if (strcmp(field->key, "state") == 0) {
            diag_capture_copy(entry->state, sizeof(entry->state), field->value);
        } else if (strcmp(field->key, "event") == 0) {
            diag_capture_copy(entry->event_name, sizeof(entry->event_name), field->value);
        } else if (strcmp(field->key, "from") == 0) {
            diag_capture_copy(entry->transition_from, sizeof(entry->transition_from), field->value);
        } else if (strcmp(field->key, "to") == 0) {
            diag_capture_copy(entry->transition_to, sizeof(entry->transition_to), field->value);
        } else if (strcmp(field->key, "role") == 0) {
            diag_capture_copy(entry->role, sizeof(entry->role), field->value);
        } else if (strcmp(field->key, "framing") == 0) {
            diag_capture_copy(entry->framing, sizeof(entry->framing), field->value);
        } else if (strcmp(field->key, "response_timeout_ms") == 0) {
            diag_capture_copy(entry->timeout_ms, sizeof(entry->timeout_ms), field->value);
        }
    }
}

static const diag_capture_entry_t* diag_find(const diag_capture_t* capture, const char* message)
{
    for (size_t i = 0; i < capture->count; ++i) {
        if (strcmp(capture->entries[i].message, message) == 0) {
            return &capture->entries[i];
        }
    }
    return NULL;
}

static const diag_capture_entry_t* diag_find_event(const diag_capture_t* capture,
                                                   const char* event_name)
{
    for (size_t i = 0; i < capture->count; ++i) {
        if (strcmp(capture->entries[i].message, "engine_event") == 0 &&
            strcmp(capture->entries[i].event_name, event_name) == 0) {
            return &capture->entries[i];
        }
    }
    return NULL;
}

static void test_engine_diag_initialisation(void)
{
    diag_capture_t capture = {0};
    mbc_diag_sink_iface_t sink = {.ctx = &capture, .emit = diag_capture_sink};

    engine_test_env_t env;
    engine_test_env_init_with_diag(&env, NULL, &sink);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &env.runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,
        .use_override = false,
        .event_cb = NULL,
        .event_ctx = NULL,
        .response_timeout_ms = 0U,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);

    const diag_capture_entry_t* init_entry = diag_find(&capture, "engine_initialised");
    assert(init_entry != NULL);
    assert(init_entry->severity == MBC_DIAG_SEVERITY_INFO);
    assert(strcmp(init_entry->role, "client") == 0);
    assert(strcmp(init_entry->framing, "tcp") == 0);
    assert(strcmp(init_entry->timeout_ms, "1000") == 0); /* default timeout applied */

    mbc_engine_shutdown(&engine);
    engine_test_env_shutdown(&env);
}

static void test_engine_diag_submit_invalid_buffer(void)
{
    diag_capture_t capture = {0};
    mbc_diag_sink_iface_t sink = {.ctx = &capture, .emit = diag_capture_sink};

    engine_test_env_t env;
    engine_test_env_init_with_diag(&env, NULL, &sink);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &env.runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,
        .use_override = false,
        .event_cb = NULL,
        .event_ctx = NULL,
        .response_timeout_ms = 0U,
    };
    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);

    diag_capture_clear(&capture);

    assert(mbc_engine_submit_request(&engine, NULL, 0U) == MBC_STATUS_INVALID_ARGUMENT);

    const diag_capture_entry_t* entry = diag_find(&capture, "submit_invalid_buffer");
    assert(entry != NULL);
    assert(entry->severity == MBC_DIAG_SEVERITY_ERROR);
    assert(strcmp(entry->status, "invalid_argument") == 0);

    mbc_engine_shutdown(&engine);
    engine_test_env_shutdown(&env);
}

static void test_engine_diag_timeout(void)
{
    diag_capture_t capture = {0};
    mbc_diag_sink_iface_t sink = {.ctx = &capture, .emit = diag_capture_sink};

    mbc_mock_transport_config_t transport_cfg = {
        .initial_now_ms = 0U,
        .yield_advance_ms = 1U,
    };

    engine_test_env_t env;
    engine_test_env_init_with_diag(&env, &transport_cfg, &sink);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &env.runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,
        .use_override = false,
        .event_cb = NULL,
        .event_ctx = NULL,
        .response_timeout_ms = 5U,
    };
    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);

    diag_capture_clear(&capture);

    mbc_pdu_t request = {0};
    assert(mbc_pdu_build_read_holding_request(&request, 1U, 0U, 1U) == MBC_STATUS_OK);

    uint8_t pdu_buf[256];
    pdu_buf[0] = request.function;
    memcpy(&pdu_buf[1], request.payload, request.payload_length);
    const size_t pdu_len = 1U + request.payload_length;

    mbc_mbap_header_t header = {
        .transaction_id = 1U,
        .protocol_id = 0U,
        .length = (uint16_t)(pdu_len + 1U),
        .unit_id = request.unit_id,
    };

    uint8_t frame[256];
    size_t frame_len = 0U;
    assert(mbc_mbap_encode(&header, pdu_buf, pdu_len, frame, sizeof(frame), &frame_len) ==
           MBC_STATUS_OK);

    assert(mbc_engine_submit_request(&engine, frame, frame_len) == MBC_STATUS_OK);

    /* Advance time beyond timeout and step the engine */
    mbc_mock_transport_advance(env.mock, 10U);

    assert(mbc_engine_step(&engine, frame_len) == MBC_STATUS_TIMEOUT);

    const diag_capture_entry_t* timeout_entry = diag_find(&capture, "response_timeout");
    assert(timeout_entry != NULL);
    assert(timeout_entry->severity == MBC_DIAG_SEVERITY_WARNING);
    assert(strcmp(timeout_entry->status, "timeout") == 0);

    const diag_capture_entry_t* timeout_event = diag_find_event(&capture, "timeout");
    assert(timeout_event != NULL);
    assert(timeout_event->severity == MBC_DIAG_SEVERITY_WARNING);

    mbc_engine_shutdown(&engine);
    engine_test_env_shutdown(&env);
}

int main(void)
{
    printf("=== Engine Diagnostics Tests Starting ===\n");
    test_engine_diag_initialisation();
    test_engine_diag_submit_invalid_buffer();
    test_engine_diag_timeout();
    printf("=== All Engine Diagnostics Tests Passed ===\n");
    return 0;
}
