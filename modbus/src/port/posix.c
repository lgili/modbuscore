#include <modbus/port/posix.h>

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <modbus/mb_err.h>

static mb_err_t posix_socket_make_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return MB_ERR_TRANSPORT;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return MB_ERR_TRANSPORT;
    }
    return MB_OK;
}

static mb_time_ms_t posix_monotonic_ms(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (mb_time_ms_t)ts.tv_sec * 1000ULL + (mb_time_ms_t)(ts.tv_nsec / 1000000ULL);
    }
#endif
    return 0U;
}

static mb_err_t posix_send(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out)
{
    mb_port_posix_socket_t *sock = (mb_port_posix_socket_t *)ctx;
    if (sock == NULL || sock->fd < 0 || buf == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_size_t total = 0U;
    while (total < len) {
        ssize_t written = write(sock->fd, buf + total, (size_t)(len - total));
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (out) {
                    out->processed = total;
                }
                return (total == 0U) ? MB_ERR_TIMEOUT : MB_OK;
            }
            return MB_ERR_TRANSPORT;
        }
        total += (mb_size_t)written;
    }

    if (out) {
        out->processed = total;
    }
    return MB_OK;
}

static mb_err_t posix_recv(void *ctx, mb_u8 *buf, mb_size_t cap, mb_transport_io_result_t *out)
{
    mb_port_posix_socket_t *sock = (mb_port_posix_socket_t *)ctx;
    if (sock == NULL || sock->fd < 0 || buf == NULL || cap == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    while (true) {
        ssize_t received = read(sock->fd, buf, (size_t)cap);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
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
}

static mb_time_ms_t posix_now(void *ctx)
{
    (void)ctx;
    return posix_monotonic_ms();
}

static void posix_yield(void *ctx)
{
    (void)ctx;
    sched_yield();
}

static void posix_socket_bind_iface(mb_port_posix_socket_t *sock)
{
    sock->iface.ctx = sock;
    sock->iface.send = posix_send;
    sock->iface.recv = posix_recv;
    sock->iface.now = posix_now;
    sock->iface.yield = posix_yield;
}

mb_err_t mb_port_posix_socket_init(mb_port_posix_socket_t *sock, int fd, bool owns_fd)
{
    if (sock == NULL || fd < 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_err_t status = posix_socket_make_nonblocking(fd);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    sock->fd = fd;
    sock->owns_fd = owns_fd;
    posix_socket_bind_iface(sock);
    return MB_OK;
}

void mb_port_posix_socket_close(mb_port_posix_socket_t *sock)
{
    if (sock == NULL) {
        return;
    }
    if (sock->owns_fd && sock->fd >= 0) {
        (void)close(sock->fd);
    }
    sock->fd = -1;
    sock->owns_fd = false;
    memset(&sock->iface, 0, sizeof(sock->iface));
}

const mb_transport_if_t *mb_port_posix_socket_iface(mb_port_posix_socket_t *sock)
{
    if (sock == NULL) {
        return NULL;
    }
    return &sock->iface;
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
mb_err_t mb_port_posix_tcp_client(mb_port_posix_socket_t *sock,
                                  const char *host,
                                  uint16_t port,
                                  mb_time_ms_t timeout_ms)
{
    if (sock == NULL || host == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%u", (unsigned)port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *res = NULL;
    const int gai = getaddrinfo(host, port_str, &hints, &res);
    if (gai != 0 || res == NULL) {
        return MB_ERR_TRANSPORT;
    }

    mb_err_t result = MB_ERR_TRANSPORT;
    for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next) {
        int fd = (int)socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }

        if (!mb_err_is_ok(posix_socket_make_nonblocking(fd))) {
            close(fd);
            continue;
        }

        int rc = connect(fd, ai->ai_addr, ai->ai_addrlen);
        if (rc < 0) {
            if (errno != EINPROGRESS) {
                close(fd);
                continue;
            }

            struct pollfd pfd;
            pfd.fd = fd;
            pfd.events = POLLOUT;
            pfd.revents = 0;

            int timeout = (timeout_ms > (mb_time_ms_t)INT_MAX) ? INT_MAX : (int)timeout_ms;
            rc = poll(&pfd, 1, timeout);
            if (rc <= 0) {
                close(fd);
                continue;
            }

            int err = 0;
            socklen_t len = sizeof(err);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0 || err != 0) {
                close(fd);
                continue;
            }
        }

        result = mb_port_posix_socket_init(sock, fd, true);
        if (mb_err_is_ok(result)) {
            break;
        }
        close(fd);
    }

    freeaddrinfo(res);
    return result;
}
// NOLINTEND(bugprone-easily-swappable-parameters)
