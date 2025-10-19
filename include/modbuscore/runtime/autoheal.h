#ifndef MODBUSCORE_RUNTIME_AUTOHEAL_H
#define MODBUSCORE_RUNTIME_AUTOHEAL_H

/**
 * @file autoheal.h
 * @brief Automatic recovery supervision (retries/backoff/circuit breaker).
 *
 * This module monitors a client mbc_engine_t and resubmits requests when
 * transient failures occur (timeouts, I/O errors, busy). It implements simple
 * exponential backoff and opens a circuit breaker when the maximum number of
 * retries is exceeded, waiting for a cooldown period before allowing new
 * submissions. All telemetry uses the structured sink (`mbc_diag_sink`).
 */

#include <modbuscore/common/status.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/runtime/runtime.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Events of interest exposed by the supervisor.
 */
typedef enum mbc_autoheal_event {
    MBC_AUTOHEAL_EVENT_ATTEMPT,         /**< A send attempt was initiated */
    MBC_AUTOHEAL_EVENT_RETRY_SCHEDULED, /**< Retry scheduled (backoff in progress) */
    MBC_AUTOHEAL_EVENT_RESPONSE_OK,     /**< Successful response received */
    MBC_AUTOHEAL_EVENT_GIVE_UP,         /**< All retries exhausted */
    MBC_AUTOHEAL_EVENT_CIRCUIT_OPEN,    /**< Circuit breaker opened */
    MBC_AUTOHEAL_EVENT_CIRCUIT_CLOSED   /**< Circuit breaker closed (supervisor ready) */
} mbc_autoheal_event_t;

/**
 * @brief Callback function to observe supervisor events.
 */
typedef void (*mbc_autoheal_observer_fn)(void* ctx, mbc_autoheal_event_t event);

/**
 * @brief Auto-heal supervisor configuration.
 */
typedef struct mbc_autoheal_config {
    const mbc_runtime_t* runtime;      /**< Associated runtime (for clock/alloc/diag) */
    uint32_t max_retries;              /**< Maximum retries before opening circuit */
    uint32_t initial_backoff_ms;       /**< Initial backoff in ms (0 => immediate retry) */
    uint32_t max_backoff_ms;           /**< Upper limit for exponential backoff */
    uint32_t cooldown_ms;              /**< Time in ms with circuit open before rearming */
    size_t request_capacity;           /**< Maximum stored frame size */
    mbc_autoheal_observer_fn observer; /**< Optional event observer */
    void* observer_ctx;                /**< Context passed to observer */
} mbc_autoheal_config_t;

/**
 * @brief Internal states visible for inspection.
 */
typedef enum mbc_autoheal_state {
    MBC_AUTOHEAL_STATE_IDLE,        /**< No pending request */
    MBC_AUTOHEAL_STATE_WAITING,     /**< Waiting for engine response */
    MBC_AUTOHEAL_STATE_SCHEDULED,   /**< Retry scheduled (waiting for backoff) */
    MBC_AUTOHEAL_STATE_CIRCUIT_OPEN /**< Circuit breaker open (cooldown active) */
} mbc_autoheal_state_t;

/**
 * @brief Opaque supervisor structure.
 */
typedef struct mbc_autoheal_supervisor {
    mbc_engine_t* engine;
    const mbc_runtime_t* runtime;
    const mbc_runtime_config_t* deps;
    mbc_diag_sink_iface_t diag;
    mbc_autoheal_config_t config;
    uint8_t* request_buffer;
    size_t request_length;
    size_t request_capacity;
    uint32_t retry_count;
    uint32_t attempt_count;
    uint32_t current_backoff_ms;
    uint64_t next_retry_ms;
    uint64_t circuit_release_ms;
    uint8_t last_status;
    bool waiting_response;
    bool request_valid;
    bool circuit_open;
    bool closed_since_last_attempt;
    mbc_pdu_t last_pdu;
    bool last_pdu_valid;
} mbc_autoheal_supervisor_t;

/**
 * @brief Initialize the auto-heal supervisor.
 *
 * @param supervisor Structure to be initialized
 * @param config Configuration (must not be NULL)
 * @param engine Already initialized engine (client)
 * @return MBC_STATUS_OK or validation/allocation error
 */
mbc_status_t mbc_autoheal_init(mbc_autoheal_supervisor_t* supervisor,
                               const mbc_autoheal_config_t* config, mbc_engine_t* engine);

/**
 * @brief Shutdown the supervisor, releasing resources.
 */
void mbc_autoheal_shutdown(mbc_autoheal_supervisor_t* supervisor);

/**
 * @brief Submit a complete frame (MBAP/RTU) and start the supervision cycle.
 *
 * @param supervisor Valid supervisor
 * @param frame Request buffer
 * @param length Frame size
 * @return MBC_STATUS_OK on immediate successful submission.
 *         MBC_STATUS_BUSY if circuit is open or waiting for cooldown.
 *         MBC_STATUS_NO_RESOURCES if frame exceeds configured capacity.
 */
mbc_status_t mbc_autoheal_submit(mbc_autoheal_supervisor_t* supervisor, const uint8_t* frame,
                                 size_t length);

/**
 * @brief Execute one supervisor iteration (including mbc_engine_step).
 *
 * @param supervisor Supervisor
 * @param budget Byte budget for engine step
 * @return Last status returned by engine (or MBC_STATUS_OK if only internal handling occurred)
 */
mbc_status_t mbc_autoheal_step(mbc_autoheal_supervisor_t* supervisor, size_t budget);

/**
 * @brief Retrieve the last successfully delivered PDU (if any).
 *
 * @param supervisor Supervisor
 * @param out Output structure
 * @return true if PDU was available, false otherwise
 */
bool mbc_autoheal_take_pdu(mbc_autoheal_supervisor_t* supervisor, mbc_pdu_t* out);

/**
 * @brief Get current state.
 */
mbc_autoheal_state_t mbc_autoheal_state(const mbc_autoheal_supervisor_t* supervisor);

/**
 * @brief Check if circuit is open.
 */
bool mbc_autoheal_is_circuit_open(const mbc_autoheal_supervisor_t* supervisor);

/**
 * @brief Return the number of retries applied in the current session.
 */
uint32_t mbc_autoheal_retry_count(const mbc_autoheal_supervisor_t* supervisor);

/**
 * @brief Clear any pending request, closing circuits and resetting counters.
 *
 * Useful when the user wants to abort the auto-heal flow (e.g., shutdown).
 */
void mbc_autoheal_reset(mbc_autoheal_supervisor_t* supervisor);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_RUNTIME_AUTOHEAL_H */
