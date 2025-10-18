#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <modbuscore/transport/mock.h>

static void test_rx_flow(void)
{
    mbc_mock_transport_config_t config = {
        .initial_now_ms = 0U,
        .recv_latency_ms = 10U,
    };

    mbc_transport_iface_t iface;
    mbc_mock_transport_t *mock = NULL;
    assert(mbc_mock_transport_create(&config, &iface, &mock) == MBC_STATUS_OK);

    const uint8_t payload[] = {0x11, 0x22, 0x33};
    assert(mbc_mock_transport_schedule_rx(mock, payload, sizeof(payload), 5U) == MBC_STATUS_OK);
    assert(mbc_mock_transport_pending_rx(mock) == 1U);

    uint8_t buffer[2];
    mbc_transport_io_t io = {0};
    assert(mbc_transport_receive(&iface, buffer, sizeof(buffer), &io) == MBC_STATUS_OK);
    assert(io.processed == 0U);

    mbc_mock_transport_advance(mock, 14U);
    assert(mbc_transport_receive(&iface, buffer, sizeof(buffer), &io) == MBC_STATUS_OK);
    assert(io.processed == 0U);

    mbc_mock_transport_advance(mock, 1U);  /* total = 15ms */
    assert(mbc_transport_receive(&iface, buffer, sizeof(buffer), &io) == MBC_STATUS_OK);
    assert(io.processed == 2U);
    assert(buffer[0] == payload[0] && buffer[1] == payload[1]);

    assert(mbc_transport_receive(&iface, buffer, sizeof(buffer), &io) == MBC_STATUS_OK);
    assert(io.processed == 1U);
    assert(buffer[0] == payload[2]);

    assert(mbc_transport_receive(&iface, buffer, sizeof(buffer), &io) == MBC_STATUS_OK);
    assert(io.processed == 0U);
    assert(mbc_mock_transport_pending_rx(mock) == 0U);

    mbc_mock_transport_destroy(mock);
}

static void test_tx_flow(void)
{
    mbc_mock_transport_config_t config = {
        .initial_now_ms = 100U,
        .send_latency_ms = 7U,
    };

    mbc_transport_iface_t iface;
    mbc_mock_transport_t *mock = NULL;
    assert(mbc_mock_transport_create(&config, &iface, &mock) == MBC_STATUS_OK);

    const uint8_t frame[] = {0xDE, 0xAD, 0xBE, 0xEF};
    mbc_transport_io_t io = {0};
    assert(mbc_transport_send(&iface, frame, sizeof(frame), &io) == MBC_STATUS_OK);
    assert(io.processed == sizeof(frame));
    assert(mbc_mock_transport_pending_tx(mock) == 1U);

    uint8_t out[4];
    size_t out_len = 0U;
    assert(mbc_mock_transport_fetch_tx(mock, out, sizeof(out), &out_len) == MBC_STATUS_OK);
    assert(out_len == 0U);  /* Ainda aguardando latência */

    mbc_mock_transport_advance(mock, 7U);
    assert(mbc_mock_transport_fetch_tx(mock, out, sizeof(out), &out_len) == MBC_STATUS_OK);
    assert(out_len == sizeof(frame));
    assert(memcmp(out, frame, sizeof(frame)) == 0);
    assert(mbc_mock_transport_pending_tx(mock) == 0U);

    mbc_mock_transport_destroy(mock);
}

static void test_reset_and_yield(void)
{
    mbc_mock_transport_config_t config = {
        .initial_now_ms = 42U,
        .yield_advance_ms = 3U,
    };

    mbc_transport_iface_t iface;
    mbc_mock_transport_t *mock = NULL;
    assert(mbc_mock_transport_create(&config, &iface, &mock) == MBC_STATUS_OK);
    assert(mbc_transport_now(&iface) == 42U);

    mbc_transport_yield(&iface);
    assert(mbc_transport_now(&iface) == 45U);

    const uint8_t data[] = {0xAA};
    assert(mbc_mock_transport_schedule_rx(mock, data, sizeof(data), 0U) == MBC_STATUS_OK);
    assert(mbc_mock_transport_pending_rx(mock) == 1U);

    mbc_mock_transport_reset(mock);
    assert(mbc_transport_now(&iface) == 42U);
    assert(mbc_mock_transport_pending_rx(mock) == 0U);
    assert(mbc_mock_transport_pending_tx(mock) == 0U);

    mbc_mock_transport_destroy(mock);
}

static void test_fetch_capacity_guard(void)
{
    mbc_transport_iface_t iface;
    mbc_mock_transport_t *mock = NULL;
    assert(mbc_mock_transport_create(NULL, &iface, &mock) == MBC_STATUS_OK);

    const uint8_t payload[] = {0x01, 0x02, 0x03};
    mbc_transport_io_t io = {0};
    assert(mbc_transport_send(&iface, payload, sizeof(payload), &io) == MBC_STATUS_OK);

    uint8_t buffer[2];
    size_t out_len = 123U;
    assert(mbc_mock_transport_fetch_tx(mock, buffer, sizeof(buffer), &out_len) == MBC_STATUS_NO_RESOURCES);
    assert(out_len == 0U);

    uint8_t bigger[3];
    assert(mbc_mock_transport_fetch_tx(mock, bigger, sizeof(bigger), &out_len) == MBC_STATUS_OK);
    assert(out_len == sizeof(payload));
    assert(memcmp(bigger, payload, sizeof(payload)) == 0);

    mbc_mock_transport_destroy(mock);
}

static void test_error_controls(void)
{
    mbc_transport_iface_t iface;
    mbc_mock_transport_t *mock = NULL;
    assert(mbc_mock_transport_create(NULL, &iface, &mock) == MBC_STATUS_OK);

    uint8_t tx_frame[] = {0x10};
    mbc_transport_io_t io = {0};
    uint8_t buffer[4] = {0};

    mbc_mock_transport_set_connected(mock, false);
    assert(mbc_transport_send(&iface, tx_frame, sizeof(tx_frame), &io) == MBC_STATUS_IO_ERROR);
    assert(mbc_transport_receive(&iface, buffer, sizeof(buffer), &io) == MBC_STATUS_IO_ERROR);

    mbc_mock_transport_set_connected(mock, true);
    mbc_mock_transport_fail_next_send(mock, MBC_STATUS_IO_ERROR);
    assert(mbc_transport_send(&iface, tx_frame, sizeof(tx_frame), &io) == MBC_STATUS_IO_ERROR);
    assert(mbc_mock_transport_pending_tx(mock) == 0U);

    assert(mbc_transport_send(&iface, tx_frame, sizeof(tx_frame), &io) == MBC_STATUS_OK);
    mbc_mock_transport_fail_next_receive(mock, MBC_STATUS_IO_ERROR);
    assert(mbc_transport_receive(&iface, buffer, sizeof(buffer), &io) == MBC_STATUS_IO_ERROR);

    const uint8_t rx_payload[] = {0xAA, 0xBB};
    assert(mbc_mock_transport_schedule_rx(mock, rx_payload, sizeof(rx_payload), 0U) == MBC_STATUS_OK);
    mbc_mock_transport_fail_next_receive(mock, MBC_STATUS_IO_ERROR);
    assert(mbc_transport_receive(&iface, buffer, sizeof(buffer), &io) == MBC_STATUS_IO_ERROR);
    assert(mbc_mock_transport_pending_rx(mock) == 1U);

    assert(mbc_transport_receive(&iface, buffer, sizeof(buffer), &io) == MBC_STATUS_OK);
    assert(io.processed == sizeof(rx_payload));
    assert(memcmp(buffer, rx_payload, sizeof(rx_payload)) == 0);

    assert(mbc_mock_transport_schedule_rx(mock, rx_payload, sizeof(rx_payload), 0U) == MBC_STATUS_OK);
    assert(mbc_mock_transport_drop_next_rx(mock) == MBC_STATUS_OK);
    assert(mbc_mock_transport_pending_rx(mock) == 0U);

    assert(mbc_transport_send(&iface, tx_frame, sizeof(tx_frame), &io) == MBC_STATUS_OK);
    assert(mbc_transport_send(&iface, tx_frame, sizeof(tx_frame), &io) == MBC_STATUS_OK);
    assert(mbc_mock_transport_drop_next_tx(mock) == MBC_STATUS_OK);

    size_t out_len = 0U;
    assert(mbc_mock_transport_fetch_tx(mock, buffer, sizeof(buffer), &out_len) == MBC_STATUS_OK);
    assert(out_len == sizeof(tx_frame));
    assert(memcmp(buffer, tx_frame, sizeof(tx_frame)) == 0);
    assert(mbc_mock_transport_drop_next_tx(mock) == MBC_STATUS_OK);
    assert(mbc_mock_transport_drop_next_tx(mock) == MBC_STATUS_NO_RESOURCES);

    mbc_mock_transport_destroy(mock);
}

int main(void)
{
    printf("=== Mock Transport Tests Starting ===\n");
    fflush(stdout);
    test_rx_flow();
    printf("test_rx_flow PASSED\n");
    test_tx_flow();
    printf("test_tx_flow PASSED\n");
    test_reset_and_yield();
    printf("test_reset_and_yield PASSED\n");
    test_fetch_capacity_guard();
    printf("test_fetch_capacity_guard PASSED\n");
    test_error_controls();
    printf("test_error_controls PASSED\n");
    printf("=== All Mock Transport Tests Passed ===\n");
    return 0;
}
