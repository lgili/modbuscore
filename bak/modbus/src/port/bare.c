#include <modbus/port/bare.h>

#include <string.h>

#include <modbus/mb_err.h>

static mb_err_t bare_validate(const mb_port_bare_transport_t *port)
{
    if (port == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (port->send_fn == NULL || port->recv_fn == NULL || port->tick_now_fn == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (port->tick_rate_hz == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    return MB_OK;
}

static mb_err_t bare_send(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out)
{
    if (ctx == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_port_bare_transport_t *port = (mb_port_bare_transport_t *)ctx;
    return port->send_fn(port->user_ctx, buf, len, out);
}

static mb_err_t bare_recv(void *ctx, mb_u8 *buf, mb_size_t cap, mb_transport_io_result_t *out)
{
    if (ctx == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_port_bare_transport_t *port = (mb_port_bare_transport_t *)ctx;
    return port->recv_fn(port->user_ctx, buf, cap, out);
}

static mb_time_ms_t bare_now(void *ctx)
{
    if (ctx == NULL) {
        return 0U;
    }

    mb_port_bare_transport_t *port = (mb_port_bare_transport_t *)ctx;
    const uint32_t ticks = port->tick_now_fn(port->clock_ctx);
    const uint64_t numerator = (uint64_t)ticks * 1000ULL;
    return (mb_time_ms_t)(numerator / port->tick_rate_hz);
}

static void bare_yield(void *ctx)
{
    if (ctx == NULL) {
        return;
    }

    mb_port_bare_transport_t *port = (mb_port_bare_transport_t *)ctx;
    if (port->yield_fn) {
        port->yield_fn(port->user_ctx);
    }
}

mb_err_t mb_port_bare_transport_init(mb_port_bare_transport_t *port,
                                     void *user_ctx,
                                     mb_port_bare_send_fn send_fn,
                                     mb_port_bare_recv_fn recv_fn,
                                     mb_port_bare_tick_now_fn tick_now_fn,
                                     uint32_t tick_rate_hz,
                                     mb_port_bare_yield_fn yield_fn,
                                     void *clock_ctx)
{
    if (port == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    memset(port, 0, sizeof(*port));

    port->user_ctx = user_ctx;
    port->send_fn = send_fn;
    port->recv_fn = recv_fn;
    port->tick_now_fn = tick_now_fn;
    port->yield_fn = yield_fn;
    port->clock_ctx = clock_ctx ? clock_ctx : user_ctx;
    port->tick_rate_hz = tick_rate_hz;

    mb_err_t status = bare_validate(port);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    port->iface.ctx = port;
    port->iface.send = bare_send;
    port->iface.recv = bare_recv;
    port->iface.now = bare_now;
    port->iface.yield = bare_yield;

    return MB_OK;
}

void mb_port_bare_transport_update_tick_rate(mb_port_bare_transport_t *port, uint32_t tick_rate_hz)
{
    if (port == NULL || tick_rate_hz == 0U) {
        return;
    }
    port->tick_rate_hz = tick_rate_hz;
}

void mb_port_bare_transport_set_clock_ctx(mb_port_bare_transport_t *port, void *clock_ctx)
{
    if (port == NULL) {
        return;
    }
    port->clock_ctx = clock_ctx;
}

const mb_transport_if_t *mb_port_bare_transport_iface(mb_port_bare_transport_t *port)
{
    if (port == NULL) {
        return NULL;
    }
    return &port->iface;
}
