#include <modbuscore/transport/posix_rtu.h>
#include <modbuscore/transport/rtu_uart.h>

#ifdef __unix__

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

typedef struct mbc_posix_rtu_ctx {
    int fd;
    mbc_rtu_uart_ctx_t *rtu;
} mbc_posix_rtu_ctx_internal_t;

static speed_t map_baud(uint32_t baud)
{
#define MAP(value, constant) case value: return constant
    switch (baud) {
        MAP(9600, B9600);
        MAP(19200, B19200);
        MAP(38400, B38400);
        MAP(57600, B57600);
        MAP(115200, B115200);
        MAP(230400, B230400);
        MAP(4800, B4800);
        MAP(2400, B2400);
        MAP(1200, B1200);
        MAP(600, B600);
        MAP(300, B300);
        MAP(110, B110);
    default:
        return 0;
    }
#undef MAP
}

static int configure_termios(int fd, const mbc_posix_rtu_config_t *config)
{
    struct termios tio;
    if (tcgetattr(fd, &tio) != 0) {
        return -1;
    }

    cfmakeraw(&tio);
    tio.c_cflag |= (CLOCAL | CREAD);

    tio.c_cflag &= ~CSIZE;
    switch (config->data_bits ? config->data_bits : 8) {
    case 5: tio.c_cflag |= CS5; break;
    case 6: tio.c_cflag |= CS6; break;
    case 7: tio.c_cflag |= CS7; break;
    default: tio.c_cflag |= CS8; break;
    }

    tio.c_cflag &= ~(PARENB | PARODD);
    switch (config->parity ? config->parity : 'N') {
    case 'E':
    case 'e':
        tio.c_cflag |= PARENB;
        break;
    case 'O':
    case 'o':
        tio.c_cflag |= PARENB | PARODD;
        break;
    default:
        break;
    }

    if ((config->stop_bits ? config->stop_bits : 1) == 2) {
        tio.c_cflag |= CSTOPB;
    } else {
        tio.c_cflag &= ~CSTOPB;
    }

    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    speed_t speed = map_baud(config->baud_rate ? config->baud_rate : 9600);
    if (speed == 0) {
        errno = EINVAL;
        return -1;
    }

    if (cfsetispeed(&tio, speed) != 0 || cfsetospeed(&tio, speed) != 0) {
        return -1;
    }

    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        return -1;
    }

    return 0;
}

static uint64_t clock_now_us(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0ULL;
    }
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)(ts.tv_nsec / 1000ULL);
}

static size_t posix_backend_write(void *ctx, const uint8_t *data, size_t length)
{
    mbc_posix_rtu_ctx_internal_t *posix = ctx;
    ssize_t rc = write(posix->fd, data, length);
    if (rc < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0U;
        }
        return 0U;
    }
    return (size_t)rc;
}

static size_t posix_backend_read(void *ctx, uint8_t *data, size_t capacity)
{
    mbc_posix_rtu_ctx_internal_t *posix = ctx;
    ssize_t rc = read(posix->fd, data, capacity);
    if (rc < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0U;
        }
        return 0U;
    }
    return (size_t)rc;
}

static void posix_backend_flush(void *ctx)
{
    mbc_posix_rtu_ctx_internal_t *posix = ctx;
    tcdrain(posix->fd);
}

static uint64_t posix_backend_now(void *ctx)
{
    (void)ctx;
    return clock_now_us();
}

static void posix_backend_delay(void *ctx, uint32_t micros)
{
    (void)ctx;
    struct timespec ts = {
        .tv_sec = micros / 1000000U,
        .tv_nsec = (long)(micros % 1000000U) * 1000L,
    };
    nanosleep(&ts, NULL);
}

mbc_status_t mbc_posix_rtu_create(const mbc_posix_rtu_config_t *config,
                                  mbc_transport_iface_t *out_iface,
                                  mbc_posix_rtu_ctx_t **out_ctx)
{
    if (!config || !config->device_path || !out_iface || !out_ctx) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    int fd = open(config->device_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return MBC_STATUS_IO_ERROR;
    }

    if (configure_termios(fd, config) != 0) {
        close(fd);
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    mbc_posix_rtu_ctx_internal_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        close(fd);
        return MBC_STATUS_NO_RESOURCES;
    }
    ctx->fd = fd;

    mbc_rtu_uart_config_t uart_cfg = {
        .backend = {
            .ctx = ctx,
            .write = posix_backend_write,
            .read = posix_backend_read,
            .flush = posix_backend_flush,
            .now_us = posix_backend_now,
            .delay_us = posix_backend_delay,
        },
        .baud_rate = config->baud_rate,
        .data_bits = config->data_bits,
        .parity_bits = (config->parity == 'E' || config->parity == 'O') ? 1U : 0U,
        .stop_bits = config->stop_bits,
        .guard_time_us = config->guard_time_us,
        .rx_buffer_capacity = config->rx_buffer_capacity,
    };

    mbc_transport_iface_t rtu_iface;
    mbc_status_t status = mbc_rtu_uart_create(&uart_cfg, &rtu_iface, &ctx->rtu);
    if (!mbc_status_is_ok(status)) {
        close(fd);
        free(ctx);
        return status;
    }

    *out_iface = rtu_iface;
    *out_ctx = ctx;
    return MBC_STATUS_OK;
}

void mbc_posix_rtu_destroy(mbc_posix_rtu_ctx_t *ctx)
{
    mbc_posix_rtu_ctx_internal_t *posix = (mbc_posix_rtu_ctx_internal_t *)ctx;
    if (!posix) {
        return;
    }
    mbc_rtu_uart_destroy(posix->rtu);
    if (posix->fd >= 0) {
        close(posix->fd);
    }
    free(posix);
}

void mbc_posix_rtu_reset(mbc_posix_rtu_ctx_t *ctx)
{
    mbc_posix_rtu_ctx_internal_t *posix = (mbc_posix_rtu_ctx_internal_t *)ctx;
    if (!posix) {
        return;
    }
    mbc_rtu_uart_reset(posix->rtu);
}

#else

mbc_status_t mbc_posix_rtu_create(const mbc_posix_rtu_config_t *config,
                                  mbc_transport_iface_t *out_iface,
                                  mbc_posix_rtu_ctx_t **out_ctx)
{
    (void)config;
    (void)out_iface;
    (void)out_ctx;
    return MBC_STATUS_UNSUPPORTED;
}

void mbc_posix_rtu_destroy(mbc_posix_rtu_ctx_t *ctx)
{
    (void)ctx;
}

void mbc_posix_rtu_reset(mbc_posix_rtu_ctx_t *ctx)
{
    (void)ctx;
}

#endif

