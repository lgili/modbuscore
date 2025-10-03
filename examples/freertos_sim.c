#include <stdio.h>
#include <string.h>

#include <modbus/port/freertos.h>
#include <modbus/transport_if.h>

#ifndef FREERTOS_SIM_BUFFER_CAPACITY
#define FREERTOS_SIM_BUFFER_CAPACITY 256U
#endif

typedef struct {
    mb_u8 data[FREERTOS_SIM_BUFFER_CAPACITY];
    size_t count;
} freertos_sim_stream_t;

static size_t sim_stream_send(void *stream, const mb_u8 *payload, size_t length, uint32_t ticks_to_wait)
{
    (void)ticks_to_wait;
    freertos_sim_stream_t *s = (freertos_sim_stream_t *)stream;
    const size_t space = FREERTOS_SIM_BUFFER_CAPACITY - s->count;
    const size_t to_copy = (length < space) ? length : space;
    memcpy(&s->data[s->count], payload, to_copy);
    s->count += to_copy;
    return to_copy;
}

static size_t sim_stream_recv(void *stream, mb_u8 *buffer, size_t capacity, uint32_t ticks_to_wait)
{
    (void)ticks_to_wait;
    freertos_sim_stream_t *s = (freertos_sim_stream_t *)stream;
    if (s->count == 0U) {
        return 0U;
    }

    const size_t to_copy = (capacity < s->count) ? capacity : s->count;
    memcpy(buffer, s->data, to_copy);
    memmove(s->data, &s->data[to_copy], s->count - to_copy);
    s->count -= to_copy;
    return to_copy;
}

static uint32_t sim_tick(void)
{
    static uint32_t tick = 0U;
    tick += 10U; // advance by 10 ticks per call
    return tick;
}

static void sim_yield(void)
{
    /* no-op in the simulation */
}

int main(void)
{
    freertos_sim_stream_t tx_stream = {0};
    freertos_sim_stream_t rx_stream = {0};

    /* Pretend that a server task has placed a Modbus frame in the RX stream. */
    const mb_u8 greeting[] = {0xDE, 0xAD, 0xBE, 0xEF};
    sim_stream_send(&rx_stream, greeting, sizeof(greeting), 0U);

    mb_port_freertos_transport_t transport;
    if (mb_port_freertos_transport_init(&transport,
                                        &tx_stream,
                                        &rx_stream,
                                        sim_stream_send,
                                        sim_stream_recv,
                                        sim_tick,
                                        sim_yield,
                                        1000U,
                                        5U) != MB_OK) {
        fprintf(stderr, "Failed to initialise FreeRTOS transport simulation\n");
        return 1;
    }

    const mb_transport_if_t *iface = mb_port_freertos_transport_iface(&transport);

    mb_transport_io_result_t io = {0};
    mb_u8 buffer[8];
    mb_err_t err = mb_transport_recv(iface, buffer, sizeof(buffer), &io);
    if (err == MB_OK) {
        printf("Received %zu bytes at t=%llumS\n", (size_t)io.processed, (unsigned long long)mb_transport_now(iface));
    } else {
        printf("No data available (%d)\n", (int)err);
    }

    const mb_u8 reply[] = {0xAA, 0x55};
    io.processed = 0U;
    err = mb_transport_send(iface, reply, sizeof(reply), &io);
    if (err == MB_OK) {
        printf("Sent %zu bytes\n", (size_t)io.processed);
    }

    printf("TX buffer contains %zu bytes\n", tx_stream.count);
    return 0;
}
