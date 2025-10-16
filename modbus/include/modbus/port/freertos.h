/**
 * @file freertos.h
 * @brief FreeRTOS-friendly transport adapters for Modbus targets.
 */

#ifndef MODBUS_PORT_FREERTOS_H
#define MODBUS_PORT_FREERTOS_H

#include <stddef.h>
#include <stdint.h>

#include <modbus/internal/transport_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Prototype matching ``xStreamBufferSend`` / ``xQueueSend`` style helpers.
 */
typedef size_t (*mb_port_freertos_stream_send_fn)(void *stream,
                                                  const mb_u8 *data,
                                                  size_t length,
                                                  uint32_t ticks_to_wait);

/**
 * @brief Prototype matching ``xStreamBufferReceive`` / ``xQueueReceive`` style helpers.
 */
typedef size_t (*mb_port_freertos_stream_recv_fn)(void *stream,
                                                  mb_u8 *buffer,
                                                  size_t capacity,
                                                  uint32_t ticks_to_wait);

/**
 * @brief Prototype returning the scheduler tick count (e.g. ``xTaskGetTickCount``).
 */
typedef uint32_t (*mb_port_freertos_tick_hook_fn)(void);

/**
 * @brief Optional yield hook (typically ``taskYIELD``).
 */
typedef void (*mb_port_freertos_yield_hook_fn)(void);

/**
 * @brief FreeRTOS transport wrapper leveraging stream buffers or queues.
 */
typedef struct mb_port_freertos_transport {
    void *tx_stream;                         /**< Stream/queue used for TX. */
    void *rx_stream;                         /**< Stream/queue used for RX. */
    mb_port_freertos_stream_send_fn send_fn; /**< Low-level send primitive. */
    mb_port_freertos_stream_recv_fn recv_fn; /**< Low-level receive primitive. */
    mb_port_freertos_tick_hook_fn tick_fn;   /**< Hook returning the current tick value. */
    mb_port_freertos_yield_hook_fn yield_fn; /**< Optional scheduler-yield hook. */
    uint32_t tick_rate_hz;                   /**< Tick frequency used for ms conversion. */
    uint32_t max_block_ticks;                /**< Maximum wait ticks for send/recv. */
    mb_transport_if_t iface;                 /**< Exposed non-blocking transport. */
} mb_port_freertos_transport_t;

/**
 * @brief Initialises a FreeRTOS-backed transport adapter.
 *
 * All parameters mirror the FreeRTOS stream/queue APIs, allowing the adapter
 * to forward I/O to stream buffers, queues, or custom shims.
 *
 * @param port            Adapter storage to initialise.
 * @param tx_stream       Stream/queue used to transmit bytes.
 * @param rx_stream       Stream/queue used to receive bytes.
 * @param send_fn         Function that pushes bytes into @p tx_stream.
 * @param recv_fn         Function that pulls bytes from @p rx_stream.
 * @param tick_fn         Hook returning the current scheduler tick count.
 * @param yield_fn        Optional cooperative-yield hook (``taskYIELD``).
 * @param tick_rate_hz    Scheduler tick rate expressed in hertz.
 * @param max_block_ticks Maximum number of ticks each I/O call may block.
 *
 * @retval MB_OK                 Adapter initialised successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Missing stream handles or callbacks.
 */
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
mb_err_t mb_port_freertos_transport_init(mb_port_freertos_transport_t *port,
                                         void *tx_stream,
                                         void *rx_stream,
                                         mb_port_freertos_stream_send_fn send_fn,
                                         mb_port_freertos_stream_recv_fn recv_fn,
                                         mb_port_freertos_tick_hook_fn tick_fn,
                                         mb_port_freertos_yield_hook_fn yield_fn,
                                         uint32_t tick_rate_hz,
                                         uint32_t max_block_ticks);
// NOLINTEND(bugprone-easily-swappable-parameters)

/**
 * @brief Updates the maximum number of ticks each I/O call may block.
 *
 * @param port   Adapter instance.
 * @param ticks  Maximum block duration expressed in scheduler ticks.
 */
void mb_port_freertos_transport_set_block_ticks(mb_port_freertos_transport_t *port, uint32_t ticks);

/**
 * @brief Updates the tick frequency (Hz) used for ms conversion.
 *
 * @param port         Adapter instance.
 * @param tick_rate_hz New tick rate; ignored when zero.
 */
void mb_port_freertos_transport_set_tick_rate(mb_port_freertos_transport_t *port, uint32_t tick_rate_hz);

/**
 * @brief Returns the transport interface managed by the adapter.
 *
 * The pointer remains valid while the adapter structure stays in scope.
 */
const mb_transport_if_t *mb_port_freertos_transport_iface(mb_port_freertos_transport_t *port);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_PORT_FREERTOS_H */
