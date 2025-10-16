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

#include <algorithm>
#include <cstring>

#include <modbus/modbus.h>
#include <modbus/internal/core.h>
#include <modbus/internal/transport_core.h>
#include <modbus/internal/frame.h>
#include "gtest/gtest.h"

#ifdef __cplusplus
extern "C"{
#endif 
extern void modbus_transport_init_mock(modbus_transport_t *transport);
extern int mock_inject_rx_data(const uint8_t *data, uint16_t length);
extern uint16_t mock_get_tx_data(uint8_t *data, uint16_t maxlen);
extern void mock_clear_tx_buffer(void);
extern void mock_advance_time(uint16_t ms);
extern const mb_transport_if_t *mock_transport_get_iface(void);
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
        modbus_context_use_internal_buffers(&ctx);
        ctx.transport = mock_transport;
        ASSERT_EQ(MODBUS_ERROR_NONE, modbus_transport_bind_legacy(&ctx.transport_iface, &ctx.transport));
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

TEST(ModbusCoreStandalone, BuildFrameRejectsNullBuffer) {
    uint8_t payload[]{0x00U};
    EXPECT_EQ(0U, modbus_build_rtu_frame(0x01U, MODBUS_FUNC_READ_HOLDING_REGISTERS,
                                         payload, sizeof payload, nullptr, 0U));
}

TEST(ModbusCoreStandalone, BuildFrameRejectsSmallBuffer) {
    uint8_t payload[]{0x00U};
    uint8_t buffer[3]{}; // Needs 4 bytes minimum
    EXPECT_EQ(0U, modbus_build_rtu_frame(0x01U, MODBUS_FUNC_READ_HOLDING_REGISTERS,
                                         payload, sizeof payload, buffer, sizeof buffer));
}

TEST(ModbusCoreStandalone, BuildFrameWithoutPayload) {
    uint8_t buffer[8]{};
    uint16_t len = modbus_build_rtu_frame(0x05U, MODBUS_FUNC_WRITE_SINGLE_REGISTER,
                                          nullptr, 0U, buffer, sizeof buffer);
    EXPECT_EQ(4U, len);
    EXPECT_EQ(buffer[0], 0x05U);
    EXPECT_EQ(buffer[1], MODBUS_FUNC_WRITE_SINGLE_REGISTER);
}

TEST(ModbusCoreStandalone, ParseFrameRejectsNullArgs) {
    uint8_t frame[]{0x01U, 0x03U, 0x00U, 0x00U};
    uint8_t address = 0U;
    uint8_t function = 0U;
    const uint8_t *payload = nullptr;
    uint16_t payload_len = 0U;

    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              modbus_parse_rtu_frame(nullptr, sizeof frame, &address, &function, &payload, &payload_len));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              modbus_parse_rtu_frame(frame, sizeof frame, nullptr, &function, &payload, &payload_len));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              modbus_parse_rtu_frame(frame, sizeof frame, &address, nullptr, &payload, &payload_len));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              modbus_parse_rtu_frame(frame, sizeof frame, &address, &function, nullptr, &payload_len));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              modbus_parse_rtu_frame(frame, sizeof frame, &address, &function, &payload, nullptr));
}

TEST(ModbusCoreStandalone, ParseFrameRejectsShortFrame) {
    uint8_t frame[]{0x01U, 0x03U, 0x02U};
    uint8_t address = 0U;
    uint8_t function = 0U;
    const uint8_t *payload = nullptr;
    uint16_t payload_len = 0U;

    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              modbus_parse_rtu_frame(frame, sizeof frame, &address, &function, &payload, &payload_len));
}

TEST_F(ModbusCoreTest, ParseFrameErrorResponse) {
    uint8_t buffer[8]{};
    uint8_t exception_code = 0x01U;
    uint16_t len = modbus_build_rtu_frame(0x0AU,
                                          MODBUS_FUNC_READ_HOLDING_REGISTERS | MODBUS_FUNC_ERROR_FRAME_HEADER,
                                          &exception_code, 1U,
                                          buffer, sizeof buffer);
    ASSERT_GT(len, 0U);

    uint8_t address = 0U;
    uint8_t function = 0U;
    const uint8_t *payload = nullptr;
    uint16_t payload_len = 0U;

    EXPECT_EQ(MODBUS_EXCEPTION_ILLEGAL_FUNCTION,
              modbus_parse_rtu_frame(buffer, len, &address, &function, &payload, &payload_len));
}

TEST_F(ModbusCoreTest, SendFrameRejectsInvalidArgs) {
    uint8_t frame[]{0x01U, 0x03U, 0x00U, 0x00U};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, modbus_send_frame(nullptr, frame, sizeof frame));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, modbus_send_frame(&ctx, nullptr, sizeof frame));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, modbus_send_frame(&ctx, frame, 0U));
}

TEST_F(ModbusCoreTest, SendFrameTransportError) {
    mock_clear_tx_buffer();
    uint8_t filler[64]{};
    for (int i = 0; i < 5; ++i) {
        if (ctx.transport.write(filler, sizeof filler) < 0) {
            break;
        }
    }

    uint8_t frame[]{0x01U, 0x03U, 0x00U, 0x00U};
    EXPECT_EQ(MODBUS_ERROR_TRANSPORT, modbus_send_frame(&ctx, frame, sizeof frame));
    mock_clear_tx_buffer();
}

TEST(ModbusCoreStandalone, ReceiveFrameRejectsInvalidArgs) {
    modbus_transport_init_mock(&mock_transport);
    modbus_context_t local_ctx{};
    local_ctx.transport = mock_transport;
    ASSERT_EQ(MODBUS_ERROR_NONE, modbus_transport_bind_legacy(&local_ctx.transport_iface, &local_ctx.transport));

    uint8_t buffer[8]{};
    uint16_t length = 0U;

    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, modbus_receive_frame(nullptr, buffer, sizeof buffer, &length));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, modbus_receive_frame(&local_ctx, nullptr, sizeof buffer, &length));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, modbus_receive_frame(&local_ctx, buffer, 3U, &length));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, modbus_receive_frame(&local_ctx, buffer, sizeof buffer, nullptr));
}

TEST(ModbusFrame, EncodeRtu)
{
    const mb_u8 payload[]{0x00U, 0x02U};
    const mb_adu_view_t adu{0x11U, MODBUS_FUNC_READ_HOLDING_REGISTERS, payload, MB_COUNTOF(payload)};

    mb_u8 frame[32]{};
    mb_size_t frame_len = 0U;
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_frame_rtu_encode(&adu, frame, sizeof frame, &frame_len));
    EXPECT_EQ(6U, frame_len);
    EXPECT_EQ(0x11U, frame[0]);
    EXPECT_EQ(MODBUS_FUNC_READ_HOLDING_REGISTERS, frame[1]);

    const mb_u16 crc = (mb_u16)((mb_u16)frame[frame_len - 1U] << 8U) | frame[frame_len - 2U];
    EXPECT_EQ(modbus_crc_with_table(frame, (mb_u16)(frame_len - 2U)), crc);
}

TEST(ModbusFrame, DecodeRtu)
{
    const mb_u8 frame[]{0x22U, 0x06U, 0x00U, 0x2AU, 0x02U, 0x1FU, 0xEFU, 0xF9U};
    mb_adu_view_t adu{};
    ASSERT_EQ(MODBUS_ERROR_NONE, mb_frame_rtu_decode(frame, MB_COUNTOF(frame), &adu));
    EXPECT_EQ(0x22U, adu.unit_id);
    EXPECT_EQ(0x06U, adu.function);
    ASSERT_NE(nullptr, adu.payload);
    EXPECT_EQ(4U, adu.payload_len);
    EXPECT_EQ(0x00U, adu.payload[0]);
    EXPECT_EQ(0x2AU, adu.payload[1]);
    EXPECT_EQ(0x02U, adu.payload[2]);
    EXPECT_EQ(0x1FU, adu.payload[3]);
}

TEST(ModbusFrame, DecodeRejectsCrcMismatch)
{
    const mb_u8 frame[]{0x01U, 0x03U, 0x02U, 0x00U, 0x01U, 0x00U, 0x00U};
    mb_adu_view_t adu{};
    EXPECT_EQ(MODBUS_ERROR_CRC, mb_frame_rtu_decode(frame, MB_COUNTOF(frame), &adu));
}

TEST(ModbusTransportIf, SendRecvShims)
{
    const mb_transport_if_t *iface = mock_transport_get_iface();
    ASSERT_NE(nullptr, iface);

    mock_clear_tx_buffer();
    const mb_u8 sample[]{0xAAU, 0xBBU, 0xCCU};
    mb_transport_io_result_t io{};
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_transport_send(iface, sample, MB_COUNTOF(sample), &io));
    EXPECT_EQ(MB_COUNTOF(sample), io.processed);

    mb_u8 verify[3]{};
    EXPECT_EQ(MB_COUNTOF(sample), mock_get_tx_data(verify, (uint16_t)sizeof verify));
    EXPECT_TRUE(std::equal(std::begin(sample), std::end(sample), std::begin(verify)));

    mock_clear_tx_buffer();
    mb_u8 rx_frame[]{0x01U, 0x03U, 0x02U, 0x00U, 0x64U};
    const mb_u16 crc = modbus_crc_with_table(rx_frame, (uint16_t)(sizeof rx_frame));
    uint8_t frame_full[sizeof rx_frame + 2U];
    memcpy(frame_full, rx_frame, sizeof rx_frame);
    frame_full[sizeof rx_frame] = (uint8_t)(crc & 0xFFU);
    frame_full[sizeof rx_frame + 1U] = (uint8_t)((crc >> 8) & 0xFFU);

    ASSERT_EQ(0, mock_inject_rx_data(frame_full, (uint16_t)sizeof frame_full));

    mb_u8 recv_buf[16]{};
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_transport_recv(iface, recv_buf, sizeof recv_buf, &io));
    EXPECT_EQ(sizeof frame_full, io.processed);
    EXPECT_TRUE(std::equal(frame_full, frame_full + io.processed, recv_buf));
}

