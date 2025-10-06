/**
 * @file server.h
 * @brief Public API for the cooperative Modbus server runtime.
 *
 * The server exposes helpers to register register regions, manage
 * prioritised request queues, and feed Modbus RTU transports while delivering
 * diagnostics and observability hooks to the application layer.
 */

#ifndef MODBUS_SERVER_H
#define MODBUS_SERVER_H

#include <modbus/conf.h>

#if !MB_CONF_BUILD_SERVER
#error "Modbus server support is disabled (enable MODBUS_ENABLE_SERVER to use this header)."
#endif

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
    mb_diag_state_t diag;
    mb_event_callback_t observer_cb;
    void *observer_user;
    bool trace_hex;
    mb_server_state_t state;

    mb_u8 rx_buffer[MB_PDU_MAX];
    mb_u8 tx_buffer[MB_PDU_MAX];
} mb_server_t;

/**
 * @brief Initialises a server object bound to a transport interface.
 *
 * All memory backing regions and request slots is supplied by the caller so the
 * server can operate without dynamic allocations.
 *
 * @param server           Server instance to initialise.
 * @param iface            Transport implementing ::mb_transport_if_t.
 * @param unit_id          Modbus unit identifier served by this instance.
 * @param regions          Array used to store region descriptors.
 * @param region_capacity  Number of entries available in @p regions.
 * @param request_pool     Array of request descriptors used as queue storage.
 * @param request_pool_len Number of entries in @p request_pool.
 *
 * @retval MB_OK                 Initialisation succeeded.
 * @retval MB_ERR_INVALID_ARGUMENT Invalid argument or missing storage.
 */
mb_err_t mb_server_init(mb_server_t *server,
                        const mb_transport_if_t *iface,
                        mb_u8 unit_id,
                        mb_server_region_t *regions,
                        mb_size_t region_capacity,
                        mb_server_request_t *request_pool,
                        mb_size_t request_pool_len);

/**
 * @brief Clears all registered register regions and pending requests.
 *
 * Pending requests are not cancelled; call @ref mb_server_submit_poison first
 * if a graceful drain is required.
 */
void mb_server_reset(mb_server_t *server);

/**
 * @brief Registers a region served exclusively through callbacks.
 *
 * Use this overload when registers are virtual or when backing storage lives
 * outside the server instance.
 *
 * @param server    Server instance.
 * @param start     First register address handled by the region.
 * @param count     Number of registers exposed by the region.
 * @param read_only Reject write requests when true.
 * @param read_cb   Callback invoked for read operations.
 * @param write_cb  Callback invoked for write operations (ignored when @p read_only is true).
 * @param user_ctx  Pointer forwarded to callbacks.
 *
 * @retval MB_OK                 Region added successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Invalid range or overlaps existing regions.
 * @retval MB_ERR_OTHER          Region capacity exhausted.
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
 *
 * The buffer provided in @p storage must contain at least @p count elements.
 *
 * @param server   Server instance.
 * @param start    First register address handled by the region.
 * @param count    Number of registers backed by @p storage.
 * @param read_only Reject write requests when true.
 * @param storage  Caller-owned register array.
 *
 * @retval MB_OK                 Region added successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Invalid storage pointer or range.
 * @retval MB_ERR_OTHER          Region capacity exhausted.
 */
mb_err_t mb_server_add_storage(mb_server_t *server,
                               mb_u16 start,
                               mb_u16 count,
                               bool read_only,
                               mb_u16 *storage);

/**
 * @brief Advances the server finite-state machine.
 *
 * Poll this function frequently to accept new Modbus requests and service the
 * queue.
 *
 * @param server Server instance.
 *
 * @retval MB_OK                 Operation succeeded or no work was pending.
 * @retval MB_ERR_INVALID_ARGUMENT @p server was NULL.
 * @retval other                 Error codes bubbled up from the active transport.
 */
mb_err_t mb_server_poll(mb_server_t *server);

/**
 * @brief Returns the number of requests currently pending in the queue.
 *
 * @param server Server instance.
 * @return Number of pending requests (0 when @p server is NULL).
 */
mb_size_t mb_server_pending(const mb_server_t *server);

/**
 * @brief Reports whether the server has no active or queued requests.
 *
 * @param server Server instance.
 *
 * @retval true  No requests are in-flight or queued.
 * @retval false There is active or pending work.
 */
bool mb_server_is_idle(const mb_server_t *server);

/**
 * @brief Limits how many requests can be staged at once.
 *
 * Setting @p capacity to zero enables an unbounded queue capped only by the
 * size of the request pool.
 *
 * @param server   Server instance.
 * @param capacity Maximum number of queued requests.
 */
void mb_server_set_queue_capacity(mb_server_t *server, mb_size_t capacity);

/**
 * @brief Retrieves the current queue capacity limit.
 *
 * @param server Server instance.
 * @return Queue capacity (0 when @p server is NULL).
 */
mb_size_t mb_server_queue_capacity(const mb_server_t *server);

/**
 * @brief Overrides the timeout used for a specific function code.
 *
 * @param server   Server instance.
 * @param function Function code whose timeout should be updated.
 * @param timeout_ms Timeout in milliseconds (0 reverts to default).
 */
void mb_server_set_fc_timeout(mb_server_t *server, mb_u8 function, mb_time_ms_t timeout_ms);

/**
 * @brief Enqueues a poison pill that flushes outstanding requests.
 *
 * @param server Server instance.
 *
 * @retval MB_OK                 Poison request queued successfully.
 * @retval MB_ERR_INVALID_ARGUMENT @p server was NULL.
 * @retval MB_ERR_NO_RESOURCES   No spare request slot was available.
 */
mb_err_t mb_server_submit_poison(mb_server_t *server);

/**
 * @brief Copies accumulated metrics into @p out_metrics.
 *
 * @param server       Server instance.
 * @param out_metrics  Destination structure; left untouched when NULL.
 */
void mb_server_get_metrics(const mb_server_t *server, mb_server_metrics_t *out_metrics);

/**
 * @brief Resets the server metrics to zero.
 *
 * @param server Server instance.
 */
void mb_server_reset_metrics(mb_server_t *server);

/**
 * @brief Injects a raw ADU into the server for testing purposes.
 *
 * Intended for simulations where the transport path is bypassed.
 *
 * @param server Server instance.
 * @param adu    ADU to inject.
 *
 * @retval MB_OK                 ADU was accepted for processing.
 * @retval MB_ERR_INVALID_ARGUMENT @p server or @p adu was NULL.
 */
mb_err_t mb_server_inject_adu(mb_server_t *server, const mb_adu_view_t *adu);

/**
 * @brief Copies diagnostic counters into @p out_diag.
 *
 * @param server   Server instance.
 * @param out_diag Destination structure; left untouched when NULL.
 */
void mb_server_get_diag(const mb_server_t *server, mb_diag_counters_t *out_diag);

/**
 * @brief Captures a snapshot of server diagnostics, including trace data when enabled.
 *
 * @param server       Server instance.
 * @param out_snapshot Destination snapshot; left untouched when NULL.
 */
void mb_server_get_diag_snapshot(const mb_server_t *server, mb_diag_snapshot_t *out_snapshot);

/**
 * @brief Clears the diagnostic counters maintained by the server.
 *
 * @param server Server instance.
 */
void mb_server_reset_diag(mb_server_t *server);

/**
 * @brief Registers an event callback invoked on state transitions and requests.
 *
 * @param server   Server instance.
 * @param callback Event sink; pass NULL to disable notifications.
 * @param user_ctx Opaque pointer forwarded to @p callback.
 */
void mb_server_set_event_callback(mb_server_t *server, mb_event_callback_t callback, void *user_ctx);

/**
 * @brief Enables or disables hexadecimal tracing of requests and responses.
 *
 * @param server Server instance.
 * @param enable ``true`` to enable tracing, ``false`` otherwise.
 */
void mb_server_set_trace_hex(mb_server_t *server, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_SERVER_H */
