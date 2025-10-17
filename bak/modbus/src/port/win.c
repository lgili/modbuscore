#ifdef _WIN32

#include <modbus/port/win.h>

#include <windows.h>

#include <limits.h>
#include <stddef.h>
#include <stdio.h>

static LONG g_win_socket_refs = 0;

static mb_err_t mb_port_win_socket_make_nonblocking(SOCKET handle)
{
    if (handle == INVALID_SOCKET) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    u_long mode = 1UL;
    if (ioctlsocket(handle, FIONBIO, &mode) != 0) {
        return MB_ERR_TRANSPORT;
    }
    return MB_OK;
}

static mb_time_ms_t mb_port_win_now(void *ctx)
{
    (void)ctx;
    return (mb_time_ms_t)GetTickCount64();
}

static void mb_port_win_yield(void *ctx)
{
    (void)ctx;
    Sleep(0);
}

static mb_err_t mb_port_win_send(void *ctx,
                                 const mb_u8 *buf,
                                 mb_size_t len,
                                 mb_transport_io_result_t *out)
{
    mb_port_win_socket_t *sock = (mb_port_win_socket_t *)ctx;
    if (sock == NULL || sock->handle == INVALID_SOCKET || buf == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_size_t total = 0U;
    while (total < len) {
        const int to_write = (len - total > (mb_size_t)INT_MAX) ? INT_MAX : (int)(len - total);
        const int sent = send(sock->handle, (const char *)buf + total, to_write, 0);
        if (sent < 0) {
            const int err = WSAGetLastError();
            if (err == WSAEINTR) {
                continue;
            }
            if (err == WSAEWOULDBLOCK) {
                if (out) {
                    out->processed = total;
                }
                return (total == 0U) ? MB_ERR_TIMEOUT : MB_OK;
            }
            return MB_ERR_TRANSPORT;
        }
        if (sent == 0) {
            break;
        }
        total += (mb_size_t)sent;
    }

    if (out) {
        out->processed = total;
    }
    return (total == len) ? MB_OK : MB_ERR_TRANSPORT;
}

static mb_err_t mb_port_win_recv(void *ctx,
                                 mb_u8 *buf,
                                 mb_size_t cap,
                                 mb_transport_io_result_t *out)
{
    mb_port_win_socket_t *sock = (mb_port_win_socket_t *)ctx;
    if (sock == NULL || sock->handle == INVALID_SOCKET || buf == NULL || cap == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

retry_recv:;
    const int to_read = (cap > (mb_size_t)INT_MAX) ? INT_MAX : (int)cap;
    const int received = recv(sock->handle, (char *)buf, to_read, 0);
    if (received < 0) {
        const int err = WSAGetLastError();
        if (err == WSAEINTR) {
            goto retry_recv;
        }
        if (err == WSAEWOULDBLOCK) {
            if (out) {
                out->processed = 0U;
            }
            return MB_ERR_TIMEOUT;
        }
        return MB_ERR_TRANSPORT;
    }

    if (received == 0) {
        if (out) {
            out->processed = 0U;
        }
        return MB_ERR_TRANSPORT;
    }

    if (out) {
        out->processed = (mb_size_t)received;
    }
    return MB_OK;
}

static void mb_port_win_bind_iface(mb_port_win_socket_t *sock)
{
    sock->iface.ctx = sock;
    sock->iface.send = mb_port_win_send;
    sock->iface.recv = mb_port_win_recv;
    sock->iface.now = mb_port_win_now;
    sock->iface.yield = mb_port_win_yield;
}

mb_err_t mb_port_win_socket_global_init(void)
{
    LONG refs = InterlockedIncrement(&g_win_socket_refs);
    if (refs == 1) {
        WSADATA data;
        const int rc = WSAStartup(MAKEWORD(2, 2), &data);
        if (rc != 0) {
            (void)InterlockedDecrement(&g_win_socket_refs);
            return MB_ERR_TRANSPORT;
        }
    }
    return MB_OK;
}

void mb_port_win_socket_global_cleanup(void)
{
    LONG refs = InterlockedDecrement(&g_win_socket_refs);
    if (refs <= 0) {
        WSACleanup();
        g_win_socket_refs = 0;
    }
}

mb_err_t mb_port_win_socket_init(mb_port_win_socket_t *sock, SOCKET handle, bool owns_handle)
{
    if (sock == NULL || handle == INVALID_SOCKET) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_err_t status = mb_port_win_socket_make_nonblocking(handle);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    sock->handle = handle;
    sock->owns_handle = owns_handle;
    mb_port_win_bind_iface(sock);
    return MB_OK;
}

void mb_port_win_socket_close(mb_port_win_socket_t *sock)
{
    if (sock == NULL) {
        return;
    }

    if (sock->owns_handle && sock->handle != INVALID_SOCKET) {
        closesocket(sock->handle);
    }

    sock->handle = INVALID_SOCKET;
    sock->owns_handle = false;
    sock->iface.ctx = NULL;
    sock->iface.send = NULL;
    sock->iface.recv = NULL;
    sock->iface.now = NULL;
    sock->iface.yield = NULL;
}

const mb_transport_if_t *mb_port_win_socket_iface(mb_port_win_socket_t *sock)
{
    if (sock == NULL) {
        return NULL;
    }
    return &sock->iface;
}

mb_err_t mb_port_win_tcp_client(mb_port_win_socket_t *sock,
                                const char *host,
                                uint16_t port,
                                mb_time_ms_t timeout_ms)
{
    if (sock == NULL || host == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    char service[8];
    _snprintf_s(service, sizeof(service), _TRUNCATE, "%u", (unsigned)port);

    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result = NULL;
    const int gai = getaddrinfo(host, service, &hints, &result);
    if (gai != 0 || result == NULL) {
        return MB_ERR_TRANSPORT;
    }

    mb_err_t status = MB_ERR_TRANSPORT;
    for (struct addrinfo *ai = result; ai != NULL; ai = ai->ai_next) {
        SOCKET handle = WSASocket(ai->ai_family, ai->ai_socktype, ai->ai_protocol, NULL, 0, 0);
        if (handle == INVALID_SOCKET) {
            continue;
        }

        if (!mb_err_is_ok(mb_port_win_socket_make_nonblocking(handle))) {
            closesocket(handle);
            continue;
        }

        const int rc = connect(handle, ai->ai_addr, (int)ai->ai_addrlen);
        if (rc != 0) {
            const int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
                closesocket(handle);
                continue;
            }

            fd_set write_set;
            FD_ZERO(&write_set);
            FD_SET(handle, &write_set);

            struct timeval tv;
            tv.tv_sec = (long)(timeout_ms / 1000U);
            tv.tv_usec = (long)((timeout_ms % 1000U) * 1000U);

            const int sel = select(0, NULL, &write_set, NULL, (timeout_ms == 0U) ? NULL : &tv);
            if (sel <= 0 || !FD_ISSET(handle, &write_set)) {
                closesocket(handle);
                continue;
            }

            int so_error = 0;
            int optlen = sizeof(so_error);
            if (getsockopt(handle, SOL_SOCKET, SO_ERROR, (char *)&so_error, &optlen) != 0 || so_error != 0) {
                closesocket(handle);
                continue;
            }
        }

        status = mb_port_win_socket_init(sock, handle, true);
        if (mb_err_is_ok(status)) {
            break;
        }

        closesocket(handle);
    }

    freeaddrinfo(result);
    return status;
}

#endif /* _WIN32 */
