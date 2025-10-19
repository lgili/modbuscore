/**
 * @file pdu.c
 * @brief Implementation of Modbus PDU encoding/decoding utilities.
 */

#include <modbuscore/protocol/pdu.h>
#include <string.h>

mbc_status_t mbc_pdu_encode(const mbc_pdu_t* pdu, uint8_t* buffer, size_t capacity,
                            size_t* out_length)
{
    if (!pdu || !buffer) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (pdu->payload_length > MBC_PDU_MAX) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    const size_t total = 2U + pdu->payload_length;
    if (capacity < total) {
        return MBC_STATUS_NO_RESOURCES;
    }

    buffer[0] = pdu->unit_id;
    buffer[1] = pdu->function;
    if (pdu->payload_length > 0U) {
        memcpy(&buffer[2], pdu->payload, pdu->payload_length);
    }

    if (out_length) {
        *out_length = total;
    }

    return MBC_STATUS_OK;
}

mbc_status_t mbc_pdu_decode(const uint8_t* buffer, size_t length, mbc_pdu_t* out)
{
    if (!buffer || !out || length < 2U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    const size_t payload_len = length - 2U;
    if (payload_len > MBC_PDU_MAX) {
        return MBC_STATUS_DECODING_ERROR;
    }

    out->unit_id = buffer[0];
    out->function = buffer[1];
    out->payload_length = payload_len;
    if (payload_len > 0U) {
        memcpy(out->payload, &buffer[2], payload_len);
    }

    return MBC_STATUS_OK;
}

mbc_status_t mbc_pdu_build_read_holding_request(mbc_pdu_t* pdu, uint8_t unit_id, uint16_t address,
                                                uint16_t quantity)
{
    if (!pdu || quantity == 0U || quantity > 125U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    pdu->unit_id = unit_id;
    pdu->function = 0x03U;
    pdu->payload[0] = (uint8_t)(address >> 8);
    pdu->payload[1] = (uint8_t)(address & 0xFFU);
    pdu->payload[2] = (uint8_t)(quantity >> 8);
    pdu->payload[3] = (uint8_t)(quantity & 0xFFU);
    pdu->payload_length = 4U;
    return MBC_STATUS_OK;
}

mbc_status_t mbc_pdu_build_write_single_register(mbc_pdu_t* pdu, uint8_t unit_id, uint16_t address,
                                                 uint16_t value)
{
    if (!pdu) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    pdu->unit_id = unit_id;
    pdu->function = 0x06U;
    pdu->payload[0] = (uint8_t)(address >> 8);
    pdu->payload[1] = (uint8_t)(address & 0xFFU);
    pdu->payload[2] = (uint8_t)(value >> 8);
    pdu->payload[3] = (uint8_t)(value & 0xFFU);
    pdu->payload_length = 4U;
    return MBC_STATUS_OK;
}

mbc_status_t mbc_pdu_build_write_multiple_registers(mbc_pdu_t* pdu, uint8_t unit_id,
                                                    uint16_t address, const uint16_t* values,
                                                    size_t quantity)
{
    if (!pdu || !values || quantity == 0U || quantity > 123U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    const size_t byte_count = quantity * 2U;
    if (byte_count + 5U > MBC_PDU_MAX) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    pdu->unit_id = unit_id;
    pdu->function = 0x10U;
    pdu->payload[0] = (uint8_t)(address >> 8);
    pdu->payload[1] = (uint8_t)(address & 0xFFU);
    pdu->payload[2] = (uint8_t)(quantity >> 8);
    pdu->payload[3] = (uint8_t)(quantity & 0xFFU);
    pdu->payload[4] = (uint8_t)byte_count;

    for (size_t i = 0U; i < quantity; ++i) {
        pdu->payload[5U + (i * 2U)] = (uint8_t)(values[i] >> 8);
        pdu->payload[6U + (i * 2U)] = (uint8_t)(values[i] & 0xFFU);
    }

    pdu->payload_length = 5U + byte_count;
    return MBC_STATUS_OK;
}

mbc_status_t mbc_pdu_parse_read_holding_response(const mbc_pdu_t* pdu, const uint8_t** out_data,
                                                 size_t* out_registers)
{
    if (!pdu || !out_data || !out_registers) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (pdu->function != 0x03U || pdu->payload_length == 0U) {
        return MBC_STATUS_DECODING_ERROR;
    }

    const uint8_t byte_count = pdu->payload[0];
    if (byte_count + 1U != pdu->payload_length || (byte_count % 2U) != 0U) {
        return MBC_STATUS_DECODING_ERROR;
    }

    *out_data = &pdu->payload[1];
    *out_registers = (size_t)(byte_count / 2U);
    return MBC_STATUS_OK;
}

mbc_status_t mbc_pdu_parse_write_single_response(const mbc_pdu_t* pdu, uint16_t* out_address,
                                                 uint16_t* out_value)
{
    if (!pdu || !out_address || !out_value) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (pdu->function != 0x06U || pdu->payload_length != 4U) {
        return MBC_STATUS_DECODING_ERROR;
    }

    *out_address = (uint16_t)((pdu->payload[0] << 8) | pdu->payload[1]);
    *out_value = (uint16_t)((pdu->payload[2] << 8) | pdu->payload[3]);
    return MBC_STATUS_OK;
}

mbc_status_t mbc_pdu_parse_write_multiple_response(const mbc_pdu_t* pdu, uint16_t* out_address,
                                                   uint16_t* out_quantity)
{
    if (!pdu || !out_address || !out_quantity) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (pdu->function != 0x10U || pdu->payload_length != 4U) {
        return MBC_STATUS_DECODING_ERROR;
    }

    *out_address = (uint16_t)((pdu->payload[0] << 8) | pdu->payload[1]);
    *out_quantity = (uint16_t)((pdu->payload[2] << 8) | pdu->payload[3]);
    return MBC_STATUS_OK;
}

mbc_status_t mbc_pdu_parse_exception(const mbc_pdu_t* pdu, uint8_t* out_function, uint8_t* out_code)
{
    if (!pdu || !out_function || !out_code) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if ((pdu->function & 0x80U) == 0U || pdu->payload_length != 1U) {
        return MBC_STATUS_DECODING_ERROR;
    }

    *out_function = (uint8_t)(pdu->function & 0x7FU);
    *out_code = pdu->payload[0];
    return MBC_STATUS_OK;
}
