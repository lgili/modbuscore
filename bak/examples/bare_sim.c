#include <stdio.h>
#include <string.h>

#include <modbus/port/bare.h>
#include <modbus/transport_if.h>

#ifndef BARE_SIM_BUFFER_CAPACITY
#define BARE_SIM_BUFFER_CAPACITY 256U
#endif

typedef struct {
    mb_u8 rx_buffer[BARE_SIM_BUFFER_CAPACITY];
    mb_size_t rx_len;
    mb_u8 tx_buffer[BARE_SIM_BUFFER_CAPACITY];
    mb_size_t tx_len;
    uint32_t ticks;
} bare_sim_link_t;

static mb_err_t bare_sim_send(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out)
{
    if (ctx == NULL || buf == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    bare_sim_link_t *link = (bare_sim_link_t *)ctx;
    const mb_size_t space = (mb_size_t)(BARE_SIM_BUFFER_CAPACITY - link->tx_len);
    const mb_size_t to_copy = (len < space) ? len : space;
    memcpy(&link->tx_buffer[link->tx_len], buf, (size_t)to_copy);
    link->tx_len += to_copy;

    if (out) {
        out->processed = to_copy;
    }

    return (to_copy == len) ? MB_OK : MB_ERR_NO_RESOURCES;
}

static mb_err_t bare_sim_recv(void *ctx, mb_u8 *buf, mb_size_t cap, mb_transport_io_result_t *out)
{
    if (ctx == NULL || buf == NULL || cap == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    bare_sim_link_t *link = (bare_sim_link_t *)ctx;
    if (link->rx_len == 0U) {
        if (out) {
            out->processed = 0U;
        }
        return MB_ERR_TIMEOUT;
    }

    const mb_size_t to_copy = (cap < link->rx_len) ? cap : link->rx_len;
    memcpy(buf, link->rx_buffer, (size_t)to_copy);
    memmove(link->rx_buffer, &link->rx_buffer[to_copy], (size_t)(link->rx_len - to_copy));
    link->rx_len -= to_copy;

    if (out) {
        out->processed = to_copy;
    }

    return MB_OK;
}

static uint32_t bare_sim_ticks(void *ctx)
{
    bare_sim_link_t *link = (bare_sim_link_t *)ctx;
    link->ticks += 5U; /* pretend timer increments every call */
    return link->ticks;
}

static void bare_sim_yield(void *ctx)
{
    (void)ctx;
}

int main(void)
{
    bare_sim_link_t link;
    memset(&link, 0, sizeof(link));

    /* Pretend our device already queued a Modbus response (e.g. echo). */
    const char *greeting = "Bare-metal hello";
    link.rx_len = (mb_size_t)strlen(greeting);
    memcpy(link.rx_buffer, greeting, (size_t)link.rx_len);

    mb_port_bare_transport_t transport;
    if (mb_port_bare_transport_init(&transport,
                                    &link,
                                    bare_sim_send,
                                    bare_sim_recv,
                                    bare_sim_ticks,
                                    1000U,
                                    bare_sim_yield,
                                    &link) != MB_OK) {
        fprintf(stderr, "Failed to initialise bare-metal transport adapter\n");
        return 1;
    }

    const mb_transport_if_t *iface = mb_port_bare_transport_iface(&transport);
    if (iface == NULL) {
        fprintf(stderr, "Transport interface unavailable\n");
        return 1;
    }

    mb_transport_io_result_t io = {0};
    mb_u8 buffer[32];
    mb_err_t err = mb_transport_recv(iface, buffer, sizeof(buffer), &io);
    if (err == MB_OK) {
        printf("RX (%zu bytes, t=%llums): %.*s\n",
               (size_t)io.processed,
               (unsigned long long)mb_transport_now(iface),
               (int)io.processed,
               buffer);
    } else {
        printf("Receive timed out (%d)\n", (int)err);
    }

    const char payload[] = "Ping from MCU";
    io.processed = 0U;
    err = mb_transport_send(iface, (const mb_u8 *)payload, (mb_size_t)strlen(payload), &io);
    if (err == MB_OK) {
        printf("TX queued (%zu bytes)\n", (size_t)io.processed);
    } else {
        printf("Send failed (%d)\n", (int)err);
    }

    printf("TX buffer snapshot: %.*s\n", (int)link.tx_len, link.tx_buffer);

    return 0;
}
