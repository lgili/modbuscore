#ifndef MODBUS_PORT_WIN_H
#define MODBUS_PORT_WIN_H

/**
 * @file win.h
 * @brief Windows Winsock helper utilities that expose an ``mb_transport_if_t``.
 */

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

#include <stdbool.h>
#include <stdint.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <modbus/mb_err.h>
#include <modbus/mb_types.h>
#include <modbus/transport_if.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Wrapper around a Winsock socket exposing ``mb_transport_if_t``.
 */
typedef struct mb_port_win_socket {
    SOCKET handle;              /**< Underlying socket handle. */
    bool owns_handle;           /**< Close the socket during teardown when true. */
    mb_transport_if_t iface;    /**< Transport interface bound to this socket. */
} mb_port_win_socket_t;

/**
 * @brief Ensures the WinSock subsystem is initialised.
 *
 * Safe to call multiple times; reference counted internally.
 */
mb_err_t mb_port_win_socket_global_init(void);

/**
 * @brief Releases a reference obtained with ::mb_port_win_socket_global_init.
 */
void mb_port_win_socket_global_cleanup(void);

/**
 * @brief Wraps an existing socket handle and configures it for non-blocking I/O.
 */
mb_err_t mb_port_win_socket_init(mb_port_win_socket_t *sock, SOCKET handle, bool owns_handle);

/**
 * @brief Tears down a previously initialised wrapper.
 */
void mb_port_win_socket_close(mb_port_win_socket_t *sock);

/**
 * @brief Returns the transport interface bound to @p sock.
 */
const mb_transport_if_t *mb_port_win_socket_iface(mb_port_win_socket_t *sock);

/**
 * @brief Creates a TCP client connection to ``host:port`` and wraps it.
 */
mb_err_t mb_port_win_tcp_client(mb_port_win_socket_t *sock,
                                const char *host,
                                uint16_t port,
                                mb_time_ms_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#endif /* MODBUS_PORT_WIN_H */
