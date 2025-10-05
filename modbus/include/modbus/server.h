/**
 * @file server.h
 * @brief Non-blocking Modbus server handling for holding registers (Gate 8).
 */

#ifndef MODBUS_SERVER_H
#define MODBUS_SERVER_H

#include <stdbool.h>
#include <stddef.h>

#include <modbus/mb_err.h>
#include <modbus/mb_types.h>
#include <modbus/observe.h>
#include <modbus/pdu.h>
#include <modbus/transport_if.h>
#include <modbus/transport/rtu.h>

#define MB_SERVER_DEFAULT_TIMEOUT_MS 200U
#define MB_SERVER_MAX_TIMEOUT_MS     60000U

#define MB_SERVER_REQUEST_HIGH_PRIORITY (1u << 0)
#define MB_SERVER_REQUEST_POISON        (1u << 1)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback prototype used to serve read requests.
 *
 * Implementations must write @p quantity values into @p out_values. The buffer
 * is guaranteed to hold at least @p quantity entries.
 */
typedef mb_err_t (*mb_server_read_fn)(mb_u16 start_addr,
                                      mb_u16 quantity,
                                      mb_u16 *out_values,
                                      mb_size_t out_cap,
                                      void *user_ctx);

/**
 * @brief Callback prototype used to serve write requests.
 *
 * Implementations receive @p quantity register values captured in @p values.
 */
typedef mb_err_t (*mb_server_write_fn)(mb_u16 start_addr,
                                       const mb_u16 *values,
                                       mb_u16 quantity,
                                       void *user_ctx);

typedef struct mb_server_request mb_server_request_t;

struct mb_server_request {
    bool in_use;
    bool queued;
    bool high_priority;
    bool poison;
    bool broadcast;
    mb_u8 unit_id;
    mb_u8 flags;
    mb_u8 function;
    mb_size_t pdu_len;
    mb_time_ms_t enqueue_time;
    mb_time_ms_t start_time;
    mb_time_ms_t deadline;
    mb_adu_view_t request_view;
    mb_u8 storage[MB_PDU_MAX];
    mb_server_request_t *next;
};

typedef struct {
    mb_u64 received;
    mb_u64 responded;
    mb_u64 broadcasts;
    mb_u64 exceptions;
    mb_u64 dropped;
    mb_u64 poison_triggers;
    mb_u64 errors;
    mb_u64 timeouts;
    mb_u64 latency_total_ms;
} mb_server_metrics_t;

typedef enum {
    MB_SERVER_STATE_IDLE = 0,
    MB_SERVER_STATE_PROCESSING,
    MB_SERVER_STATE_DRAINING
} mb_server_state_t;

/**
 * @brief Register mapping entry.
 *
 * Each region exports a contiguous block of holding registers. Requests must be
 * fully contained inside a single region; otherwise an Illegal Data Address
 * exception is reported.
 */
typedef struct {
    mb_u16 start;                /**< First register address served by this region. */
    mb_u16 count;                /**< Number of registers in the region. */
    bool read_only;              /**< Reject write requests when true. */
    mb_server_read_fn read_cb;   /**< Optional read callback (may be `NULL`). */
    mb_server_write_fn write_cb; /**< Optional write callback (may be `NULL`). */
    void *user_ctx;              /**< User context forwarded to callbacks. */
    mb_u16 *storage;             /**< Optional backing storage for direct access. */
} mb_server_region_t;

/**
 * @brief Server runtime object.
 */
typedef struct {
    const mb_transport_if_t *iface;
    mb_rtu_transport_t rtu;
    mb_u8 unit_id;

    mb_server_region_t *regions;
    mb_size_t region_cap;
    mb_size_t region_count;

    mb_server_request_t *pool;
    mb_size_t pool_size;
    mb_server_request_t *pending_head;
    mb_server_request_t *pending_tail;
    mb_server_request_t *current;
    mb_size_t queue_capacity;
    mb_size_t pending_count;
    mb_time_ms_t fc_timeouts[256];
    mb_server_metrics_t metrics;
    mb_diag_counters_t diag;
    mb_event_callback_t observer_cb;
    void *observer_user;
    bool trace_hex;
    mb_server_state_t state;

    mb_u8 rx_buffer[MB_PDU_MAX];
    mb_u8 tx_buffer[MB_PDU_MAX];
} mb_server_t;

/**
 * @brief Initialises a server object bound to a transport.
 */
mb_err_t mb_server_init(mb_server_t *server,
                        const mb_transport_if_t *iface,
                        mb_u8 unit_id,
                        mb_server_region_t *regions,
                        mb_size_t region_capacity,
                        mb_server_request_t *request_pool,
                        mb_size_t request_pool_len);

/**
 * @brief Clears all registered regions.
 */
void mb_server_reset(mb_server_t *server);

/**
 * @brief Registers a region served exclusively through callbacks.
 */
mb_err_t mb_server_add_region(mb_server_t *server,
                              mb_u16 start,
                              mb_u16 count,
                              bool read_only,
                              mb_server_read_fn read_cb,
                              mb_server_write_fn write_cb,
                              void *user_ctx);

/**
 * @brief Registers a region backed by caller-provided storage.
 */
mb_err_t mb_server_add_storage(mb_server_t *server,
                               mb_u16 start,
                               mb_u16 count,
                               bool read_only,
                               mb_u16 *storage);

/**
 * @brief Advances the server FSM.
 */
mb_err_t mb_server_poll(mb_server_t *server);

mb_size_t mb_server_pending(const mb_server_t *server);

bool mb_server_is_idle(const mb_server_t *server);

void mb_server_set_queue_capacity(mb_server_t *server, mb_size_t capacity);

mb_size_t mb_server_queue_capacity(const mb_server_t *server);

void mb_server_set_fc_timeout(mb_server_t *server, mb_u8 function, mb_time_ms_t timeout_ms);

mb_err_t mb_server_submit_poison(mb_server_t *server);

void mb_server_get_metrics(const mb_server_t *server, mb_server_metrics_t *out_metrics);

void mb_server_reset_metrics(mb_server_t *server);

mb_err_t mb_server_inject_adu(mb_server_t *server, const mb_adu_view_t *adu);

void mb_server_get_diag(const mb_server_t *server, mb_diag_counters_t *out_diag);

void mb_server_reset_diag(mb_server_t *server);

void mb_server_set_event_callback(mb_server_t *server, mb_event_callback_t callback, void *user_ctx);

void mb_server_set_trace_hex(mb_server_t *server, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_SERVER_H */
