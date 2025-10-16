#include <algorithm>
#include <array>

#include <gtest/gtest.h>

extern "C" {
#include <modbus/internal/pdu.h>
}

namespace {

TEST(PduFc01, BuildRequestEncodesFields)
{
    mb_u8 buffer[5]{};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_read_coils_request(buffer, sizeof buffer, 0x0013U, 10U));

    const std::array<mb_u8, 5> expected{MB_PDU_FC_READ_COILS, 0x00U, 0x13U, 0x00U, 0x0AU};
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), std::begin(buffer)));
}

TEST(PduFc01, BuildRequestRejectsInvalidQuantity)
{
    mb_u8 buffer[5]{};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_read_coils_request(buffer, sizeof buffer, 0x0000U, 0U));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_read_coils_request(buffer, sizeof buffer, 0x0000U, MB_PDU_FC01_MAX_COILS + 1U));
}

TEST(PduFc01, ParseRequest)
{
    const std::array<mb_u8, 5> frame{MB_PDU_FC_READ_COILS, 0x00U, 0x20U, 0x00U, 0x10U};
    mb_u16 address = 0U;
    mb_u16 quantity = 0U;
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_read_coils_request(frame.data(), frame.size(), &address, &quantity));

    EXPECT_EQ(0x0020U, address);
    EXPECT_EQ(0x0010U, quantity);
}

TEST(PduFc01, BuildResponsePacksBits)
{
    mb_u8 buffer[2 + 2]{};
    const std::array<bool, 10> coils{true, false, true, true, false, false, false, true, true, false};

    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_read_coils_response(buffer, sizeof buffer, coils.data(), static_cast<mb_u16>(coils.size())));

    EXPECT_EQ(MB_PDU_FC_READ_COILS, buffer[0]);
    EXPECT_EQ(2U, buffer[1]);
    EXPECT_EQ(0x8DU, buffer[2]);
    EXPECT_EQ(0x01U, buffer[3]);
}

TEST(PduFc01, ParseResponse)
{
    const std::array<mb_u8, 4> frame{MB_PDU_FC_READ_COILS, 0x02U, 0xCDU, 0x01U};
    const mb_u8 *payload = nullptr;
    mb_u8 byte_count = 0U;

    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_read_coils_response(frame.data(), frame.size(), &payload, &byte_count));

    ASSERT_NE(nullptr, payload);
    EXPECT_EQ(2U, byte_count);
    EXPECT_EQ(frame.data() + 2, payload);
}

TEST(PduFc02, BuildRequestEncodesFields)
{
    mb_u8 buffer[5]{};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_read_discrete_inputs_request(buffer, sizeof buffer, 0x0100U, 16U));

    const std::array<mb_u8, 5> expected{MB_PDU_FC_READ_DISCRETE_INPUTS, 0x01U, 0x00U, 0x00U, 0x10U};
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), std::begin(buffer)));
}

TEST(PduFc02, BuildResponsePacksBits)
{
    mb_u8 buffer[2 + 1]{};
    const std::array<bool, 8> inputs{true, true, false, false, true, false, true, false};

    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_read_discrete_inputs_response(buffer, sizeof buffer, inputs.data(), static_cast<mb_u16>(inputs.size())));

    EXPECT_EQ(MB_PDU_FC_READ_DISCRETE_INPUTS, buffer[0]);
    EXPECT_EQ(1U, buffer[1]);
    EXPECT_EQ(0x53U, buffer[2]);
}

TEST(PduFc02, ParseResponseRejectsLengthMismatch)
{
    const std::array<mb_u8, 3> frame{MB_PDU_FC_READ_DISCRETE_INPUTS, 0x02U, 0xAAU};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_read_discrete_inputs_response(frame.data(), frame.size(), nullptr, nullptr));
}

TEST(PduFc04, BuildRequestEncodesFields)
{
    mb_u8 buffer[5]{};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_read_input_request(buffer, sizeof buffer, 0x0001U, 4U));

    const std::array<mb_u8, 5> expected{MB_PDU_FC_READ_INPUT_REGISTERS, 0x00U, 0x01U, 0x00U, 0x04U};
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), std::begin(buffer)));
}

TEST(PduFc04, ParseRequest)
{
    const std::array<mb_u8, 5> frame{MB_PDU_FC_READ_INPUT_REGISTERS, 0x00U, 0x10U, 0x00U, 0x02U};
    mb_u16 address = 0U;
    mb_u16 quantity = 0U;
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_read_input_request(frame.data(), frame.size(), &address, &quantity));

    EXPECT_EQ(0x0010U, address);
    EXPECT_EQ(0x0002U, quantity);
}

TEST(PduFc04, BuildResponse)
{
    mb_u8 buffer[2 + 4]{};
    const std::array<mb_u16, 2> regs{0x1111U, 0x2222U};

    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_read_input_response(buffer, sizeof buffer, regs.data(), static_cast<mb_u16>(regs.size())));

    EXPECT_EQ(MB_PDU_FC_READ_INPUT_REGISTERS, buffer[0]);
    EXPECT_EQ(4U, buffer[1]);
    EXPECT_EQ(0x11U, buffer[2]);
    EXPECT_EQ(0x11U, buffer[3]);
    EXPECT_EQ(0x22U, buffer[4]);
    EXPECT_EQ(0x22U, buffer[5]);
}

TEST(PduFc04, ParseResponse)
{
    const std::array<mb_u8, 6> frame{MB_PDU_FC_READ_INPUT_REGISTERS, 0x04U, 0x12U, 0x34U, 0x56U, 0x78U};
    const mb_u8 *payload = nullptr;
    mb_u16 count = 0U;

    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_read_input_response(frame.data(), frame.size(), &payload, &count));

    ASSERT_NE(nullptr, payload);
    EXPECT_EQ(2U, count);
    EXPECT_EQ(frame.data() + 2, payload);
}

TEST(PduFc05, BuildAndParse)
{
    mb_u8 buffer[5]{};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_write_single_coil_request(buffer, sizeof buffer, 0x0005U, true));

    mb_u16 address = 0U;
    bool state = false;
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_write_single_coil_request(buffer, sizeof buffer, &address, &state));

    EXPECT_EQ(0x0005U, address);
    EXPECT_TRUE(state);

    state = false;
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_write_single_coil_response(buffer, sizeof buffer, &address, &state));
    EXPECT_TRUE(state);
}

TEST(PduFc05, ParseRejectsInvalidValue)
{
    const std::array<mb_u8, 5> frame{MB_PDU_FC_WRITE_SINGLE_COIL, 0x00U, 0x01U, 0x12U, 0x34U};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_write_single_coil_request(frame.data(), frame.size(), nullptr, nullptr));
}

TEST(PduFc07, BuildAndParse)
{
    mb_u8 request[1]{};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_read_exception_status_request(request, sizeof request));

    EXPECT_EQ(MB_PDU_FC_READ_EXCEPTION_STATUS, request[0]);
    EXPECT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_read_exception_status_request(request, sizeof request));

    mb_u8 response[2]{};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_read_exception_status_response(response, sizeof response, 0xAAU));

    mb_u8 status = 0U;
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_read_exception_status_response(response, sizeof response, &status));
    EXPECT_EQ(0xAAU, status);
}

TEST(PduFc07, ParseRequestRejectsWrongLength)
{
    const std::array<mb_u8, 2> frame{MB_PDU_FC_READ_EXCEPTION_STATUS, 0x00U};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_read_exception_status_request(frame.data(), frame.size()));
}

TEST(PduFc11, BuildAndParse)
{
    mb_u8 request[1]{};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_report_server_id_request(request, sizeof request));
    EXPECT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_report_server_id_request(request, sizeof request));

    const std::array<mb_u8, 3> payload{0x42U, 0x10U, 0x01U};
    mb_u8 response[2 + payload.size()]{};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_report_server_id_response(response, sizeof response, payload.data(), payload.size()));

    const mb_u8 *parsed_payload = nullptr;
    mb_u8 byte_count = 0U;
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_report_server_id_response(response, sizeof response, &parsed_payload, &byte_count));

    ASSERT_NE(nullptr, parsed_payload);
    EXPECT_EQ(payload.size(), static_cast<size_t>(byte_count));
    EXPECT_TRUE(std::equal(payload.begin(), payload.end(), parsed_payload));
}

TEST(PduFc11, ParseResponseRejectsLengthMismatch)
{
    const std::array<mb_u8, 4> frame{MB_PDU_FC_REPORT_SERVER_ID, 0x03U, 0xDEU, 0xADU};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_report_server_id_response(frame.data(), frame.size(), nullptr, nullptr));
}

TEST(PduFc0F, BuildRequest)
{
    mb_u8 buffer[6 + 2]{};
    const std::array<bool, 10> coils{true, false, true, false, true, false, false, true, true, false};

    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_write_multiple_coils_request(buffer, sizeof buffer, 0x0100U, coils.data(), static_cast<mb_u16>(coils.size())));

    EXPECT_EQ(MB_PDU_FC_WRITE_MULTIPLE_COILS, buffer[0]);
    EXPECT_EQ(0x01U, buffer[1]);
    EXPECT_EQ(0x00U, buffer[2]);
    EXPECT_EQ(0x00U, buffer[3]);
    EXPECT_EQ(coils.size(), static_cast<size_t>(buffer[4]));
    EXPECT_EQ(2U, buffer[5]);
    EXPECT_EQ(0x95U, buffer[6]);
    EXPECT_EQ(0x01U, buffer[7]);
}

TEST(PduFc0F, ParseRequest)
{
    const std::array<mb_u8, 7> frame{MB_PDU_FC_WRITE_MULTIPLE_COILS, 0x00U, 0x64U, 0x00U, 0x08U, 0x01U, 0xAAU};
    mb_u16 addr = 0U;
    mb_u16 count = 0U;
    mb_u8 byte_count = 0U;
    const mb_u8 *payload = nullptr;

    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_write_multiple_coils_request(frame.data(), frame.size(), &addr, &count, &byte_count, &payload));

    EXPECT_EQ(0x0064U, addr);
    EXPECT_EQ(8U, count);
    EXPECT_EQ(1U, byte_count);
    ASSERT_NE(nullptr, payload);
    EXPECT_EQ(frame.data() + 6, payload);
}

TEST(PduFc0F, ParseRequestRejectsByteCountMismatch)
{
    const std::array<mb_u8, 7> frame{MB_PDU_FC_WRITE_MULTIPLE_COILS, 0x00U, 0x01U, 0x00U, 0x09U, 0x01U, 0xFFU};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_write_multiple_coils_request(frame.data(), frame.size(), nullptr, nullptr, nullptr, nullptr));
}

TEST(PduFc0F, BuildAndParseResponse)
{
    mb_u8 buffer[5]{};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_write_multiple_coils_response(buffer, sizeof buffer, 0x0002U, 8U));

    mb_u16 addr = 0U;
    mb_u16 count = 0U;
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_write_multiple_coils_response(buffer, sizeof buffer, &addr, &count));

    EXPECT_EQ(0x0002U, addr);
    EXPECT_EQ(8U, count);
}

TEST(PduFc16Mask, BuildAndParse)
{
    mb_u8 buffer[7]{};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_mask_write_register_request(buffer, sizeof buffer, 0x1234U, 0x0F0FU, 0xF0F0U));

    mb_u16 address = 0U;
    mb_u16 and_mask = 0U;
    mb_u16 or_mask = 0U;
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_mask_write_register_request(buffer, sizeof buffer, &address, &and_mask, &or_mask));

    EXPECT_EQ(0x1234U, address);
    EXPECT_EQ(0x0F0FU, and_mask);
    EXPECT_EQ(0xF0F0U, or_mask);

    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_mask_write_register_response(buffer, sizeof buffer, 0x1234U, 0x0F0FU, 0xF0F0U));
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_mask_write_register_response(buffer, sizeof buffer, &address, &and_mask, &or_mask));

    EXPECT_EQ(0x1234U, address);
    EXPECT_EQ(0x0F0FU, and_mask);
    EXPECT_EQ(0xF0F0U, or_mask);
}

TEST(PduFc16Mask, ParseRejectsInvalidLength)
{
    const std::array<mb_u8, 6> frame{MB_PDU_FC_MASK_WRITE_REGISTER, 0x00U, 0x10U, 0xFFU, 0x00U, 0xAAU};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_mask_write_register_request(frame.data(), frame.size(), nullptr, nullptr, nullptr));
}

TEST(PduFc17, BuildRequest)
{
    mb_u8 buffer[10 + 4]{};
    const std::array<mb_u16, 2> write_regs{0xAAAAU, 0x5555U};

    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_read_write_multiple_request(buffer, sizeof buffer,
                                                        0x0010U, 2U,
                                                        0x0020U, write_regs.data(), static_cast<mb_u16>(write_regs.size())));

    EXPECT_EQ(MB_PDU_FC_READ_WRITE_MULTIPLE_REGISTERS, buffer[0]);
    EXPECT_EQ(0x00U, buffer[1]);
    EXPECT_EQ(0x10U, buffer[2]);
    EXPECT_EQ(0x00U, buffer[3]);
    EXPECT_EQ(0x02U, buffer[4]);
    EXPECT_EQ(0x00U, buffer[5]);
    EXPECT_EQ(0x20U, buffer[6]);
    EXPECT_EQ(0x00U, buffer[7]);
    EXPECT_EQ(write_regs.size(), static_cast<size_t>(buffer[8]));
    EXPECT_EQ(4U, buffer[9]);
}

TEST(PduFc17, ParseRequest)
{
    const std::array<mb_u8, 14> frame{
        MB_PDU_FC_READ_WRITE_MULTIPLE_REGISTERS,
        0x00U, 0x08U,
        0x00U, 0x02U,
        0x00U, 0x20U,
        0x00U, 0x02U,
        0x04U,
        0x12U, 0x34U,
        0x56U, 0x78U};

    mb_u16 read_addr = 0U;
    mb_u16 read_qty = 0U;
    mb_u16 write_addr = 0U;
    mb_u16 write_qty = 0U;
    const mb_u8 *payload = nullptr;

    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_read_write_multiple_request(frame.data(), frame.size(),
                                                        &read_addr, &read_qty,
                                                        &write_addr, &write_qty,
                                                        &payload));

    EXPECT_EQ(0x0008U, read_addr);
    EXPECT_EQ(2U, read_qty);
    EXPECT_EQ(0x0020U, write_addr);
    EXPECT_EQ(2U, write_qty);
    ASSERT_NE(nullptr, payload);
    EXPECT_EQ(frame.data() + 10, payload);
}

TEST(PduFc17, BuildResponse)
{
    mb_u8 buffer[2 + 4]{};
    const std::array<mb_u16, 2> read_regs{0x0F0FU, 0xF0F0U};

    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_read_write_multiple_response(buffer, sizeof buffer, read_regs.data(), static_cast<mb_u16>(read_regs.size())));

    EXPECT_EQ(MB_PDU_FC_READ_WRITE_MULTIPLE_REGISTERS, buffer[0]);
    EXPECT_EQ(4U, buffer[1]);
}

TEST(PduFc17, ParseResponse)
{
    const std::array<mb_u8, 6> frame{MB_PDU_FC_READ_WRITE_MULTIPLE_REGISTERS, 0x04U, 0xAAU, 0xBBU, 0xCCU, 0xDDU};
    const mb_u8 *payload = nullptr;
    mb_u16 count = 0U;

    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_read_write_multiple_response(frame.data(), frame.size(), &payload, &count));

    ASSERT_NE(nullptr, payload);
    EXPECT_EQ(2U, count);
    EXPECT_EQ(frame.data() + 2, payload);
}


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
              mb_pdu_build_read_holding_response(buffer, sizeof buffer, registers.data(), static_cast<mb_u16>(registers.size())));

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
              mb_pdu_build_read_holding_response(buffer, sizeof buffer, registers.data(), static_cast<mb_u16>(registers.size())));
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
              mb_pdu_build_write_multiple_request(buffer, sizeof buffer, 0x1234U, regs.data(), static_cast<mb_u16>(regs.size())));

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
              mb_pdu_build_write_multiple_request(buffer, sizeof buffer, 0x0000U, regs.data(), static_cast<mb_u16>(regs.size())));
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

TEST(PduException, BuildAndParse)
{
    mb_u8 buffer[2]{};
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_build_exception(buffer, sizeof buffer, MB_PDU_FC_READ_HOLDING_REGISTERS, MB_EX_ILLEGAL_DATA_VALUE));

    EXPECT_EQ((mb_u8)(MB_PDU_FC_READ_HOLDING_REGISTERS | MB_PDU_EXCEPTION_BIT), buffer[0]);
    EXPECT_EQ(MB_EX_ILLEGAL_DATA_VALUE, buffer[1]);

    mb_u8 function = 0U;
    mb_u8 code = 0U;
    ASSERT_EQ(MODBUS_ERROR_NONE,
              mb_pdu_parse_exception(buffer, sizeof buffer, &function, &code));

    EXPECT_EQ(MB_PDU_FC_READ_HOLDING_REGISTERS, function);
    EXPECT_EQ(MB_EX_ILLEGAL_DATA_VALUE, code);
}

TEST(PduException, RejectsInvalidInputs)
{
    mb_u8 buffer[2]{};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_exception(nullptr, sizeof buffer, MB_PDU_FC_READ_HOLDING_REGISTERS, MB_EX_ILLEGAL_FUNCTION));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_exception(buffer, 1U, MB_PDU_FC_READ_HOLDING_REGISTERS, MB_EX_ILLEGAL_FUNCTION));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_exception(buffer, sizeof buffer, (mb_u8)(MB_PDU_FC_READ_HOLDING_REGISTERS | MB_PDU_EXCEPTION_BIT), MB_EX_ILLEGAL_FUNCTION));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_build_exception(buffer, sizeof buffer, MB_PDU_FC_READ_HOLDING_REGISTERS, 0U));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_exception(nullptr, 2U, nullptr, nullptr));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_exception(buffer, 1U, nullptr, nullptr));
    buffer[0] = MB_PDU_FC_READ_HOLDING_REGISTERS;
    buffer[1] = MB_EX_ILLEGAL_FUNCTION;
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_exception(buffer, sizeof buffer, nullptr, nullptr));

    buffer[0] = (mb_u8)(MB_PDU_FC_READ_HOLDING_REGISTERS | MB_PDU_EXCEPTION_BIT);
    buffer[1] = 0x55U;
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_pdu_parse_exception(buffer, sizeof buffer, nullptr, nullptr));
}

} // namespace
