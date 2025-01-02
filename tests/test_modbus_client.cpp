// /**
//  * @file test_modbus_master.cpp
//  * @brief Unit tests for the Modbus Master using Google Test.
//  *
//  * This file tests the Modbus Master implementation:
//  * - Initialization and timeout configuration.
//  * - Sending a read holding registers request (0x03).
//  * - Handling no-response timeouts.
//  * - Receiving a valid response and parsing data.
//  * - Handling Modbus exception responses from the slave.
//  *
//  * We assume that:
//  * - The master uses the mock transport and we've integrated it.
//  * - The master FSM is tested by calling modbus_master_poll() regularly and
//  *   injecting events such as received data.
//  *
//  * Author:
//  * Date: 2024-12-20
//  */

// #include <modbus/modbus.h>
// #include "gtest/gtest.h"

// #ifdef __cplusplus
// extern "C"{
// #endif
// extern void modbus_transport_init_mock(modbus_transport_t *transport);
// extern int mock_inject_rx_data(const uint8_t *data, uint16_t length);
// extern uint16_t mock_get_tx_data(uint8_t *data, uint16_t maxlen);
// extern void mock_clear_tx_buffer(void);
// extern void mock_advance_time(uint16_t ms);
// #ifdef __cplusplus
// }
// #endif


// static modbus_transport_t mock_transport;
// static modbus_context_t g_modbus_ctx; // The master context

// class ModbusMasterTest : public ::testing::Test {
// protected:
//     void SetUp() override {
//         modbus_transport_init_mock(&mock_transport);
//         memset(&g_modbus_ctx, 0, sizeof(g_modbus_ctx));

//         uint16_t baud = 19200;
//         modbus_client_create(&g_modbus_ctx, &mock_transport, &baud);
//     }

//     void TearDown() override {
//         // nothing special
//     }

//     void pollMaster(unsigned times=1) {
//         for (unsigned i=0; i<times; i++) {
//             mock_advance_time(50);
//             modbus_client_data_t *client = (modbus_client_data_t *)g_modbus_ctx.user_data;
//             uint8_t data[64];
//             // simulating rx interrupt
//             if(client->fsm.current_state->id == MODBUS_CLIENT_STATE_WAITING_RESPONSE) {
//                 // modbus_client_receive_data_event
//                 uint8_t size_read = client->ctx->transport.read(data, 64); // read max size
//                 if(size_read > 0) {
//                     for (size_t i = 0; i < size_read; i++)
//                     {
//                         modbus_client_receive_data_event(&client->fsm, data[i]);
//                         mock_advance_time(5);
//                     }
                    
//                 }
//             }
//             //modbus_master_poll(&ctx);
//         }
//     }
// };

// TEST_F(ModbusMasterTest, SetTimeout) {
//     modbus_error_t err = modbus_client_set_timeout(&g_modbus_ctx, 2000);
//     EXPECT_EQ(err, MODBUS_ERROR_NONE);
//     // Just trust that it's set internally, no direct getter here.
// }

// TEST_F(ModbusMasterTest, NoResponseTimeout) {
//     // Send a read holding registers request
//     modbus_error_t err = modbus_client_read_holding_registers(&g_modbus_ctx, 0x01, 0x0000, 2);
//     EXPECT_EQ(err, MODBUS_ERROR_NONE);

//     // After requesting, the master should send a frame immediately
//     pollMaster();
//     uint8_t tx_buf[20];
//     uint16_t tx_len = mock_get_tx_data(tx_buf, 20);
//     EXPECT_GT(tx_len, 0); // Something was sent

//     // Advance time beyond default timeout (1000ms) to trigger timeout
//     mock_advance_time(1500);
//     pollMaster(10); // multiple polls to process the timeout
//     // After timeout, the master should go to error and then back to idle.
//     // We do not have direct assertions here other than no crash.
//     // In a real scenario, we might need to inspect internal state or have hooks for error handling.
//     // For now, just ensure no crash and no exceptions.
// }

// // TEST_F(ModbusMasterTest, ValidResponse) {
// //     // Send request
// //     modbus_master_read_holding_registers(&ctx, 0x01, 0x0000, 2);
// //     pollMaster();
// //     // Check TX
// //     uint8_t tx_buf[20];
// //     uint16_t tx_len = mock_get_tx_data(tx_buf, 20);
// //     EXPECT_GT(tx_len, 0);

// //     // Inject a valid response: for 2 registers, byte_count=4, values=0x1111,0x2222
// //     // Frame: [0x01][0x03][0x04][0x11][0x11][0x22][0x22][CRC low][CRC high]
// //     // We'll build it using modbus_build_rtu_frame for accuracy
// //     uint8_t resp_payload[5]; 
// //     // payload: byte_count=4, data=11 11 22 22
// //     resp_payload[0] = 0x04; 
// //     resp_payload[1] = 0x11; resp_payload[2] = 0x11;
// //     resp_payload[3] = 0x22; resp_payload[4] = 0x22;
// //     uint8_t resp_frame[20];
// //     uint16_t resp_len = modbus_build_rtu_frame(0x01, 0x03, resp_payload, 5, resp_frame, 20);
// //     ASSERT_GT(resp_len, 0);

// //     // Inject response
// //     mock_inject_rx_data(resp_frame, resp_len);

// //     // Poll until response is processed
// //     pollMaster(10);

// //     // Now get the data from master
// //     int16_t read_data[10];
// //     uint16_t count = modbus_master_get_read_data(&ctx, read_data, 10);
// //     EXPECT_EQ(count, 2);
// //     EXPECT_EQ(read_data[0], 0x1111);
// //     EXPECT_EQ(read_data[1], 0x2222);
// // }

// // TEST_F(ModbusMasterTest, ExceptionResponse) {
// //     // Request read
// //     modbus_master_read_holding_registers(&ctx, 0x01, 0x0000, 2);
// //     pollMaster();
// //     // Check something was sent
// //     uint8_t tx_buf[20];
// //     uint16_t tx_len = mock_get_tx_data(tx_buf, 20);
// //     EXPECT_GT(tx_len, 0);

// //     // Exception response: function=0x83 (error), exception code=2 (Illegal Data Address)
// //     uint8_t exc_payload[1];
// //     exc_payload[0] = 0x02; // exception code
// //     uint8_t exc_frame[20];
// //     uint16_t exc_len = modbus_build_rtu_frame(0x01, 0x83, exc_payload, 1, exc_frame, 20);
// //     ASSERT_GT(exc_len, 0);

// //     mock_inject_rx_data(exc_frame, exc_len);
// //     pollMaster(10); 
// //     // The master should handle exception and return to idle, no data read.
// //     int16_t read_data[10];
// //     uint16_t count = modbus_master_get_read_data(&ctx, read_data, 10);
// //     EXPECT_EQ(count, 0); // no data since it was an exception
// // }






// test_modbus_master.cpp

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
static modbus_context_t g_modbus_ctx; // The master context

class ModbusMasterTest : public ::testing::Test {
protected:
    void SetUp() override {
        modbus_transport_init_mock(&mock_transport);
        memset(&g_modbus_ctx, 0, sizeof(g_modbus_ctx));

        uint16_t baud = 19200;
        modbus_client_create(&g_modbus_ctx, &mock_transport, &baud);
    }

    void TearDown() override {
        // Reset buffers after each test
        mock_clear_tx_buffer();
    }

    // Poll the master and handle incoming data
    void pollMaster(unsigned times=1) {
        for (unsigned i=0; i<times; i++) {
            mock_advance_time(50); // Simulate time passing

            modbus_client_data_t *client = (modbus_client_data_t *)g_modbus_ctx.user_data;
            uint8_t data[64];
            // Simulate RX interrupt if waiting for a response
            if(client->fsm.current_state->id == MODBUS_CLIENT_STATE_WAITING_RESPONSE) {
                // Read data from mock transport's rx buffer
                uint8_t size_read = client->ctx->transport.read(data, 64); // Adjust based on your mock_transport
                if(size_read > 0) {
                    for (size_t i = 0; i < size_read; i++) {
                        modbus_client_receive_data_event(&client->fsm, data[i]);
                        mock_advance_time(5);
                    }
                }
            }

            modbus_client_poll(&g_modbus_ctx);
        }
    }

    // Helper function to simulate slave response based on master's request
    void mock_slave_respond() {
        // Read the transmitted data from the mock transport's tx buffer
        uint8_t tx_buf[256];
        uint16_t tx_len = mock_get_tx_data(tx_buf, sizeof(tx_buf));
        if(tx_len == 0) return; // No data sent by master

        // Simple parsing: assuming single request per test
        if(tx_len < 8) {
            // Not enough data for a Modbus RTU request
            return;
        }

        uint8_t slave_address = tx_buf[0];
        uint8_t function = tx_buf[1];

        if(function == 0x03) { // Read Holding Registers
            uint16_t start_addr = (tx_buf[2] << 8) | tx_buf[3];
            uint16_t quantity = (tx_buf[4] << 8) | tx_buf[5];

            // Generate response: byte_count + data + CRC
            uint8_t resp_payload[1 + 2 * quantity];
            resp_payload[0] = 2 * quantity; // byte_count
            for(int i=0; i < quantity; i++) {
                // For testing, set register value to (start_addr + i) * 0x100 + i
                // e.g., start_addr=0x0000, i=0: 0x0000, i=1: 0x0101
                uint16_t reg_val = (start_addr + i) * 0x100 + i;
                resp_payload[1 + 2*i] = (reg_val >> 8) & 0xFF; // high byte
                resp_payload[1 + 2*i + 1] = reg_val & 0xFF;    // low byte
            }

            uint8_t resp_frame[256];
            uint16_t resp_len = modbus_build_rtu_frame(slave_address, function, resp_payload, 1 + 2 * quantity, resp_frame, sizeof(resp_frame));
            if(resp_len > 0) {
                mock_inject_rx_data(resp_frame, resp_len);
            }

        } else if(function == 0x06) { // Write Single Register
            uint16_t reg_addr = (tx_buf[2] << 8) | tx_buf[3];
            uint16_t reg_val = (tx_buf[4] << 8) | tx_buf[5];

            // For simplicity, echo back the same frame as response
            uint8_t resp_frame[256];
            uint16_t resp_len = modbus_build_rtu_frame(slave_address, function, &tx_buf[2], 4, resp_frame, sizeof(resp_frame));
            if(resp_len > 0) {
                mock_inject_rx_data(resp_frame, resp_len);
            }

        }
        // Handle other function codes as needed
    }
};

TEST_F(ModbusMasterTest, SetTimeout) {
    modbus_error_t err = modbus_client_set_timeout(&g_modbus_ctx, 2000);
    EXPECT_EQ(err, MODBUS_ERROR_NONE);
    // Just trust that it's set internally, no direct getter here.
}

TEST_F(ModbusMasterTest, NoResponseTimeout) {
    // Send a read holding registers request
    modbus_error_t err = modbus_client_read_holding_registers(&g_modbus_ctx, 0x01, 0x0000, 2);
    EXPECT_EQ(err, MODBUS_ERROR_NONE);

    // Poll to send the request
    pollMaster(1);

    // Check that something was sent
    uint8_t tx_buf[256];
    uint16_t tx_len = mock_get_tx_data(tx_buf, sizeof(tx_buf));
    EXPECT_GT(tx_len, 0); // Master sent a request

    // No response is injected, so advance time to trigger timeout
    mock_advance_time(1500);
    pollMaster(30); // multiple polls to process the timeout

    // Now, we would like to check if the master handled the timeout correctly.
    // Since we don't have access to internal states, we may need to add hooks or callbacks.
    // Alternatively, check that no data was read
    int16_t read_data[10];
    uint16_t count = modbus_client_get_read_data(&g_modbus_ctx, read_data, 10);
    EXPECT_EQ(count, 0); // No data was read due to timeout
}

TEST_F(ModbusMasterTest, ValidResponse) {
    // Send a read holding registers request
    modbus_error_t err = modbus_client_read_holding_registers(&g_modbus_ctx, 0x01, 0x0000, 2);
    EXPECT_EQ(err, MODBUS_ERROR_NONE);

    // Poll to send the request
    pollMaster(1);

    // Check that something was sent
    uint8_t tx_buf[256];
    uint16_t tx_len = mock_get_tx_data(tx_buf, sizeof(tx_buf));
    EXPECT_GT(tx_len, 0); // Master sent a request

    // Simulate slave responding to the request
    mock_slave_respond();

    // Poll to process the response
    pollMaster(10);

    // Retrieve data
    int16_t read_data[10];
    uint16_t count = modbus_client_get_read_data(&g_modbus_ctx, read_data, 10);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(read_data[0], 0x0000); // (0x0000 + 0) * 0x100 + 0 = 0x0000
    EXPECT_EQ(read_data[1], 0x0101); // (0x0000 + 1) * 0x100 + 1 = 0x0101
}

// TEST_F(ModbusMasterTest, WriteSingleRegisterValid) {
//     // Assume that writing register 0x0001 with value 0x1234

//     // Send write single register request
//     uint16_t reg_addr = 0x0001;
//     uint16_t reg_val = 0x1234;
//     modbus_error_t err = modbus_client_write_single_register(&g_modbus_ctx, 0x01, reg_addr, reg_val);
//     EXPECT_EQ(err, MODBUS_ERROR_NONE);

//     // Poll to send the request
//     pollMaster(1);

//     // Check that something was sent
//     uint8_t tx_buf[256];
//     uint16_t tx_len = mock_get_tx_data(tx_buf, sizeof(tx_buf));
//     EXPECT_GT(tx_len, 0); // Master sent a request

//     // Simulate slave responding to the write request by echoing back the request
//     mock_slave_respond();

//     // Poll to process the response
//     pollMaster(10);

//     // Since it's a write, the master might not have data to read, but could have status
//     // Depending on implementation, check for success or status
//     // For simplicity, check that no data was read
//     int16_t read_data[10];
//     uint16_t count = modbus_master_get_read_data(&g_modbus_ctx, read_data, 10);
//     EXPECT_EQ(count, 0); // No data read for write
// }

// TEST_F(ModbusMasterTest, ExceptionResponse) {
//     // Send a read holding registers request with invalid address to trigger exception
//     modbus_error_t err = modbus_client_read_holding_registers(&g_modbus_ctx, 0x01, 0xFFFF, 1); // Invalid address
//     EXPECT_EQ(err, MODBUS_ERROR_NONE);

//     // Poll to send the request
//     pollMaster(1);

//     // Check that something was sent
//     uint8_t tx_buf[256];
//     uint16_t tx_len = mock_get_tx_data(tx_buf, sizeof(tx_buf));
//     EXPECT_GT(tx_len, 0); // Master sent a request

//     // Simulate slave responding with exception
//     // Build exception response: address, function + 0x80, exception code
//     uint8_t exc_payload = 0x02; // Exception code: Illegal Data Address
//     uint8_t exc_frame[256];
//     uint16_t exc_len = modbus_build_rtu_frame(0x01, 0x83, &exc_payload, 1, exc_frame, sizeof(exc_frame)); // 0x03 + 0x80 = 0x83
//     ASSERT_GT(exc_len, 0);

//     mock_inject_rx_data(exc_frame, exc_len);

//     // Poll to process the response
//     pollMaster(10);

//     // Depending on implementation, the master might have a way to report errors
//     // Since we don't have access to internal states, we could check that no data was read
//     int16_t read_data[10];
//     uint16_t count = modbus_master_get_read_data(&g_modbus_ctx, read_data, 10);
//     EXPECT_EQ(count, 0); // No data read due to exception

//     // Alternatively, if the master has error reporting mechanisms, check them
// }
