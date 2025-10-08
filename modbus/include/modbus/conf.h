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
 * @brief Enable zero-copy IO statistics tracking.
 * 
 * When enabled, tracks memcpy vs zero-copy operations for performance analysis.
 * Useful for testing and optimization, but adds small overhead.
 */
#ifndef MB_CONF_ENABLE_IOVEC_STATS
#define MB_CONF_ENABLE_IOVEC_STATS 0
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

#ifndef MB_CONF_CLIENT_POLL_BUDGET_STEPS
#define MB_CONF_CLIENT_POLL_BUDGET_STEPS 0U
#endif

#ifndef MB_CONF_SERVER_POLL_BUDGET_STEPS
#define MB_CONF_SERVER_POLL_BUDGET_STEPS 0U
#endif

#ifndef MB_CONF_CLIENT_SUBSTATE_DEADLINE_MS
#define MB_CONF_CLIENT_SUBSTATE_DEADLINE_MS 2U
#endif

#ifndef MB_CONF_SERVER_SUBSTATE_DEADLINE_MS
#define MB_CONF_SERVER_SUBSTATE_DEADLINE_MS 2U
#endif

#ifndef MB_CONF_CLIENT_POLL_HOOK
#define MB_CONF_CLIENT_POLL_HOOK(client_ptr, phase) do { (void)(client_ptr); (void)(phase); } while (0)
#endif

#ifndef MB_CONF_SERVER_POLL_HOOK
#define MB_CONF_SERVER_POLL_HOOK(server_ptr, phase) do { (void)(server_ptr); (void)(phase); } while (0)
#endif

/* ========================================================================== */
/* ISR-Safe Mode Configuration (Gate 23)                                     */
/* ========================================================================== */

/**
 * @brief Enable ISR-safe mode for fast half-duplex turnaround.
 *
 * When enabled, provides mb_on_rx_chunk_from_isr() and mb_try_tx_from_isr()
 * for minimal latency RX→TX transitions (<100µs target).
 */
#ifndef MB_CONF_ENABLE_ISR_MODE
#define MB_CONF_ENABLE_ISR_MODE 0
#endif

/**
 * @brief Suppress heavy logging in ISR context.
 *
 * When enabled, MB_ISR_SAFE_LOG() becomes a no-op in ISR context to minimize
 * interrupt latency. Critical errors may still be logged.
 */
#ifndef MB_CONF_ISR_SUPPRESS_LOGGING
#define MB_CONF_ISR_SUPPRESS_LOGGING 1
#endif

/**
 * @brief Enable runtime assertions (including MB_ASSERT_NOT_ISR).
 *
 * Disable in production for minimal overhead.
 */
#ifndef MB_CONF_ENABLE_ASSERTIONS
#define MB_CONF_ENABLE_ASSERTIONS 0
#endif

/* ========================================================================== */
/* QoS and Backpressure Configuration (Gate 24)                              */
/* ========================================================================== */

/**
 * @brief Enable Quality of Service (QoS) and backpressure management.
 *
 * When enabled, provides priority-aware queue management to prevent
 * head-of-line blocking and ensure critical transactions meet latency targets.
 */
#ifndef MB_CONF_ENABLE_QOS
#define MB_CONF_ENABLE_QOS 0
#endif

/**
 * @brief Default high priority queue capacity.
 *
 * Size of the high priority (critical) transaction queue.
 */
#ifndef MB_CONF_QOS_HIGH_QUEUE_CAPACITY
#define MB_CONF_QOS_HIGH_QUEUE_CAPACITY 8
#endif

/**
 * @brief Default normal priority queue capacity.
 *
 * Size of the normal priority (best-effort) transaction queue.
 */
#ifndef MB_CONF_QOS_NORMAL_QUEUE_CAPACITY
#define MB_CONF_QOS_NORMAL_QUEUE_CAPACITY 24
#endif

#endif /* MODBUS_CONF_H */
