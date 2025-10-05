/**
 * @file client.h
 * @brief Non-blocking Modbus client transaction manager (Gate 5).
 */

#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H

#include <stdbool.h>
#include <stddef.h>

#include <modbus/frame.h>
#include <modbus/mb_err.h>
#include <modbus/mb_types.h>
#include <modbus/observe.h>
#include <modbus/transport/rtu.h>
#include <modbus/transport/tcp.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_CLIENT_DEFAULT_TIMEOUT_MS 1000U
#define MB_CLIENT_DEFAULT_RETRY_BACKOFF_MS 500U
#define MB_CLIENT_DEFAULT_WATCHDOG_MS 5000U
#define MB_CLIENT_MAX_TIMEOUT_MS 60000U

struct mb_client;
struct mb_client_txn;

#define MB_CLIENT_REQUEST_NO_RESPONSE   (1u << 0)
#define MB_CLIENT_REQUEST_HIGH_PRIORITY (1u << 1)
#define MB_CLIENT_REQUEST_POISON        (1u << 2)

typedef void (*mb_client_callback_t)(struct mb_client *client,
                                     const struct mb_client_txn *txn,
                                     mb_err_t status,
                                     const mb_adu_view_t *response,
                                     void *user_ctx);

typedef struct mb_client_request {
    uint32_t flags;
    mb_adu_view_t request;
    mb_time_ms_t timeout_ms;
    mb_u8 max_retries;
    mb_time_ms_t retry_backoff_ms;
    mb_client_callback_t callback;
    void *user_ctx;
} mb_client_request_t;

typedef struct mb_client_txn {
    bool in_use;
    bool queued;
    bool completed;
    bool cancelled;
    bool callback_pending;
    bool expect_response;
    bool high_priority;
    bool poison;
    mb_client_request_t cfg;
    mb_err_t status;
    mb_u8 retry_count;
    mb_u8 max_retries;
    mb_time_ms_t timeout_ms;
    mb_time_ms_t base_timeout_ms;
    mb_time_ms_t retry_backoff_ms;
    mb_time_ms_t deadline;
    mb_time_ms_t watchdog_deadline;
    mb_time_ms_t next_attempt_ms;
    mb_time_ms_t start_time;
    mb_adu_view_t request_view;
    mb_adu_view_t response_view;
    mb_u8 request_storage[MB_PDU_MAX];
    mb_u8 response_storage[MB_PDU_MAX];
    mb_u16 tid;
    struct mb_client_txn *next;
} mb_client_txn_t;

typedef enum {
    MB_CLIENT_STATE_IDLE = 0,
    MB_CLIENT_STATE_WAITING,
    MB_CLIENT_STATE_BACKOFF
} mb_client_state_t;

typedef enum {
    MB_CLIENT_TRANSPORT_RTU = 0,
    MB_CLIENT_TRANSPORT_TCP
} mb_client_transport_t;

typedef struct {
    mb_u64 submitted;
    mb_u64 completed;
    mb_u64 retries;
    mb_u64 timeouts;
    mb_u64 errors;
    mb_u64 cancelled;
    mb_u64 poison_triggers;
    mb_u64 bytes_tx;
    mb_u64 bytes_rx;
    mb_u64 response_count;
    mb_u64 response_latency_total_ms;
} mb_client_metrics_t;

typedef struct mb_client {
    const mb_transport_if_t *iface;
    mb_rtu_transport_t rtu;
    mb_tcp_transport_t tcp;
    mb_client_transport_t transport;
    mb_client_txn_t *pool;
    mb_size_t pool_size;
    mb_client_txn_t *pending_head;
    mb_client_txn_t *pending_tail;
    mb_client_txn_t *current;
    mb_client_state_t state;
    mb_time_ms_t watchdog_ms;
    mb_u16 next_tid;
    mb_size_t queue_capacity;
    mb_size_t pending_count;
    mb_time_ms_t fc_timeouts[256];
    mb_client_metrics_t metrics;
    mb_diag_counters_t diag;
    mb_event_callback_t observer_cb;
    void *observer_user;
    bool trace_hex;
} mb_client_t;

mb_err_t mb_client_init(mb_client_t *client,
                        const mb_transport_if_t *iface,
                        mb_client_txn_t *txn_pool,
                        mb_size_t txn_pool_len);

mb_err_t mb_client_init_tcp(mb_client_t *client,
                            const mb_transport_if_t *iface,
                            mb_client_txn_t *txn_pool,
                            mb_size_t txn_pool_len);

mb_err_t mb_client_submit(mb_client_t *client,
                          const mb_client_request_t *request,
                          mb_client_txn_t **out_txn);

mb_err_t mb_client_submit_poison(mb_client_t *client);

mb_err_t mb_client_cancel(mb_client_t *client, mb_client_txn_t *txn);

mb_err_t mb_client_poll(mb_client_t *client);

void mb_client_set_watchdog(mb_client_t *client, mb_time_ms_t watchdog_ms);

bool mb_client_is_idle(const mb_client_t *client);

mb_size_t mb_client_pending(const mb_client_t *client);

void mb_client_set_queue_capacity(mb_client_t *client, mb_size_t capacity);

mb_size_t mb_client_queue_capacity(const mb_client_t *client);

void mb_client_set_fc_timeout(mb_client_t *client, mb_u8 function, mb_time_ms_t timeout_ms);

void mb_client_get_metrics(const mb_client_t *client, mb_client_metrics_t *out_metrics);

void mb_client_reset_metrics(mb_client_t *client);

void mb_client_get_diag(const mb_client_t *client, mb_diag_counters_t *out_diag);

void mb_client_reset_diag(mb_client_t *client);

void mb_client_set_event_callback(mb_client_t *client, mb_event_callback_t callback, void *user_ctx);

void mb_client_set_trace_hex(mb_client_t *client, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_CLIENT_H */
