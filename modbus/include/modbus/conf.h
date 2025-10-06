/**
 * @file modbus_conf.h
 * @brief Configuration header for the Modbus library.
 *
 * This file contains compile-time configuration options, macros, and default
 * values that can be adjusted for different environments or constraints.
 * Users or integrators may modify these definitions before building the library
 * to customize its behavior.
 *
 * These settings should be included before any other modbus headers, ensuring
 * all modules see the same configuration.
 *
 * Author:
 * Date: 2024-12-20
 */

#ifndef MODBUS_CONF_H
#define MODBUS_CONF_H

#ifndef MB_CONF_BUILD_CLIENT
#define MB_CONF_BUILD_CLIENT 1
#endif

#ifndef MB_CONF_BUILD_SERVER
#define MB_CONF_BUILD_SERVER 1
#endif

#ifndef MB_CONF_TRANSPORT_RTU
#define MB_CONF_TRANSPORT_RTU 1
#endif

#ifndef MB_CONF_TRANSPORT_ASCII
#define MB_CONF_TRANSPORT_ASCII 1
#endif

#ifndef MB_CONF_TRANSPORT_TCP
#define MB_CONF_TRANSPORT_TCP 1
#endif

#define MB_CONF_PROFILE_TINY   0
#define MB_CONF_PROFILE_LEAN   1
#define MB_CONF_PROFILE_FULL   2
#define MB_CONF_PROFILE_CUSTOM 3

#ifndef MB_CONF_PROFILE
#define MB_CONF_PROFILE MB_CONF_PROFILE_LEAN
#endif

#ifndef MB_CONF_ENABLE_FC01
#define MB_CONF_ENABLE_FC01 1
#endif

#ifndef MB_CONF_ENABLE_FC02
#define MB_CONF_ENABLE_FC02 1
#endif

#ifndef MB_CONF_ENABLE_FC03
#define MB_CONF_ENABLE_FC03 1
#endif

#ifndef MB_CONF_ENABLE_FC04
#define MB_CONF_ENABLE_FC04 1
#endif

#ifndef MB_CONF_ENABLE_FC05
#define MB_CONF_ENABLE_FC05 1
#endif

#ifndef MB_CONF_ENABLE_FC06
#define MB_CONF_ENABLE_FC06 1
#endif

#ifndef MB_CONF_ENABLE_FC07
#define MB_CONF_ENABLE_FC07 1
#endif

#ifndef MB_CONF_ENABLE_FC0F
#define MB_CONF_ENABLE_FC0F 1
#endif

#ifndef MB_CONF_ENABLE_FC10
#define MB_CONF_ENABLE_FC10 1
#endif

#ifndef MB_CONF_ENABLE_FC11
#define MB_CONF_ENABLE_FC11 1
#endif

#ifndef MB_CONF_ENABLE_FC16
#define MB_CONF_ENABLE_FC16 1
#endif

#ifndef MB_CONF_ENABLE_FC17
#define MB_CONF_ENABLE_FC17 1
#endif

#ifndef MB_CONF_DIAG_ENABLE_COUNTERS
#define MB_CONF_DIAG_ENABLE_COUNTERS 1
#endif

#ifndef MB_CONF_DIAG_ENABLE_TRACE
#define MB_CONF_DIAG_ENABLE_TRACE 0
#endif

#ifndef MB_CONF_DIAG_TRACE_DEPTH
#define MB_CONF_DIAG_TRACE_DEPTH 64
#endif

#define LOG_ENABLED
/**
 * @brief Maximum size of holding registers array in the server.
 */
#ifndef MAX_SIZE_HOLDING_REGISTERS
#define MAX_SIZE_HOLDING_REGISTERS 64
#endif

/**
 * @brief Maximum address for holding registers.
 */
#ifndef MAX_ADDRESS_HOLDING_REGISTERS
#define MAX_ADDRESS_HOLDING_REGISTERS 65535
#endif

/**
 * @brief Maximum number of registers that can be read or written at once.
 */
#ifndef MODBUS_MAX_READ_WRITE_SIZE
#define MODBUS_MAX_READ_WRITE_SIZE 0x07D0
#endif

/**
 * @brief Maximum device info packages in server mode.
 */
#ifndef MAX_DEVICE_PACKAGES
#define MAX_DEVICE_PACKAGES 5
#endif

/**
 * @brief Maximum length of each device info package.
 */
#ifndef MAX_DEVICE_PACKAGE_VALUES
#define MAX_DEVICE_PACKAGE_VALUES 8
#endif

/**
 * @brief Default Modbus baudrate (for RTU).
 */
#ifndef MODBUS_BAUDRATE
#define MODBUS_BAUDRATE 19200
#endif

/**
 * @brief Size of receive and transmit buffers.
 */
#ifndef MODBUS_RECEIVE_BUFFER_SIZE
#define MODBUS_RECEIVE_BUFFER_SIZE 256
#endif

#ifndef MODBUS_SEND_BUFFER_SIZE
#define MODBUS_SEND_BUFFER_SIZE 256
#endif

#ifndef MASTER_DEFAULT_TIMEOUT_MS
#define MASTER_DEFAULT_TIMEOUT_MS  1000
#endif

/**
 * @brief Maximum number of simultaneous Modbus TCP connections handled by helper utilities.
 */
#ifndef MB_TCP_MAX_CONNECTIONS
#define MB_TCP_MAX_CONNECTIONS 4
#endif

/**
 * @brief If you want to enable or disable certain functions, you can define
 * macros here. For example:
 * #define ENABLE_BOOTLOADER 1
 */

/* Add more configurations as needed */

typedef enum mb_conf_client_poll_phase {
	MB_CONF_CLIENT_POLL_PHASE_ENTER = 0,
	MB_CONF_CLIENT_POLL_PHASE_AFTER_TRANSPORT,
	MB_CONF_CLIENT_POLL_PHASE_AFTER_STATE,
	MB_CONF_CLIENT_POLL_PHASE_EXIT
} mb_conf_client_poll_phase_t;

typedef enum mb_conf_server_poll_phase {
	MB_CONF_SERVER_POLL_PHASE_ENTER = 0,
	MB_CONF_SERVER_POLL_PHASE_AFTER_TRANSPORT,
	MB_CONF_SERVER_POLL_PHASE_AFTER_STATE,
	MB_CONF_SERVER_POLL_PHASE_EXIT
} mb_conf_server_poll_phase_t;

#ifndef MB_CONF_CLIENT_POLL_HOOK
#define MB_CONF_CLIENT_POLL_HOOK(client_ptr, phase) do { (void)(client_ptr); (void)(phase); } while (0)
#endif

#ifndef MB_CONF_SERVER_POLL_HOOK
#define MB_CONF_SERVER_POLL_HOOK(server_ptr, phase) do { (void)(server_ptr); (void)(phase); } while (0)
#endif

#endif /* MODBUS_CONF_H */
