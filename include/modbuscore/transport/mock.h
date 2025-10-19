#ifndef MODBUSCORE_TRANSPORT_MOCK_H
#define MODBUSCORE_TRANSPORT_MOCK_H

/**
 * @file mock.h
 * @brief Deterministic mock transport for ModbusCore testing.
 *
 * The goal of this driver is to enable 100% controlled network simulations,
 * including configurable latencies for send/receive and manual advancement
 * of the monotonic clock. No calls block; all data is stored in internal
 * queues and can be inspected or delivered on demand by tests.
 */

#include <modbuscore/common/status.h>
#include <modbuscore/transport/iface.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Optional configuration for the mock transport.
 *
 * All fields are optional and default to 0 if @p config is NULL
 * in mbc_mock_transport_create().
 */
typedef struct mbc_mock_transport_config {
    uint32_t initial_now_ms;   /**< Initial timestamp of internal clock (default 0). */
    uint32_t send_latency_ms;  /**< Latency applied to each send before becoming available to
                                  tests. */
    uint32_t recv_latency_ms;  /**< Base latency applied to frames enqueued via schedule
                                  (default 0). */
    uint32_t yield_advance_ms; /**< Time increment applied when calling yield (default 0 = no
                                  advance). */
} mbc_mock_transport_config_t;

/**
 * @brief Opaque mock context (defined internally).
 */
typedef struct mbc_mock_transport mbc_mock_transport_t;

/**
 * @brief Instantiate the mock transport and fill a ModbusCore interface.
 *
 * @param config Optional configuration (NULL uses defaults).
 * @param out_iface Transport interface filled on output.
 * @param out_ctx Allocated internal context (use destroy when done).
 *
 * @return MBC_STATUS_OK on success or corresponding error code.
 */
mbc_status_t mbc_mock_transport_create(const mbc_mock_transport_config_t* config,
                                       mbc_transport_iface_t* out_iface,
                                       mbc_mock_transport_t** out_ctx);

/**
 * @brief Destroy the mock, releasing all allocated resources.
 */
void mbc_mock_transport_destroy(mbc_mock_transport_t* ctx);

/**
 * @brief Reset the mock to initial state, discarding queues and
 *        returning clock to @ref mbc_mock_transport_config::initial_now_ms.
 */
void mbc_mock_transport_reset(mbc_mock_transport_t* ctx);

/**
 * @brief Advance the internal clock by @p delta_ms milliseconds.
 */
void mbc_mock_transport_advance(mbc_mock_transport_t* ctx, uint32_t delta_ms);

/**
 * @brief Schedule data to be received by the next mbc_transport_receive().
 *
 * Bytes will be available after `config->recv_latency_ms + delay_ms`.
 *
 * @param ctx Previously created mock.
 * @param data Data buffer to be copied to the queue.
 * @param length Number of bytes (must be > 0).
 * @param delay_ms Additional latency before data becomes ready.
 *
 * @return MBC_STATUS_OK on success, MBC_STATUS_NO_RESOURCES if memory exhausted,
 *         or MBC_STATUS_INVALID_ARGUMENT if parameters are invalid.
 */
mbc_status_t mbc_mock_transport_schedule_rx(mbc_mock_transport_t* ctx, const uint8_t* data,
                                            size_t length, uint32_t delay_ms);

/**
 * @brief Retrieve the next transmitted frame, if already available.
 *
 * @param ctx Previously created mock.
 * @param buffer Destination buffer for copy (must not be NULL).
 * @param capacity Destination buffer capacity.
 * @param out_length Number of bytes copied. Set to 0 if nothing ready.
 *
 * @return MBC_STATUS_OK on success,
 *         MBC_STATUS_NO_RESOURCES if @p capacity is insufficient,
 *         MBC_STATUS_INVALID_ARGUMENT for invalid parameters.
 */
mbc_status_t mbc_mock_transport_fetch_tx(mbc_mock_transport_t* ctx, uint8_t* buffer,
                                         size_t capacity, size_t* out_length);

/**
 * @brief Report how many frames are waiting for RX delivery.
 */
size_t mbc_mock_transport_pending_rx(const mbc_mock_transport_t* ctx);

/**
 * @brief Report how many transmitted frames are waiting to be collected by tests.
 */
size_t mbc_mock_transport_pending_tx(const mbc_mock_transport_t* ctx);

/**
 * @brief Set the connection state of the mock (true = connected).
 *
 * When disconnected, send/receive operations return MBC_STATUS_IO_ERROR.
 */
void mbc_mock_transport_set_connected(mbc_mock_transport_t* ctx, bool connected);

/**
 * @brief Make the next send call return @p status without enqueuing data.
 */
void mbc_mock_transport_fail_next_send(mbc_mock_transport_t* ctx, mbc_status_t status);

/**
 * @brief Make the next receive call return @p status.
 */
void mbc_mock_transport_fail_next_receive(mbc_mock_transport_t* ctx, mbc_status_t status);

/**
 * @brief Discard the next pending RX frame (regardless of ready state).
 */
mbc_status_t mbc_mock_transport_drop_next_rx(mbc_mock_transport_t* ctx);

/**
 * @brief Discard the next pending TX frame (before fetch).
 */
mbc_status_t mbc_mock_transport_drop_next_tx(mbc_mock_transport_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_TRANSPORT_MOCK_H */
