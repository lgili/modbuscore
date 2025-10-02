#include <algorithm>
#include <array>

#include <gtest/gtest.h>

extern "C" {
#include <modbus/pdu.h>
}

namespace {

TEST(PduFc03, BuildRequestEncodesFields)
{
    mb_u8 buffer[5]{};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_read_holding_request(buffer, sizeof buffer, 0x1234U, 10U));

    const std::array<mb_u8, 5> expected{MB_PDU_FC_READ_HOLDING_REGISTERS, 0x12U, 0x34U, 0x00U, 0x0AU};
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), std::begin(buffer)));
}

TEST(PduFc03, BuildRequestRejectsInvalidQuantity)
{
    mb_u8 buffer[5]{};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_read_holding_request(buffer, sizeof buffer, 0x0000U, 0U));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_read_holding_request(buffer, sizeof buffer, 0x0000U, MB_PDU_FC03_MAX_REGISTERS + 1U));
}

TEST(PduFc03, BuildRequestRejectsNullBuffer)
{
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_read_holding_request(nullptr, 5U, 0x0000U, 1U));
}

TEST(PduFc03, BuildRequestRejectsSmallBuffer)
{
    mb_u8 buffer[4]{};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_read_holding_request(buffer, sizeof buffer, 0x0000U, 1U));
}

TEST(PduFc03, ParseRequest)
{
    const std::array<mb_u8, 5> frame{MB_PDU_FC_READ_HOLDING_REGISTERS, 0x00U, 0x08U, 0x00U, 0x7DU};
    mb_u16 address = 0U;
    mb_u16 quantity = 0U;
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_read_holding_request(frame.data(), frame.size(), &address, &quantity));

    EXPECT_EQ(0x0008U, address);
    EXPECT_EQ(0x007DU, quantity);
}

TEST(PduFc03, ParseRequestRejectsBadLength)
{
    const std::array<mb_u8, 4> frame{MB_PDU_FC_READ_HOLDING_REGISTERS, 0x00U, 0x08U, 0x00U};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_read_holding_request(frame.data(), frame.size(), nullptr, nullptr));
}

TEST(PduFc03, ParseRequestRejectsWrongFunction)
{
    const std::array<mb_u8, 5> frame{0x04U, 0x00U, 0x08U, 0x00U, 0x01U};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_read_holding_request(frame.data(), frame.size(), nullptr, nullptr));
}

TEST(PduFc03, ParseRequestRejectsQuantityOutOfRange)
{
    std::array<mb_u8, 5> frame{MB_PDU_FC_READ_HOLDING_REGISTERS, 0x00U, 0x08U, 0x00U, 0x00U};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_read_holding_request(frame.data(), frame.size(), nullptr, nullptr));

    frame[3] = static_cast<mb_u8>((MB_PDU_FC03_MAX_REGISTERS + 1U) >> 8);
    frame[4] = static_cast<mb_u8>((MB_PDU_FC03_MAX_REGISTERS + 1U) & 0xFFU);
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_read_holding_request(frame.data(), frame.size(), nullptr, nullptr));
}

TEST(PduFc03, BuildResponse)
{
    mb_u8 buffer[2 + 10]{};
    const std::array<mb_u16, 5> registers{0x1111U, 0x2222U, 0x3333U, 0x4444U, 0x5555U};

    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_read_holding_response(buffer, sizeof buffer, registers.data(), registers.size()));

    EXPECT_EQ(MB_PDU_FC_READ_HOLDING_REGISTERS, buffer[0]);
    EXPECT_EQ(registers.size() * 2U, buffer[1]);

    for (size_t i = 0; i < registers.size(); ++i) {
        const mb_u8 hi = buffer[2 + i * 2];
        const mb_u8 lo = buffer[2 + i * 2 + 1];
        const mb_u16 value = static_cast<mb_u16>((static_cast<mb_u16>(hi) << 8) | lo);
        EXPECT_EQ(registers[i], value);
    }
}

TEST(PduFc03, BuildResponseRejectsInvalidInputs)
{
    mb_u8 buffer[2 + MB_PDU_FC03_MAX_REGISTERS * 2U]{};
    std::array<mb_u16, MB_PDU_FC03_MAX_REGISTERS> registers{};

    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_read_holding_response(nullptr, sizeof buffer, registers.data(), 1U));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_read_holding_response(buffer, sizeof buffer, nullptr, 1U));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_read_holding_response(buffer, sizeof buffer, registers.data(), 0U));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_read_holding_response(buffer, sizeof buffer, registers.data(), MB_PDU_FC03_MAX_REGISTERS + 1U));
}

TEST(PduFc03, BuildResponseRejectsSmallBuffer)
{
    mb_u8 buffer[2]{};
    const std::array<mb_u16, 1> registers{0xAAAAU};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_read_holding_response(buffer, sizeof buffer, registers.data(), registers.size()));
}

TEST(PduFc03, ParseResponse)
{
    const std::array<mb_u8, 12> frame{
        MB_PDU_FC_READ_HOLDING_REGISTERS, 0x0AU,
        0x12U, 0x34U,
        0x56U, 0x78U,
        0x9AU, 0xBCU,
        0xDEU, 0xF0U,
        0x11U, 0x22U};

    const mb_u8 *payload = nullptr;
    mb_u16 registers = 0U;
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_read_holding_response(frame.data(), frame.size(), &payload, &registers));

    ASSERT_NE(nullptr, payload);
    EXPECT_EQ(5U, registers);
    EXPECT_EQ(frame.data() + 2, payload);
}

TEST(PduFc03, ParseResponseRejectsOddByteCount)
{
    const std::array<mb_u8, 5> frame{MB_PDU_FC_READ_HOLDING_REGISTERS, 0x03U, 0xAAU, 0xBBU, 0xCCU};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_read_holding_response(frame.data(), frame.size(), nullptr, nullptr));
}

TEST(PduFc03, ParseResponseRejectsWrongFunction)
{
    const std::array<mb_u8, 4> frame{0x81U, 0x02U, 0x00U, 0x00U};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_read_holding_response(frame.data(), frame.size(), nullptr, nullptr));
}

TEST(PduFc03, ParseResponseRejectsLengthMismatch)
{
    const std::array<mb_u8, 5> frame{MB_PDU_FC_READ_HOLDING_REGISTERS, 0x02U, 0xAAU, 0xBBU, 0xCCU};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_read_holding_response(frame.data(), frame.size(), nullptr, nullptr));
}

TEST(PduFc03, ParseResponseRejectsZeroRegisters)
{
    const std::array<mb_u8, 2> frame{MB_PDU_FC_READ_HOLDING_REGISTERS, 0x00U};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_read_holding_response(frame.data(), frame.size(), nullptr, nullptr));
}

TEST(PduFc06, BuildAndParse)
{
    mb_u8 buffer[5]{};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_write_single_request(buffer, sizeof buffer, 0x00FFU, 0xABCDU));

    mb_u16 address = 0U;
    mb_u16 value = 0U;
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_write_single_request(buffer, sizeof buffer, &address, &value));

    EXPECT_EQ(0x00FFU, address);
    EXPECT_EQ(0xABCDU, value);
}

TEST(PduFc06, RequestRejectsNullBuffer)
{
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_write_single_request(nullptr, 5U, 0x0000U, 0x0000U));
}

TEST(PduFc06, RequestRejectsSmallBuffer)
{
    mb_u8 buffer[4]{};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_write_single_request(buffer, sizeof buffer, 0x0000U, 0x0000U));
}

TEST(PduFc06, ParseRequestRejectsWrongFunction)
{
    const std::array<mb_u8, 5> frame{0x05U, 0x00U, 0x01U, 0x00U, 0x02U};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_write_single_request(frame.data(), frame.size(), nullptr, nullptr));
}

TEST(PduFc16, BuildRequest)
{
    mb_u8 buffer[6 + 6]{};
    const std::array<mb_u16, 3> regs{0x0102U, 0x0304U, 0x0506U};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_write_multiple_request(buffer, sizeof buffer, 0x1234U, regs.data(), regs.size()));

    EXPECT_EQ(MB_PDU_FC_WRITE_MULTIPLE_REGISTERS, buffer[0]);
    EXPECT_EQ(0x12U, buffer[1]);
    EXPECT_EQ(0x34U, buffer[2]);
    EXPECT_EQ(0x00U, buffer[3]);
    EXPECT_EQ(regs.size(), static_cast<size_t>(buffer[4]));
    EXPECT_EQ(regs.size() * 2U, static_cast<size_t>(buffer[5]));

    for (size_t i = 0; i < regs.size(); ++i) {
        const size_t offset = 6U + i * 2U;
        const mb_u16 value = static_cast<mb_u16>((static_cast<mb_u16>(buffer[offset]) << 8) | buffer[offset + 1U]);
        EXPECT_EQ(regs[i], value);
    }
}

TEST(PduFc16, BuildRequestRejectsInvalidCount)
{
    mb_u8 buffer[16]{};
    const mb_u16 values[1]{0x0000U};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_write_multiple_request(buffer, sizeof buffer, 0x0000U, values, 0U));
}

TEST(PduFc16, BuildRequestRejectsNullValues)
{
    mb_u8 buffer[16]{};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_write_multiple_request(buffer, sizeof buffer, 0x0000U, nullptr, 1U));
}

TEST(PduFc16, BuildRequestRejectsLargeCount)
{
    mb_u8 buffer[300]{};
    std::array<mb_u16, MB_PDU_FC16_MAX_REGISTERS + 1U> regs{};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_write_multiple_request(buffer, sizeof buffer, 0x0000U, regs.data(), MB_PDU_FC16_MAX_REGISTERS + 1U));
}

TEST(PduFc16, BuildRequestRejectsSmallBuffer)
{
    mb_u8 buffer[4]{};
    const std::array<mb_u16, 1> regs{0xAAAAU};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_write_multiple_request(buffer, sizeof buffer, 0x0000U, regs.data(), regs.size()));
}

TEST(PduFc16, ParseRequest)
{
    std::array<mb_u8, 12> frame{
        MB_PDU_FC_WRITE_MULTIPLE_REGISTERS, 0x00U, 0x10U,
        0x00U, 0x03U,
        0x06U,
        0xAAU, 0xBBU,
        0xCCU, 0xDDU,
        0xEEU, 0xFFU};

    mb_u16 addr = 0U;
    mb_u16 count = 0U;
    const mb_u8 *payload = nullptr;
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_write_multiple_request(frame.data(), frame.size(), &addr, &count, &payload));

    EXPECT_EQ(0x0010U, addr);
    EXPECT_EQ(3U, count);
    ASSERT_NE(nullptr, payload);
    EXPECT_EQ(frame.data() + 6, payload);
}

TEST(PduFc16, ParseRequestRejectsMismatchedLength)
{
    std::array<mb_u8, 11> frame{
        MB_PDU_FC_WRITE_MULTIPLE_REGISTERS, 0x00U, 0x10U,
        0x00U, 0x02U,
        0x05U,
        0xAAU, 0xBBU,
        0xCCU, 0xDDU,
        0xEEU};

    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_write_multiple_request(frame.data(), frame.size(), nullptr, nullptr, nullptr));
}

TEST(PduFc16, ParseRequestRejectsBadQuantity)
{
    std::array<mb_u8, 10> frame{
        MB_PDU_FC_WRITE_MULTIPLE_REGISTERS, 0x00U, 0x10U,
        0x00U, 0x00U,
        0x02U,
        0xAAU, 0xBBU,
        0xCCU, 0xDDU};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_write_multiple_request(frame.data(), frame.size(), nullptr, nullptr, nullptr));
}

TEST(PduFc16, ParseRequestRejectsOddByteCount)
{
    std::array<mb_u8, 11> frame{
        MB_PDU_FC_WRITE_MULTIPLE_REGISTERS, 0x00U, 0x10U,
        0x00U, 0x02U,
        0x05U,
        0xAAU, 0xBBU,
        0xCCU, 0xDDU,
        0xEEU};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_write_multiple_request(frame.data(), frame.size(), nullptr, nullptr, nullptr));
}

TEST(PduFc16, ParseRequestRejectsByteCountMismatch)
{
    std::array<mb_u8, 12> frame{
        MB_PDU_FC_WRITE_MULTIPLE_REGISTERS, 0x00U, 0x10U,
        0x00U, 0x03U,
        0x06U,
        0xAAU, 0xBBU,
        0xCCU, 0xDDU,
        0xEEU, 0xFFU};
    frame[5] = 0x04U; // mismatched with quantity * 2
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_write_multiple_request(frame.data(), frame.size(), nullptr, nullptr, nullptr));
}

TEST(PduFc16, ParseRequestRejectsWrongFunction)
{
    std::array<mb_u8, 9> frame{0x11U, 0x00U, 0x10U, 0x00U, 0x01U, 0x02U, 0xAAU, 0xBBU, 0x00U};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_write_multiple_request(frame.data(), frame.size(), nullptr, nullptr, nullptr));
}

TEST(PduFc16, BuildAndParseResponse)
{
    mb_u8 buffer[5]{};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_write_multiple_response(buffer, sizeof buffer, 0x0010U, 2U));

    mb_u16 addr = 0U;
    mb_u16 count = 0U;
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_write_multiple_response(buffer, sizeof buffer, &addr, &count));

    EXPECT_EQ(0x0010U, addr);
    EXPECT_EQ(2U, count);
}

TEST(PduFc16, BuildResponseRejectsInvalidInputs)
{
    mb_u8 buffer[5]{};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_write_multiple_response(nullptr, sizeof buffer, 0x0000U, 1U));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_write_multiple_response(buffer, sizeof buffer, 0x0000U, 0U));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_write_multiple_response(buffer, 4U, 0x0000U, 1U));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_write_multiple_response(buffer, sizeof buffer, 0x0000U, MB_PDU_FC16_MAX_REGISTERS + 1U));
}

TEST(PduFc16, ParseResponseRejectsInvalidInputs)
{
    const std::array<mb_u8, 5> frame{MB_PDU_FC_WRITE_MULTIPLE_REGISTERS, 0x00U, 0x10U, 0x00U, 0x02U};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_write_multiple_response(nullptr, frame.size(), nullptr, nullptr));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_write_multiple_response(frame.data(), 4U, nullptr, nullptr));
}

TEST(PduFc16, ParseResponseRejectsWrongFunction)
{
    const std::array<mb_u8, 5> frame{0x11U, 0x00U, 0x10U, 0x00U, 0x01U};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_write_multiple_response(frame.data(), frame.size(), nullptr, nullptr));
}

TEST(PduFc16, ParseResponseRejectsCountOutOfRange)
{
    std::array<mb_u8, 5> frame{MB_PDU_FC_WRITE_MULTIPLE_REGISTERS, 0x00U, 0x10U, 0x00U, 0x00U};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_write_multiple_response(frame.data(), frame.size(), nullptr, nullptr));

    frame[3] = static_cast<mb_u8>((MB_PDU_FC16_MAX_REGISTERS + 1U) >> 8);
    frame[4] = static_cast<mb_u8>((MB_PDU_FC16_MAX_REGISTERS + 1U) & 0xFFU);
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_write_multiple_response(frame.data(), frame.size(), nullptr, nullptr));
}

} // namespace
