#include <gtest/gtest.h>

#include <errno.h>
#include <string>

#include <modbus/compat/libmodbus.h>
#include <modbus/compat/modbus-errno.h>

class LibmodbusCompatTest : public ::testing::Test {
protected:
    void TearDown() override {
        if (ctx != nullptr) {
            modbus_free(ctx);
            ctx = nullptr;
        }
    }

    modbus_t *ctx = nullptr;
};

TEST_F(LibmodbusCompatTest, NewTcpContextInitialisesDefaults)
{
    ctx = modbus_new_tcp("127.0.0.1", 1502);
    ASSERT_NE(ctx, nullptr);

    EXPECT_EQ(modbus_get_slave(ctx), 1);
    EXPECT_EQ(modbus_set_slave(ctx, 17), 0);
    EXPECT_EQ(modbus_get_slave(ctx), 17);

    uint32_t sec = 0;
    uint32_t usec = 0;
    EXPECT_EQ(modbus_get_response_timeout(ctx, &sec, &usec), 0);
    EXPECT_EQ(sec, 1U);
    EXPECT_EQ(usec, 0U);

    EXPECT_EQ(modbus_set_response_timeout(ctx, 2, 500000), 0);
    EXPECT_EQ(modbus_get_response_timeout(ctx, &sec, &usec), 0);
    EXPECT_EQ(sec, 2U);
    EXPECT_EQ(usec, 500000U);
}

TEST_F(LibmodbusCompatTest, OperationsWithoutConnectionSetNotConnected)
{
    ctx = modbus_new_tcp("127.0.0.1", 1502);
    ASSERT_NE(ctx, nullptr);

    uint16_t buffer[4] = {0};
    errno = 0;
    modbus_errno = 0;

    EXPECT_EQ(modbus_read_registers(ctx, 0, 4, buffer), -1);
    EXPECT_EQ(errno, ENOTCONN);
    EXPECT_EQ(modbus_errno, ENOTCONN);

    EXPECT_EQ(modbus_write_register(ctx, 0, 1), -1);
    EXPECT_EQ(errno, ENOTCONN);
    EXPECT_EQ(modbus_errno, ENOTCONN);

    EXPECT_EQ(modbus_write_registers(ctx, 0, 4, buffer), -1);
    EXPECT_EQ(errno, ENOTCONN);
    EXPECT_EQ(modbus_errno, ENOTCONN);
}

TEST_F(LibmodbusCompatTest, StrerrorHandlesCompatCodes)
{
    const std::string timeout_msg = modbus_strerror(EMBETIMEDOUT);
    EXPECT_NE(timeout_msg.find("timeout"), std::string::npos);

    const std::string crc_msg = modbus_strerror(EMBBADCRC);
    EXPECT_NE(crc_msg.find("CRC"), std::string::npos);

    const std::string illegal_msg = modbus_strerror(EMBXILFUN);
    EXPECT_NE(illegal_msg.find("Illegal"), std::string::npos);
}
