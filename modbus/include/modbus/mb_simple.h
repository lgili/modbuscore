/**
 * @file mb_simple.h
 * @brief Simplified Modbus API - The easiest way to use ModbusCore
 *
 * This is the NEW recommended API for ModbusCore v2.0+. It provides a unified,
 * intuitive interface that combines the best of mb_host, client_sync, and client.
 *
 * DESIGN GOALS:
 * - Maximum simplicity for common use cases (80% of users)
 * - Minimal boilerplate code
 * - Automatic resource management
 * - Clear, self-documenting API
 * - Full backward compatibility (old APIs unchanged)
 *
 * QUICK START (TCP Client in 3 lines):
 * -------------------------------------
 * @code
 * #include <modbus/mb_simple.h>
 *
 * mb_t *mb = mb_create_tcp("192.168.1.10:502");
 * mb_read_holding(mb, 1, 0, 10, registers);
 * mb_destroy(mb);
 * @endcode
 *
 * EVEN SIMPLER (auto-cleanup on GCC/Clang):
 * ------------------------------------------
 * @code
 * MB_AUTO(mb, mb_create_tcp("192.168.1.10:502"));
 * mb_read_holding(mb, 1, 0, 10, registers);
 * // Auto-cleanup when mb goes out of scope
 * @endcode
 *
 * RTU CLIENT (3 lines):
 * ---------------------
 * @code
 * MB_AUTO(mb, mb_create_rtu("/dev/ttyUSB0", 115200));
 * mb_write_register(mb, 1, 100, 1234);
 * // Auto-cleanup
 * @endcode
 *
 * @note Works with all configuration profiles (SIMPLE, EMBEDDED, GATEWAY, FULL)
 * @note For advanced use cases, the full client.h/server.h APIs remain available
 * @see docs/API_SIMPLIFIED.md for complete guide
 *
 * @version 2.0
 * @date 2025-01-15
 */

#ifndef MODBUS_MB_SIMPLE_H
#define MODBUS_MB_SIMPLE_H

#include <modbus/mb_err.h>
#include <modbus/mb_types.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                           CORE TYPES                                       */
/* ========================================================================== */

/**
 * @brief Unified Modbus handle for client and server operations.
 *
 * This is an opaque handle that represents a Modbus connection or server.
 * It automatically manages:
 * - Transport (TCP/RTU/ASCII)
 * - Transaction pools
 * - Buffers
 * - Timeouts
 * - Error state
 *
 * Create with: mb_create_tcp(), mb_create_rtu(), mb_create_server()
 * Destroy with: mb_destroy() or MB_AUTO() for automatic cleanup
 */
typedef struct mb mb_t;

/**
 * @brief Connection options for customizing behavior.
 *
 * For most use cases, pass NULL to connection functions for defaults.
 * Use this only when you need fine-tuned control.
 */
typedef struct {
    uint32_t timeout_ms;        /**< Request timeout (default: 1000ms) */
    uint8_t max_retries;        /**< Max retry attempts (default: 3) */
    uint8_t pool_size;          /**< Transaction pool size (default: 8) */
    bool enable_logging;        /**< Enable debug logging (default: false) */
    bool enable_diagnostics;    /**< Enable diagnostics collection (default: true) */
} mb_options_t;

/**
 * @brief Initialize options structure with sensible defaults.
 *
 * Always call this before customizing options.
 *
 * @param opts Options structure to initialize (must not be NULL)
 *
 * @example
 * @code
 * mb_options_t opts;
 * mb_options_init(&opts);
 * opts.timeout_ms = 5000;     // Override just what you need
 * opts.enable_logging = true;
 *
 * mb_t *mb = mb_create_tcp_ex("192.168.1.10:502", &opts);
 * @endcode
 */
void mb_options_init(mb_options_t *opts);

/* ========================================================================== */
/*                           CONNECTION MANAGEMENT                            */
/* ========================================================================== */

/**
 * @brief Create a Modbus TCP client connection.
 *
 * This is the easiest way to connect to a Modbus TCP server.
 * All resources are automatically allocated and managed.
 *
 * @param endpoint Server address in format "host:port" or "host" (default port 502)
 *                 Examples: "192.168.1.10:502", "localhost", "modbus.local:5020"
 *
 * @return Modbus handle on success, NULL on failure (check errno)
 *
 * @note Connection timeout is 5 seconds
 * @note Auto-allocates transaction pool (8 slots), buffers (256 bytes)
 *
 * @example
 * @code
 * mb_t *mb = mb_create_tcp("192.168.1.10:502");
 * if (!mb) {
 *     perror("Connection failed");
 *     return 1;
 * }
 * // ... use mb ...
 * mb_destroy(mb);
 * @endcode
 */
mb_t *mb_create_tcp(const char *endpoint);

/**
 * @brief Create a Modbus TCP client with custom options.
 *
 * @param endpoint Server address (see mb_create_tcp())
 * @param opts Custom options (or NULL for defaults)
 *
 * @return Modbus handle on success, NULL on failure
 *
 * @example
 * @code
 * mb_options_t opts;
 * mb_options_init(&opts);
 * opts.timeout_ms = 2000;     // 2 seconds
 * opts.max_retries = 5;        // Retry 5 times
 *
 * mb_t *mb = mb_create_tcp_ex("192.168.1.10:502", &opts);
 * @endcode
 */
mb_t *mb_create_tcp_ex(const char *endpoint, const mb_options_t *opts);

/**
 * @brief Create a Modbus RTU client connection.
 *
 * Opens a serial port and initializes an RTU client with standard 8N1 framing.
 *
 * @param device Serial device path
 *               - Linux: "/dev/ttyUSB0", "/dev/ttyS0"
 *               - Windows: "COM3", "COM10"
 *               - macOS: "/dev/cu.usbserial"
 * @param baudrate Baud rate (common: 9600, 19200, 38400, 57600, 115200)
 *
 * @return Modbus handle on success, NULL on failure
 *
 * @note Uses 8 data bits, no parity, 1 stop bit
 * @note T3.5 character timeout is calculated automatically based on baudrate
 *
 * @example
 * @code
 * mb_t *mb = mb_create_rtu("/dev/ttyUSB0", 115200);
 * if (!mb) {
 *     perror("Failed to open serial port");
 *     return 1;
 * }
 * // ... use mb ...
 * mb_destroy(mb);
 * @endcode
 */
mb_t *mb_create_rtu(const char *device, uint32_t baudrate);

/**
 * @brief Create a Modbus RTU client with custom options.
 *
 * @param device Serial device path
 * @param baudrate Baud rate
 * @param opts Custom options (or NULL for defaults)
 *
 * @return Modbus handle on success, NULL on failure
 */
mb_t *mb_create_rtu_ex(const char *device, uint32_t baudrate, const mb_options_t *opts);

/**
 * @brief Destroy a Modbus handle and free all resources.
 *
 * Closes the connection, frees all allocated memory, and invalidates the handle.
 * Safe to call with NULL pointer (no-op).
 *
 * @param mb Modbus handle to destroy (may be NULL)
 *
 * @note After calling this, the handle is invalid and must not be used
 * @note Automatically called by MB_AUTO() macro when variable goes out of scope
 *
 * @example
 * @code
 * mb_t *mb = mb_create_tcp("192.168.1.10:502");
 * // ... use mb ...
 * mb_destroy(mb);
 * mb = NULL;  // Good practice to avoid dangling pointer
 * @endcode
 */
void mb_destroy(mb_t *mb);

/**
 * @brief Check if connection is still alive.
 *
 * @param mb Modbus handle
 * @return true if connected and healthy, false otherwise
 */
bool mb_is_connected(const mb_t *mb);

/**
 * @brief Attempt to reconnect after connection loss.
 *
 * @param mb Modbus handle
 * @return MB_OK on success, error code otherwise
 */
mb_err_t mb_reconnect(mb_t *mb);

/* ========================================================================== */
/*                           CLIENT OPERATIONS (READ)                         */
/* ========================================================================== */

/**
 * @brief Read holding registers (function code 0x03).
 *
 * Reads one or more 16-bit holding registers from a Modbus slave.
 * This is the most common Modbus operation.
 *
 * @param mb Modbus client handle
 * @param unit_id Slave/unit ID (1-247, or 0 for broadcast)
 * @param address Starting register address (0-65535)
 * @param count Number of registers to read (1-125)
 * @param out_registers Output buffer (must hold at least count uint16_t values)
 *
 * @return MB_OK on success, error code otherwise
 *         - MB_ERR_TIMEOUT: No response within timeout
 *         - MB_ERR_EXCEPTION: Slave returned exception (check mb_last_exception())
 *         - MB_ERR_INVALID_ARGUMENT: Invalid parameters
 *
 * @note Registers are returned in host byte order (no manual swapping needed)
 * @note Function blocks until response or timeout
 * @note For async operation, use the full client.h API
 *
 * @example
 * @code
 * uint16_t regs[10];
 * mb_err_t err = mb_read_holding(mb, 1, 0, 10, regs);
 * if (err == MB_OK) {
 *     printf("Register 0: %u\n", regs[0]);
 * } else {
 *     fprintf(stderr, "Read failed: %s\n", mb_error_string(err));
 * }
 * @endcode
 */
mb_err_t mb_read_holding(mb_t *mb, uint8_t unit_id, uint16_t address,
                         uint16_t count, uint16_t *out_registers);

/**
 * @brief Read input registers (function code 0x04).
 *
 * Similar to mb_read_holding() but for read-only input registers.
 *
 * @param mb Modbus client handle
 * @param unit_id Slave/unit ID (1-247)
 * @param address Starting register address
 * @param count Number of registers to read (1-125)
 * @param out_registers Output buffer
 *
 * @return MB_OK on success, error code otherwise
 */
mb_err_t mb_read_input(mb_t *mb, uint8_t unit_id, uint16_t address,
                       uint16_t count, uint16_t *out_registers);

/**
 * @brief Read coils (function code 0x01).
 *
 * Reads one or more 1-bit coils (digital outputs) from a Modbus slave.
 *
 * @param mb Modbus client handle
 * @param unit_id Slave/unit ID (1-247)
 * @param address Starting coil address
 * @param count Number of coils to read (1-2000)
 * @param out_coils Output buffer (packed 8 coils per byte, LSB first)
 *
 * @return MB_OK on success, error code otherwise
 *
 * @note Buffer must be at least (count + 7) / 8 bytes
 * @note To extract individual coils: `bool coil_n = (out_coils[n/8] >> (n%8)) & 1;`
 */
mb_err_t mb_read_coils(mb_t *mb, uint8_t unit_id, uint16_t address,
                       uint16_t count, uint8_t *out_coils);

/**
 * @brief Read discrete inputs (function code 0x02).
 *
 * Similar to mb_read_coils() but for read-only discrete inputs.
 *
 * @param mb Modbus client handle
 * @param unit_id Slave/unit ID (1-247)
 * @param address Starting input address
 * @param count Number of inputs to read (1-2000)
 * @param out_inputs Output buffer (packed 8 inputs per byte, LSB first)
 *
 * @return MB_OK on success, error code otherwise
 */
mb_err_t mb_read_discrete(mb_t *mb, uint8_t unit_id, uint16_t address,
                          uint16_t count, uint8_t *out_inputs);

/* ========================================================================== */
/*                           CLIENT OPERATIONS (WRITE)                        */
/* ========================================================================== */

/**
 * @brief Write a single holding register (function code 0x06).
 *
 * Writes a 16-bit value to a single holding register.
 *
 * @param mb Modbus client handle
 * @param unit_id Slave/unit ID (1-247, or 0 for broadcast)
 * @param address Register address
 * @param value Value to write (0-65535)
 *
 * @return MB_OK on success, error code otherwise
 *
 * @example
 * @code
 * mb_err_t err = mb_write_register(mb, 1, 100, 1234);
 * if (err != MB_OK) {
 *     fprintf(stderr, "Write failed: %s\n", mb_error_string(err));
 * }
 * @endcode
 */
mb_err_t mb_write_register(mb_t *mb, uint8_t unit_id, uint16_t address, uint16_t value);

/**
 * @brief Write a single coil (function code 0x05).
 *
 * Writes a boolean value to a single coil (digital output).
 *
 * @param mb Modbus client handle
 * @param unit_id Slave/unit ID (1-247, or 0 for broadcast)
 * @param address Coil address
 * @param value Coil state (true = ON/0xFF00, false = OFF/0x0000)
 *
 * @return MB_OK on success, error code otherwise
 */
mb_err_t mb_write_coil(mb_t *mb, uint8_t unit_id, uint16_t address, bool value);

/**
 * @brief Write multiple holding registers (function code 0x10).
 *
 * Writes multiple 16-bit values to consecutive holding registers.
 * More efficient than multiple mb_write_register() calls.
 *
 * @param mb Modbus client handle
 * @param unit_id Slave/unit ID (1-247, or 0 for broadcast)
 * @param address Starting register address
 * @param count Number of registers to write (1-123)
 * @param registers Input buffer containing count uint16_t values
 *
 * @return MB_OK on success, error code otherwise
 *
 * @example
 * @code
 * uint16_t values[] = {100, 200, 300, 400, 500};
 * mb_err_t err = mb_write_registers(mb, 1, 0, 5, values);
 * @endcode
 */
mb_err_t mb_write_registers(mb_t *mb, uint8_t unit_id, uint16_t address,
                            uint16_t count, const uint16_t *registers);

/**
 * @brief Write multiple coils (function code 0x0F).
 *
 * Writes multiple boolean values to consecutive coils.
 *
 * @param mb Modbus client handle
 * @param unit_id Slave/unit ID (1-247, or 0 for broadcast)
 * @param address Starting coil address
 * @param count Number of coils to write (1-1968)
 * @param coils Input buffer (packed 8 coils per byte, LSB first)
 *
 * @return MB_OK on success, error code otherwise
 */
mb_err_t mb_write_coils(mb_t *mb, uint8_t unit_id, uint16_t address,
                        uint16_t count, const uint8_t *coils);

/* ========================================================================== */
/*                           CONFIGURATION                                    */
/* ========================================================================== */

/**
 * @brief Set the default timeout for all requests.
 *
 * @param mb Modbus handle
 * @param timeout_ms Timeout in milliseconds (0 uses default: 1000ms)
 */
void mb_set_timeout(mb_t *mb, uint32_t timeout_ms);

/**
 * @brief Get the current timeout setting.
 *
 * @param mb Modbus handle
 * @return Timeout in milliseconds
 */
uint32_t mb_get_timeout(const mb_t *mb);

/**
 * @brief Enable or disable debug logging to stderr.
 *
 * @param mb Modbus handle
 * @param enable true to enable logging, false to disable
 */
void mb_set_logging(mb_t *mb, bool enable);

/* ========================================================================== */
/*                           ERROR HANDLING                                   */
/* ========================================================================== */

/**
 * @brief Get a human-readable error string.
 *
 * Converts error codes to descriptive strings for display/logging.
 *
 * @param err Error code from mb_* functions
 * @return Static string describing the error (never NULL)
 *
 * @example
 * @code
 * mb_err_t err = mb_read_holding(mb, 1, 0, 10, regs);
 * if (err != MB_OK) {
 *     fprintf(stderr, "Error: %s\n", mb_error_string(err));
 * }
 * @endcode
 */
const char *mb_error_string(mb_err_t err);

/**
 * @brief Get the last Modbus exception code received from server.
 *
 * When a mb_* function returns MB_ERR_EXCEPTION, this retrieves the
 * specific exception code sent by the Modbus slave.
 *
 * @param mb Modbus handle
 * @return Exception code (1-4, 5-6, etc.) or 0 if no exception occurred
 *
 * Common exception codes:
 * - 0x01: Illegal Function
 * - 0x02: Illegal Data Address
 * - 0x03: Illegal Data Value
 * - 0x04: Slave Device Failure
 *
 * @example
 * @code
 * mb_err_t err = mb_write_register(mb, 1, 9999, 100);
 * if (err == MB_ERR_EXCEPTION) {
 *     uint8_t exc = mb_last_exception(mb);
 *     printf("Server exception: 0x%02X\n", exc);
 *     if (exc == 0x02) {
 *         printf("(Illegal address)\n");
 *     }
 * }
 * @endcode
 */
uint8_t mb_last_exception(const mb_t *mb);

/* ========================================================================== */
/*                           AUTO-CLEANUP MACROS (GCC/Clang)                 */
/* ========================================================================== */

/**
 * @brief Automatic cleanup attribute (GCC/Clang only).
 *
 * When a variable declared with MB_AUTO goes out of scope, mb_destroy()
 * is called automatically. This prevents resource leaks even when returning
 * early or on error paths.
 *
 * On MSVC (which doesn't support cleanup attribute), MB_AUTO is equivalent
 * to a regular pointer and you must call mb_destroy() manually.
 *
 * @example
 * @code
 * void my_function(void) {
 *     MB_AUTO(mb, mb_create_tcp("192.168.1.10:502"));
 *     if (!mb) {
 *         return; // No leak - mb_destroy() called automatically
 *     }
 *
 *     mb_err_t err = mb_read_holding(mb, 1, 0, 10, regs);
 *     if (err != MB_OK) {
 *         return; // No leak - mb_destroy() called automatically
 *     }
 *
 *     // ... more code ...
 *
 *     // mb_destroy() called automatically at end of scope
 * }
 * @endcode
 */
#if defined(__GNUC__) || defined(__clang__)
    #define MB_AUTO_CLEANUP __attribute__((cleanup(mb_auto_cleanup_)))
    #define MB_AUTO(name, create_expr) \
        mb_t *name MB_AUTO_CLEANUP = (create_expr)
#else
    /* No auto-cleanup on MSVC - must call mb_destroy() manually */
    #define MB_AUTO(name, create_expr) \
        mb_t *name = (create_expr)
    #warning "MB_AUTO cleanup not supported on this compiler - call mb_destroy() manually"
#endif

/* Internal cleanup helper (used by MB_AUTO macro) */
static inline void mb_auto_cleanup_(mb_t **mb_ptr) {
    if (mb_ptr && *mb_ptr) {
        mb_destroy(*mb_ptr);
    }
}

/* ========================================================================== */
/*                           CONVENIENCE MACROS                               */
/* ========================================================================== */

/**
 * @brief Macro to simplify error checking with early return.
 *
 * Executes an expression, and if it returns an error, prints the message
 * and returns the error code from the current function.
 *
 * @example
 * @code
 * mb_err_t my_function(mb_t *mb) {
 *     MB_CHECK(mb_write_register(mb, 1, 100, 1234), "Failed to write reg 100");
 *     MB_CHECK(mb_write_register(mb, 1, 101, 5678), "Failed to write reg 101");
 *     return MB_OK;
 * }
 * @endcode
 */
#define MB_CHECK(expr, msg) do { \
    mb_err_t _err = (expr); \
    if (_err != MB_OK) { \
        fprintf(stderr, "%s: %s\n", (msg), mb_error_string(_err)); \
        return _err; \
    } \
} while (0)

/**
 * @brief Macro for simple error logging (doesn't return).
 *
 * Like MB_CHECK but only logs the error, doesn't return.
 *
 * @example
 * @code
 * MB_LOG_ERROR(mb_write_register(mb, 1, 100, 1234), "Write failed");
 * // Execution continues even if error occurred
 * @endcode
 */
#define MB_LOG_ERROR(expr, msg) do { \
    mb_err_t _err = (expr); \
    if (_err != MB_OK) { \
        fprintf(stderr, "%s: %s\n", (msg), mb_error_string(_err)); \
    } \
} while (0)

/**
 * @brief Assert that expression succeeds, abort on error.
 *
 * For cases where failure is unrecoverable.
 *
 * @example
 * @code
 * MB_ASSERT(mb_write_register(mb, 1, 100, 1234), "Critical write failed");
 * // Program terminates if write fails
 * @endcode
 */
#define MB_ASSERT(expr, msg) do { \
    mb_err_t _err = (expr); \
    if (_err != MB_OK) { \
        fprintf(stderr, "FATAL: %s: %s\n", (msg), mb_error_string(_err)); \
        abort(); \
    } \
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MB_SIMPLE_H */
