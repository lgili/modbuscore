#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include <modbuscore/transport/iface.h>

typedef struct {
    size_t send_calls;
    size_t recv_calls;
    uint64_t timestamp;
    size_t yield_calls;
} transport_stats_t;

static mbc_status_t mock_send(void *ctx,
                              const uint8_t *buffer,
                              size_t length,
                              mbc_transport_io_t *out)
{
    transport_stats_t *stats = (transport_stats_t *)ctx;
    stats->send_calls++;
    if (out) {
        out->processed = length;
    }

    return buffer && length ? MBC_STATUS_OK : MBC_STATUS_INVALID_ARGUMENT;
}

static mbc_status_t mock_recv(void *ctx,
                              uint8_t *buffer,
                              size_t capacity,
                              mbc_transport_io_t *out)
{
    transport_stats_t *stats = (transport_stats_t *)ctx;
    stats->recv_calls++;
    if (capacity == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }
    if (buffer && capacity > 0U && out) {
        buffer[0] = 0xAA;
        out->processed = 1U;
    }
    return MBC_STATUS_OK;
}

static uint64_t mock_now(void *ctx)
{
    transport_stats_t *stats = (transport_stats_t *)ctx;
    return ++stats->timestamp;
}

static void mock_yield(void *ctx)
{
    transport_stats_t *stats = (transport_stats_t *)ctx;
    stats->yield_calls++;
}

static void test_invalid_arguments(void)
{
    uint8_t buffer[8];
    mbc_transport_io_t io = {0};
    mbc_transport_iface_t iface = {0};

    assert(mbc_transport_send(NULL, buffer, sizeof(buffer), &io) == MBC_STATUS_INVALID_ARGUMENT);
    assert(mbc_transport_receive(NULL, buffer, sizeof(buffer), &io) == MBC_STATUS_INVALID_ARGUMENT);

    iface.send = mock_send;
    iface.receive = mock_recv;
    assert(mbc_transport_send(&iface, NULL, sizeof(buffer), &io) == MBC_STATUS_INVALID_ARGUMENT);
    assert(mbc_transport_receive(&iface, buffer, 0U, &io) == MBC_STATUS_INVALID_ARGUMENT);
}

static void test_send_receive_success(void)
{
    transport_stats_t stats = {0};
    mbc_transport_iface_t iface = {
        .ctx = &stats,
        .send = mock_send,
        .receive = mock_recv,
        .now = mock_now,
        .yield = mock_yield,
    };

    uint8_t buffer[4] = {0};
    mbc_transport_io_t io = {0};

    assert(mbc_transport_send(&iface, buffer, sizeof(buffer), &io) == MBC_STATUS_OK);
    assert(io.processed == sizeof(buffer));
    assert(stats.send_calls == 1U);

    io.processed = 0U;
    assert(mbc_transport_receive(&iface, buffer, sizeof(buffer), &io) == MBC_STATUS_OK);
    assert(io.processed == 1U);
    assert(buffer[0] == (uint8_t)0xAA);
    assert(stats.recv_calls == 1U);

    assert(mbc_transport_send(&iface, buffer, 0U, &io) == MBC_STATUS_OK);
    assert(io.processed == 0U);
}

static void test_time_and_yield(void)
{
    transport_stats_t stats = {0};
    mbc_transport_iface_t iface = {
        .ctx = &stats,
        .now = mock_now,
        .yield = mock_yield,
    };

    assert(mbc_transport_now(NULL) == 0ULL);
    assert(mbc_transport_now(&iface) == 1ULL);
    assert(mbc_transport_now(&iface) == 2ULL);

    mbc_transport_yield(NULL);
    mbc_transport_yield(&iface);
    assert(stats.yield_calls == 1U);
}

int main(void)
{
    test_invalid_arguments();
    test_send_receive_success();
    test_time_and_yield();
    return 0;
}
