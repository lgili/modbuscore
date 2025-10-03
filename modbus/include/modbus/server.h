/**
 * @file server.h
 * @brief Non-blocking Modbus server handling for holding registers (Gate 6).
 */

#ifndef MODBUS_SERVER_H
#define MODBUS_SERVER_H

#include <stdbool.h>
#include <stddef.h>

#include <modbus/mb_err.h>
#include <modbus/mb_types.h>
#include <modbus/pdu.h>
#include <modbus/transport_if.h>
#include <modbus/transport/rtu.h>

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
                        mb_size_t region_capacity);

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

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_SERVER_H */
