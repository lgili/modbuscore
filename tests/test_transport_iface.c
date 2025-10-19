#include <assert.h>
#include <modbuscore/transport/iface.h>
#include <modbuscore/transport/mock.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_invalid_arguments(void)
{
    printf("[test_invalid_arguments] START\n");
    uint8_t buffer[8];
    mbc_transport_io_t io = {0};
    mbc_transport_iface_t iface = {0};

    printf("[test_invalid_arguments] Testing NULL iface...\n");
    assert(mbc_transport_send(NULL, buffer, sizeof(buffer), &io) == MBC_STATUS_INVALID_ARGUMENT);
    assert(mbc_transport_receive(NULL, buffer, sizeof(buffer), &io) == MBC_STATUS_INVALID_ARGUMENT);

    printf("[test_invalid_arguments] Creating mock...\n");
    mbc_mock_transport_t* mock = NULL;
    assert(mbc_mock_transport_create(NULL, &iface, &mock) == MBC_STATUS_OK);
    printf("[test_invalid_arguments] Mock created: %p\n", (void*)mock);

    printf("[test_invalid_arguments] Testing invalid buffer...\n");
    assert(mbc_transport_send(&iface, NULL, sizeof(buffer), &io) == MBC_STATUS_INVALID_ARGUMENT);
    assert(mbc_transport_receive(&iface, buffer, 0U, &io) == MBC_STATUS_INVALID_ARGUMENT);

    printf("[test_invalid_arguments] Destroying mock...\n");
    mbc_mock_transport_destroy(mock);
    printf("[test_invalid_arguments] DONE\n");
}

static void test_send_receive_success(void)
{
    mbc_transport_iface_t iface;
    mbc_mock_transport_t* mock = NULL;
    assert(mbc_mock_transport_create(NULL, &iface, &mock) == MBC_STATUS_OK);

    uint8_t buffer[4] = {0};
    mbc_transport_io_t io = {0};

    assert(mbc_transport_send(&iface, buffer, sizeof(buffer), &io) == MBC_STATUS_OK);
    assert(io.processed == sizeof(buffer));

    uint8_t tx_copy[4] = {0};
    size_t tx_len = 0U;
    assert(mbc_mock_transport_fetch_tx(mock, tx_copy, sizeof(tx_copy), &tx_len) == MBC_STATUS_OK);
    assert(tx_len == sizeof(buffer));
    assert(memcmp(tx_copy, buffer, sizeof(buffer)) == 0);

    const uint8_t rx_payload[] = {0xAA};
    assert(mbc_mock_transport_schedule_rx(mock, rx_payload, sizeof(rx_payload), 0U) ==
           MBC_STATUS_OK);
    io.processed = 0U;
    buffer[0] = 0U;
    assert(mbc_transport_receive(&iface, buffer, sizeof(buffer), &io) == MBC_STATUS_OK);
    assert(io.processed == sizeof(rx_payload));
    assert(buffer[0] == rx_payload[0]);

    assert(mbc_transport_send(&iface, buffer, 0U, &io) == MBC_STATUS_OK);
    assert(io.processed == 0U);

    mbc_mock_transport_destroy(mock);
}

static void test_time_and_yield(void)
{
    mbc_transport_iface_t iface;
    mbc_mock_transport_t* mock = NULL;

    mbc_mock_transport_config_t config = {
        .initial_now_ms = 10U,
        .yield_advance_ms = 5U,
    };

    assert(mbc_mock_transport_create(&config, &iface, &mock) == MBC_STATUS_OK);

    assert(mbc_transport_now(NULL) == 0ULL);
    assert(mbc_transport_now(&iface) == 10ULL);
    mbc_transport_yield(&iface);
    assert(mbc_transport_now(&iface) == 15ULL);

    mbc_mock_transport_destroy(mock);
    mbc_transport_yield(NULL);
}

int main(void)
{
    printf("=== Transport Interface Tests Starting ===\n");
    fflush(stdout);
    test_invalid_arguments();
    test_send_receive_success();
    test_time_and_yield();
    printf("=== All Transport Interface Tests Passed ===\n");
    return 0;
}
