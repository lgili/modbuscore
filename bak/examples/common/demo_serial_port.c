#include "demo_serial_port.h"

#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#endif

#if !defined(_WIN32)
#include <sched.h>
#endif

#if defined(_WIN32)
#define DEMO_SERIAL_HANDLE(port) ((HANDLE)(port)->handle)
#else
#define DEMO_SERIAL_FD(port) ((port)->fd)
#endif

static mb_err_t demo_serial_send(void *ctx,
                                 const mb_u8 *buf,
                                 mb_size_t len,
                                 mb_transport_io_result_t *out)
{
    demo_serial_port_t *port = (demo_serial_port_t *)ctx;
    if (port == NULL || !port->active || buf == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

#if defined(_WIN32)
    HANDLE handle = DEMO_SERIAL_HANDLE(port);
    if (handle == INVALID_HANDLE_VALUE || handle == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_size_t total = 0U;
    while (total < len) {
        DWORD written = 0U;
        const DWORD to_write = (DWORD)((len - total) > (mb_size_t)DWORD_MAX ? DWORD_MAX : (len - total));
        if (!WriteFile(handle, buf + total, to_write, &written, NULL)) {
            return MB_ERR_TRANSPORT;
        }
        if (written == 0U) {
            break;
        }
        total += written;
    }

    if (out) {
        out->processed = total;
    }
    return (total == len) ? MB_OK : MB_ERR_TRANSPORT;
#else
    int fd = DEMO_SERIAL_FD(port);
    if (fd < 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_size_t total = 0U;
    while (total < len) {
        ssize_t written = write(fd, buf + total, len - total);
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
        if (written == 0) {
            break;
        }
        total += (mb_size_t)written;
    }

    if (out) {
        out->processed = total;
    }
    return (total == len) ? MB_OK : MB_ERR_TRANSPORT;
#endif
}

static mb_err_t demo_serial_recv(void *ctx,
                                 mb_u8 *buf,
                                 mb_size_t cap,
                                 mb_transport_io_result_t *out)
{
    demo_serial_port_t *port = (demo_serial_port_t *)ctx;
    if (port == NULL || !port->active || buf == NULL || cap == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

#if defined(_WIN32)
    HANDLE handle = DEMO_SERIAL_HANDLE(port);
    if (handle == INVALID_HANDLE_VALUE || handle == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    DWORD received = 0U;
    if (!ReadFile(handle, buf, (DWORD)cap, &received, NULL)) {
        return MB_ERR_TRANSPORT;
    }

    if (received == 0U) {
        if (out) {
            out->processed = 0U;
        }
        return MB_ERR_TIMEOUT;
    }

    if (out) {
        out->processed = received;
    }
    return MB_OK;
#else
    int fd = DEMO_SERIAL_FD(port);
    if (fd < 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    while (true) {
        ssize_t received = read(fd, buf, cap);
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
            return MB_ERR_TIMEOUT;
        }
        if (out) {
            out->processed = (mb_size_t)received;
        }
        return MB_OK;
    }
#endif
}

static mb_time_ms_t demo_serial_now(void *ctx)
{
    (void)ctx;
#if defined(_WIN32)
    return (mb_time_ms_t)GetTickCount64();
#else
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (mb_time_ms_t)((ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL));
    }
#endif
    return 0U;
#endif
}

static void demo_serial_yield(void *ctx)
{
    (void)ctx;
#if defined(_WIN32)
    Sleep(0);
#else
    sched_yield();
#endif
}

#if !defined(_WIN32)
static speed_t demo_serial_baud_to_termios(unsigned baudrate)
{
    switch (baudrate) {
    case 9600:
        return B9600;
    case 19200:
        return B19200;
    case 38400:
        return B38400;
    case 57600:
        return B57600;
    case 115200:
        return B115200;
#ifdef B230400
    case 230400:
        return B230400;
#endif
#ifdef B460800
    case 460800:
        return B460800;
#endif
    default:
        return (speed_t)0;
    }
}
#endif

mb_err_t demo_serial_port_open(demo_serial_port_t *port,
                               const char *device,
                               unsigned baudrate)
{
    if (port == NULL || device == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    memset(port, 0, sizeof(*port));

#if defined(_WIN32)
    HANDLE handle = CreateFileA(device,
                                GENERIC_READ | GENERIC_WRITE,
                                0,
                                NULL,
                                OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);
    if (handle == INVALID_HANDLE_VALUE) {
        return MB_ERR_TRANSPORT;
    }

    if (!SetupComm(handle, 4096, 4096)) {
        CloseHandle(handle);
        return MB_ERR_TRANSPORT;
    }

    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle, &dcb)) {
        CloseHandle(handle);
        return MB_ERR_TRANSPORT;
    }

    dcb.BaudRate = baudrate;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(handle, &dcb)) {
        CloseHandle(handle);
        return MB_ERR_TRANSPORT;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 0;

    if (!SetCommTimeouts(handle, &timeouts)) {
        CloseHandle(handle);
        return MB_ERR_TRANSPORT;
    }

    PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);

    port->handle = handle;
#else
    speed_t speed = demo_serial_baud_to_termios(baudrate);
    if (speed == (speed_t)0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return MB_ERR_TRANSPORT;
    }

    struct termios tio;
    memset(&tio, 0, sizeof(tio));
    if (tcgetattr(fd, &tio) != 0) {
        close(fd);
        return MB_ERR_TRANSPORT;
    }

    cfmakeraw(&tio);
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    tio.c_cflag |= CS8;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    if (cfsetispeed(&tio, speed) != 0 || cfsetospeed(&tio, speed) != 0) {
        close(fd);
        return MB_ERR_TRANSPORT;
    }

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        close(fd);
        return MB_ERR_TRANSPORT;
    }

    tcflush(fd, TCIOFLUSH);

    port->fd = fd;
#endif

    port->active = true;
    port->iface.ctx = port;
    port->iface.send = demo_serial_send;
    port->iface.recv = demo_serial_recv;
    port->iface.now = demo_serial_now;
    port->iface.yield = demo_serial_yield;

    return MB_OK;
}

void demo_serial_port_close(demo_serial_port_t *port)
{
    if (port == NULL || !port->active) {
        return;
    }

#if defined(_WIN32)
    HANDLE handle = DEMO_SERIAL_HANDLE(port);
    if (handle != NULL && handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
    }
    port->handle = NULL;
#else
    int fd = DEMO_SERIAL_FD(port);
    if (fd >= 0) {
        close(fd);
    }
    port->fd = -1;
#endif

    port->iface.ctx = NULL;
    port->iface.send = NULL;
    port->iface.recv = NULL;
    port->iface.now = NULL;
    port->iface.yield = NULL;
    port->active = false;
}

const mb_transport_if_t *demo_serial_port_iface(demo_serial_port_t *port)
{
    if (port == NULL || !port->active) {
        return NULL;
    }
    return &port->iface;
}
