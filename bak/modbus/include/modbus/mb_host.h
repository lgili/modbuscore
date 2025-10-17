/**
 * @file mb_host.h
 * @brief Simplified API for host applications (desktop/Linux/macOS/Windows) and desktop applications.
 *
 * This header provides a streamlined interface for common Modbus operations,
 * hiding transport setup complexity and offering synchronous helpers. Perfect
 * for learning, testing, and simple desktop tools.
 *
 * **Quick Start (TCP Client):**
 * @code
 * #include <modbus/mb_host.h>
 *
 * int main(void) {
 *     // Connect to Modbus TCP server
 *     mb_host_client_t *client = mb_host_tcp_connect("192.168.1.10:502");
 *     if (client == NULL) {
 *         printf("Connection failed\n");
 *         return 1;
 *     }
 *
 *     // Read 10 holding registers
 *     uint16_t registers[10];
 *     mb_err_t err = mb_host_read_holding(client, 1, 0x1000, 10, registers);
 *     if (err == MB_OK) {
 *         printf("Register 0: %u\n", registers[0]);
 *     } else {
 *         printf("Error: %s\n", mb_host_error_string(err));
 *     }
 *
 *     mb_host_disconnect(client);
 *     return 0;
 * }
 * @endcode
 *
 * @note This API is designed for simplicity, not performance. For production
 *       embedded systems, use the full `mb_client_*` API with custom transports.
 *
 * @warning Thread safety: Each `mb_host_client_t` must be used from a single thread.
 */

#ifndef MODBUS_MB_HOST_H
#define MODBUS_MB_HOST_H

#include <modbus/mb_err.h>
#include <modbus/mb_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle for quick API client.
 *
 * Internally manages transport, transaction pool, and client state.
 */
typedef struct mb_host_client mb_host_client_t;

/* -------------------------------------------------------------------------- */
/*                            Connection Management                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Connect to a Modbus TCP server.
 *
 * Establishes a TCP connection and initializes the Modbus client. The
 * connection is blocking and will timeout after 5 seconds.
 *
 * @param host_port Server address in format "host:port" or "host" (defaults to port 502).
 *                  Examples: "192.168.1.10:502", "localhost", "modbus.example.com:5020"
 *
 * @return Opaque client handle on success, NULL on failure (check errno for details).
 *
 * @note The returned handle must be freed with mb_host_disconnect() when done.
 *
 * @example
 * @code
 * mb_host_client_t *client = mb_host_tcp_connect("192.168.1.10:502");
 * if (client == NULL) {
 *     perror("Connection failed");
 *     return 1;
 * }
 * // ... use client ...
 * mb_host_disconnect(client);
 * @endcode
 */
mb_host_client_t *mb_host_tcp_connect(const char *host_port);

/**
 * @brief Connect to a Modbus RTU serial device.
 *
 * Opens a serial port and initializes the RTU client with standard 8N1 framing.
 *
 * @param device Serial device path (e.g., "/dev/ttyUSB0", "COM3", "/dev/ttyS1")
 * @param baudrate Baud rate (9600, 19200, 38400, 57600, 115200, etc.)
 *
 * @return Opaque client handle on success, NULL on failure.
 *
 * @note Uses default settings: 8 data bits, no parity, 1 stop bit, T3.5 = 5ms.
 *       For custom framing, use the full `mb_client_*` API.
 *
 * @example
 * @code
 * mb_host_client_t *client = mb_host_rtu_connect("/dev/ttyUSB0", 115200);
 * if (client == NULL) {
 *     fprintf(stderr, "Failed to open serial port\n");
 *     return 1;
 * }
 * // ... use client ...
 * mb_host_disconnect(client);
 * @endcode
 */
mb_host_client_t *mb_host_rtu_connect(const char *device, uint32_t baudrate);

/**
 * @brief Disconnect and free all resources.
 *
 * Closes the underlying transport (TCP socket or serial port) and releases
 * memory. The client handle becomes invalid after this call.
 *
 * @param client Client handle returned by mb_host_tcp_connect() or mb_host_rtu_connect().
 *               Passing NULL is safe (no-op).
 */
void mb_host_disconnect(mb_host_client_t *client);

/* -------------------------------------------------------------------------- */
/*                          Synchronous Read Operations                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Read holding registers (function code 0x03).
 *
 * Sends a request and blocks until the response is received or a timeout occurs.
 * Default timeout is 1000ms per request.
 *
 * @param client Client handle from mb_host_tcp_connect() or mb_host_rtu_connect()
 * @param unit_id Modbus unit/slave ID (1-247)
 * @param address Starting register address (0-65535)
 * @param count Number of registers to read (1-125)
 * @param out_registers Output buffer (must hold at least `count` uint16_t values)
 *
 * @return MB_OK on success, error code otherwise.
 *         Use mb_host_error_string() to get a human-readable error message.
 *
 * @note Registers are returned in host byte order (no manual byte swapping needed).
 *
 * @example
 * @code
 * uint16_t regs[10];
 * mb_err_t err = mb_host_read_holding(client, 1, 0x1000, 10, regs);
 * if (err != MB_OK) {
 *     printf("Read failed: %s\n", mb_host_error_string(err));
 * } else {
 *     printf("First register: %u\n", regs[0]);
 * }
 * @endcode
 */
mb_err_t mb_host_read_holding(mb_host_client_t *client,
                               uint8_t unit_id,
                               uint16_t address,
                               uint16_t count,
                               uint16_t *out_registers);

/**
 * @brief Read input registers (function code 0x04).
 *
 * @param client Client handle
 * @param unit_id Modbus unit/slave ID (1-247)
 * @param address Starting register address (0-65535)
 * @param count Number of registers to read (1-125)
 * @param out_registers Output buffer (must hold at least `count` uint16_t values)
 *
 * @return MB_OK on success, error code otherwise.
 */
mb_err_t mb_host_read_input(mb_host_client_t *client,
                             uint8_t unit_id,
                             uint16_t address,
                             uint16_t count,
                             uint16_t *out_registers);

/**
 * @brief Read coils (function code 0x01).
 *
 * @param client Client handle
 * @param unit_id Modbus unit/slave ID (1-247)
 * @param address Starting coil address (0-65535)
 * @param count Number of coils to read (1-2000)
 * @param out_coils Output buffer (must hold at least `(count + 7) / 8` bytes)
 *
 * @return MB_OK on success, error code otherwise.
 *
 * @note Coils are packed 8 per byte (LSB first). Use bit operations to extract individual values.
 */
mb_err_t mb_host_read_coils(mb_host_client_t *client,
                             uint8_t unit_id,
                             uint16_t address,
                             uint16_t count,
                             uint8_t *out_coils);

/**
 * @brief Read discrete inputs (function code 0x02).
 *
 * @param client Client handle
 * @param unit_id Modbus unit/slave ID (1-247)
 * @param address Starting input address (0-65535)
 * @param count Number of inputs to read (1-2000)
 * @param out_inputs Output buffer (must hold at least `(count + 7) / 8` bytes)
 *
 * @return MB_OK on success, error code otherwise.
 */
mb_err_t mb_host_read_discrete(mb_host_client_t *client,
                               uint8_t unit_id,
                               uint16_t address,
                               uint16_t count,
                               uint8_t *out_inputs);

/* -------------------------------------------------------------------------- */
/*                          Synchronous Write Operations                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief Write a single holding register (function code 0x06).
 *
 * @param client Client handle
 * @param unit_id Modbus unit/slave ID (1-247)
 * @param address Register address (0-65535)
 * @param value Value to write (0-65535)
 *
 * @return MB_OK on success, error code otherwise.
 *
 * @example
 * @code
 * mb_err_t err = mb_host_write_single_register(client, 1, 0x2000, 1234);
 * if (err != MB_OK) {
 *     printf("Write failed: %s\n", mb_host_error_string(err));
 * }
 * @endcode
 */
mb_err_t mb_host_write_single_register(mb_host_client_t *client,
                                       uint8_t unit_id,
                                       uint16_t address,
                                       uint16_t value);

/**
 * @brief Write a single coil (function code 0x05).
 *
 * @param client Client handle
 * @param unit_id Modbus unit/slave ID (1-247)
 * @param address Coil address (0-65535)
 * @param value Coil state (true = ON, false = OFF)
 *
 * @return MB_OK on success, error code otherwise.
 */
mb_err_t mb_host_write_single_coil(mb_host_client_t *client,
                                   uint8_t unit_id,
                                   uint16_t address,
                                   bool value);

/**
 * @brief Write multiple holding registers (function code 0x10).
 *
 * @param client Client handle
 * @param unit_id Modbus unit/slave ID (1-247)
 * @param address Starting register address (0-65535)
 * @param count Number of registers to write (1-123)
 * @param registers Input buffer containing `count` uint16_t values
 *
 * @return MB_OK on success, error code otherwise.
 *
 * @example
 * @code
 * uint16_t values[] = {100, 200, 300};
 * mb_err_t err = mb_host_write_multiple_registers(client, 1, 0x2000, 3, values);
 * @endcode
 */
mb_err_t mb_host_write_multiple_registers(mb_host_client_t *client,
                                          uint8_t unit_id,
                                          uint16_t address,
                                          uint16_t count,
                                          const uint16_t *registers);

/**
 * @brief Write multiple coils (function code 0x0F).
 *
 * @param client Client handle
 * @param unit_id Modbus unit/slave ID (1-247)
 * @param address Starting coil address (0-65535)
 * @param count Number of coils to write (1-1968)
 * @param coils Input buffer (packed 8 coils per byte, LSB first)
 *
 * @return MB_OK on success, error code otherwise.
 */
mb_err_t mb_host_write_multiple_coils(mb_host_client_t *client,
                                      uint8_t unit_id,
                                      uint16_t address,
                                      uint16_t count,
                                      const uint8_t *coils);

/* -------------------------------------------------------------------------- */
/*                              Configuration                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Set the default timeout for synchronous operations.
 *
 * @param client Client handle
 * @param timeout_ms Timeout in milliseconds (default: 1000ms)
 *
 * @example
 * @code
 * mb_host_set_timeout(client, 5000); // 5 seconds for slow devices
 * @endcode
 */
void mb_host_set_timeout(mb_host_client_t *client, uint32_t timeout_ms);

/**
 * @brief Enable or disable console logging.
 *
 * When enabled, the quick API prints diagnostic messages to stdout/stderr.
 *
 * @param client Client handle
 * @param enable true to enable logging, false to disable (default: disabled)
 *
 * @example
 * @code
 * mb_host_enable_logging(client, true); // Debug mode
 * @endcode
 */
void mb_host_enable_logging(mb_host_client_t *client, bool enable);

/* -------------------------------------------------------------------------- */
/*                              Error Handling                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Get a human-readable error message.
 *
 * Converts error codes to descriptive strings for debugging and logging.
 *
 * @param err Error code returned by quick API functions
 *
 * @return Static string describing the error (never NULL)
 *
 * @example
 * @code
 * mb_err_t err = mb_host_read_holding(client, 1, 0, 10, regs);
 * if (err != MB_OK) {
 *     fprintf(stderr, "Error: %s\n", mb_host_error_string(err));
 * }
 * @endcode
 */
const char *mb_host_error_string(mb_err_t err);

/**
 * @brief Get the last Modbus exception code received.
 *
 * When a function returns MB_ERR_EXCEPTION, this retrieves the specific
 * exception code sent by the server.
 *
 * @param client Client handle
 *
 * @return Exception code (1-4, 5-6, etc.) or 0 if no exception occurred.
 *
 * @example
 * @code
 * mb_err_t err = mb_host_write_single_register(client, 1, 0x9999, 100);
 * if (err == MB_ERR_EXCEPTION) {
 *     uint8_t exc = mb_host_last_exception(client);
 *     printf("Server returned exception 0x%02X\n", exc);
 * }
 * @endcode
 */
uint8_t mb_host_last_exception(mb_host_client_t *client);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MB_HOST_H */
