/**
 * @file test_client_convenience.cpp
 * @brief Unit tests for high-level convenience client API (mb_client_* functions)
 */

#include <gtest/gtest.h>

extern "C" {
#include <modbus/client.h>
#include <modbus/pdu.h>
#include <modbus/mb_err.h>
#include <modbus/transport.h>
}

extern "C" {
void modbus_transport_init_mock(modbus_transport_t *transport);
int mock_inject_rx_data(const uint8_t *data, uint16_t length);
uint16_t mock_get_tx_data(uint8_t *data, uint16_t maxlen);
void mock_clear_tx_buffer(void);
void mock_advance_time(uint16_t ms);
const mb_transport_if_t *mock_transport_get_iface(void);
}

namespace {

class MbClientConvenienceTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        modbus_transport_init_mock(&legacy_transport_);
        iface_ = mock_transport_get_iface();
        ASSERT_NE(nullptr, iface_);
        
        // Initialize client with transaction pool
        ASSERT_EQ(MODBUS_ERROR_NONE,
            mb_client_init(&client_, iface_, txn_pool_.data(), txn_pool_.size())
        );
        
        mb_client_set_watchdog(&client_, 200U);
        mock_clear_tx_buffer();
    }

    void TearDown() override
    {
        mock_clear_tx_buffer();
    }

    modbus_transport_t legacy_transport_;
    const mb_transport_if_t *iface_;
    mb_client_t client_;
    std::array<mb_client_txn_t, 8> txn_pool_;
};

/* -------------------------------------------------------------------------- */
/*                      Read Holding Registers (FC03)                         */
/* -------------------------------------------------------------------------- */

TEST_F(MbClientConvenienceTest, ReadHoldingRegisters_Success)
{
    // Request 5 holding registers starting at address 100
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_read_holding_registers(&client_, 0x11, 100, 5, &txn);
    
    ASSERT_EQ(MODBUS_ERROR_NONE, err);
    ASSERT_NE(nullptr, txn);
}

TEST_F(MbClientConvenienceTest, ReadHoldingRegisters_NullClient)
{
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_read_holding_registers(nullptr, 0x11, 0, 1, &txn);
    EXPECT_NE(MODBUS_ERROR_NONE, err);
}

TEST_F(MbClientConvenienceTest, ReadHoldingRegisters_NullOutput)
{
    mb_err_t err = mb_client_read_holding_registers(&client_, 0x11, 0, 1, nullptr);
    EXPECT_NE(MODBUS_ERROR_NONE, err);
}

TEST_F(MbClientConvenienceTest, ReadHoldingRegisters_ZeroCount)
{
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_read_holding_registers(&client_, 0x11, 0, 0, &txn);
    EXPECT_NE(MODBUS_ERROR_NONE, err);
}

/* -------------------------------------------------------------------------- */
/*                      Read Input Registers (FC04)                           */
/* -------------------------------------------------------------------------- */

TEST_F(MbClientConvenienceTest, ReadInputRegisters_Success)
{
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_read_input_registers(&client_, 0x11, 200, 3, &txn);
    ASSERT_EQ(MODBUS_ERROR_NONE, err);
    ASSERT_NE(nullptr, txn);
}

TEST_F(MbClientConvenienceTest, ReadInputRegisters_NullClient)
{
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_read_input_registers(nullptr, 0x11, 0, 1, &txn);
    EXPECT_NE(MODBUS_ERROR_NONE, err);
}

/* -------------------------------------------------------------------------- */
/*                          Read Coils (FC01)                                 */
/* -------------------------------------------------------------------------- */

TEST_F(MbClientConvenienceTest, ReadCoils_Success)
{
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_read_coils(&client_, 0x11, 0, 16, &txn);
    ASSERT_EQ(MODBUS_ERROR_NONE, err);
    ASSERT_NE(nullptr, txn);
}

TEST_F(MbClientConvenienceTest, ReadCoils_NullClient)
{
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_read_coils(nullptr, 0x11, 0, 1, &txn);
    EXPECT_FALSE(mb_err_is_ok(err));
}

/* -------------------------------------------------------------------------- */
/*                      Read Discrete Inputs (FC02)                           */
/* -------------------------------------------------------------------------- */

TEST_F(MbClientConvenienceTest, ReadDiscreteInputs_Success)
{
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_read_discrete_inputs(&client_, 0x11, 0, 8, &txn);
    ASSERT_EQ(MODBUS_ERROR_NONE, err);
    ASSERT_NE(nullptr, txn);
}

TEST_F(MbClientConvenienceTest, ReadDiscreteInputs_NullClient)
{
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_read_discrete_inputs(nullptr, 0x11, 0, 1, &txn);
    EXPECT_FALSE(mb_err_is_ok(err));
}

/* -------------------------------------------------------------------------- */
/*                      Write Single Coil (FC05)                              */
/* -------------------------------------------------------------------------- */

TEST_F(MbClientConvenienceTest, WriteSingleCoil_Success)
{
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_write_single_coil(&client_, 0x11, 50, true, &txn);
    ASSERT_EQ(MODBUS_ERROR_NONE, err);
    ASSERT_NE(nullptr, txn);
}

TEST_F(MbClientConvenienceTest, WriteSingleCoil_NullClient)
{
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_write_single_coil(nullptr, 0x11, 0, true, &txn);
    EXPECT_FALSE(mb_err_is_ok(err));
}

/* -------------------------------------------------------------------------- */
/*                      Write Single Register (FC06)                          */
/* -------------------------------------------------------------------------- */

TEST_F(MbClientConvenienceTest, WriteSingleRegister_Success)
{
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_write_single_register(&client_, 0x11, 100, 0x1234, &txn);
    ASSERT_EQ(MODBUS_ERROR_NONE, err);
    ASSERT_NE(nullptr, txn);
}

TEST_F(MbClientConvenienceTest, WriteSingleRegister_NullClient)
{
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_write_single_register(nullptr, 0x11, 0, 0, &txn);
    EXPECT_FALSE(mb_err_is_ok(err));
}

/* -------------------------------------------------------------------------- */
/*                      Write Multiple Coils (FC0F)                           */
/* -------------------------------------------------------------------------- */

TEST_F(MbClientConvenienceTest, WriteMultipleCoils_Success)
{
    uint8_t coil_data[] = {0xFF, 0x00}; // 16 coils
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_write_multiple_coils(&client_, 0x11, 0, 16, coil_data, &txn);
    ASSERT_EQ(MODBUS_ERROR_NONE, err);
    ASSERT_NE(nullptr, txn);
}

TEST_F(MbClientConvenienceTest, WriteMultipleCoils_NullClient)
{
    uint8_t coil_data[] = {0xFF};
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_write_multiple_coils(nullptr, 0x11, 0, 8, coil_data, &txn);
    EXPECT_FALSE(mb_err_is_ok(err));
}

TEST_F(MbClientConvenienceTest, WriteMultipleCoils_NullData)
{
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_write_multiple_coils(&client_, 0x11, 0, 8, nullptr, &txn);
    EXPECT_FALSE(mb_err_is_ok(err));
}

/* -------------------------------------------------------------------------- */
/*                    Write Multiple Registers (FC10)                         */
/* -------------------------------------------------------------------------- */

TEST_F(MbClientConvenienceTest, WriteMultipleRegisters_Success)
{
    uint16_t reg_data[] = {0x1234, 0x5678, 0xABCD};
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_write_multiple_registers(&client_, 0x11, 100, 3, reg_data, &txn);
    ASSERT_EQ(MODBUS_ERROR_NONE, err);
    ASSERT_NE(nullptr, txn);
}

TEST_F(MbClientConvenienceTest, WriteMultipleRegisters_NullClient)
{
    uint16_t reg_data[] = {0x1234};
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_write_multiple_registers(nullptr, 0x11, 0, 1, reg_data, &txn);
    EXPECT_FALSE(mb_err_is_ok(err));
}

TEST_F(MbClientConvenienceTest, WriteMultipleRegisters_NullData)
{
    mb_client_txn_t *txn = nullptr;
    mb_err_t err = mb_client_write_multiple_registers(&client_, 0x11, 0, 1, nullptr, &txn);
    EXPECT_FALSE(mb_err_is_ok(err));
}

/* -------------------------------------------------------------------------- */
/*                          Integration Tests                                 */
/* -------------------------------------------------------------------------- */

TEST_F(MbClientConvenienceTest, MultipleTransactions_Sequential)
{
    // Submit multiple transactions
    mb_client_txn_t *txn1 = nullptr;
    mb_client_txn_t *txn2 = nullptr;
    mb_client_txn_t *txn3 = nullptr;
    
    mb_err_t err1 = mb_client_read_holding_registers(&client_, 0x11, 0, 1, &txn1);
    mb_err_t err2 = mb_client_read_input_registers(&client_, 0x11, 100, 2, &txn2);
    mb_err_t err3 = mb_client_write_single_register(&client_, 0x11, 200, 42, &txn3);
    
    EXPECT_EQ(MODBUS_ERROR_NONE, err1);
    EXPECT_EQ(MODBUS_ERROR_NONE, err2);
    EXPECT_EQ(MODBUS_ERROR_NONE, err3);
    
    ASSERT_NE(nullptr, txn1);
    ASSERT_NE(nullptr, txn2);
    ASSERT_NE(nullptr, txn3);
}

} // namespace
