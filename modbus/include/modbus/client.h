/**
 * @file client.h
 * @brief Public API for the asynchronous Modbus client state machine.
 *
 * The client manages a queue of protocol transactions, applies retry and
 * watchdog policies, and bridges the higher layers to RTU or TCP transports
 * without blocking the caller.
 */

#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H

#include <modbus/conf.h>

#if !MB_CONF_BUILD_CLIENT
#error "Modbus client support is disabled (enable MODBUS_ENABLE_CLIENT to use this header)."
#endif

#include <stdbool.h>
#include <stddef.h>

#include <modbus/frame.h>
#include <modbus/mb_err.h>
#include <modbus/mb_types.h>
#include <modbus/observe.h>

#if MB_CONF_ENABLE_POWER_MANAGEMENT
#include <modbus/mb_power.h>
#endif

/* Forward declaration for power management (when not included above) */
#if !MB_CONF_ENABLE_POWER_MANAGEMENT
typedef struct mb_idle_config mb_idle_config_t;
#endif

#if MB_CONF_TRANSPORT_RTU
#include <modbus/transport/rtu.h>
#endif
#if MB_CONF_TRANSPORT_TCP
#include <modbus/transport/tcp.h>
#endif

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
    mb_poll_tx_phase_t tx_phase;
    mb_poll_rx_phase_t rx_phase;
    mb_time_ms_t tx_deadline_ms;
    mb_time_ms_t rx_deadline_ms;
    bool rx_pending;
    mb_err_t rx_status;
    mb_adu_view_t rx_view;
    struct mb_client_txn *next;
} mb_client_txn_t;

typedef enum {
    MB_CLIENT_STATE_IDLE = 0,
    MB_CLIENT_STATE_PREPARING,
    MB_CLIENT_STATE_SENDING,
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
    mb_time_ms_t step_max_jitter_ms;
    mb_time_ms_t step_avg_jitter_ms;
} mb_client_metrics_t;

typedef struct mb_client {
    const mb_transport_if_t *iface;
#if MB_CONF_TRANSPORT_RTU
    mb_rtu_transport_t rtu;
#endif
#if MB_CONF_TRANSPORT_TCP
    mb_tcp_transport_t tcp;
#endif
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
    mb_diag_state_t diag;
    mb_event_callback_t observer_cb;
    void *observer_user;
    bool trace_hex;
    mb_poll_jitter_t poll_jitter;
#if MB_CONF_ENABLE_POWER_MANAGEMENT
    mb_idle_config_t idle_config;
#endif
} mb_client_t;

/**
 * @brief Initialises a Modbus client instance bound to a transport.
 *
 * The client adopts @p iface as its non-blocking transport, clears the
 * transaction pool and applies default timeout/backoff configuration. Callers
 * must provide storage for the transaction descriptors in @p txn_pool.
 *
 * @param client        Client object to initialise.
 * @param iface         Transport interface implementing @ref mb_transport_if_t.
 * @param txn_pool      Array of transaction slots managed by the client.
 * @param txn_pool_len  Number of elements in @p txn_pool.
 *
 * @retval MB_OK                 Initialisation succeeded.
 * @retval MB_ERR_INVALID_ARGUMENT  One of the arguments was NULL or invalid.
 */
#if MB_CONF_TRANSPORT_RTU
mb_err_t mb_client_init(mb_client_t *client,
                        const mb_transport_if_t *iface,
                        mb_client_txn_t *txn_pool,
                        mb_size_t txn_pool_len);
#endif

/**
 * @brief Initialises a client instance targeting Modbus TCP transports.
 *
 * This variant wires the TCP transport helper, enabling transaction ID routing
 * and multi-socket aware callbacks. Parameters mirror @ref mb_client_init.
 *
 * @see mb_client_init
 *
 * @retval MB_OK                 Initialisation succeeded.
 * @retval MB_ERR_INVALID_ARGUMENT  One of the arguments was NULL or invalid.
 */
#if MB_CONF_TRANSPORT_TCP
mb_err_t mb_client_init_tcp(mb_client_t *client,
                            const mb_transport_if_t *iface,
                            mb_client_txn_t *txn_pool,
                            mb_size_t txn_pool_len);
#endif

/**
 * @brief Queues a Modbus request for transmission.
 *
 * The request is copied into an available transaction slot. When @p out_txn is
 * not NULL it receives the descriptor so callers can later inspect state or
 * cancel the operation. Requests flagged with
 * ::MB_CLIENT_REQUEST_HIGH_PRIORITY bypass the FIFO order while
 * ::MB_CLIENT_REQUEST_POISON flushes the queue.
 *
 * @param client   Client instance obtained from @ref mb_client_init.
 * @param request  Immutable request description; payload is copied internally.
 * @param out_txn  Optional pointer that receives the allocated transaction.
 *
 * @retval MB_OK              The request was queued successfully.
 * @retval MB_ERR_INVALID_ARGUMENT  Arguments were NULL or payload exceeded ::MB_PDU_MAX.
 * @retval MB_ERR_NO_RESOURCES No transaction slots or queue capacity available.
 */
mb_err_t mb_client_submit(mb_client_t *client,
                          const mb_client_request_t *request,
                          mb_client_txn_t **out_txn);

/**
 * @brief Injects a high-priority poison pill that drains the queue.
 *
 * The client stops after current work completes, leaving the FSM idle. Useful
 * when shutting down or when transports need to be reconfigured.
 *
 * @param client Client instance.
 *
 * @retval MB_OK                 Poison request queued successfully.
 * @retval MB_ERR_INVALID_ARGUMENT @p client was NULL.
 * @retval MB_ERR_NO_RESOURCES   No transaction slot available for the poison pill.
 */
mb_err_t mb_client_submit_poison(mb_client_t *client);

/**
 * @brief Cancels a previously submitted transaction.
 *
 * If the transaction is in-flight it is finalised with ::MB_ERR_CANCELLED. If
 * it was queued, it is removed from the pending list.
 *
 * @param client Client instance.
 * @param txn    Transaction descriptor obtained from @ref mb_client_submit.
 *
 * @retval MB_OK                  The transaction was cancelled successfully.
 * @retval MB_ERR_INVALID_ARGUMENT The transaction pointer was NULL or not in use.
 */
mb_err_t mb_client_cancel(mb_client_t *client, mb_client_txn_t *txn);

/**
 * @brief Advances the client finite-state machine.
 *
 * Poll this function from your event loop to progress retries, timeouts and
 * transport I/O.
 *
 * @param client Client instance.
 * @retval MB_OK             Operation succeeded or no work was required.
 * @retval MB_ERR_INVALID_ARGUMENT  @p client was NULL.
 * @retval other             Error codes bubbled up from the active transport.
 */
mb_err_t mb_client_poll(mb_client_t *client);

/**
 * @brief Advances the client FSM by at most @p steps micro-steps.
 *
 * This variant mirrors @ref mb_client_poll but enforces a cooperative budget,
 * ensuring the caller regains control after a bounded amount of work. Passing
 * ``0`` consumes as many steps as required (legacy behaviour).
 *
 * @param client Client instance.
 * @param steps  Maximum micro-steps to execute (0 means unbounded).
 *
 * @retval MB_OK             Operation succeeded or no work was required.
 * @retval MB_ERR_INVALID_ARGUMENT  @p client was NULL.
 * @retval other             Error codes bubbled up from the active transport.
 */
mb_err_t mb_client_poll_with_budget(mb_client_t *client, mb_size_t steps);

/**
 * @brief Configures the watchdog window for in-flight transactions.
 *
 * When non-zero, transactions exceeding @p watchdog_ms without progress are
 * aborted with ::MB_ERR_TRANSPORT.
 *
 * @param client      Client instance.
 * @param watchdog_ms Maximum inactivity window in milliseconds (0 disables).
 */
void mb_client_set_watchdog(mb_client_t *client, mb_time_ms_t watchdog_ms);

/**
 * @brief Reports whether the client has no active or queued transactions.
 *
 * @param client Client instance.
 *
 * @retval true  No transactions active or queued.
 * @retval false At least one transaction is pending.
 */
bool mb_client_is_idle(const mb_client_t *client);

/**
 * @brief Returns the number of transactions currently active or queued.
 *
 * @param client Client instance.
 * @return Number of transactions in-flight or awaiting dispatch.
 */
mb_size_t mb_client_pending(const mb_client_t *client);

/**
 * @brief Limits the amount of concurrent work accepted by the client.
 *
 * Set @p capacity to zero to fall back to the pool size (default behaviour).
 *
 * @param client   Client instance.
 * @param capacity Maximum number of transactions allowed simultaneously.
 */
void mb_client_set_queue_capacity(mb_client_t *client, mb_size_t capacity);

/**
 * @brief Retrieves the current queue capacity limit.
 *
 * @param client Client instance.
 * @return Configured queue capacity (0 when @p client is NULL).
 */
mb_size_t mb_client_queue_capacity(const mb_client_t *client);

/**
 * @brief Overrides the timeout for a specific function code.
 *
 * Passing a zero timeout clears the override and reverts to the default
 * exponential strategy.
 *
 * @param client   Client instance.
 * @param function Function code to override.
 * @param timeout_ms New timeout in milliseconds (0 clears override).
 */
void mb_client_set_fc_timeout(mb_client_t *client, mb_u8 function, mb_time_ms_t timeout_ms);

/**
 * @brief Copies the current client metrics into @p out_metrics.
 *
 * @param client      Client instance.
 * @param out_metrics Destination structure; left untouched when NULL.
 */
void mb_client_get_metrics(const mb_client_t *client, mb_client_metrics_t *out_metrics);

/**
 * @brief Resets all accumulated client metrics back to zero.
 *
 * @param client Client instance.
 */
void mb_client_reset_metrics(mb_client_t *client);

/**
 * @brief Copies diagnostic counters into @p out_diag.
 *
 * @param client   Client instance.
 * @param out_diag Destination structure; left untouched when NULL.
 */
void mb_client_get_diag(const mb_client_t *client, mb_diag_counters_t *out_diag);

/**
 * @brief Captures a snapshot of diagnostic counters and trace entries.
 *
 * Trace data is only populated when ::MB_CONF_DIAG_ENABLE_TRACE is enabled at
 * build time. The snapshot is safe to inspect outside of the polling context.
 *
 * @param client        Client instance.
 * @param out_snapshot  Destination snapshot; left untouched when NULL.
 */
void mb_client_get_diag_snapshot(const mb_client_t *client, mb_diag_snapshot_t *out_snapshot);

/**
 * @brief Clears the diagnostic counters collected for this client.
 *
 * @param client Client instance.
 */
void mb_client_reset_diag(mb_client_t *client);

/**
 * @brief Registers an event callback to observe client activity.
 *
 * When @p callback is non-NULL the current state is emitted immediately.
 *
 * @param client    Client instance.
 * @param callback  Event sink invoked on state and transaction updates.
 * @param user_ctx  Opaque pointer forwarded to @p callback.
 */
void mb_client_set_event_callback(mb_client_t *client, mb_event_callback_t callback, void *user_ctx);

/**
 * @brief Toggles hexadecimal tracing of TX/RX PDUs to the logging sink.
 *
 * @param client Client instance.
 * @param enable ``true`` to enable tracing, ``false`` to disable.
 */
void mb_client_set_trace_hex(mb_client_t *client, bool enable);

/* -------------------------------------------------------------------------- */
/* Convenience Functions - High-level API for common operations              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Read holding registers (FC03).
 *
 * Builds a Read Holding Registers request and submits it to the client FSM.
 * This is a convenience wrapper around mb_client_submit() that handles PDU
 * building automatically.
 *
 * @param client     Client instance.
 * @param unit_id    Modbus unit/slave address.
 * @param start_addr Starting register address.
 * @param quantity   Number of registers to read (1-125).
 * @param out_txn    Receives the transaction descriptor.
 *
 * @retval MB_OK                   Request submitted successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Invalid parameters.
 * @retval MB_ERR_NO_RESOURCES     No transaction slots available.
 */
#if MB_CONF_ENABLE_FC03
mb_err_t mb_client_read_holding_registers(mb_client_t *client,
                                          uint8_t unit_id,
                                          uint16_t start_addr,
                                          uint16_t quantity,
                                          mb_client_txn_t **out_txn);
#endif

/**
 * @brief Read input registers (FC04).
 *
 * @param client     Client instance.
 * @param unit_id    Modbus unit/slave address.
 * @param start_addr Starting register address.
 * @param quantity   Number of registers to read (1-125).
 * @param out_txn    Receives the transaction descriptor.
 *
 * @retval MB_OK                   Request submitted successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Invalid parameters.
 * @retval MB_ERR_NO_RESOURCES     No transaction slots available.
 */
#if MB_CONF_ENABLE_FC04
mb_err_t mb_client_read_input_registers(mb_client_t *client,
                                        uint8_t unit_id,
                                        uint16_t start_addr,
                                        uint16_t quantity,
                                        mb_client_txn_t **out_txn);
#endif

/**
 * @brief Read coils (FC01).
 *
 * @param client     Client instance.
 * @param unit_id    Modbus unit/slave address.
 * @param start_addr Starting coil address.
 * @param quantity   Number of coils to read (1-2000).
 * @param out_txn    Receives the transaction descriptor.
 *
 * @retval MB_OK                   Request submitted successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Invalid parameters.
 * @retval MB_ERR_NO_RESOURCES     No transaction slots available.
 */
#if MB_CONF_ENABLE_FC01
mb_err_t mb_client_read_coils(mb_client_t *client,
                              uint8_t unit_id,
                              uint16_t start_addr,
                              uint16_t quantity,
                              mb_client_txn_t **out_txn);
#endif

/**
 * @brief Read discrete inputs (FC02).
 *
 * @param client     Client instance.
 * @param unit_id    Modbus unit/slave address.
 * @param start_addr Starting input address.
 * @param quantity   Number of inputs to read (1-2000).
 * @param out_txn    Receives the transaction descriptor.
 *
 * @retval MB_OK                   Request submitted successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Invalid parameters.
 * @retval MB_ERR_NO_RESOURCES     No transaction slots available.
 */
#if MB_CONF_ENABLE_FC02
mb_err_t mb_client_read_discrete_inputs(mb_client_t *client,
                                        uint8_t unit_id,
                                        uint16_t start_addr,
                                        uint16_t quantity,
                                        mb_client_txn_t **out_txn);
#endif

/**
 * @brief Write single register (FC06).
 *
 * @param client   Client instance.
 * @param unit_id  Modbus unit/slave address.
 * @param address  Register address.
 * @param value    Value to write.
 * @param out_txn  Receives the transaction descriptor.
 *
 * @retval MB_OK                   Request submitted successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Invalid parameters.
 * @retval MB_ERR_NO_RESOURCES     No transaction slots available.
 */
#if MB_CONF_ENABLE_FC06
mb_err_t mb_client_write_single_register(mb_client_t *client,
                                         uint8_t unit_id,
                                         uint16_t address,
                                         uint16_t value,
                                         mb_client_txn_t **out_txn);
#endif

/**
 * @brief Write single coil (FC05).
 *
 * @param client   Client instance.
 * @param unit_id  Modbus unit/slave address.
 * @param address  Coil address.
 * @param value    Coil state (true = ON, false = OFF).
 * @param out_txn  Receives the transaction descriptor.
 *
 * @retval MB_OK                   Request submitted successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Invalid parameters.
 * @retval MB_ERR_NO_RESOURCES     No transaction slots available.
 */
#if MB_CONF_ENABLE_FC05
mb_err_t mb_client_write_single_coil(mb_client_t *client,
                                     uint8_t unit_id,
                                     uint16_t address,
                                     bool value,
                                     mb_client_txn_t **out_txn);
#endif

/**
 * @brief Write multiple registers (FC16).
 *
 * @param client     Client instance.
 * @param unit_id    Modbus unit/slave address.
 * @param start_addr Starting register address.
 * @param quantity   Number of registers to write (1-123).
 * @param values     Array of register values.
 * @param out_txn    Receives the transaction descriptor.
 *
 * @retval MB_OK                   Request submitted successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Invalid parameters.
 * @retval MB_ERR_NO_RESOURCES     No transaction slots available.
 */
#if MB_CONF_ENABLE_FC10
mb_err_t mb_client_write_multiple_registers(mb_client_t *client,
                                            uint8_t unit_id,
                                            uint16_t start_addr,
                                            uint16_t quantity,
                                            const uint16_t *values,
                                            mb_client_txn_t **out_txn);
#endif

/**
 * @brief Write multiple coils (FC15).
 *
 * @param client     Client instance.
 * @param unit_id    Modbus unit/slave address.
 * @param start_addr Starting coil address.
 * @param quantity   Number of coils to write (1-1968).
 * @param values     Packed coil values (bit array).
 * @param out_txn    Receives the transaction descriptor.
 *
 * @retval MB_OK                   Request submitted successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Invalid parameters.
 * @retval MB_ERR_NO_RESOURCES     No transaction slots available.
 */
#if MB_CONF_ENABLE_FC0F
mb_err_t mb_client_write_multiple_coils(mb_client_t *client,
                                        uint8_t unit_id,
                                        uint16_t start_addr,
                                        uint16_t quantity,
                                        const uint8_t *values,
                                        mb_client_txn_t **out_txn);
#endif

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_CLIENT_H */
