#include <gtest/gtest.h>

extern "C" {
#include <modbus/mb_err.h>
}

TEST(MbErr, DetectsExceptions)
{
    EXPECT_TRUE(mb_err_is_exception(MODBUS_EXCEPTION_ILLEGAL_FUNCTION));
    EXPECT_TRUE(mb_err_is_exception(MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE));
    EXPECT_FALSE(mb_err_is_exception(MODBUS_ERROR_INVALID_ARGUMENT));
    EXPECT_FALSE(mb_err_is_exception(MODBUS_ERROR_NONE));
}

TEST(MbErr, ProvidesDescriptiveStrings)
{
    EXPECT_STREQ("OK", mb_err_str(MODBUS_ERROR_NONE));
    EXPECT_STREQ("Invalid argument", mb_err_str(MODBUS_ERROR_INVALID_ARGUMENT));
    EXPECT_STREQ("Timeout", mb_err_str(MODBUS_ERROR_TIMEOUT));
    EXPECT_STREQ("Transport error", mb_err_str(MODBUS_ERROR_TRANSPORT));
    EXPECT_STREQ("CRC error", mb_err_str(MODBUS_ERROR_CRC));
    EXPECT_STREQ("Illegal function", mb_err_str(MODBUS_EXCEPTION_ILLEGAL_FUNCTION));
    EXPECT_STREQ("Illegal data address", mb_err_str(MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS));
    EXPECT_STREQ("Illegal data value", mb_err_str(MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE));
    EXPECT_STREQ("Server device failure", mb_err_str(MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE));
}

TEST(MbErr, UnknownCodesFallBack)
{
    EXPECT_STREQ("Unknown error", mb_err_str((mb_err_t)-999));
    EXPECT_STREQ("Unknown error", mb_err_str((mb_err_t)1234));
}
