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

#if !defined(_WIN32)
#include <termios.h>

/**
 * @brief Convert baudrate to termios speed constant.
 */
static speed_t baudrate_to_speed(uint32_t baudrate)
{
    switch (baudrate) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
#ifdef B230400
        case 230400: return B230400;
#endif
#ifdef B460800
        case 460800: return B460800;
#endif
#ifdef B500000
        case 500000: return B500000;
#endif
#ifdef B576000
        case 576000: return B576000;
#endif
#ifdef B921600
        case 921600: return B921600;
#endif
        default:     return B9600; // fallback
    }
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
mb_err_t mb_port_posix_serial_open(mb_port_posix_socket_t *sock,
                                   const char *device,
                                   uint32_t baudrate,
                                   mb_parity_t parity,
                                   uint8_t data_bits,
                                   uint8_t stop_bits)
{
    if (sock == NULL || device == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Open serial device
    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return MB_ERR_TRANSPORT;
    }

    // Configure serial port
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return MB_ERR_TRANSPORT;
    }

    // Set baudrate
    speed_t speed = baudrate_to_speed(baudrate);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    // Set data bits (CS5/CS6/CS7/CS8)
    tty.c_cflag &= ~CSIZE;
    switch (data_bits) {
        case 5: tty.c_cflag |= CS5; break;
        case 6: tty.c_cflag |= CS6; break;
        case 7: tty.c_cflag |= CS7; break;
        case 8: // fallthrough
        default: tty.c_cflag |= CS8; break;
    }

    // Set parity
    switch (parity) {
        case MB_PARITY_NONE:
            tty.c_cflag &= ~PARENB;
            break;
        case MB_PARITY_EVEN:
            tty.c_cflag |= PARENB;
            tty.c_cflag &= ~PARODD;
            break;
        case MB_PARITY_ODD:
            tty.c_cflag |= PARENB;
            tty.c_cflag |= PARODD;
            break;
    }

    // Set stop bits
    if (stop_bits == 2) {
        tty.c_cflag |= CSTOPB;
    } else {
        tty.c_cflag &= ~CSTOPB;
    }

    // Enable receiver, ignore modem control lines
    tty.c_cflag |= CREAD | CLOCAL;

    // Raw mode (no input processing)
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // No output processing
    tty.c_oflag &= ~OPOST;

    // No input processing (disable software flow control)
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    // Non-blocking reads with timeout
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1; // 0.1 second timeout

    // Apply configuration
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return MB_ERR_TRANSPORT;
    }

    // Flush any pending data
    tcflush(fd, TCIOFLUSH);

    // Initialize socket wrapper
    return mb_port_posix_socket_init(sock, fd, true);
}
// NOLINTEND(bugprone-easily-swappable-parameters)
#endif // !_WIN32
