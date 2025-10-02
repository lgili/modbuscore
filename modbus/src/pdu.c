/**
 * @file pdu.c
 */

#include <modbus/pdu.h>

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

    mb_u8 function = (mb_u8)(pdu[0] & (mb_u8)~MB_PDU_EXCEPTION_BIT);
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
