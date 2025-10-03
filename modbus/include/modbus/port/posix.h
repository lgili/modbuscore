/**
 * @file posix.h
 * @brief POSIX-backed transport helpers for Modbus clients and servers (Gate 9).
 */

#ifndef MODBUS_PORT_POSIX_H
#define MODBUS_PORT_POSIX_H

#include <stdbool.h>

#include <modbus/transport_if.h>

#ifdef __cplusplus
extern "C" {
#endif

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
 * The descriptor is switched to non-blocking mode.  When @p owns_fd is true the
 * wrapper closes the descriptor from :c:func:`mb_port_posix_socket_close`.
 */
mb_err_t mb_port_posix_socket_init(mb_port_posix_socket_t *sock, int fd, bool owns_fd);

/**
 * @brief Closes the underlying descriptor when owned and resets the wrapper.
 */
void mb_port_posix_socket_close(mb_port_posix_socket_t *sock);

/**
 * @brief Returns the transport interface bound to @p sock.
 */
const mb_transport_if_t *mb_port_posix_socket_iface(mb_port_posix_socket_t *sock);

/**
 * @brief Connects to a Modbus TCP server using POSIX sockets.
 *
 * Creates a new client socket, connects to ``host:port`` with the provided
 * timeout, and populates @p sock on success (owning the descriptor).
 */
mb_err_t mb_port_posix_tcp_client(mb_port_posix_socket_t *sock,
                                  const char *host,
                                  uint16_t port,
                                  mb_time_ms_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_PORT_POSIX_H */
