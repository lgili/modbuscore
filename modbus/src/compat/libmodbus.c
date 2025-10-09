#include <modbus/compat/libmodbus.h>
#include <modbus/compat/modbus-errno.h>

#include <modbus/mb_host.h>
#include <modbus/mb_err.h>

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ENOTCONN
#define ENOTCONN EMBEILLSTATE
#endif

#ifndef ENOTSUP
#define ENOTSUP EMBEILLSTATE
#endif

#define COMPAT_DEFAULT_TIMEOUT_MS 1000U

typedef enum {
    MODBUS_COMPAT_BACKEND_NONE = 0,
    MODBUS_COMPAT_BACKEND_RTU,
    MODBUS_COMPAT_BACKEND_TCP
} modbus_compat_backend_t;

struct modbus_compat_context {
    modbus_compat_backend_t backend;
    char *device;
    int baud;
    char parity;
    int data_bits;
    int stop_bits;
    char *host;
    int port;
    int slave;
    bool debug_enabled;
    bool connected;
    mb_host_client_t *client;
    uint32_t timeout_ms;
};

int modbus_errno = 0;

static uint32_t clamp_timeout_ms(uint64_t total_us)
{
    const uint64_t ms = (total_us + 999ULL) / 1000ULL;
    if (ms > UINT32_MAX) {
        return UINT32_MAX;
    }
    return (uint32_t)ms;
}

static int map_exception_to_errno(mb_err_t err)
{
    switch ((int)err) {
    case MB_EX_ILLEGAL_FUNCTION:
        return EMBXILFUN;
    case MB_EX_ILLEGAL_DATA_ADDRESS:
        return EMBXILADR;
    case MB_EX_ILLEGAL_DATA_VALUE:
        return EMBXILVAL;
    case MB_EX_SERVER_DEVICE_FAILURE:
        return EMBXSFAIL;
    case MB_EX_ACKNOWLEDGE:
        return EMBXACK;
    case MB_EX_SERVER_DEVICE_BUSY:
        return EMBXSBUSY;
    case MB_EX_NEGATIVE_ACKNOWLEDGE:
        return EMBXNACK;
    case MB_EX_MEMORY_PARITY_ERROR:
        return EMBXMEMPAR;
    case MB_EX_GATEWAY_PATH_UNAVAILABLE:
        return EMBXGPATH;
    case MB_EX_GATEWAY_TARGET_FAILED:
        return EMBXGTAR;
    default:
        return EMBEDATA;
    }
}

static int map_error_to_errno(mb_err_t err)
{
    switch ((int)err) {
    case MB_OK:
        return 0;
    case MB_ERR_INVALID_ARGUMENT:
        return EINVAL;
    case MB_ERR_TIMEOUT:
        return EMBETIMEDOUT;
    case MB_ERR_TRANSPORT:
        return EMBECONNRESET;
    case MB_ERR_CRC:
        return EMBBADCRC;
    case MB_ERR_INVALID_REQUEST:
        return EMBBADDATA;
    case MB_ERR_OTHER_REQUESTS:
    case MB_ERR_OTHER:
        return EMBEDATA;
    case MB_ERR_CANCELLED:
#ifdef ECANCELED
        return ECANCELED;
#else
        return EMBEILLSTATE;
#endif
    case MB_ERR_NO_RESOURCES:
        return EMBEBUSY;
    case MB_ERR_BUSY:
        return EMBEBUSY;
    default:
        return EMBEILLSTATE;
    }
}

static int propagate_error(modbus_t *ctx, mb_err_t err)
{
    int mapped;

    if (mb_err_is_exception(err)) {
        mapped = map_exception_to_errno(err);
    } else {
        mapped = map_error_to_errno(err);
    }

    modbus_errno = mapped;
    errno = mapped;
    (void)ctx;
    return -1;
}

static bool ensure_connected(modbus_t *ctx)
{
    if (ctx == NULL || !ctx->connected || ctx->client == NULL) {
        modbus_errno = ENOTCONN;
        errno = ENOTCONN;
        return false;
    }
    return true;
}

static modbus_t *alloc_context(void)
{
    modbus_t *ctx = (modbus_t *)calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        modbus_errno = ENOMEM;
        errno = ENOMEM;
        return NULL;
    }

    ctx->backend = MODBUS_COMPAT_BACKEND_NONE;
    ctx->slave = 1;
    ctx->timeout_ms = COMPAT_DEFAULT_TIMEOUT_MS;
    ctx->debug_enabled = false;
    ctx->connected = false;
    ctx->client = NULL;
    return ctx;
}

modbus_t *modbus_new_rtu(const char *device,
                         int baud,
                         char parity,
                         int data_bit,
                         int stop_bit)
{
    if (device == NULL || baud <= 0) {
        modbus_errno = EINVAL;
        errno = EINVAL;
        return NULL;
    }

    modbus_t *ctx = alloc_context();
    if (ctx == NULL) {
        return NULL;
    }

    ctx->backend = MODBUS_COMPAT_BACKEND_RTU;
    ctx->baud = baud;
    ctx->parity = parity;
    ctx->data_bits = data_bit;
    ctx->stop_bits = stop_bit;
    ctx->device = strdup(device);
    if (ctx->device == NULL) {
        modbus_free(ctx);
        modbus_errno = ENOMEM;
        errno = ENOMEM;
        return NULL;
    }
    modbus_errno = 0;
    errno = 0;
    return ctx;
}

modbus_t *modbus_new_tcp(const char *ip, int port)
{
    if (ip == NULL || port <= 0) {
        modbus_errno = EINVAL;
        errno = EINVAL;
        return NULL;
    }

    modbus_t *ctx = alloc_context();
    if (ctx == NULL) {
        return NULL;
    }

    ctx->backend = MODBUS_COMPAT_BACKEND_TCP;
    ctx->host = strdup(ip);
    if (ctx->host == NULL) {
        modbus_free(ctx);
        modbus_errno = ENOMEM;
        errno = ENOMEM;
        return NULL;
    }
    ctx->port = port;
    modbus_errno = 0;
    errno = 0;
    return ctx;
}

void modbus_free(modbus_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    modbus_close(ctx);
    free(ctx->device);
    free(ctx->host);
    free(ctx);
}

static int connect_rtu(modbus_t *ctx)
{
    if (ctx->parity != 'N' && ctx->parity != 'n') {
        modbus_errno = ENOTSUP;
        errno = ENOTSUP;
        return -1;
    }
    if (ctx->data_bits != 8 || ctx->stop_bits != 1) {
        modbus_errno = ENOTSUP;
        errno = ENOTSUP;
        return -1;
    }

    mb_host_client_t *client = mb_host_rtu_connect(ctx->device, (uint32_t)ctx->baud);
    if (client == NULL) {
        modbus_errno = errno != 0 ? errno : EMBECONNRESET;
        errno = modbus_errno;
        return -1;
    }

    ctx->client = client;
    ctx->connected = true;
    mb_host_set_timeout(ctx->client, ctx->timeout_ms);
    mb_host_enable_logging(ctx->client, ctx->debug_enabled);
    return 0;
}

static int connect_tcp(modbus_t *ctx)
{
    char host_port[256];
    if (ctx->host == NULL) {
        modbus_errno = EINVAL;
        errno = EINVAL;
        return -1;
    }

    if ((size_t)snprintf(host_port, sizeof(host_port), "%s:%d", ctx->host, ctx->port) >= sizeof(host_port)) {
        modbus_errno = EINVAL;
        errno = EINVAL;
        return -1;
    }

    mb_host_client_t *client = mb_host_tcp_connect(host_port);
    if (client == NULL) {
        modbus_errno = errno != 0 ? errno : EMBECONNRESET;
        errno = modbus_errno;
        return -1;
    }

    ctx->client = client;
    ctx->connected = true;
    mb_host_set_timeout(ctx->client, ctx->timeout_ms);
    mb_host_enable_logging(ctx->client, ctx->debug_enabled);
    return 0;
}

int modbus_connect(modbus_t *ctx)
{
    if (ctx == NULL) {
        modbus_errno = EINVAL;
        errno = EINVAL;
        return -1;
    }

    if (ctx->connected) {
        return 0;
    }

    int rc = -1;
    switch (ctx->backend) {
    case MODBUS_COMPAT_BACKEND_RTU:
        rc = connect_rtu(ctx);
        break;
    case MODBUS_COMPAT_BACKEND_TCP:
        rc = connect_tcp(ctx);
        break;
    case MODBUS_COMPAT_BACKEND_NONE:
    default:
        modbus_errno = ENOTSUP;
        errno = ENOTSUP;
        rc = -1;
        break;
    }

    if (rc == 0) {
        modbus_errno = 0;
        errno = 0;
    }
    return rc;
}

void modbus_close(modbus_t *ctx)
{
    if (ctx == NULL || ctx->client == NULL) {
        return;
    }

    mb_host_disconnect(ctx->client);
    ctx->client = NULL;
    ctx->connected = false;
    modbus_errno = 0;
    errno = 0;
}

int modbus_set_slave(modbus_t *ctx, int slave)
{
    if (ctx == NULL || slave < 0 || slave > 247) {
        modbus_errno = EINVAL;
        errno = EINVAL;
        return -1;
    }

    ctx->slave = slave;
    modbus_errno = 0;
    errno = 0;
    return 0;
}

int modbus_get_slave(modbus_t *ctx)
{
    if (ctx == NULL) {
        modbus_errno = EINVAL;
        errno = EINVAL;
        return -1;
    }
    modbus_errno = 0;
    errno = 0;
    return ctx->slave;
}

int modbus_set_response_timeout(modbus_t *ctx, uint32_t seconds, uint32_t microseconds)
{
    if (ctx == NULL) {
        modbus_errno = EINVAL;
        errno = EINVAL;
        return -1;
    }

    const uint64_t total_us = ((uint64_t)seconds * 1000000ULL) + (uint64_t)microseconds;
    ctx->timeout_ms = clamp_timeout_ms(total_us);
    if (ctx->connected && ctx->client != NULL) {
        mb_host_set_timeout(ctx->client, ctx->timeout_ms);
    }
    modbus_errno = 0;
    errno = 0;
    return 0;
}

int modbus_get_response_timeout(modbus_t *ctx, uint32_t *seconds, uint32_t *microseconds)
{
    if (ctx == NULL) {
        modbus_errno = EINVAL;
        errno = EINVAL;
        return -1;
    }

    if (seconds != NULL) {
        *seconds = ctx->timeout_ms / 1000U;
    }
    if (microseconds != NULL) {
        *microseconds = (ctx->timeout_ms % 1000U) * 1000U;
    }
    modbus_errno = 0;
    errno = 0;
    return 0;
}

int modbus_set_debug(modbus_t *ctx, int flag)
{
    if (ctx == NULL) {
        modbus_errno = EINVAL;
        errno = EINVAL;
        return -1;
    }

    ctx->debug_enabled = (flag != 0);
    if (ctx->connected && ctx->client != NULL) {
        mb_host_enable_logging(ctx->client, ctx->debug_enabled);
    }
    modbus_errno = 0;
    errno = 0;
    return 0;
}

int modbus_read_registers(modbus_t *ctx, int address, int nb, uint16_t *dest)
{
    if (!ensure_connected(ctx)) {
        return -1;
    }
    if (dest == NULL || nb <= 0) {
        modbus_errno = EINVAL;
        errno = EINVAL;
        return -1;
    }

    const mb_err_t err = mb_host_read_holding(ctx->client,
                                              (uint8_t)ctx->slave,
                                              (uint16_t)address,
                                              (uint16_t)nb,
                                              dest);
    if (mb_err_is_ok(err)) {
        modbus_errno = 0;
        errno = 0;
        return nb;
    }
    return propagate_error(ctx, err);
}

int modbus_write_register(modbus_t *ctx, int address, int value)
{
    if (!ensure_connected(ctx)) {
        return -1;
    }

    const mb_err_t err = mb_host_write_single_register(ctx->client,
                                                       (uint8_t)ctx->slave,
                                                       (uint16_t)address,
                                                       (uint16_t)value);
    if (mb_err_is_ok(err)) {
        modbus_errno = 0;
        errno = 0;
        return 1;
    }
    return propagate_error(ctx, err);
}

int modbus_write_registers(modbus_t *ctx, int address, int nb, const uint16_t *data)
{
    if (!ensure_connected(ctx)) {
        return -1;
    }
    if (data == NULL || nb <= 0) {
        modbus_errno = EINVAL;
        errno = EINVAL;
        return -1;
    }

    const mb_err_t err = mb_host_write_multiple_registers(ctx->client,
                                                          (uint8_t)ctx->slave,
                                                          (uint16_t)address,
                                                          (uint16_t)nb,
                                                          data);
    if (mb_err_is_ok(err)) {
        modbus_errno = 0;
        errno = 0;
        return nb;
    }
    return propagate_error(ctx, err);
}

int modbus_flush(modbus_t *ctx)
{
    if (ctx == NULL) {
        modbus_errno = EINVAL;
        errno = EINVAL;
        return -1;
    }
    /* All interactions are synchronous; nothing to flush. */
    modbus_errno = 0;
    errno = 0;
    return 0;
}

const char *modbus_strerror(int errnum)
{
    switch (errnum) {
    case EMBXILFUN:
        return "Illegal function";
    case EMBXILADR:
        return "Illegal data address";
    case EMBXILVAL:
        return "Illegal data value";
    case EMBXSFAIL:
        return "Server device failure";
    case EMBXACK:
        return "Acknowledge";
    case EMBXSBUSY:
        return "Server busy";
    case EMBXNACK:
        return "Negative acknowledge";
    case EMBXMEMPAR:
        return "Memory parity error";
    case EMBXGPATH:
        return "Gateway path unavailable";
    case EMBXGTAR:
        return "Gateway target device failed to respond";
    case EMBBADCRC:
        return "Bad CRC received";
    case EMBBADDATA:
        return "Invalid Modbus frame";
    case EMBADSLAVE:
        return "Unexpected slave ID in response";
    case EMBMDATA:
        return "Malformed data";
    case EMBEDATA:
        return "Protocol data error";
    case EMBEBUSY:
        return "Resource busy";
    case EMBETIMEDOUT:
        return "Response timeout";
    case EMBECONNRESET:
        return "Connection reset by peer";
    case EMBEILLSTATE:
        return "Illegal state for request";
    default:
        return strerror(errnum);
    }
}
