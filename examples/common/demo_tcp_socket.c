#include "demo_tcp_socket.h"

#include <string.h>

#if defined(_WIN32)
mb_err_t demo_tcp_socket_global_init(void)
{
    return mb_port_win_socket_global_init();
}

void demo_tcp_socket_global_cleanup(void)
{
    mb_port_win_socket_global_cleanup();
}
#endif

mb_err_t demo_tcp_socket_connect(demo_tcp_socket_t *sock,
                                 const char *host,
                                 uint16_t port,
                                 mb_time_ms_t timeout_ms)
{
    if (sock == NULL || host == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    memset(sock, 0, sizeof(*sock));

#if defined(_WIN32)
    mb_err_t status = mb_port_win_socket_global_init();
    if (!mb_err_is_ok(status)) {
        return status;
    }

    status = mb_port_win_tcp_client(&sock->win, host, port, timeout_ms);
    if (!mb_err_is_ok(status)) {
        mb_port_win_socket_global_cleanup();
        return status;
    }
#else
    mb_err_t status = mb_port_posix_tcp_client(&sock->posix, host, port, timeout_ms);
    if (!mb_err_is_ok(status)) {
        return status;
    }
#endif

    sock->active = true;
    return MB_OK;
}

const mb_transport_if_t *demo_tcp_socket_iface(demo_tcp_socket_t *sock)
{
    if (sock == NULL || !sock->active) {
        return NULL;
    }
#if defined(_WIN32)
    return mb_port_win_socket_iface(&sock->win);
#else
    return mb_port_posix_socket_iface(&sock->posix);
#endif
}

void demo_tcp_socket_close(demo_tcp_socket_t *sock)
{
    if (sock == NULL || !sock->active) {
        return;
    }

#if defined(_WIN32)
    mb_port_win_socket_close(&sock->win);
    mb_port_win_socket_global_cleanup();
#else
    mb_port_posix_socket_close(&sock->posix);
#endif

    sock->active = false;
}
