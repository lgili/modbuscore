#include "modbus_zephyr_quickstart.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

#ifndef CONFIG_MODBUS_ZEPHYR_CLIENT_POOL_SIZE
#define CONFIG_MODBUS_ZEPHYR_CLIENT_POOL_SIZE 4
#endif

#ifndef CONFIG_MODBUS_ZEPHYR_CLIENT_QUEUE_CAPACITY
#define CONFIG_MODBUS_ZEPHYR_CLIENT_QUEUE_CAPACITY 0
#endif

#ifndef CONFIG_MODBUS_ZEPHYR_SOCKET_TIMEOUT_MS
#define CONFIG_MODBUS_ZEPHYR_SOCKET_TIMEOUT_MS 20
#endif

#ifndef CONFIG_MODBUS_ZEPHYR_YIELD_MS
#define CONFIG_MODBUS_ZEPHYR_YIELD_MS 1
#endif

LOG_MODULE_REGISTER(modbus_qs, LOG_LEVEL_INF);

static mb_err_t modbus_zephyr_tcp_send(void *ctx,
                                       const mb_u8 *buf,
                                       mb_size_t len,
                                       mb_transport_io_result_t *out)
{
    struct modbus_zephyr_tcp_transport *transport = ctx;
    if (!transport || transport->sock < 0 || buf == NULL || len == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    ssize_t sent = zsock_send(transport->sock, buf, len, MSG_DONTWAIT);
    if (sent < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (out != NULL) {
                out->processed = 0U;
            }
            return MB_OK;
        }
        LOG_ERR("send failed (%d)", errno);
        transport->connected = false;
        return MB_ERR_TRANSPORT;
    }

    if (out != NULL) {
        out->processed = (mb_size_t)sent;
    }

    if (sent != (ssize_t)len) {
        LOG_WRN("Partial send (%d/%u)", (int)sent, (unsigned)len);
        transport->connected = false;
        return MB_ERR_TRANSPORT;
    }

    return MB_OK;
}

static mb_err_t modbus_zephyr_tcp_recv(void *ctx,
                                       mb_u8 *buf,
                                       mb_size_t cap,
                                       mb_transport_io_result_t *out)
{
    struct modbus_zephyr_tcp_transport *transport = ctx;
    if (!transport || transport->sock < 0 || buf == NULL || cap == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    ssize_t received = zsock_recv(transport->sock, buf, cap, MSG_DONTWAIT);
    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (out != NULL) {
                out->processed = 0U;
            }
            return MB_OK;
        }
        LOG_ERR("recv failed (%d)", errno);
        transport->connected = false;
        return MB_ERR_TRANSPORT;
    }

    if (received == 0) {
        LOG_WRN("Peer closed connection");
        transport->connected = false;
        return MB_ERR_TRANSPORT;
    }

    if (out != NULL) {
        out->processed = (mb_size_t)received;
    }

    return MB_OK;
}

static mb_time_ms_t modbus_zephyr_now(void *ctx)
{
    ARG_UNUSED(ctx);
    return (mb_time_ms_t)k_uptime_get();
}

static void modbus_zephyr_yield(void *ctx)
{
    ARG_UNUSED(ctx);
#if CONFIG_MODBUS_ZEPHYR_YIELD_MS > 0
    k_msleep(CONFIG_MODBUS_ZEPHYR_YIELD_MS);
#else
    k_yield();
#endif
}

mb_err_t modbus_zephyr_client_init(modbus_zephyr_client_t *handle)
{
    if (handle == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    memset(handle, 0, sizeof(*handle));
    handle->transport.sock = -1;
    handle->transport.recv_timeout_ms = CONFIG_MODBUS_ZEPHYR_SOCKET_TIMEOUT_MS;
    handle->transport.connected = false;

    handle->iface.ctx = &handle->transport;
    handle->iface.send = modbus_zephyr_tcp_send;
    handle->iface.recv = modbus_zephyr_tcp_recv;
    handle->iface.now = modbus_zephyr_now;
    handle->iface.yield = modbus_zephyr_yield;

    mb_err_t err = mb_client_init(&handle->client,
                                  &handle->iface,
                                  handle->pool,
                                  MB_COUNTOF(handle->pool));
    if (err != MB_OK) {
        LOG_ERR("mb_client_init failed (%d)", (int)err);
        memset(handle, 0, sizeof(*handle));
        return err;
    }

#if CONFIG_MODBUS_ZEPHYR_CLIENT_QUEUE_CAPACITY > 0
    mb_client_set_queue_capacity(&handle->client, CONFIG_MODBUS_ZEPHYR_CLIENT_QUEUE_CAPACITY);
#endif

    return MB_OK;
}

static void modbus_zephyr_transport_close(struct modbus_zephyr_tcp_transport *transport)
{
    if (transport->sock >= 0) {
        (void)zsock_close(transport->sock);
        transport->sock = -1;
    }
    transport->connected = false;
}

void modbus_zephyr_client_shutdown(modbus_zephyr_client_t *handle)
{
    if (handle == NULL) {
        return;
    }

    modbus_zephyr_transport_close(&handle->transport);
    memset(handle, 0, sizeof(*handle));
}

int modbus_zephyr_client_connect(modbus_zephyr_client_t *handle,
                                 const struct sockaddr *addr,
                                 socklen_t addrlen)
{
    if (handle == NULL || addr == NULL) {
        return -EINVAL;
    }

    int sock = zsock_socket(addr->sa_family, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        LOG_ERR("socket failed (%d)", errno);
        return -errno;
    }

    if (zsock_connect(sock, addr, addrlen) < 0) {
        int errsv = errno;
        LOG_ERR("connect failed (%d)", errsv);
        (void)zsock_close(sock);
        return -errsv;
    }

    int flags = zsock_fcntl(sock, F_GETFL, 0);
    if (flags >= 0) {
        (void)zsock_fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }

#if CONFIG_MODBUS_ZEPHYR_SOCKET_TIMEOUT_MS > 0
    struct timeval tv = {
        .tv_sec = CONFIG_MODBUS_ZEPHYR_SOCKET_TIMEOUT_MS / 1000,
        .tv_usec = (CONFIG_MODBUS_ZEPHYR_SOCKET_TIMEOUT_MS % 1000) * 1000,
    };
    (void)zsock_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    handle->transport.sock = sock;
    handle->transport.connected = true;

    LOG_INF("Connected Modbus/TCP socket (%d)", sock);
    return 0;
}

void modbus_zephyr_client_disconnect(modbus_zephyr_client_t *handle)
{
    if (handle == NULL) {
        return;
    }
    modbus_zephyr_transport_close(&handle->transport);
}

static mb_err_t modbus_zephyr_submit(modbus_zephyr_client_t *handle,
                                     uint8_t unit_id,
                                     const mb_u8 *pdu,
                                     mb_size_t len,
                                     mb_client_callback_t callback,
                                     void *user_ctx)
{
    if (!handle || !pdu || len == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (!handle->transport.connected) {
        return MB_ERR_TRANSPORT;
    }

    mb_client_request_t request = {
        .flags = 0U,
        .request = {
            .unit_id = unit_id,
            .function = pdu[0],
            .payload = &pdu[1],
            .payload_len = len - 1U,
        },
        .timeout_ms = 0U,
        .max_retries = 0U,
        .retry_backoff_ms = 0U,
        .callback = callback,
        .user_ctx = user_ctx,
    };

    return mb_client_submit(&handle->client, &request, NULL);
}

mb_err_t modbus_zephyr_submit_read_holding(modbus_zephyr_client_t *handle,
                                           uint8_t unit_id,
                                           uint16_t start_address,
                                           uint16_t quantity,
                                           mb_client_callback_t callback,
                                           void *user_ctx)
{
    mb_u8 pdu[5] = {0};
    mb_err_t err = mb_pdu_build_read_holding_request(pdu, sizeof(pdu), start_address, quantity);
    if (err != MB_OK) {
        return err;
    }
    return modbus_zephyr_submit(handle, unit_id, pdu, sizeof(pdu), callback, user_ctx);
}

mb_err_t modbus_zephyr_submit_write_single(modbus_zephyr_client_t *handle,
                                           uint8_t unit_id,
                                           uint16_t address,
                                           uint16_t value,
                                           mb_client_callback_t callback,
                                           void *user_ctx)
{
    mb_u8 pdu[5] = {0};
    mb_err_t err = mb_pdu_build_write_single_request(pdu, sizeof(pdu), address, value);
    if (err != MB_OK) {
        return err;
    }
    return modbus_zephyr_submit(handle, unit_id, pdu, sizeof(pdu), callback, user_ctx);
}
