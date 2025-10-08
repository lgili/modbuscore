/**
 * @file mb_server_convenience.h
 * @brief Convenience helpers for setting up Modbus servers quickly.
 *
 * This module provides simplified server setup and data registration functions.
 * Unlike the client convenience API (which wraps individual transactions), the
 * server convenience API focuses on:
 *
 * 1. Simplified initialization (transport + mapping in one call)
 * 2. Easy data region registration
 * 3. Standard configurations for common use cases
 *
 * The event loop (mb_server_poll) must still be called by the application,
 * as servers are inherently event-driven.
 *
 * Example usage:
 * @code
 * // 1. Allocate data arrays
 * uint16_t holding_regs[100];
 * uint16_t input_regs[50];
 *
 * // 2. Create server with convenience API
 * mb_server_t server;
 * mb_err_t err = mb_server_create_tcp(&server, 502, 0x01);
 *
 * // 3. Register data regions
 * mb_server_add_holding(&server, 0, holding_regs, 100);
 * mb_server_add_input(&server, 0, input_regs, 50);
 *
 * // 4. Run event loop (application's responsibility)
 * while (running) {
 *     mb_server_poll(&server);
 * }
 * @endcode
 */

#ifndef MODBUS_SERVER_CONVENIENCE_H
#define MODBUS_SERVER_CONVENIENCE_H

#include <modbus/conf.h>

#if MB_CONF_BUILD_SERVER

#include <modbus/server.h>
#include <modbus/mapping.h>
#include <modbus/mb_err.h>
#include <modbus/transport_if.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Server context with convenience wrappers.
 *
 * Encapsulates server FSM, mapping, and transport for simplified setup.
 */
typedef struct mb_server_convenience mb_server_convenience_t;

/**
 * @brief Configuration for TCP server creation.
 */
typedef struct {
    uint16_t port;                /**< TCP port to listen on (default: 502) */
    uint8_t unit_id;              /**< Modbus unit ID (default: 1) */
    uint16_t max_regions;         /**< Max data regions (default: 16) */
    uint16_t max_requests;        /**< Max concurrent requests (default: 8) */
} mb_server_tcp_config_t;

/**
 * @brief Configuration for RTU server creation.
 */
typedef struct {
    const char *device;           /**< Serial device path (e.g., "/dev/ttyUSB0") */
    uint32_t baudrate;            /**< Baud rate (9600, 19200, etc.) */
    uint8_t unit_id;              /**< Modbus unit/slave address */
    uint16_t max_regions;         /**< Max data regions (default: 16) */
    uint16_t max_requests;        /**< Max concurrent requests (default: 8) */
} mb_server_rtu_config_t;

/* -------------------------------------------------------------------------- */
/*                          Server Creation                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create TCP Modbus server with default configuration.
 *
 * This is a convenience wrapper that:
 * - Creates TCP transport on specified port
 * - Initializes server FSM with mapping
 * - Allocates region and request pools with sensible defaults
 *
 * @param server   Pointer to server context (will be initialized)
 * @param port     TCP port to listen on (standard: 502)
 * @param unit_id  Modbus unit identifier (typically 1)
 *
 * @return MB_OK on success, error code otherwise.
 *
 * @note After creation, use mb_server_add_*() to register data regions.
 * @note Application must call mb_server_poll() in a loop.
 * @note Call mb_server_convenience_destroy() when done.
 */
#if MB_CONF_TRANSPORT_TCP
mb_err_t mb_server_create_tcp(mb_server_t *server,
                               uint16_t port,
                               uint8_t unit_id);

/**
 * @brief Create TCP server with custom configuration.
 *
 * @param server  Pointer to server context
 * @param config  Detailed configuration
 *
 * @return MB_OK on success, error code otherwise.
 */
mb_err_t mb_server_create_tcp_ex(mb_server_t *server,
                                  const mb_server_tcp_config_t *config);
#endif

/**
 * @brief Create RTU Modbus server with default configuration.
 *
 * @param server    Pointer to server context
 * @param device    Serial device path (e.g., "/dev/ttyUSB0")
 * @param baudrate  Baud rate (9600, 19200, 38400, 57600, 115200)
 * @param unit_id   Modbus unit/slave address (1-247)
 *
 * @return MB_OK on success, error code otherwise.
 */
#if MB_CONF_TRANSPORT_RTU
mb_err_t mb_server_create_rtu(mb_server_t *server,
                               const char *device,
                               uint32_t baudrate,
                               uint8_t unit_id);

/**
 * @brief Create RTU server with custom configuration.
 *
 * @param server  Pointer to server context
 * @param config  Detailed configuration
 *
 * @return MB_OK on success, error code otherwise.
 */
mb_err_t mb_server_create_rtu_ex(mb_server_t *server,
                                  const mb_server_rtu_config_t *config);
#endif

/* -------------------------------------------------------------------------- */
/*                       Data Region Registration                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Register holding registers (read/write, FC03/06/10/16).
 *
 * @param server      Server instance
 * @param start_addr  Starting address
 * @param registers   Pointer to register array (caller owns memory)
 * @param count       Number of registers
 *
 * @return MB_OK on success, error code otherwise.
 *
 * @note The memory pointed to by 'registers' must remain valid for the
 *       lifetime of the server.
 */
#if MB_CONF_ENABLE_FC03 || MB_CONF_ENABLE_FC06 || MB_CONF_ENABLE_FC10 || MB_CONF_ENABLE_FC16
mb_err_t mb_server_add_holding(mb_server_t *server,
                                uint16_t start_addr,
                                uint16_t *registers,
                                uint16_t count);
#endif

/**
 * @brief Register input registers (read-only, FC04).
 *
 * @param server      Server instance
 * @param start_addr  Starting address
 * @param registers   Pointer to register array (caller owns memory)
 * @param count       Number of registers
 *
 * @return MB_OK on success, error code otherwise.
 */
#if MB_CONF_ENABLE_FC04
mb_err_t mb_server_add_input(mb_server_t *server,
                              uint16_t start_addr,
                              const uint16_t *registers,
                              uint16_t count);
#endif

/**
 * @brief Register coils (read/write, FC01/05/0F).
 *
 * @param server      Server instance
 * @param start_addr  Starting address
 * @param coils       Pointer to coil array (packed bits, caller owns memory)
 * @param count       Number of coils
 *
 * @return MB_OK on success, error code otherwise.
 */
#if MB_CONF_ENABLE_FC01 || MB_CONF_ENABLE_FC05 || MB_CONF_ENABLE_FC0F
mb_err_t mb_server_add_coils(mb_server_t *server,
                              uint16_t start_addr,
                              uint8_t *coils,
                              uint16_t count);
#endif

/**
 * @brief Register discrete inputs (read-only, FC02).
 *
 * @param server      Server instance
 * @param start_addr  Starting address
 * @param inputs      Pointer to input array (packed bits, caller owns memory)
 * @param count       Number of discrete inputs
 *
 * @return MB_OK on success, error code otherwise.
 */
#if MB_CONF_ENABLE_FC02
mb_err_t mb_server_add_discrete(mb_server_t *server,
                                 uint16_t start_addr,
                                 const uint8_t *inputs,
                                 uint16_t count);
#endif

/* -------------------------------------------------------------------------- */
/*                            Cleanup                                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief Destroy server and free internal resources.
 *
 * @param server  Server instance to destroy
 *
 * @note This does NOT free the data arrays registered via mb_server_add_*().
 *       The caller is responsible for managing those.
 */
void mb_server_convenience_destroy(mb_server_t *server);

#ifdef __cplusplus
}
#endif

#endif /* MB_CONF_BUILD_SERVER */

#endif /* MODBUS_SERVER_CONVENIENCE_H */
