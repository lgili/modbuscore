/**
 * @file test_modbus_server.cpp
 * @brief Unit tests for the Modbus Server (Slave) using Google Test.
 *
 * This file tests:
 * - Initializing the server.
 * - Registering holding registers (both read/write and read-only).
 * - Handling valid read (0x03) and write single register (0x06) requests.
 * - Responding correctly to broadcast requests.
 * - Generating exception responses for invalid data address or attempts to write read-only registers.
 * - Ensuring the FSM transitions correctly and the final response is sent via the mock transport.
 *
 * We assume the server uses the mock transport. The server FSM is progressed by calling
 * modbus_server_poll() periodically, and incoming data is simulated by injecting bytes
 * into the mock transport.
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

// We need a global context as per our previous assumption
modbus_context_t g_modbus_ctx;
static modbus_transport_t mock_transport;

// Let's define some holding registers to be used in tests
static int16_t test_reg_100;   // RW
static int16_t test_reg_200;   // RO
static int16_t test_reg_array[3]; // RW array

class ModbusServerTest : public ::testing::Test {
protected:
    uint16_t device_addr_{};
    uint16_t baud_{};

    void SetUp() override {
        modbus_transport_init_mock(&mock_transport);
        memset(&g_modbus_ctx, 0, sizeof(g_modbus_ctx));

        device_addr_ = 10;
        baud_ = 19200;

        modbus_error_t err = modbus_server_create(&g_modbus_ctx, &mock_transport, &device_addr_, &baud_);
        ASSERT_EQ(err, MODBUS_ERROR_NONE);


        // Initialize registers
        test_reg_100 = 0x1234;
        test_reg_200 = 0x7777;
        test_reg_array[0] = 0x1111;
        test_reg_array[1] = 0x2222;
        test_reg_array[2] = 0x3333;


        // Register test_reg_100 at address 100 (RW)
        err = modbus_set_holding_register(&g_modbus_ctx, 30, &test_reg_100, false, NULL, NULL);
        EXPECT_EQ(err, MODBUS_ERROR_NONE);

        // Register test_reg_200 at address 200 (RO)
        err = modbus_set_holding_register(&g_modbus_ctx, 10, &test_reg_200, true, NULL, NULL);
        EXPECT_EQ(err, MODBUS_ERROR_NONE);

        // Register test_reg_array at addresses 300,301,302 (RW)
        err = modbus_set_array_holding_register(&g_modbus_ctx, 20, 3, test_reg_array, false, NULL, NULL);
        EXPECT_EQ(err, MODBUS_ERROR_NONE);


        
    }

    void TearDown() override {
        // Nothing special
    }

    void pollServer(unsigned times=1) {
        for (unsigned iteration = 0; iteration < times; ++iteration) {
            mock_advance_time(50);
            modbus_server_data_t *server = (modbus_server_data_t *)g_modbus_ctx.user_data;
            uint8_t data[64];

            // simulating rx interrupt
            if(server->fsm.current_state->id == MODBUS_SERVER_STATE_IDLE || server->fsm.current_state->id == MODBUS_SERVER_STATE_RECEIVING) {
                uint8_t size_read = server->ctx->transport.read(data, 64); // read max size
                if(size_read > 0) {
                    for (uint8_t idx = 0; idx < size_read; ++idx)
                    {
                        modbus_server_receive_data_from_uart_event(&server->fsm, data[idx]);
                        mock_advance_time(5);
                    }
                    
                }
            }

            modbus_server_poll(&g_modbus_ctx);
        }
    }
};

TEST_F(ModbusServerTest, ValidReadRequest) {
    // Build a read holding registers request (0x03) for address=100, quantity=1
    uint8_t req_payload[4] = {0x00, 0x0A, 0x00, 0x01}; // start=10, qty=1
    uint8_t req_frame[20];
    uint16_t req_len = modbus_build_rtu_frame(10, 0x03, req_payload, 4, req_frame, 20);
    ASSERT_GT(req_len, 0);

    // Inject request
    mock_inject_rx_data(req_frame, req_len);
    pollServer(30);

    // Check response: should return 2 bytes of data: test_reg_100=0x1234
    uint8_t resp[20];
    uint16_t resp_len = mock_get_tx_data(resp, 20);
    ASSERT_GT(resp_len, 0);

    // Parse response to verify correctness
    uint8_t address, function;
    const uint8_t *payload;
    uint16_t payload_len = 0;
    modbus_error_t err = modbus_parse_rtu_frame(resp, resp_len, &address, &function, &payload, &payload_len);
    EXPECT_EQ(err, MODBUS_ERROR_NONE);
    EXPECT_EQ(address, 10);
    EXPECT_EQ(function, 0x03);
    EXPECT_EQ(payload_len, 3); // byte_count=2 + 2 data bytes
    EXPECT_EQ(payload[0], 2);  // byte_count=2
    // Data
    uint16_t val = (payload[1] << 8) | payload[2];
    EXPECT_EQ(val, 0x7777);
}

TEST_F(ModbusServerTest, WriteSingleRegisterRW) {
    // Write to address=30, value=0x5555
    uint8_t req_payload[4] = {0x00, 0x1E, 0x55, 0x55};
    uint8_t req_frame[20];
    uint16_t req_len = modbus_build_rtu_frame(10, 0x06, req_payload, 4, req_frame, 20);
    ASSERT_GT(req_len, 0);

    mock_inject_rx_data(req_frame, req_len);
    pollServer(30);

    // Check response: should echo the same address and value
    uint8_t resp[20];
    uint16_t resp_len = mock_get_tx_data(resp, 20);
    ASSERT_GT(resp_len, 0);

    uint8_t address, function;
    const uint8_t *payload;
    uint16_t payload_len = 0;
    modbus_error_t err = modbus_parse_rtu_frame(resp, resp_len, &address, &function, &payload, &payload_len);
    EXPECT_EQ(err, MODBUS_ERROR_NONE);
    EXPECT_EQ(address, 10);
    EXPECT_EQ(function, 0x06);
    EXPECT_EQ(payload_len, 4); 
    EXPECT_EQ((payload[0]<<8)|payload[1], 30);
    EXPECT_EQ((payload[2]<<8)|payload[3], 0x5555);

    // Verify register changed
    EXPECT_EQ(test_reg_100, 0x5555);
}

TEST_F(ModbusServerTest, WriteSingleRegisterRO) {
    // Attempt to write read-only register at address=200
    uint8_t req_payload[4] = {0x00, 0x0A, 0x12, 0x12}; // addr=10, value=0x1212
    uint8_t req_frame[20];
    uint16_t req_len = modbus_build_rtu_frame(10, 0x06, req_payload, 4, req_frame, 20);
    ASSERT_GT(req_len, 0);

    mock_inject_rx_data(req_frame, req_len);
    pollServer(30);

    // Should respond with an exception
    uint8_t resp[20];
    uint16_t resp_len = mock_get_tx_data(resp, 20);
    EXPECT_GT(resp_len, 0);

    uint8_t address, function;
    const uint8_t *payload;
    uint16_t payload_len = 0;
    modbus_error_t err = modbus_parse_rtu_frame(resp, resp_len, &address, &function, &payload, &payload_len);
    EXPECT_EQ(err, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    EXPECT_EQ(address, 10);
    EXPECT_EQ(function, 0x86); // 0x06 | 0x80 = 0x86 error
    // payload[0] is the exception code
    EXPECT_EQ(payload_len, 0);
    // Just check that it's an exception (code may vary in this implementation)
    // Ensure register wasn't changed
    EXPECT_EQ(test_reg_200, 0x7777);
}

TEST_F(ModbusServerTest, BroadcastRequestNoResponse) {
    // Broadcast address = 0x00, function=0x06 write single reg address=30, value=0x2222
    uint8_t req_payload[4] = {0x00, 0x1E, 0x22, 0x22};
    uint8_t req_frame[20];
    uint16_t req_len = modbus_build_rtu_frame(0x00, 0x06, req_payload, 4, req_frame, 20);
    ASSERT_GT(req_len, 0);

    mock_inject_rx_data(req_frame, req_len);
    pollServer(30);

    // No response should be sent
    uint8_t resp[20];
    uint16_t resp_len = mock_get_tx_data(resp, 20);
    EXPECT_EQ(resp_len, 0);

    // Register should have been written anyway
    EXPECT_EQ(test_reg_100, 0x2222);
}

TEST_F(ModbusServerTest, WrongDeviceRequestNoResponse) {
    // Broadcast address = 0x00, function=0x06 write single reg address=30, value=0x2222
    uint8_t req_payload[4] = {0x00, 0x1E, 0x22, 0x22};
    uint8_t req_frame[20];
    uint16_t req_len = modbus_build_rtu_frame(0x22, 0x06, req_payload, 4, req_frame, 20);
    ASSERT_GT(req_len, 0);

    mock_inject_rx_data(req_frame, req_len);
    pollServer(30);

    // No response should be sent
    uint8_t resp[20];
    uint16_t resp_len = mock_get_tx_data(resp, 20);
    EXPECT_EQ(resp_len, 0);

    // Ensure register wasn't changed
    EXPECT_EQ(test_reg_100, 0x1234);
}

TEST_F(ModbusServerTest, InvalidAddressException) {
    // Try to read from address=9999, quantity=1 which we didn't register
    uint8_t req_payload[4] = {0x27, 0x0F, 0x00, 0x01}; // address=9999 (0x270F), qty=1
    uint8_t req_frame[20];
    uint16_t req_len = modbus_build_rtu_frame(10, 0x03, req_payload, 4, req_frame, 20);
    ASSERT_GT(req_len, 0);

    mock_inject_rx_data(req_frame, req_len);
    pollServer(30);

    // Expect an exception response
    uint8_t resp[20];
    uint16_t resp_len = mock_get_tx_data(resp, 20);
    EXPECT_GT(resp_len, 0);

    uint8_t address, function;
    const uint8_t *payload;
    uint16_t payload_len = 0;
    modbus_error_t err = modbus_parse_rtu_frame(resp, resp_len, &address, &function, &payload, &payload_len);
    EXPECT_EQ(err, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
    EXPECT_EQ(address, 10);
    EXPECT_TRUE((function & 0x80) != 0); // error response
}


TEST_F(ModbusServerTest, InvalidFrameException) {
    // Try to read from address=10
    // force wrong frame to simulate noise or bronken cable
    uint8_t req_payload[2] = {0x00, 0x1E}; // address=9999 (0x270F), qty=1
    uint8_t req_frame[20];
    uint16_t req_len = modbus_build_rtu_frame(10, 0x06, req_payload, 2, req_frame, 20);
    ASSERT_GT(req_len, 0);

    mock_inject_rx_data(req_frame, req_len -2); // remove crc to simulate package broken
    pollServer(30);

    // Expect an exception response
    uint8_t resp[20];
    uint16_t resp_len = mock_get_tx_data(resp, 20);
    EXPECT_EQ(resp_len, 0);  // wrong package, do not respond (check if this the correct behavior)

       
    // Ensure register wasn't changed
    EXPECT_EQ(test_reg_100, 0x1234);
}
