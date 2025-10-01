/**
 * @file test_modbus_transport.cpp
 * @brief Unit tests for the mock transport implementation using Google Test.
 *
 * This file tests the functionality of the mock transport layer implemented in
 * modbus_transport.c. The mock provides:
 * - mock_inject_rx_data() to simulate incoming bytes.
 * - mock_get_tx_data() to retrieve what was "written" to the transport.
 * - mock_advance_time() to simulate the passage of time for timeout checks.
 * - initialized via modbus_transport_init_mock().
 *
 * We verify:
 * - Reading data after injecting it.
 * - Writing data and checking if it is stored properly.
 * - Time measurement functions work as expected.
 *
 * Author:
 * Date: 2024-12-20
 */

#include <modbus/modbus.h>
#include "gtest/gtest.h"

#ifdef __cplusplus
extern "C"{
#endif 
// Since we have a mock implementation, we must declare these external functions.
// They should be defined in the mock implementation file (modbus_transport.c).
extern void modbus_transport_init_mock(modbus_transport_t *transport);
extern int mock_inject_rx_data(const uint8_t *data, uint16_t length);
extern uint16_t mock_get_tx_data(uint8_t *data, uint16_t maxlen);
extern void mock_clear_tx_buffer(void);
extern void mock_advance_time(uint16_t ms);
#ifdef __cplusplus
}
#endif

static modbus_transport_t mock_transport;

class ModbusTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        modbus_transport_init_mock(&mock_transport);
    }
};

TEST_F(ModbusTransportTest, InjectAndReadData) {
    uint8_t test_data[] = {0x11, 0x22, 0x33};
    int res = mock_inject_rx_data(test_data, (uint16_t)sizeof(test_data));
    EXPECT_EQ(res, 0);

    // Try to read the injected data
    uint8_t read_buf[3];
    int32_t read_count = mock_transport.read(read_buf, 3);
    EXPECT_EQ(read_count, 3);
    EXPECT_EQ(read_buf[0], 0x11);
    EXPECT_EQ(read_buf[1], 0x22);
    EXPECT_EQ(read_buf[2], 0x33);
}

TEST_F(ModbusTransportTest, PartialRead) {
    uint8_t test_data[] = {0xA5, 0x5A};
    mock_inject_rx_data(test_data, 2);

    // Try reading more than available
    uint8_t read_buf[4];
    int32_t read_count = mock_transport.read(read_buf, 4);
    // Should return 2 since only 2 are available
    EXPECT_EQ(read_count, 2);
    EXPECT_EQ(read_buf[0], 0xA5);
    EXPECT_EQ(read_buf[1], 0x5A);
}

TEST_F(ModbusTransportTest, WriteData) {
    uint8_t out_data[] = {0x10, 0x20, 0x30};
    int32_t written = mock_transport.write(out_data, 3);
    EXPECT_EQ(written, 3);

    uint8_t verify_buf[3];
    uint16_t got = mock_get_tx_data(verify_buf, 3);
    EXPECT_EQ(got, 3);
    EXPECT_EQ(verify_buf[0], 0x10);
    EXPECT_EQ(verify_buf[1], 0x20);
    EXPECT_EQ(verify_buf[2], 0x30);

    // Clear TX and verify it's empty
    mock_clear_tx_buffer();
    got = mock_get_tx_data(verify_buf, 3);
    EXPECT_EQ(got, 0);
}

TEST_F(ModbusTransportTest, TimeMeasurement) {
    uint16_t ref = mock_transport.get_reference_msec();
    EXPECT_EQ(ref, 0);

    mock_advance_time(100);
    uint16_t elapsed = mock_transport.measure_time_msec(ref);
    EXPECT_EQ(elapsed, 100);

    // Advance more time
    mock_advance_time(50);
    elapsed = mock_transport.measure_time_msec(ref);
    EXPECT_EQ(elapsed, 150);
}

TEST_F(ModbusTransportTest, ChangeBaudrateAndRestartUart) {
    // Just test if these functions do not crash and return a dummy value
    if (mock_transport.change_baudrate) {
        uint16_t new_baud = mock_transport.change_baudrate(9600);
        // Expect the mock to return a default value (e.g., 19200)
        EXPECT_EQ(new_baud, 19200);
    }

    if (mock_transport.restart_uart) {
        // Should not fail or crash
        mock_transport.restart_uart();
    }

    // write_gpio and parse_bootloader_request are optional,
    // we can just call them to ensure no crash
    if (mock_transport.write_gpio) {
        uint8_t res = mock_transport.write_gpio(1, 1);
        EXPECT_EQ(res, 0);
    }

    if (mock_transport.parse_bootloader_request) {
        uint8_t buf[10];
        uint16_t size = 10;
        uint8_t res = mock_transport.parse_bootloader_request(buf, &size);
        EXPECT_EQ(res, 0);
    }
}

