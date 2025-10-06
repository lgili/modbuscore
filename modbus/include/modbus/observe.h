/**
 * @file observe.h
 * @brief Observability primitives (events, diagnostics and tracing controls).
 */

#ifndef MODBUS_OBSERVE_H
#define MODBUS_OBSERVE_H

#include <stdbool.h>
#include <stddef.h>

#include <modbus/frame.h>
#include <modbus/mb_err.h>
#include <modbus/mb_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Identifies the component emitting an observability event. */
typedef enum {
    MB_EVENT_SOURCE_CLIENT = 0,
    MB_EVENT_SOURCE_SERVER = 1
} mb_event_source_t;

/** @brief Event categories surfaced through the observability callback. */
typedef enum {
    MB_EVENT_CLIENT_STATE_ENTER = 0,
    MB_EVENT_CLIENT_STATE_EXIT,
    MB_EVENT_CLIENT_TX_SUBMIT,
    MB_EVENT_CLIENT_TX_COMPLETE,
    MB_EVENT_SERVER_STATE_ENTER,
    MB_EVENT_SERVER_STATE_EXIT,
    MB_EVENT_SERVER_REQUEST_ACCEPT,
    MB_EVENT_SERVER_REQUEST_COMPLETE
} mb_event_type_t;

/** @brief Payload carried by an observability event. */
typedef struct mb_event {
    mb_event_source_t source;
    mb_event_type_t type;
    mb_time_ms_t timestamp;
    union {
        struct {
            mb_u8 state; /**< Client state identifier (see @ref mb_client_state_t). */
        } client_state;
        struct {
            mb_u8 state; /**< Server state identifier. */
        } server_state;
        struct {
            mb_u8 function;      /**< Function code associated with the transaction. */
            mb_err_t status;     /**< Completion status (MB_OK on success). */
            bool expect_response;/**< Whether the transaction expects a response. */
        } client_txn;
        struct {
            mb_u8 function;  /**< Function code of the request. */
            bool broadcast;  /**< True when the request was a broadcast (unit id == 0). */
            mb_err_t status; /**< Resulting status (exception or transport error). */
        } server_req;
    } data;
} mb_event_t;

/** @brief Callback signature used to surface observability events. */
typedef void (*mb_event_callback_t)(const mb_event_t *event, void *user_ctx);

/** @brief Canonical buckets used for error diagnostics counters. */
typedef enum {
    MB_DIAG_ERR_SLOT_OK = 0,
    MB_DIAG_ERR_SLOT_INVALID_ARGUMENT,
    MB_DIAG_ERR_SLOT_TIMEOUT,
    MB_DIAG_ERR_SLOT_TRANSPORT,
    MB_DIAG_ERR_SLOT_CRC,
    MB_DIAG_ERR_SLOT_INVALID_REQUEST,
    MB_DIAG_ERR_SLOT_OTHER_REQUESTS,
    MB_DIAG_ERR_SLOT_OTHER,
    MB_DIAG_ERR_SLOT_CANCELLED,
    MB_DIAG_ERR_SLOT_NO_RESOURCES,
    MB_DIAG_ERR_SLOT_EXCEPTION_ILLEGAL_FUNCTION,
    MB_DIAG_ERR_SLOT_EXCEPTION_ILLEGAL_DATA_ADDRESS,
    MB_DIAG_ERR_SLOT_EXCEPTION_ILLEGAL_DATA_VALUE,
    MB_DIAG_ERR_SLOT_EXCEPTION_SERVER_DEVICE_FAILURE,
    MB_DIAG_ERR_SLOT_EXCEPTION_ACKNOWLEDGE,
    MB_DIAG_ERR_SLOT_EXCEPTION_SERVER_DEVICE_BUSY,
    MB_DIAG_ERR_SLOT_EXCEPTION_NEGATIVE_ACKNOWLEDGE,
    MB_DIAG_ERR_SLOT_EXCEPTION_MEMORY_PARITY_ERROR,
    MB_DIAG_ERR_SLOT_EXCEPTION_GATEWAY_PATH_UNAVAILABLE,
    MB_DIAG_ERR_SLOT_EXCEPTION_GATEWAY_TARGET_DEVICE_FAILED,
    MB_DIAG_ERR_SLOT_MAX
} mb_diag_err_slot_t;

/** @brief Aggregated counters grouped by function code and error bucket. */
typedef struct {
    mb_u64 function[256];
    mb_u64 error[MB_DIAG_ERR_SLOT_MAX];
} mb_diag_counters_t;

void mb_diag_reset(mb_diag_counters_t *diag);
void mb_diag_record_fc(mb_diag_counters_t *diag, mb_u8 function);
void mb_diag_record_error(mb_diag_counters_t *diag, mb_err_t err);
mb_diag_err_slot_t mb_diag_slot_from_error(mb_err_t err);
const char *mb_diag_err_slot_str(mb_diag_err_slot_t slot);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_OBSERVE_H */
