/**
 * @file pdu.c
 */

#include <modbus/pdu.h>

#include <string.h>

static void mb_pdu_write_u16(mb_u8 *out, mb_u16 value)
{
    out[0] = (mb_u8)((value >> 8) & 0xFFU);
    out[1] = (mb_u8)(value & 0xFFU);
}

static mb_u16 mb_pdu_read_u16(const mb_u8 *in)
{
    return (mb_u16)(((mb_u16)in[0] << 8) | (mb_u16)in[1]);
}

static bool mb_pdu_validate_capacity(mb_size_t required, mb_size_t out_cap)
{
    if (required > MB_PDU_MAX) {
        return false;
    }
    return required <= out_cap;
}

static mb_size_t mb_pdu_bits_to_bytes(mb_u16 bit_count)
{
    return (mb_size_t)((bit_count + 7U) / 8U);
}

static void mb_pdu_pack_bits_payload(mb_u8 *dest, const bool *bits, mb_u16 count)
{
    for (mb_u16 i = 0U; i < count; ++i) {
        if (bits[i]) {
            const mb_u16 byte_index = (mb_u16)(i / 8U);
            const mb_u8 bit_index = (mb_u8)(i % 8U);
            dest[byte_index] = (mb_u8)(dest[byte_index] | (mb_u8)(1U << bit_index));
        }
    }
}

static bool mb_pdu_is_valid_coil_encoding(mb_u16 value)
{
    return (value == MB_PDU_COIL_OFF_VALUE) || (value == MB_PDU_COIL_ON_VALUE);
}

mb_err_t mb_pdu_build_read_coils_request(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, mb_u16 quantity)
{
    const mb_size_t required = 5U;

    if (out == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((quantity < MB_PDU_FC01_MIN_COILS) || (quantity > MB_PDU_FC01_MAX_COILS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_READ_COILS;
    mb_pdu_write_u16(&out[1], start_addr);
    mb_pdu_write_u16(&out[3], quantity);

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_read_coils_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_quantity)
{
    if (pdu == NULL || len != 5U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_READ_COILS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u16 quantity = mb_pdu_read_u16(&pdu[3]);
    if ((quantity < MB_PDU_FC01_MIN_COILS) || (quantity > MB_PDU_FC01_MAX_COILS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_addr != NULL) {
        *out_addr = mb_pdu_read_u16(&pdu[1]);
    }

    if (out_quantity != NULL) {
        *out_quantity = quantity;
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_read_coils_response(mb_u8 *out, mb_size_t out_cap, const bool *coils, mb_u16 count)
{
    if (out == NULL || coils == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((count < MB_PDU_FC01_MIN_COILS) || (count > MB_PDU_FC01_MAX_COILS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    const mb_size_t byte_count = mb_pdu_bits_to_bytes(count);
    const mb_size_t required = 2U + byte_count;

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_READ_COILS;
    out[1] = (mb_u8)byte_count;
    memset(&out[2], 0, byte_count);
    mb_pdu_pack_bits_payload(&out[2], coils, count);

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_read_coils_response(const mb_u8 *pdu, mb_size_t len, const mb_u8 **out_payload, mb_u8 *out_byte_count)
{
    if (pdu == NULL || len < 3U || len > MB_PDU_MAX) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_READ_COILS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u8 byte_count = pdu[1];
    const mb_u8 min_bytes = (mb_u8)mb_pdu_bits_to_bytes(MB_PDU_FC01_MIN_COILS);
    const mb_u8 max_bytes = (mb_u8)mb_pdu_bits_to_bytes(MB_PDU_FC01_MAX_COILS);

    if ((byte_count < min_bytes) || (byte_count > max_bytes)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (len != (mb_size_t)(2U + byte_count)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_payload != NULL) {
        *out_payload = &pdu[2];
    }

    if (out_byte_count != NULL) {
        *out_byte_count = byte_count;
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_read_discrete_inputs_request(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, mb_u16 quantity)
{
    const mb_size_t required = 5U;

    if (out == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((quantity < MB_PDU_FC02_MIN_INPUTS) || (quantity > MB_PDU_FC02_MAX_INPUTS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_READ_DISCRETE_INPUTS;
    mb_pdu_write_u16(&out[1], start_addr);
    mb_pdu_write_u16(&out[3], quantity);

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_read_discrete_inputs_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_quantity)
{
    if (pdu == NULL || len != 5U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_READ_DISCRETE_INPUTS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u16 quantity = mb_pdu_read_u16(&pdu[3]);
    if ((quantity < MB_PDU_FC02_MIN_INPUTS) || (quantity > MB_PDU_FC02_MAX_INPUTS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_addr != NULL) {
        *out_addr = mb_pdu_read_u16(&pdu[1]);
    }

    if (out_quantity != NULL) {
        *out_quantity = quantity;
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_read_discrete_inputs_response(mb_u8 *out, mb_size_t out_cap, const bool *inputs, mb_u16 count)
{
    if (out == NULL || inputs == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((count < MB_PDU_FC02_MIN_INPUTS) || (count > MB_PDU_FC02_MAX_INPUTS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    const mb_size_t byte_count = mb_pdu_bits_to_bytes(count);
    const mb_size_t required = 2U + byte_count;

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_READ_DISCRETE_INPUTS;
    out[1] = (mb_u8)byte_count;
    memset(&out[2], 0, byte_count);
    mb_pdu_pack_bits_payload(&out[2], inputs, count);

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_read_discrete_inputs_response(const mb_u8 *pdu, mb_size_t len, const mb_u8 **out_payload, mb_u8 *out_byte_count)
{
    if (pdu == NULL || len < 3U || len > MB_PDU_MAX) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_READ_DISCRETE_INPUTS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u8 byte_count = pdu[1];
    const mb_u8 min_bytes = (mb_u8)mb_pdu_bits_to_bytes(MB_PDU_FC02_MIN_INPUTS);
    const mb_u8 max_bytes = (mb_u8)mb_pdu_bits_to_bytes(MB_PDU_FC02_MAX_INPUTS);

    if ((byte_count < min_bytes) || (byte_count > max_bytes)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (len != (mb_size_t)(2U + byte_count)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_payload != NULL) {
        *out_payload = &pdu[2];
    }

    if (out_byte_count != NULL) {
        *out_byte_count = byte_count;
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_read_holding_request(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, mb_u16 quantity)
{
    const mb_size_t required = 5U;

    if (out == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((quantity < MB_PDU_FC03_MIN_REGISTERS) || (quantity > MB_PDU_FC03_MAX_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_READ_HOLDING_REGISTERS;
    mb_pdu_write_u16(&out[1], start_addr);
    mb_pdu_write_u16(&out[3], quantity);

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_read_holding_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_quantity)
{
    if (pdu == NULL || len != 5U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_READ_HOLDING_REGISTERS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u16 quantity = mb_pdu_read_u16(&pdu[3]);
    if ((quantity < MB_PDU_FC03_MIN_REGISTERS) || (quantity > MB_PDU_FC03_MAX_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_addr != NULL) {
        *out_addr = mb_pdu_read_u16(&pdu[1]);
    }

    if (out_quantity != NULL) {
        *out_quantity = quantity;
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_read_holding_response(mb_u8 *out, mb_size_t out_cap, const mb_u16 *registers, mb_u16 count)
{
    if (out == NULL || registers == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((count < MB_PDU_FC03_MIN_REGISTERS) || (count > MB_PDU_FC03_MAX_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    const mb_size_t byte_count = (mb_size_t)count * 2U;
    const mb_size_t required = 2U + byte_count;

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_READ_HOLDING_REGISTERS;
    out[1] = (mb_u8)byte_count;

    for (mb_u16 i = 0U; i < count; ++i) {
        mb_pdu_write_u16(&out[2U + (i * 2U)], registers[i]);
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_read_holding_response(const mb_u8 *pdu, mb_size_t len, const mb_u8 **out_payload, mb_u16 *out_register_count)
{
    if (pdu == NULL || len < 2U || len > MB_PDU_MAX) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_READ_HOLDING_REGISTERS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u8 byte_count = pdu[1];
    if ((byte_count & 0x01U) != 0U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (len != (mb_size_t)(2U + byte_count)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u16 reg_count = (mb_u16)(byte_count / 2U);
    if ((reg_count < MB_PDU_FC03_MIN_REGISTERS) || (reg_count > MB_PDU_FC03_MAX_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_payload != NULL) {
        *out_payload = &pdu[2];
    }

    if (out_register_count != NULL) {
        *out_register_count = reg_count;
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_read_input_request(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, mb_u16 quantity)
{
    const mb_size_t required = 5U;

    if (out == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((quantity < MB_PDU_FC04_MIN_REGISTERS) || (quantity > MB_PDU_FC04_MAX_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_READ_INPUT_REGISTERS;
    mb_pdu_write_u16(&out[1], start_addr);
    mb_pdu_write_u16(&out[3], quantity);

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_read_input_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_quantity)
{
    if (pdu == NULL || len != 5U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_READ_INPUT_REGISTERS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u16 quantity = mb_pdu_read_u16(&pdu[3]);
    if ((quantity < MB_PDU_FC04_MIN_REGISTERS) || (quantity > MB_PDU_FC04_MAX_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_addr != NULL) {
        *out_addr = mb_pdu_read_u16(&pdu[1]);
    }

    if (out_quantity != NULL) {
        *out_quantity = quantity;
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_read_input_response(mb_u8 *out, mb_size_t out_cap, const mb_u16 *registers, mb_u16 count)
{
    if (out == NULL || registers == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((count < MB_PDU_FC04_MIN_REGISTERS) || (count > MB_PDU_FC04_MAX_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    const mb_size_t byte_count = (mb_size_t)count * 2U;
    const mb_size_t required = 2U + byte_count;

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_READ_INPUT_REGISTERS;
    out[1] = (mb_u8)byte_count;

    for (mb_u16 i = 0U; i < count; ++i) {
        mb_pdu_write_u16(&out[2U + (i * 2U)], registers[i]);
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_read_input_response(const mb_u8 *pdu, mb_size_t len, const mb_u8 **out_payload, mb_u16 *out_register_count)
{
    if (pdu == NULL || len < 2U || len > MB_PDU_MAX) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_READ_INPUT_REGISTERS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u8 byte_count = pdu[1];
    if ((byte_count & 0x01U) != 0U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (len != (mb_size_t)(2U + byte_count)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u16 reg_count = (mb_u16)(byte_count / 2U);
    if ((reg_count < MB_PDU_FC04_MIN_REGISTERS) || (reg_count > MB_PDU_FC04_MAX_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_payload != NULL) {
        *out_payload = &pdu[2];
    }

    if (out_register_count != NULL) {
        *out_register_count = reg_count;
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_write_single_request(mb_u8 *out, mb_size_t out_cap, mb_u16 address, mb_u16 value)
{
    const mb_size_t required = 5U;

    if (out == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_WRITE_SINGLE_REGISTER;
    mb_pdu_write_u16(&out[1], address);
    mb_pdu_write_u16(&out[3], value);

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_write_single_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_address, mb_u16 *out_value)
{
    if (pdu == NULL || len != 5U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_WRITE_SINGLE_REGISTER) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_address != NULL) {
        *out_address = mb_pdu_read_u16(&pdu[1]);
    }

    if (out_value != NULL) {
        *out_value = mb_pdu_read_u16(&pdu[3]);
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_write_single_response(mb_u8 *out, mb_size_t out_cap, mb_u16 address, mb_u16 value)
{
    return mb_pdu_build_write_single_request(out, out_cap, address, value);
}

mb_err_t mb_pdu_parse_write_single_response(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_address, mb_u16 *out_value)
{
    return mb_pdu_parse_write_single_request(pdu, len, out_address, out_value);
}

mb_err_t mb_pdu_build_write_single_coil_request(mb_u8 *out, mb_size_t out_cap, mb_u16 address, bool coil_on)
{
    const mb_size_t required = 5U;

    if (out == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_WRITE_SINGLE_COIL;
    mb_pdu_write_u16(&out[1], address);
    mb_pdu_write_u16(&out[3], coil_on ? MB_PDU_COIL_ON_VALUE : MB_PDU_COIL_OFF_VALUE);

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_write_single_coil_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_address, bool *out_coil_on)
{
    if (pdu == NULL || len != 5U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_WRITE_SINGLE_COIL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u16 raw_value = mb_pdu_read_u16(&pdu[3]);
    if (!mb_pdu_is_valid_coil_encoding(raw_value)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_address != NULL) {
        *out_address = mb_pdu_read_u16(&pdu[1]);
    }

    if (out_coil_on != NULL) {
        *out_coil_on = (raw_value == MB_PDU_COIL_ON_VALUE);
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_write_single_coil_response(mb_u8 *out, mb_size_t out_cap, mb_u16 address, bool coil_on)
{
    return mb_pdu_build_write_single_coil_request(out, out_cap, address, coil_on);
}

mb_err_t mb_pdu_parse_write_single_coil_response(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_address, bool *out_coil_on)
{
    return mb_pdu_parse_write_single_coil_request(pdu, len, out_address, out_coil_on);
}

mb_err_t mb_pdu_build_write_multiple_request(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, const mb_u16 *values, mb_u16 count)
{
    if (out == NULL || values == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((count < MB_PDU_FC16_MIN_REGISTERS) || (count > MB_PDU_FC16_MAX_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    const mb_size_t byte_count = (mb_size_t)count * 2U;
    const mb_size_t required = 6U + byte_count;

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_WRITE_MULTIPLE_REGISTERS;
    mb_pdu_write_u16(&out[1], start_addr);
    mb_pdu_write_u16(&out[3], count);
    out[5] = (mb_u8)byte_count;

    for (mb_u16 i = 0U; i < count; ++i) {
        mb_pdu_write_u16(&out[6U + (i * 2U)], values[i]);
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_write_multiple_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_count, const mb_u8 **out_payload)
{
    if (pdu == NULL || len < 8U || len > MB_PDU_MAX) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_WRITE_MULTIPLE_REGISTERS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u16 quantity = mb_pdu_read_u16(&pdu[3]);
    if ((quantity < MB_PDU_FC16_MIN_REGISTERS) || (quantity > MB_PDU_FC16_MAX_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u8 byte_count = pdu[5];
    if ((byte_count & 0x01U) != 0U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (len != (mb_size_t)(6U + byte_count)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (byte_count != (mb_u8)(quantity * 2U)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_addr != NULL) {
        *out_addr = mb_pdu_read_u16(&pdu[1]);
    }

    if (out_count != NULL) {
        *out_count = quantity;
    }

    if (out_payload != NULL) {
        *out_payload = &pdu[6];
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_write_multiple_response(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, mb_u16 count)
{
    const mb_size_t required = 5U;

    if (out == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((count < MB_PDU_FC16_MIN_REGISTERS) || (count > MB_PDU_FC16_MAX_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_WRITE_MULTIPLE_REGISTERS;
    mb_pdu_write_u16(&out[1], start_addr);
    mb_pdu_write_u16(&out[3], count);

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_write_multiple_response(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_count)
{
    if (pdu == NULL || len != 5U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_WRITE_MULTIPLE_REGISTERS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u16 count = mb_pdu_read_u16(&pdu[3]);
    if ((count < MB_PDU_FC16_MIN_REGISTERS) || (count > MB_PDU_FC16_MAX_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_addr != NULL) {
        *out_addr = mb_pdu_read_u16(&pdu[1]);
    }

    if (out_count != NULL) {
        *out_count = count;
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_write_multiple_coils_request(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, const bool *coils, mb_u16 count)
{
    if (out == NULL || coils == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((count < MB_PDU_FC0F_MIN_COILS) || (count > MB_PDU_FC0F_MAX_COILS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    const mb_size_t byte_count = mb_pdu_bits_to_bytes(count);
    const mb_size_t required = 6U + byte_count;

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_WRITE_MULTIPLE_COILS;
    mb_pdu_write_u16(&out[1], start_addr);
    mb_pdu_write_u16(&out[3], count);
    out[5] = (mb_u8)byte_count;
    memset(&out[6], 0, byte_count);
    mb_pdu_pack_bits_payload(&out[6], coils, count);

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_write_multiple_coils_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_count, mb_u8 *out_byte_count, const mb_u8 **out_payload)
{
    if (pdu == NULL || len < 7U || len > MB_PDU_MAX) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_WRITE_MULTIPLE_COILS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u16 quantity = mb_pdu_read_u16(&pdu[3]);
    if ((quantity < MB_PDU_FC0F_MIN_COILS) || (quantity > MB_PDU_FC0F_MAX_COILS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u8 byte_count = pdu[5];
    mb_u8 expected = (mb_u8)mb_pdu_bits_to_bytes(quantity);
    if ((byte_count == 0U) || (byte_count != expected)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (len != (mb_size_t)(6U + byte_count)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_addr != NULL) {
        *out_addr = mb_pdu_read_u16(&pdu[1]);
    }

    if (out_count != NULL) {
        *out_count = quantity;
    }

    if (out_byte_count != NULL) {
        *out_byte_count = byte_count;
    }

    if (out_payload != NULL) {
        *out_payload = &pdu[6];
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_write_multiple_coils_response(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, mb_u16 count)
{
    const mb_size_t required = 5U;

    if (out == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((count < MB_PDU_FC0F_MIN_COILS) || (count > MB_PDU_FC0F_MAX_COILS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_WRITE_MULTIPLE_COILS;
    mb_pdu_write_u16(&out[1], start_addr);
    mb_pdu_write_u16(&out[3], count);

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_write_multiple_coils_response(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_count)
{
    if (pdu == NULL || len != 5U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_WRITE_MULTIPLE_COILS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u16 count = mb_pdu_read_u16(&pdu[3]);
    if ((count < MB_PDU_FC0F_MIN_COILS) || (count > MB_PDU_FC0F_MAX_COILS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_addr != NULL) {
        *out_addr = mb_pdu_read_u16(&pdu[1]);
    }

    if (out_count != NULL) {
        *out_count = count;
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_read_write_multiple_request(mb_u8 *out, mb_size_t out_cap, mb_u16 read_start_addr, mb_u16 read_quantity, mb_u16 write_start_addr, const mb_u16 *write_values, mb_u16 write_quantity)
{
    if (out == NULL || write_values == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((read_quantity < MB_PDU_FC17_MIN_READ_REGISTERS) || (read_quantity > MB_PDU_FC17_MAX_READ_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((write_quantity < MB_PDU_FC17_MIN_WRITE_REGISTERS) || (write_quantity > MB_PDU_FC17_MAX_WRITE_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    const mb_size_t write_byte_count = (mb_size_t)write_quantity * 2U;
    const mb_size_t required = 10U + write_byte_count;

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_READ_WRITE_MULTIPLE_REGISTERS;
    mb_pdu_write_u16(&out[1], read_start_addr);
    mb_pdu_write_u16(&out[3], read_quantity);
    mb_pdu_write_u16(&out[5], write_start_addr);
    mb_pdu_write_u16(&out[7], write_quantity);
    out[9] = (mb_u8)write_byte_count;

    for (mb_u16 i = 0U; i < write_quantity; ++i) {
        mb_pdu_write_u16(&out[10U + (i * 2U)], write_values[i]);
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_read_write_multiple_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_read_addr, mb_u16 *out_read_quantity, mb_u16 *out_write_addr, mb_u16 *out_write_quantity, const mb_u8 **out_write_payload)
{
    if (pdu == NULL || len < 12U || len > MB_PDU_MAX) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_READ_WRITE_MULTIPLE_REGISTERS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u16 read_quantity = mb_pdu_read_u16(&pdu[3]);
    if ((read_quantity < MB_PDU_FC17_MIN_READ_REGISTERS) || (read_quantity > MB_PDU_FC17_MAX_READ_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u16 write_quantity = mb_pdu_read_u16(&pdu[7]);
    if ((write_quantity < MB_PDU_FC17_MIN_WRITE_REGISTERS) || (write_quantity > MB_PDU_FC17_MAX_WRITE_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u8 byte_count = pdu[9];
    if ((byte_count & 0x01U) != 0U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (byte_count != (mb_u8)(write_quantity * 2U)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (len != (mb_size_t)(10U + byte_count)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_read_addr != NULL) {
        *out_read_addr = mb_pdu_read_u16(&pdu[1]);
    }

    if (out_read_quantity != NULL) {
        *out_read_quantity = read_quantity;
    }

    if (out_write_addr != NULL) {
        *out_write_addr = mb_pdu_read_u16(&pdu[5]);
    }

    if (out_write_quantity != NULL) {
        *out_write_quantity = write_quantity;
    }

    if (out_write_payload != NULL) {
        *out_write_payload = &pdu[10];
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_read_write_multiple_response(mb_u8 *out, mb_size_t out_cap, const mb_u16 *read_registers, mb_u16 read_quantity)
{
    if (out == NULL || read_registers == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((read_quantity < MB_PDU_FC17_MIN_READ_REGISTERS) || (read_quantity > MB_PDU_FC17_MAX_READ_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    const mb_size_t byte_count = (mb_size_t)read_quantity * 2U;
    const mb_size_t required = 2U + byte_count;

    if (!mb_pdu_validate_capacity(required, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = MB_PDU_FC_READ_WRITE_MULTIPLE_REGISTERS;
    out[1] = (mb_u8)byte_count;

    for (mb_u16 i = 0U; i < read_quantity; ++i) {
        mb_pdu_write_u16(&out[2U + (i * 2U)], read_registers[i]);
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_read_write_multiple_response(const mb_u8 *pdu, mb_size_t len, const mb_u8 **out_payload, mb_u16 *out_register_count)
{
    if (pdu == NULL || len < 2U || len > MB_PDU_MAX) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (pdu[0] != MB_PDU_FC_READ_WRITE_MULTIPLE_REGISTERS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u8 byte_count = pdu[1];
    if ((byte_count & 0x01U) != 0U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (len != (mb_size_t)(2U + byte_count)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u16 reg_count = (mb_u16)(byte_count / 2U);
    if ((reg_count < MB_PDU_FC17_MIN_READ_REGISTERS) || (reg_count > MB_PDU_FC17_MAX_READ_REGISTERS)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_payload != NULL) {
        *out_payload = &pdu[2];
    }

    if (out_register_count != NULL) {
        *out_register_count = reg_count;
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_build_exception(mb_u8 *out, mb_size_t out_cap, mb_u8 function, mb_u8 exception_code)
{
    if (out == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((function & MB_PDU_EXCEPTION_BIT) != 0U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (exception_code < MB_EX_ILLEGAL_FUNCTION || exception_code > MB_EX_SERVER_DEVICE_FAILURE) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!mb_pdu_validate_capacity(2U, out_cap)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out[0] = (mb_u8)(function | MB_PDU_EXCEPTION_BIT);
    out[1] = exception_code;
    return MODBUS_ERROR_NONE;
}

mb_err_t mb_pdu_parse_exception(const mb_u8 *pdu, mb_size_t len, mb_u8 *out_function, mb_u8 *out_exception)
{
    if (pdu == NULL || len != 2U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((pdu[0] & MB_PDU_EXCEPTION_BIT) == 0U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u8 function = (mb_u8)(pdu[0] & 0x7FU);
    mb_u8 code = pdu[1];

    if (code < MB_EX_ILLEGAL_FUNCTION || code > MB_EX_SERVER_DEVICE_FAILURE) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (out_function != NULL) {
        *out_function = function;
    }

    if (out_exception != NULL) {
        *out_exception = code;
    }

    return MODBUS_ERROR_NONE;
}
