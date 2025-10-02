/**
 * @file frame.c
 * @brief Implementation of Modbus ADU framing helpers.
 */

#include <string.h>

#include <modbus/frame.h>
#include <modbus/pdu.h>
#include <modbus/utils.h>

mb_err_t mb_frame_rtu_encode(const mb_adu_view_t *adu,
                             mb_u8 *out_adu,
                             mb_size_t out_cap,
                             mb_size_t *out_len)
{
    if (out_len) {
        *out_len = 0U;
    }

    if (!adu || !out_adu) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((adu->payload_len + 1U) > MB_PDU_MAX) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    const mb_size_t required = 1U + 1U + adu->payload_len + 2U;
    if (out_cap < required) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_size_t index = 0U;
    out_adu[index++] = adu->unit_id;
    out_adu[index++] = adu->function;

    if (adu->payload && adu->payload_len > 0U) {
        memcpy(&out_adu[index], adu->payload, adu->payload_len);
        index += adu->payload_len;
    }

    const mb_u16 crc = modbus_crc_with_table(out_adu, (mb_u16)index);
    out_adu[index++] = (mb_u8)(crc & 0xFFU);
    out_adu[index++] = (mb_u8)((crc >> 8) & 0xFFU);

    if (out_len) {
        *out_len = index;
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_frame_rtu_decode(const mb_u8 *adu,
                             mb_size_t adu_len,
                             mb_adu_view_t *out)
{
    if (!adu || !out) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (adu_len < 4U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    const mb_u16 computed_crc = modbus_crc_with_table(adu, (mb_u16)(adu_len - 2U));
    const mb_u16 frame_crc = (mb_u16)((mb_u16)adu[adu_len - 1U] << 8U) | adu[adu_len - 2U];
    if (computed_crc != frame_crc) {
        return MODBUS_ERROR_CRC;
    }

    const mb_size_t payload_len = adu_len - 4U; /* address + function + crc(2) */
    if ((payload_len + 1U) > MB_PDU_MAX) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    out->unit_id = adu[0];
    out->function = adu[1];
    out->payload = (payload_len > 0U) ? &adu[2] : NULL;
    out->payload_len = payload_len;

    return MODBUS_ERROR_NONE;
}
