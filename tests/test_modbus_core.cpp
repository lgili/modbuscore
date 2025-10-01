/**
 * @file test_modbus_core.cpp
 * @brief Unit tests for the Modbus core functions using Google Test.
 *
 * This file tests functionalities from modbus_core.h and modbus_core.c:
 * - Building RTU frames (modbus_build_rtu_frame).
 * - Parsing RTU frames (modbus_parse_rtu_frame).
 * - Sending and receiving frames via the provided transport.
 * - Checking CRC and validating frames.
 * - Converting exception codes to errors.
 * - Resetting buffers.
 *
 * We will use the mock transport initialized similarly to previous tests.
 *
 * Author:
 * Date: 2024-12-20
 */

#include <modbus/modbus.h>
#include "gtest/gtest.h"

#ifdef __cplusplus
extern "C"{
#endif 
extern void modbus_transport_init_mock(modbus_transport_t *transport);
extern int mock_inject_rx_data(const uint8_t *data, uint16_t length);
extern uint16_t mock_get_tx_data(uint8_t *data, uint16_t maxlen);
extern void mock_clear_tx_buffer(void);
extern void mock_advance_time(uint16_t ms);
#ifdef __cplusplus
}
#endif

static modbus_transport_t mock_transport;
static modbus_context_t ctx;

class ModbusCoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        modbus_transport_init_mock(&mock_transport);
        memset(&ctx, 0, sizeof(ctx));
        ctx.transport = mock_transport;
    }
};

TEST_F(ModbusCoreTest, BuildRtuFrame) {
    uint8_t data[] = {0x00, 0x0A}; // start_addr=0x0000, quantity=0x000A
    uint8_t out_buffer[10];
    uint16_t len = modbus_build_rtu_frame(0x01, 0x03, data, 2, out_buffer, sizeof(out_buffer));
    EXPECT_GT(len, 0);

    // Frame should be: [0x01][0x03][0x00][0x0A][CRC low][CRC high]
    EXPECT_EQ(out_buffer[0], 0x01);
    EXPECT_EQ(out_buffer[1], 0x03);
    EXPECT_EQ(out_buffer[2], 0x00);
    EXPECT_EQ(out_buffer[3], 0x0A);
    // We won't check CRC exact value here, just ensure length and no zero length.
}

TEST_F(ModbusCoreTest, ParseRtuFrameValid) {
    // Build a valid frame and parse it
    uint8_t out_buffer[10];
    uint8_t data[] = {0x00, 0x0A};
    uint16_t len = modbus_build_rtu_frame(0x01, 0x03, data, 2, out_buffer, sizeof(out_buffer));
    ASSERT_GT(len, 0);

    uint8_t address, function;
    const uint8_t *payload;
    uint16_t payload_len;
    modbus_error_t err = modbus_parse_rtu_frame(out_buffer, len, &address, &function, &payload, &payload_len);
    EXPECT_EQ(err, MODBUS_ERROR_NONE);
    EXPECT_EQ(address, 0x01);
    EXPECT_EQ(function, 0x03);
    EXPECT_EQ(payload_len, 2); // two bytes: 0x00,0x0A
    EXPECT_EQ(payload[0], 0x00);
    EXPECT_EQ(payload[1], 0x0A);
}

TEST_F(ModbusCoreTest, ParseRtuFrameInvalidCrc) {
    // Create a frame and then tamper with CRC
    uint8_t out_buffer[10];
    uint8_t data[] = {0x00, 0x0A};
    uint16_t len = modbus_build_rtu_frame(0x01, 0x03, data, 2, out_buffer, sizeof(out_buffer));

    // Corrupt one byte
    out_buffer[len - 1] ^= 0xFF;

    uint8_t address, function;
    const uint8_t *payload;
    uint16_t payload_len;
    modbus_error_t err = modbus_parse_rtu_frame(out_buffer, len, &address, &function, &payload, &payload_len);
    EXPECT_EQ(err, MODBUS_ERROR_CRC);
}

TEST_F(ModbusCoreTest, SendFrame) {
    uint8_t frame[] = {0x01, 0x03, 0x00, 0x0A, 0xC5, 0xCD}; // example frame
    modbus_error_t err = modbus_send_frame(&ctx, frame, (uint16_t)sizeof(frame));
    EXPECT_EQ(err, MODBUS_ERROR_NONE);

    uint8_t verify[10];
    uint16_t got = mock_get_tx_data(verify, 10);
    EXPECT_EQ(got, 6);
    EXPECT_EQ(memcmp(verify, frame, 6), 0);
}

TEST_F(ModbusCoreTest, ReceiveFrame) {
    // Inject a valid frame into RX and try to receive it
    uint8_t frame[] = {0x01, 0x03, 0x00, 0x0A, 0xC5, 0xCD};
    mock_inject_rx_data(frame, 6);

    uint8_t out_buf[20];
    uint16_t out_len = 0;
    modbus_error_t err = modbus_receive_frame(&ctx, out_buf, (uint16_t)sizeof(out_buf), &out_len);
    // Depending on the receive_frame logic, it might block or require multiple calls
    // For simplicity, assume it returns after reading all data.
    EXPECT_EQ(err, MODBUS_ERROR_NONE);
    EXPECT_EQ(out_len, 6);
    EXPECT_EQ(memcmp(out_buf, frame, 6), 0);
}

TEST_F(ModbusCoreTest, ExceptionToError) {
    EXPECT_EQ(modbus_exception_to_error(1), MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
    EXPECT_EQ(modbus_exception_to_error(2), MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    EXPECT_EQ(modbus_exception_to_error(3), MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE);
    EXPECT_EQ(modbus_exception_to_error(4), MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE);
    EXPECT_EQ(modbus_exception_to_error(99), MODBUS_ERROR_OTHER); // unknown exception
}

TEST_F(ModbusCoreTest, ResetBuffers) {
    ctx.rx_count = 10;
    ctx.tx_index = 5;
    ctx.tx_raw_index = 3;
    ctx.rx_buffer[0] = 0x55;
    ctx.tx_buffer[0] = 0xAA;
    ctx.tx_raw_buffer[0] = 0xCC;
    modbus_reset_buffers(&ctx);
    EXPECT_EQ(ctx.rx_count, 0);
    EXPECT_EQ(ctx.tx_index, 0);
    EXPECT_EQ(ctx.tx_raw_index, 0);
    EXPECT_EQ(ctx.rx_buffer[0], 0x00);
    EXPECT_EQ(ctx.tx_buffer[0], 0x00);
    EXPECT_EQ(ctx.tx_raw_buffer[0], 0x00);
}

