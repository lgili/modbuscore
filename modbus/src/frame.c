/**
 * @file frame.c
 * @brief Implementation of Modbus ADU framing helpers.
 */

#include <stdbool.h>
#include <string.h>

#include <modbus/internal/frame.h>
#include <modbus/internal/pdu.h>
#include <modbus/internal/utils.h>

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

#if MB_CONF_TRANSPORT_ASCII
static mb_u8 mb_frame_ascii_lrc(const mb_u8 *data, mb_size_t len)
{
    mb_u32 sum = 0U;
    for (mb_size_t i = 0U; i < len; ++i) {
        sum += data[i];
    }

    sum &= 0xFFU;
    return (mb_u8)(((~sum) + 1U) & 0xFFU);
}

static bool mb_frame_ascii_nibble(mb_u8 ch, mb_u8 *out)
{
    if (out == NULL) {
        return false;
    }

    if (ch >= '0' && ch <= '9') {
        *out = (mb_u8)(ch - '0');
        return true;
    }

    if (ch >= 'A' && ch <= 'F') {
        *out = (mb_u8)(10U + (ch - 'A'));
        return true;
    }

    if (ch >= 'a' && ch <= 'f') {
        *out = (mb_u8)(10U + (ch - 'a'));
        return true;
    }

    return false;
}

static bool mb_frame_ascii_hex_pair_to_byte(mb_u8 hi, mb_u8 lo, mb_u8 *out)
{
    mb_u8 high = 0U;
    mb_u8 low = 0U;

    if (!mb_frame_ascii_nibble(hi, &high) || !mb_frame_ascii_nibble(lo, &low)) {
        return false;
    }

    *out = (mb_u8)((high << 4U) | low);
    return true;
}

mb_err_t mb_frame_ascii_encode(const mb_adu_view_t *adu,
                               mb_u8 *out_ascii,
                               mb_size_t out_cap,
                               mb_size_t *out_len)
{
    if (out_len) {
        *out_len = 0U;
    }

    if (!adu || !out_ascii) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if ((adu->payload_len + 1U) > MB_PDU_MAX) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (adu->payload_len > 0U && adu->payload == NULL) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    const mb_size_t bytes_len = 2U + adu->payload_len;
    mb_u8 bytes[MB_PDU_MAX + 3U];

    bytes[0] = adu->unit_id;
    bytes[1] = adu->function;

    if (adu->payload_len > 0U) {
        memcpy(&bytes[2], adu->payload, adu->payload_len);
    }

    const mb_u8 lrc = mb_frame_ascii_lrc(bytes, bytes_len);
    bytes[bytes_len] = lrc;

    const mb_size_t data_len = bytes_len + 1U;
    const mb_size_t required = 1U + (data_len * 2U) + 2U;
    if (out_cap < required) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    static const char hex_digits[] = "0123456789ABCDEF";

    mb_size_t index = 0U;
    out_ascii[index++] = ':';

    for (mb_size_t i = 0U; i < data_len; ++i) {
        const mb_u8 value = bytes[i];
        out_ascii[index++] = (mb_u8)hex_digits[(value >> 4U) & 0x0FU];
        out_ascii[index++] = (mb_u8)hex_digits[value & 0x0FU];
    }

    out_ascii[index++] = '\r';
    out_ascii[index++] = '\n';

    if (out_len) {
        *out_len = index;
    }

    return MODBUS_ERROR_NONE;
}

mb_err_t mb_frame_ascii_decode(const mb_u8 *ascii,
                               mb_size_t ascii_len,
                               mb_adu_view_t *out,
                               mb_u8 *payload_buf,
                               mb_size_t payload_cap)
{
    if (!ascii || !out) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (ascii_len < 9U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (ascii[0] != ':') {
        return MODBUS_ERROR_INVALID_REQUEST;
    }

    if (ascii[ascii_len - 2U] != '\r' || ascii[ascii_len - 1U] != '\n') {
        return MODBUS_ERROR_INVALID_REQUEST;
    }

    const mb_size_t hex_digits = ascii_len - 3U; /* strip ':' and CRLF */
    if ((hex_digits % 2U) != 0U) {
        return MODBUS_ERROR_INVALID_REQUEST;
    }

    const mb_size_t byte_count = hex_digits / 2U;
    if (byte_count < 3U) {
        return MODBUS_ERROR_INVALID_REQUEST;
    }

    if (byte_count > (MB_PDU_MAX + 3U)) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_u8 bytes[MB_PDU_MAX + 3U];
    const mb_u8 *cursor = &ascii[1];

    for (mb_size_t i = 0U; i < byte_count; ++i) {
        mb_u8 value = 0U;
        if (!mb_frame_ascii_hex_pair_to_byte(cursor[0], cursor[1], &value)) {
            return MODBUS_ERROR_INVALID_REQUEST;
        }
        bytes[i] = value;
        cursor += 2;
    }

    const mb_u8 expected_lrc = bytes[byte_count - 1U];
    const mb_u8 computed_lrc = mb_frame_ascii_lrc(bytes, byte_count - 1U);
    if (expected_lrc != computed_lrc) {
        return MODBUS_ERROR_CRC;
    }

    const mb_size_t payload_len = byte_count - 3U;
    if ((payload_len + 1U) > MB_PDU_MAX) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    memset(out, 0, sizeof *out);
    out->unit_id = bytes[0];
    out->function = bytes[1];
    out->payload_len = payload_len;

    if (payload_len > 0U) {
        if (!payload_buf || payload_cap < payload_len) {
            return MODBUS_ERROR_INVALID_ARGUMENT;
        }
        memcpy(payload_buf, &bytes[2], payload_len);
        out->payload = payload_buf;
    } else {
        out->payload = NULL;
    }

    return MODBUS_ERROR_NONE;
}
#endif /* MB_CONF_TRANSPORT_ASCII */
