/**
 * @file autoheal.c
 * @brief Implementation of automatic recovery supervisor.
 */

#include <modbuscore/runtime/autoheal.h>
#include <modbuscore/runtime/diagnostics.h>
#include <modbuscore/runtime/runtime.h>
#include <modbuscore/transport/iface.h>
#include <stdio.h>
#include <string.h>

#define AUTOHEAL_COMPONENT "runtime.autoheal"

static uint64_t autoheal_now(const mbc_autoheal_supervisor_t* supervisor)
{
    if (!supervisor || !supervisor->deps || !supervisor->deps->clock.now_ms) {
        return 0ULL;
    }
    return supervisor->deps->clock.now_ms(supervisor->deps->clock.ctx);
}

static void emit_diag(mbc_autoheal_supervisor_t* supervisor, mbc_diag_severity_t severity,
                      const char* message, uint32_t code, const mbc_diag_kv_t* fields,
                      size_t field_count)
{
    if (!supervisor || !supervisor->diag.emit) {
        return;
    }

    mbc_diag_event_t evt = {
        .severity = severity,
        .component = AUTOHEAL_COMPONENT,
        .message = message,
        .fields = fields,
        .field_count = field_count,
        .code = code,
        .timestamp_ms = autoheal_now(supervisor),
    };
    supervisor->diag.emit(supervisor->diag.ctx, &evt);
}

static void notify_observer(mbc_autoheal_supervisor_t* supervisor, mbc_autoheal_event_t event)
{
    if (supervisor && supervisor->config.observer) {
        supervisor->config.observer(supervisor->config.observer_ctx, event);
    }
}

static void reset_internal_state(mbc_autoheal_supervisor_t* supervisor, bool preserve_request)
{
    if (!supervisor) {
        return;
    }

    supervisor->waiting_response = false;
    supervisor->retry_count = 0U;
    supervisor->attempt_count = 0U;
    supervisor->current_backoff_ms = supervisor->config.initial_backoff_ms;
    supervisor->next_retry_ms = 0U;
    supervisor->last_pdu_valid = false;
    supervisor->last_status = (uint8_t)MBC_STATUS_OK;
    supervisor->closed_since_last_attempt = true;

    if (!preserve_request) {
        supervisor->request_valid = false;
        supervisor->request_length = 0U;
    }
}

static void close_circuit(mbc_autoheal_supervisor_t* supervisor)
{
    if (!supervisor || !supervisor->circuit_open) {
        return;
    }
    supervisor->circuit_open = false;
    supervisor->circuit_release_ms = 0ULL;
    emit_diag(supervisor, MBC_DIAG_SEVERITY_INFO, "circuit_closed", 0U, NULL, 0U);
    notify_observer(supervisor, MBC_AUTOHEAL_EVENT_CIRCUIT_CLOSED);
}

static void open_circuit(mbc_autoheal_supervisor_t* supervisor)
{
    if (!supervisor || supervisor->circuit_open) {
        return;
    }

    supervisor->circuit_open = true;
    supervisor->circuit_release_ms = autoheal_now(supervisor) + supervisor->config.cooldown_ms;
    supervisor->waiting_response = false;
    supervisor->request_valid = false;
    supervisor->next_retry_ms = 0ULL;

    emit_diag(supervisor, MBC_DIAG_SEVERITY_WARNING, "circuit_open", 0U, NULL, 0U);
    notify_observer(supervisor, MBC_AUTOHEAL_EVENT_CIRCUIT_OPEN);
}

static void schedule_retry(mbc_autoheal_supervisor_t* supervisor, mbc_status_t failure_status)
{
    if (!supervisor) {
        return;
    }

    supervisor->waiting_response = false;
    supervisor->last_status = (uint8_t)failure_status;

    supervisor->retry_count++;
    supervisor->attempt_count++;

    if (supervisor->retry_count > supervisor->config.max_retries) {
        emit_diag(supervisor, MBC_DIAG_SEVERITY_ERROR, "retries_exhausted",
                  (uint32_t)(-failure_status), NULL, 0U);
        notify_observer(supervisor, MBC_AUTOHEAL_EVENT_GIVE_UP);
        open_circuit(supervisor);
        return;
    }

    if (supervisor->current_backoff_ms == 0U) {
        supervisor->current_backoff_ms = supervisor->config.initial_backoff_ms;
        if (supervisor->current_backoff_ms == 0U) {
            supervisor->current_backoff_ms = 1U;
        }
    } else {
        uint64_t doubled = (uint64_t)supervisor->current_backoff_ms * 2ULL;
        supervisor->current_backoff_ms = (uint32_t)((doubled > supervisor->config.max_backoff_ms)
                                                        ? supervisor->config.max_backoff_ms
                                                        : doubled);
    }

    supervisor->next_retry_ms = autoheal_now(supervisor) + supervisor->current_backoff_ms;

    char retry_buf[16];
    char delay_buf[16];
    snprintf(retry_buf, sizeof(retry_buf), "%u", supervisor->retry_count);
    snprintf(delay_buf, sizeof(delay_buf), "%u", supervisor->current_backoff_ms);

    mbc_diag_kv_t fields[2] = {
        {.key = "retry", .value = retry_buf},
        {.key = "delay_ms", .value = delay_buf},
    };
    emit_diag(supervisor, MBC_DIAG_SEVERITY_INFO, "retry_scheduled", (uint32_t)(-failure_status),
              fields, 2U);
    notify_observer(supervisor, MBC_AUTOHEAL_EVENT_RETRY_SCHEDULED);
}

static mbc_status_t attempt_send(mbc_autoheal_supervisor_t* supervisor)
{
    if (!supervisor || !supervisor->request_valid || supervisor->circuit_open) {
        return MBC_STATUS_BUSY;
    }

    if (supervisor->waiting_response) {
        return MBC_STATUS_BUSY;
    }

    mbc_status_t status = mbc_engine_submit_request(supervisor->engine, supervisor->request_buffer,
                                                    supervisor->request_length);
    if (mbc_status_is_ok(status)) {
        supervisor->waiting_response = true;
        supervisor->attempt_count++;
        supervisor->closed_since_last_attempt = false;
        supervisor->next_retry_ms = 0ULL;

        char attempt_buf[16];
        snprintf(attempt_buf, sizeof(attempt_buf), "%u", supervisor->attempt_count);
        mbc_diag_kv_t fields = {.key = "attempt", .value = attempt_buf};
        emit_diag(supervisor, MBC_DIAG_SEVERITY_INFO, "attempt_started", 0U, &fields, 1U);
        notify_observer(supervisor, MBC_AUTOHEAL_EVENT_ATTEMPT);
        return MBC_STATUS_OK;
    }

    if (status == MBC_STATUS_BUSY) {
        uint32_t defer_ms =
            supervisor->config.initial_backoff_ms > 0U ? supervisor->config.initial_backoff_ms : 1U;
        supervisor->next_retry_ms = autoheal_now(supervisor) + defer_ms;
        supervisor->current_backoff_ms = defer_ms;

        char delay_buf[16];
        snprintf(delay_buf, sizeof(delay_buf), "%u", defer_ms);
        mbc_diag_kv_t fields = {.key = "delay_ms", .value = delay_buf};
        emit_diag(supervisor, MBC_DIAG_SEVERITY_DEBUG, "retry_deferred_busy", 0U, &fields, 1U);
        return status;
    }

    schedule_retry(supervisor, status);
    return status;
}

static void handle_successful_response(mbc_autoheal_supervisor_t* supervisor, mbc_pdu_t* pdu)
{
    if (!supervisor) {
        return;
    }
    supervisor->waiting_response = false;
    supervisor->request_valid = false;
    supervisor->last_pdu = *pdu;
    supervisor->last_pdu_valid = true;
    supervisor->retry_count = 0U;
    supervisor->current_backoff_ms = supervisor->config.initial_backoff_ms;
    supervisor->next_retry_ms = 0ULL;
    supervisor->attempt_count = 0U;
    supervisor->closed_since_last_attempt = true;

    emit_diag(supervisor, MBC_DIAG_SEVERITY_INFO, "response_success", 0U, NULL, 0U);
    notify_observer(supervisor, MBC_AUTOHEAL_EVENT_RESPONSE_OK);
}

mbc_status_t mbc_autoheal_init(mbc_autoheal_supervisor_t* supervisor,
                               const mbc_autoheal_config_t* config, mbc_engine_t* engine)
{
    if (!supervisor || !config || !config->runtime || !engine) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!mbc_runtime_is_ready(config->runtime)) {
        return MBC_STATUS_NOT_INITIALISED;
    }

    const mbc_runtime_config_t* deps = mbc_runtime_dependencies(config->runtime);
    if (!deps || !deps->allocator.alloc || !deps->allocator.free) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (config->max_retries == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    size_t capacity = config->request_capacity > 0U ? config->request_capacity : 260U;
    uint8_t* buffer = deps->allocator.alloc(deps->allocator.ctx, capacity);
    if (!buffer) {
        return MBC_STATUS_NO_RESOURCES;
    }

    *supervisor = (mbc_autoheal_supervisor_t){
        .engine = engine,
        .runtime = config->runtime,
        .deps = deps,
        .diag = deps->diag,
        .config = *config,
        .request_buffer = buffer,
        .request_capacity = capacity,
    };

    if (supervisor->config.max_backoff_ms == 0U) {
        supervisor->config.max_backoff_ms =
            supervisor->config.initial_backoff_ms > 0U ? supervisor->config.initial_backoff_ms : 1U;
    }
    if (supervisor->config.max_backoff_ms < supervisor->config.initial_backoff_ms) {
        supervisor->config.max_backoff_ms = supervisor->config.initial_backoff_ms;
    }

    reset_internal_state(supervisor, false);
    emit_diag(supervisor, MBC_DIAG_SEVERITY_INFO, "autoheal_initialised", 0U, NULL, 0U);
    return MBC_STATUS_OK;
}

void mbc_autoheal_shutdown(mbc_autoheal_supervisor_t* supervisor)
{
    if (!supervisor) {
        return;
    }

    if (supervisor->deps && supervisor->request_buffer) {
        supervisor->deps->allocator.free(supervisor->deps->allocator.ctx,
                                         supervisor->request_buffer);
    }

    memset(supervisor, 0, sizeof(*supervisor));
}

mbc_status_t mbc_autoheal_submit(mbc_autoheal_supervisor_t* supervisor, const uint8_t* frame,
                                 size_t length)
{
    if (!supervisor || !frame || length == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (supervisor->circuit_open) {
        uint64_t now = autoheal_now(supervisor);
        if (now >= supervisor->circuit_release_ms) {
            close_circuit(supervisor);
        } else {
            emit_diag(supervisor, MBC_DIAG_SEVERITY_WARNING, "submit_rejected_circuit_open", 0U,
                      NULL, 0U);
            return MBC_STATUS_BUSY;
        }
    }

    if (length > supervisor->request_capacity) {
        emit_diag(supervisor, MBC_DIAG_SEVERITY_ERROR, "submit_too_large", 0U, NULL, 0U);
        return MBC_STATUS_NO_RESOURCES;
    }

    memcpy(supervisor->request_buffer, frame, length);
    supervisor->request_length = length;
    supervisor->request_valid = true;
    supervisor->retry_count = 0U;
    supervisor->attempt_count = 0U;
    supervisor->current_backoff_ms = supervisor->config.initial_backoff_ms;
    supervisor->next_retry_ms = 0ULL;
    supervisor->last_pdu_valid = false;

    return attempt_send(supervisor);
}

mbc_status_t mbc_autoheal_step(mbc_autoheal_supervisor_t* supervisor, size_t budget)
{
    if (!supervisor) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    uint64_t now = autoheal_now(supervisor);
    if (supervisor->circuit_open && now >= supervisor->circuit_release_ms) {
        close_circuit(supervisor);
    }

    mbc_status_t engine_status = MBC_STATUS_OK;

    if (supervisor->waiting_response) {
        engine_status = mbc_engine_step(supervisor->engine, budget);

        if (!mbc_status_is_ok(engine_status)) {
            schedule_retry(supervisor, engine_status);
        }

        if (!supervisor->circuit_open) {
            mbc_pdu_t pdu = {0};
            while (mbc_engine_take_pdu(supervisor->engine, &pdu)) {
                handle_successful_response(supervisor, &pdu);
            }
        }
    }

    if (!supervisor->waiting_response && supervisor->request_valid && !supervisor->circuit_open) {
        if (supervisor->next_retry_ms != 0ULL && now >= supervisor->next_retry_ms) {
            attempt_send(supervisor);
        }
    }

    return engine_status;
}

bool mbc_autoheal_take_pdu(mbc_autoheal_supervisor_t* supervisor, mbc_pdu_t* out)
{
    if (!supervisor || !out || !supervisor->last_pdu_valid) {
        return false;
    }

    *out = supervisor->last_pdu;
    supervisor->last_pdu_valid = false;
    return true;
}

mbc_autoheal_state_t mbc_autoheal_state(const mbc_autoheal_supervisor_t* supervisor)
{
    if (!supervisor) {
        return MBC_AUTOHEAL_STATE_IDLE;
    }
    if (supervisor->circuit_open) {
        return MBC_AUTOHEAL_STATE_CIRCUIT_OPEN;
    }
    if (supervisor->waiting_response) {
        return MBC_AUTOHEAL_STATE_WAITING;
    }
    if (supervisor->request_valid && supervisor->next_retry_ms != 0ULL) {
        return MBC_AUTOHEAL_STATE_SCHEDULED;
    }
    return supervisor->request_valid ? MBC_AUTOHEAL_STATE_WAITING : MBC_AUTOHEAL_STATE_IDLE;
}

bool mbc_autoheal_is_circuit_open(const mbc_autoheal_supervisor_t* supervisor)
{
    return supervisor ? supervisor->circuit_open : false;
}

uint32_t mbc_autoheal_retry_count(const mbc_autoheal_supervisor_t* supervisor)
{
    return supervisor ? supervisor->retry_count : 0U;
}

void mbc_autoheal_reset(mbc_autoheal_supervisor_t* supervisor)
{
    if (!supervisor) {
        return;
    }
    close_circuit(supervisor);
    reset_internal_state(supervisor, false);
    emit_diag(supervisor, MBC_DIAG_SEVERITY_INFO, "autoheal_reset", 0U, NULL, 0U);
}
