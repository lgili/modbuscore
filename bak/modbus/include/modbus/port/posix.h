/**
 * @file posix.h
 * @brief POSIX-backed transport helpers for Modbus clients and servers.
 */

#ifndef MODBUS_PORT_POSIX_H
#define MODBUS_PORT_POSIX_H

#include <stdbool.h>

#include <modbus/internal/transport_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Serial port parity modes for RTU configuration.
 */
typedef enum {
    MB_PARITY_NONE = 0,  /**< No parity bit */
    MB_PARITY_EVEN = 1,  /**< Even parity */
    MB_PARITY_ODD = 2    /**< Odd parity */
} mb_parity_t;

/**
 * @brief POSIX socket wrapper that exposes an ``mb_transport_if_t``.
 */
typedef struct mb_port_posix_socket {
    int fd;                    /**< Underlying file descriptor. */
    bool owns_fd;              /**< Close the descriptor on teardown when true. */
    mb_transport_if_t iface;   /**< Transport interface bound to this socket. */
} mb_port_posix_socket_t;

/**
 * @brief Initialises a POSIX socket wrapper around an existing file descriptor.
 *
 * The descriptor is switched to non-blocking mode. When @p owns_fd is ``true``
 * the wrapper becomes responsible for closing it during
 * ::mb_port_posix_socket_close.
 *
 * @param sock     Wrapper to initialise.
 * @param fd       Socket/file descriptor already opened by the caller.
 * @param owns_fd  Set to ``true`` if the wrapper should close the descriptor.
 *
 * @retval MB_OK                 Wrapper initialised successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Invalid pointer or descriptor.
 * @retval MB_ERR_TRANSPORT       ioctl/fcntl configuration failed.
 */
mb_err_t mb_port_posix_socket_init(mb_port_posix_socket_t *sock, int fd, bool owns_fd);

/**
 * @brief Closes the underlying descriptor when owned and resets the wrapper.
 *
 * Safe to call multiple times; subsequent invocations become no-ops.
 *
 * @param sock Wrapper previously initialised with ::mb_port_posix_socket_init.
 */
void mb_port_posix_socket_close(mb_port_posix_socket_t *sock);

/**
 * @brief Returns the transport interface bound to @p sock.
 *
 * The returned pointer remains valid until ::mb_port_posix_socket_close is
 * called.
 *
 * @param sock Wrapper initialised with ::mb_port_posix_socket_init.
 */
const mb_transport_if_t *mb_port_posix_socket_iface(mb_port_posix_socket_t *sock);

/**
 * @brief Connects to a Modbus TCP server using POSIX sockets.
 *
 * Creates a new client socket, connects to ``host:port`` with the provided
 * timeout, and populates @p sock on success (owning the descriptor).
 *
 * @param sock        Wrapper that receives the connected socket.
 * @param host        Remote hostname or IP string.
 * @param port        Remote TCP port.
 * @param timeout_ms  Connection timeout in milliseconds.
 *
 * @retval MB_OK                 Connection established successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Invalid parameter.
 * @retval MB_ERR_TRANSPORT       Connect failed or timed out.
 */
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
mb_err_t mb_port_posix_tcp_client(mb_port_posix_socket_t *sock,
                                  const char *host,
                                  uint16_t port,
                                  mb_time_ms_t timeout_ms);
// NOLINTEND(bugprone-easily-swappable-parameters)

/**
 * @brief Opens a serial port for Modbus RTU communication.
 *
 * Creates a new serial connection with the specified parameters and populates
 * @p sock on success (owning the file descriptor). The port is configured for
 * raw, non-blocking I/O suitable for RTU framing.
 *
 * @param sock        Wrapper that receives the opened serial port.
 * @param device      Serial device path (e.g., "/dev/ttyUSB0", "/dev/ttyS1").
 * @param baudrate    Baud rate (9600, 19200, 38400, 57600, 115200, etc.).
 * @param parity      Parity mode (MB_PARITY_NONE, MB_PARITY_EVEN, MB_PARITY_ODD).
 * @param data_bits   Number of data bits (5, 6, 7, or 8).
 * @param stop_bits   Number of stop bits (1 or 2).
 *
 * @retval MB_OK                 Serial port opened successfully.
 * @retval MB_ERR_INVALID_ARGUMENT Invalid parameter.
 * @retval MB_ERR_TRANSPORT       Failed to open or configure device.
 *
 * @note Not available on Windows. Use Windows-specific serial APIs instead.
 */
mb_err_t mb_port_posix_serial_open(mb_port_posix_socket_t *sock,
                                   const char *device,
                                   uint32_t baudrate,
                                   mb_parity_t parity,
                                   uint8_t data_bits,
                                   uint8_t stop_bits);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_PORT_POSIX_H */
