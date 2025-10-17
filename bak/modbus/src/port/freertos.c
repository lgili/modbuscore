#include <modbus/port/freertos.h>

#include <string.h>

#include <modbus/mb_err.h>

static mb_err_t freertos_validate(const mb_port_freertos_transport_t *port)
{
    if (port == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    if (port->send_fn == NULL || port->recv_fn == NULL || port->tick_fn == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    if (port->tick_rate_hz == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    if (port->tx_stream == NULL || port->rx_stream == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }
    return MB_OK;
}

static mb_err_t freertos_send(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out)
{
    if (ctx == NULL || buf == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_port_freertos_transport_t *port = (mb_port_freertos_transport_t *)ctx;
    mb_size_t total = 0U;

    while (total < len) {
        const size_t written = port->send_fn(port->tx_stream,
                                             buf + total,
                                             (size_t)(len - total),
                                             port->max_block_ticks);
        if (written == 0U) {
            if (out) {
                out->processed = total;
            }
            return (total == 0U) ? MB_ERR_TIMEOUT : MB_OK;
        }
        total += (mb_size_t)written;
    }

    if (out) {
        out->processed = total;
    }
    return MB_OK;
}

static mb_err_t freertos_recv(void *ctx, mb_u8 *buf, mb_size_t cap, mb_transport_io_result_t *out)
{
    if (ctx == NULL || buf == NULL || cap == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_port_freertos_transport_t *port = (mb_port_freertos_transport_t *)ctx;

    const size_t received = port->recv_fn(port->rx_stream, buf, (size_t)cap, port->max_block_ticks);
    if (received == 0U) {
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

static mb_time_ms_t freertos_now(void *ctx)
{
    if (ctx == NULL) {
        return 0U;
    }

    mb_port_freertos_transport_t *port = (mb_port_freertos_transport_t *)ctx;
    const uint32_t ticks = port->tick_fn();
    const uint64_t numerator = (uint64_t)ticks * 1000ULL;
    return (mb_time_ms_t)(numerator / port->tick_rate_hz);
}

static void freertos_yield(void *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mb_port_freertos_transport_t *port = (mb_port_freertos_transport_t *)ctx;
    if (port->yield_fn) {
        port->yield_fn();
    }
}

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
mb_err_t mb_port_freertos_transport_init(mb_port_freertos_transport_t *port,
                                         void *tx_stream,
                                         void *rx_stream,
                                         mb_port_freertos_stream_send_fn send_fn,
                                         mb_port_freertos_stream_recv_fn recv_fn,
                                         mb_port_freertos_tick_hook_fn tick_fn,
                                         mb_port_freertos_yield_hook_fn yield_fn,
                                         uint32_t tick_rate_hz,
                                         uint32_t max_block_ticks)
{
    if (port == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    memset(port, 0, sizeof(*port));

    port->tx_stream = tx_stream;
    port->rx_stream = rx_stream;
    port->send_fn = send_fn;
    port->recv_fn = recv_fn;
    port->tick_fn = tick_fn;
    port->yield_fn = yield_fn;
    port->tick_rate_hz = tick_rate_hz;
    port->max_block_ticks = max_block_ticks;

    mb_err_t status = freertos_validate(port);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    port->iface.ctx = port;
    port->iface.send = freertos_send;
    port->iface.recv = freertos_recv;
    port->iface.now = freertos_now;
    port->iface.yield = yield_fn ? freertos_yield : NULL;

    return MB_OK;
}
// NOLINTEND(bugprone-easily-swappable-parameters)

void mb_port_freertos_transport_set_block_ticks(mb_port_freertos_transport_t *port, uint32_t ticks)
{
    if (port == NULL) {
        return;
    }
    port->max_block_ticks = ticks;
}

void mb_port_freertos_transport_set_tick_rate(mb_port_freertos_transport_t *port, uint32_t tick_rate_hz)
{
    if (port == NULL || tick_rate_hz == 0U) {
        return;
    }
    port->tick_rate_hz = tick_rate_hz;
}

const mb_transport_if_t *mb_port_freertos_transport_iface(mb_port_freertos_transport_t *port)
{
    if (port == NULL) {
        return NULL;
    }
    return &port->iface;
}
