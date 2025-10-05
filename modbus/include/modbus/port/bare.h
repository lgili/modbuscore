/**
 * @file bare.h
 * @brief Bare-metal friendly transport helpers for Modbus integrations.
 */

#ifndef MODBUS_PORT_BARE_H
#define MODBUS_PORT_BARE_H

#include <stdint.h>

#include <modbus/transport_if.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function pointer type for sending bytes over a bare-metal transport.
 */
typedef mb_err_t (*mb_port_bare_send_fn)(void *ctx,
                                         const mb_u8 *buf,
                                         mb_size_t len,
                                         mb_transport_io_result_t *out);

/**
 * @brief Function pointer type for receiving bytes from a bare-metal transport.
 */
typedef mb_err_t (*mb_port_bare_recv_fn)(void *ctx,
                                         mb_u8 *buf,
                                         mb_size_t cap,
                                         mb_transport_io_result_t *out);

/**
 * @brief Function pointer returning the current scheduler ticks.
 */
typedef uint32_t (*mb_port_bare_tick_now_fn)(void *ctx);

/**
 * @brief Optional cooperative-yield hook for MCUs that provide it.
 */
typedef void (*mb_port_bare_yield_fn)(void *ctx);

/**
 * @brief Helper structure that binds user callbacks to an ``mb_transport_if_t``.
 */
typedef struct mb_port_bare_transport {
    void *user_ctx;                       /**< Opaque handle forwarded to send/recv. */
    void *clock_ctx;                      /**< Clock handle (defaults to ``user_ctx``). */
    mb_port_bare_send_fn send_fn;         /**< Low-level send primitive. */
    mb_port_bare_recv_fn recv_fn;         /**< Low-level receive primitive. */
    mb_port_bare_tick_now_fn tick_now_fn; /**< Returns scheduler ticks. */
    mb_port_bare_yield_fn yield_fn;       /**< Optional CPU yield hook. */
    uint32_t tick_rate_hz;                /**< Tick frequency (Hz) for ms conversion. */
    mb_transport_if_t iface;              /**< Exposed transport interface. */
} mb_port_bare_transport_t;

/**
 * @brief Initialises a bare-metal transport adapter.
 *
 * @param port          Storage for the adapter.
 * @param user_ctx      Handle forwarded to ``send``/``recv`` callbacks.
 * @param send_fn       Non-blocking send primitive.
 * @param recv_fn       Non-blocking receive primitive.
 * @param tick_now_fn   Returns the current scheduler tick count.
 * @param tick_rate_hz  Tick frequency used to derive milliseconds.
 * @param yield_fn      Optional CPU-yield hook (may be ``NULL``).
 * @param clock_ctx     Optional clock handle (defaults to ``user_ctx`` when ``NULL``).
 *
 * @retval MB_OK                 Adapter initialised successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Missing callbacks or invalid tick parameters.
 */
mb_err_t mb_port_bare_transport_init(mb_port_bare_transport_t *port,
                                     void *user_ctx,
                                     mb_port_bare_send_fn send_fn,
                                     mb_port_bare_recv_fn recv_fn,
                                     mb_port_bare_tick_now_fn tick_now_fn,
                                     uint32_t tick_rate_hz,
                                     mb_port_bare_yield_fn yield_fn,
                                     void *clock_ctx);

/**
 * @brief Updates the tick rate used for millisecond conversion.
 *
 * @param port          Adapter instance.
 * @param tick_rate_hz  New tick rate (Hz); ignored when zero.
 */
void mb_port_bare_transport_update_tick_rate(mb_port_bare_transport_t *port, uint32_t tick_rate_hz);

/**
 * @brief Allows using a dedicated clock context separate from ``user_ctx``.
 *
 * @param port       Adapter instance.
 * @param clock_ctx  Context forwarded to the tick callback.
 */
void mb_port_bare_transport_set_clock_ctx(mb_port_bare_transport_t *port, void *clock_ctx);

/**
 * @brief Returns the configured transport interface.
 *
 * The pointer remains valid for as long as @p port is alive.
 */
const mb_transport_if_t *mb_port_bare_transport_iface(mb_port_bare_transport_t *port);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_PORT_BARE_H */
