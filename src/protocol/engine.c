/**
 * @file engine.c
 * @brief Protocol engine implementation with FSM and framing support.
 *
 * This file implements the core protocol engine that handles:
 * - Request/response state machine
 * - Frame reception and PDU decoding
 * - Both RTU and TCP (MBAP) framing modes
 * - Timeout management
 * - Event notifications
 */

#include <modbuscore/protocol/crc.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/mbap.h>
#include <modbuscore/protocol/pdu.h>
#include <stdio.h>
#include <string.h>

#define MBC_EXPECTED_UNSUPPORTED ((size_t)-1)
#define ENGINE_DIAG_COMPONENT "protocol.engine"

/* Forward declarations of private functions */
static void emit_event(mbc_engine_t* engine, mbc_engine_event_type_t type);
static void enter_state(mbc_engine_t* engine, mbc_engine_state_t next);
static void reset_rx_buffer(mbc_engine_t* engine);
static size_t determine_expected_length(const mbc_engine_t* engine);
static void emit_diag(mbc_engine_t* engine, mbc_diag_severity_t severity, const char* message,
                      uint32_t code, const mbc_diag_kv_t* fields, size_t field_count);
static void emit_diag_status(mbc_engine_t* engine, mbc_diag_severity_t severity,
                             const char* message, mbc_status_t status);
static const char* engine_event_name(mbc_engine_event_type_t type);
static const char* engine_state_name(mbc_engine_state_t state);
static const char* engine_role_name(mbc_engine_role_t role);
static const char* framing_mode_name(mbc_framing_mode_t mode);
static const char* status_name(mbc_status_t status);
static uint32_t status_code(mbc_status_t status);
static mbc_diag_severity_t event_severity(mbc_engine_event_type_t type);
static void emit_state_transition_diag(mbc_engine_t* engine, mbc_engine_state_t from,
                                       mbc_engine_state_t to);
static void emit_engine_init_diag(mbc_engine_t* engine);

mbc_status_t mbc_engine_init(mbc_engine_t* engine, const mbc_engine_config_t* config)
{
    if (!engine || !config || !config->runtime) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!mbc_runtime_is_ready(config->runtime)) {
        return MBC_STATUS_NOT_INITIALISED;
    }

    const mbc_runtime_config_t* deps = mbc_runtime_dependencies(config->runtime);
    if (!deps) {
        return MBC_STATUS_NOT_INITIALISED;
    }

    mbc_transport_iface_t transport =
        config->use_override ? config->transport_override : deps->transport;
    if (!transport.send || !transport.receive) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    uint32_t timeout = config->response_timeout_ms ? config->response_timeout_ms : 1000U;

    *engine = (mbc_engine_t){
        .state = MBC_ENGINE_STATE_IDLE,
        .role = config->role,
        .framing = config->framing,
        .runtime = config->runtime,
        .transport = transport,
        .diag = deps->diag,
        .initialised = true,
        .event_cb = config->event_cb,
        .event_ctx = config->event_ctx,
        .response_timeout_ms = timeout,
        .last_activity_ms = mbc_transport_now(&transport),
        .rx_length = 0U,
        .expected_length = 0U,
        .pdu_ready = false,
        .last_mbap_header = {0},
        .last_mbap_valid = false,
    };

    emit_engine_init_diag(engine);

    return MBC_STATUS_OK;
}

void mbc_engine_shutdown(mbc_engine_t* engine)
{
    if (!engine) {
        return;
    }

    if (engine->initialised) {
        emit_diag(engine, MBC_DIAG_SEVERITY_INFO, "engine_shutdown", 0U, NULL, 0U);
    }

    *engine = (mbc_engine_t){0};
}

bool mbc_engine_is_ready(const mbc_engine_t* engine) { return engine && engine->initialised; }

mbc_status_t mbc_engine_step(mbc_engine_t* engine, size_t budget)
{
    if (!mbc_engine_is_ready(engine)) {
        emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "step_not_ready",
                         MBC_STATUS_NOT_INITIALISED);
        return MBC_STATUS_NOT_INITIALISED;
    }

    if (budget == 0U) {
        emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "step_invalid_budget",
                         MBC_STATUS_INVALID_ARGUMENT);
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    emit_event(engine, MBC_ENGINE_EVENT_STEP_BEGIN);

    mbc_status_t status = MBC_STATUS_OK;

    while (budget-- > 0U) {
        size_t space = sizeof(engine->rx_buffer) - engine->rx_length;
        if (space == 0U) {
            emit_event(engine, MBC_ENGINE_EVENT_STEP_END);
            emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "step_rx_buffer_full",
                             MBC_STATUS_NO_RESOURCES);
            return MBC_STATUS_NO_RESOURCES;
        }

        mbc_transport_io_t io = {0};
        status = mbc_transport_receive(&engine->transport, &engine->rx_buffer[engine->rx_length],
                                       space, &io);
        if (!mbc_status_is_ok(status)) {
            break;
        }

        if (io.processed == 0U) {
            break;
        }

        engine->rx_length += io.processed;
        engine->last_activity_ms = mbc_transport_now(&engine->transport);
        emit_event(engine, MBC_ENGINE_EVENT_RX_READY);

        if (engine->role == MBC_ENGINE_ROLE_CLIENT) {
            if (engine->state != MBC_ENGINE_STATE_WAIT_RESPONSE) {
                enter_state(engine, MBC_ENGINE_STATE_WAIT_RESPONSE);
            }
        } else {
            if (engine->state == MBC_ENGINE_STATE_IDLE) {
                enter_state(engine, MBC_ENGINE_STATE_RECEIVING);
            }
        }

        if (engine->expected_length == 0U) {
            const size_t expected = determine_expected_length(engine);
            if (expected == MBC_EXPECTED_UNSUPPORTED) {
                reset_rx_buffer(engine);
                emit_event(engine, MBC_ENGINE_EVENT_STEP_END);
                emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "step_unsupported_function",
                                 MBC_STATUS_UNSUPPORTED);
                return MBC_STATUS_UNSUPPORTED;
            }
            engine->expected_length = expected;
        }

        if (engine->expected_length > 0U && engine->rx_length >= engine->expected_length) {
            mbc_pdu_t decoded = {0};
            mbc_status_t decode_status;

            size_t frame_length = engine->expected_length;

            /* TCP mode: unwrap MBAP first */
            if (engine->framing == MBC_FRAMING_TCP) {
                mbc_mbap_header_t mbap_header;
                const uint8_t* pdu_data = NULL;
                size_t pdu_length = 0;

                decode_status = mbc_mbap_decode(engine->rx_buffer, frame_length, &mbap_header,
                                                &pdu_data, &pdu_length);
                reset_rx_buffer(engine);

                if (!mbc_status_is_ok(decode_status)) {
                    emit_event(engine, MBC_ENGINE_EVENT_STEP_END);
                    enter_state(engine, MBC_ENGINE_STATE_IDLE);
                    emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "mbap_decode_failed",
                                     decode_status);
                    return decode_status;
                }

                /* Reconstruct PDU with unit ID from MBAP header */
                if (pdu_length == 0 || pdu_length > MBC_PDU_MAX + 1) {
                    emit_event(engine, MBC_ENGINE_EVENT_STEP_END);
                    enter_state(engine, MBC_ENGINE_STATE_IDLE);
                    emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "pdu_length_invalid",
                                     MBC_STATUS_DECODING_ERROR);
                    return MBC_STATUS_DECODING_ERROR;
                }

                decoded.unit_id = mbap_header.unit_id;
                decoded.function = pdu_data[0];
                decoded.payload_length = pdu_length - 1;
                if (decoded.payload_length > 0) {
                    memcpy(decoded.payload, &pdu_data[1], decoded.payload_length);
                }
                engine->last_mbap_header = mbap_header;
                engine->last_mbap_valid = true;
            } else {
                if (frame_length < 4U) {
                    reset_rx_buffer(engine);
                    emit_event(engine, MBC_ENGINE_EVENT_STEP_END);
                    enter_state(engine, MBC_ENGINE_STATE_IDLE);
                    emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "rtu_frame_too_short",
                                     MBC_STATUS_DECODING_ERROR);
                    return MBC_STATUS_DECODING_ERROR;
                }

                if (!mbc_crc16_validate(engine->rx_buffer, frame_length)) {
                    reset_rx_buffer(engine);
                    emit_event(engine, MBC_ENGINE_EVENT_STEP_END);
                    enter_state(engine, MBC_ENGINE_STATE_IDLE);
                    emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "crc_validation_failed",
                                     MBC_STATUS_DECODING_ERROR);
                    return MBC_STATUS_DECODING_ERROR;
                }

                const size_t pdu_length = frame_length - 2U;
                decode_status = mbc_pdu_decode(engine->rx_buffer, pdu_length, &decoded);
                reset_rx_buffer(engine);

                if (!mbc_status_is_ok(decode_status)) {
                    emit_event(engine, MBC_ENGINE_EVENT_STEP_END);
                    enter_state(engine, MBC_ENGINE_STATE_IDLE);
                    emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "rtu_decode_failed",
                                     decode_status);
                    return decode_status;
                }
            }

            engine->current_pdu = decoded;
            engine->pdu_ready = true;
            emit_event(engine, MBC_ENGINE_EVENT_PDU_READY);

            enter_state(engine, MBC_ENGINE_STATE_IDLE);

            break;
        }
    }

    emit_event(engine, MBC_ENGINE_EVENT_STEP_END);

    if (!mbc_status_is_ok(status)) {
        emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "step_receive_failed", status);
        enter_state(engine, MBC_ENGINE_STATE_IDLE);
        return status;
    }

    if (engine->role == MBC_ENGINE_ROLE_CLIENT && engine->state == MBC_ENGINE_STATE_WAIT_RESPONSE) {
        const uint64_t now = mbc_transport_now(&engine->transport);
        if (now - engine->last_activity_ms >= engine->response_timeout_ms) {
            emit_event(engine, MBC_ENGINE_EVENT_TIMEOUT);
            enter_state(engine, MBC_ENGINE_STATE_IDLE);
            emit_diag_status(engine, MBC_DIAG_SEVERITY_WARNING, "response_timeout",
                             MBC_STATUS_TIMEOUT);
            return MBC_STATUS_TIMEOUT;
        }
    }

    return status;
}

mbc_status_t mbc_engine_submit_request(mbc_engine_t* engine, const uint8_t* buffer, size_t length)
{
    if (!mbc_engine_is_ready(engine)) {
        emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "submit_not_ready",
                         MBC_STATUS_NOT_INITIALISED);
        return MBC_STATUS_NOT_INITIALISED;
    }

    if (!buffer || length == 0U) {
        emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "submit_invalid_buffer",
                         MBC_STATUS_INVALID_ARGUMENT);
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (engine->role == MBC_ENGINE_ROLE_CLIENT) {
        if (engine->state != MBC_ENGINE_STATE_IDLE) {
            emit_diag_status(engine, MBC_DIAG_SEVERITY_WARNING, "submit_busy", MBC_STATUS_BUSY);
            return MBC_STATUS_BUSY;
        }
    } else {
        if (engine->state != MBC_ENGINE_STATE_IDLE && engine->state != MBC_ENGINE_STATE_RECEIVING) {
            emit_diag_status(engine, MBC_DIAG_SEVERITY_WARNING, "submit_busy", MBC_STATUS_BUSY);
            return MBC_STATUS_BUSY;
        }
    }

    mbc_transport_io_t io = {0};

    mbc_engine_state_t previous = engine->state;
    enter_state(engine, MBC_ENGINE_STATE_SENDING);

    const uint8_t* tx_buffer = buffer;
    size_t tx_length = length;
    uint8_t frame_with_crc[sizeof(engine->rx_buffer)];

    if (engine->framing == MBC_FRAMING_RTU) {
        if (length + 2U > sizeof(frame_with_crc)) {
            enter_state(engine, previous);
            emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "submit_frame_too_large",
                             MBC_STATUS_NO_RESOURCES);
            return MBC_STATUS_NO_RESOURCES;
        }
        memcpy(frame_with_crc, buffer, length);
        uint16_t crc = mbc_crc16(buffer, length);
        frame_with_crc[length] = (uint8_t)(crc & 0xFFU);
        frame_with_crc[length + 1U] = (uint8_t)((crc >> 8) & 0xFFU);
        tx_buffer = frame_with_crc;
        tx_length = length + 2U;
    }

    mbc_status_t status = mbc_transport_send(&engine->transport, tx_buffer, tx_length, &io);
    if (!mbc_status_is_ok(status)) {
        enter_state(engine, previous);
        emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "transport_send_failed", status);
        return status;
    }

    /* Check if all bytes were sent */
    if (io.processed != tx_length) {
        enter_state(engine, previous);
        emit_diag_status(engine, MBC_DIAG_SEVERITY_ERROR, "transport_partial_send",
                         MBC_STATUS_IO_ERROR);
        return MBC_STATUS_IO_ERROR; /* Partial send is an error in this implementation */
    }

    emit_event(engine, MBC_ENGINE_EVENT_TX_SENT);

    enter_state(engine, (engine->role == MBC_ENGINE_ROLE_CLIENT) ? MBC_ENGINE_STATE_WAIT_RESPONSE
                                                                 : MBC_ENGINE_STATE_IDLE);
    reset_rx_buffer(engine);
    return MBC_STATUS_OK;
}

bool mbc_engine_take_pdu(mbc_engine_t* engine, mbc_pdu_t* out)
{
    if (!engine || !out || !engine->pdu_ready) {
        return false;
    }

    *out = engine->current_pdu;
    engine->pdu_ready = false;
    return true;
}

bool mbc_engine_last_mbap_header(const mbc_engine_t* engine, mbc_mbap_header_t* out)
{
    if (!engine || !out || !engine->last_mbap_valid) {
        return false;
    }
    *out = engine->last_mbap_header;
    return true;
}

/**
 * @brief Emit an event to the registered callback.
 *
 * @param engine Engine instance
 * @param type Event type
 */
static void emit_event(mbc_engine_t* engine, mbc_engine_event_type_t type)
{
    const uint64_t timestamp = mbc_transport_now(&engine->transport);

    if (engine->event_cb) {
        mbc_engine_event_t evt = {
            .type = type,
            .timestamp_ms = timestamp,
        };
        engine->event_cb(engine->event_ctx, &evt);
    }

    const char* event_label = engine_event_name(type);
    const char* state_label = engine_state_name(engine->state);
    mbc_diag_kv_t fields[2];
    size_t field_count = 0U;

    if (event_label) {
        fields[field_count++] = (mbc_diag_kv_t){.key = "event", .value = event_label};
    }
    if (state_label) {
        fields[field_count++] = (mbc_diag_kv_t){.key = "state", .value = state_label};
    }

    emit_diag(engine, event_severity(type), "engine_event", 0U, fields, field_count);
}

/**
 * @brief Enter a new FSM state and emit state change event.
 *
 * @param engine Engine instance
 * @param next New state
 */
static void enter_state(mbc_engine_t* engine, mbc_engine_state_t next)
{
    const mbc_engine_state_t previous = engine->state;
    engine->state = next;
    engine->last_activity_ms = mbc_transport_now(&engine->transport);
    emit_state_transition_diag(engine, previous, next);
    emit_event(engine, MBC_ENGINE_EVENT_STATE_CHANGE);
}

/**
 * @brief Reset receive buffer state.
 *
 * @param engine Engine instance
 */
static void reset_rx_buffer(mbc_engine_t* engine)
{
    engine->rx_length = 0U;
    engine->expected_length = 0U;
}

/**
 * @brief Determine expected frame length based on current buffer content.
 *
 * This function analyzes the partial frame in the RX buffer and determines
 * how many total bytes are expected for a complete frame. The logic differs
 * between RTU and TCP (MBAP) framing modes.
 *
 * @param engine Engine instance
 * @return Expected frame length in bytes, 0 if not enough data yet,
 *         MBC_EXPECTED_UNSUPPORTED if unsupported function code
 */
static size_t determine_expected_length(const mbc_engine_t* engine)
{
    /* TCP mode uses MBAP framing */
    if (engine->framing == MBC_FRAMING_TCP) {
        return mbc_mbap_expected_length(engine->rx_buffer, engine->rx_length);
    }

    /* RTU mode */
    if (engine->rx_length < 2U) {
        return 0U;
    }

    const uint8_t function = engine->rx_buffer[1];
    const bool exception = (function & 0x80U) != 0U;
    const uint8_t base = function & 0x7FU;

    if (exception) {
        return (engine->rx_length >= 3U) ? 5U : 0U;
    }

    if (engine->role == MBC_ENGINE_ROLE_SERVER) {
        switch (base) {
        case 0x03:
        case 0x06:
            return 8U;
        case 0x10:
            if (engine->rx_length >= 7U) {
                const uint8_t byte_count = engine->rx_buffer[6];
                return (size_t)(7U + byte_count + 2U);
            }
            return 0U;
        default:
            return MBC_EXPECTED_UNSUPPORTED;
        }
    }

    /* Client waiting for response (RTU mode) */
    switch (base) {
    case 0x03:
        if (engine->rx_length >= 3U) {
            const uint8_t byte_count = engine->rx_buffer[2];
            return (size_t)(3U + byte_count + 2U);
        }
        return 0U;
    case 0x06: /* FC06 and FC16 have same response length */
    case 0x10:
        return 8U;
    default:
        return MBC_EXPECTED_UNSUPPORTED;
    }
}

static void emit_diag(mbc_engine_t* engine, mbc_diag_severity_t severity, const char* message,
                      uint32_t code, const mbc_diag_kv_t* fields, size_t field_count)
{
    if (!engine || !engine->diag.emit) {
        return;
    }

    mbc_diag_event_t evt = {
        .severity = severity,
        .component = ENGINE_DIAG_COMPONENT,
        .message = message,
        .fields = fields,
        .field_count = field_count,
        .code = code,
        .timestamp_ms = mbc_transport_now(&engine->transport),
    };
    engine->diag.emit(engine->diag.ctx, &evt);
}

static uint32_t status_code(mbc_status_t status)
{
    return (status >= 0) ? (uint32_t)status : (uint32_t)(-status);
}

static const char* status_name(mbc_status_t status)
{
    switch (status) {
    case MBC_STATUS_OK:
        return "ok";
    case MBC_STATUS_INVALID_ARGUMENT:
        return "invalid_argument";
    case MBC_STATUS_ALREADY_INITIALISED:
        return "already_initialised";
    case MBC_STATUS_NOT_INITIALISED:
        return "not_initialised";
    case MBC_STATUS_UNSUPPORTED:
        return "unsupported";
    case MBC_STATUS_IO_ERROR:
        return "io_error";
    case MBC_STATUS_BUSY:
        return "busy";
    case MBC_STATUS_NO_RESOURCES:
        return "no_resources";
    case MBC_STATUS_DECODING_ERROR:
        return "decoding_error";
    case MBC_STATUS_TIMEOUT:
        return "timeout";
    default:
        return "unknown";
    }
}

static const char* engine_event_name(mbc_engine_event_type_t type)
{
    switch (type) {
    case MBC_ENGINE_EVENT_RX_READY:
        return "rx_ready";
    case MBC_ENGINE_EVENT_TX_SENT:
        return "tx_sent";
    case MBC_ENGINE_EVENT_STEP_BEGIN:
        return "step_begin";
    case MBC_ENGINE_EVENT_STEP_END:
        return "step_end";
    case MBC_ENGINE_EVENT_STATE_CHANGE:
        return "state_change";
    case MBC_ENGINE_EVENT_PDU_READY:
        return "pdu_ready";
    case MBC_ENGINE_EVENT_TIMEOUT:
        return "timeout";
    default:
        return "unknown";
    }
}

static const char* engine_state_name(mbc_engine_state_t state)
{
    switch (state) {
    case MBC_ENGINE_STATE_IDLE:
        return "idle";
    case MBC_ENGINE_STATE_RECEIVING:
        return "receiving";
    case MBC_ENGINE_STATE_SENDING:
        return "sending";
    case MBC_ENGINE_STATE_WAIT_RESPONSE:
        return "wait_response";
    default:
        return "unknown";
    }
}

static const char* engine_role_name(mbc_engine_role_t role)
{
    switch (role) {
    case MBC_ENGINE_ROLE_CLIENT:
        return "client";
    case MBC_ENGINE_ROLE_SERVER:
        return "server";
    default:
        return "unknown";
    }
}

static const char* framing_mode_name(mbc_framing_mode_t mode)
{
    switch (mode) {
    case MBC_FRAMING_RTU:
        return "rtu";
    case MBC_FRAMING_TCP:
        return "tcp";
    default:
        return "unknown";
    }
}

static mbc_diag_severity_t event_severity(mbc_engine_event_type_t type)
{
    switch (type) {
    case MBC_ENGINE_EVENT_TX_SENT:
    case MBC_ENGINE_EVENT_PDU_READY:
        return MBC_DIAG_SEVERITY_INFO;
    case MBC_ENGINE_EVENT_TIMEOUT:
        return MBC_DIAG_SEVERITY_WARNING;
    case MBC_ENGINE_EVENT_STATE_CHANGE:
        return MBC_DIAG_SEVERITY_DEBUG;
    case MBC_ENGINE_EVENT_RX_READY:
    case MBC_ENGINE_EVENT_STEP_BEGIN:
    case MBC_ENGINE_EVENT_STEP_END:
    default:
        return MBC_DIAG_SEVERITY_TRACE;
    }
}

static void emit_diag_status(mbc_engine_t* engine, mbc_diag_severity_t severity,
                             const char* message, mbc_status_t status)
{
    const char* status_label = status_name(status);
    const char* state_label = engine ? engine_state_name(engine->state) : NULL;

    mbc_diag_kv_t fields[2];
    size_t field_count = 0U;

    if (status_label) {
        fields[field_count++] = (mbc_diag_kv_t){.key = "status", .value = status_label};
    }
    if (state_label) {
        fields[field_count++] = (mbc_diag_kv_t){.key = "state", .value = state_label};
    }

    emit_diag(engine, severity, message, status_code(status), fields, field_count);
}

static void emit_state_transition_diag(mbc_engine_t* engine, mbc_engine_state_t from,
                                       mbc_engine_state_t to)
{
    const char* from_label = engine_state_name(from);
    const char* to_label = engine_state_name(to);

    mbc_diag_kv_t fields[2];
    size_t field_count = 0U;

    if (from_label) {
        fields[field_count++] = (mbc_diag_kv_t){.key = "from", .value = from_label};
    }
    if (to_label) {
        fields[field_count++] = (mbc_diag_kv_t){.key = "to", .value = to_label};
    }

    emit_diag(engine, MBC_DIAG_SEVERITY_DEBUG, "state_transition", 0U, fields, field_count);
}

static void emit_engine_init_diag(mbc_engine_t* engine)
{
    if (!engine) {
        return;
    }

    const char* role_label = engine_role_name(engine->role);
    const char* framing_label = framing_mode_name(engine->framing);

    char timeout_buf[16];
    const int written =
        snprintf(timeout_buf, sizeof(timeout_buf), "%u", (unsigned)engine->response_timeout_ms);
    const char* timeout_label =
        (written > 0 && written < (int)sizeof(timeout_buf)) ? timeout_buf : NULL;

    mbc_diag_kv_t fields[3];
    size_t field_count = 0U;

    if (role_label) {
        fields[field_count++] = (mbc_diag_kv_t){.key = "role", .value = role_label};
    }
    if (framing_label) {
        fields[field_count++] = (mbc_diag_kv_t){.key = "framing", .value = framing_label};
    }
    if (timeout_label) {
        fields[field_count++] =
            (mbc_diag_kv_t){.key = "response_timeout_ms", .value = timeout_label};
    }

    emit_diag(engine, MBC_DIAG_SEVERITY_INFO, "engine_initialised", 0U, fields, field_count);
}
