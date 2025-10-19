/**
 * @file mbap.c
 * @brief Implementation of MBAP framing for Modbus TCP.
 */

#include <modbuscore/protocol/mbap.h>
#include <string.h>

mbc_status_t mbc_mbap_encode(const mbc_mbap_header_t* header, const uint8_t* pdu_buffer,
                             size_t pdu_length, uint8_t* out_buffer, size_t out_capacity,
                             size_t* out_length)
{
    if (!header || !pdu_buffer || !out_buffer || pdu_length == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    /* Calculate total frame size: MBAP header (7 bytes) + PDU */
    const size_t total_size = MBC_MBAP_HEADER_SIZE + pdu_length;

    if (out_capacity < total_size) {
        return MBC_STATUS_NO_RESOURCES;
    }

    if (total_size > MBC_MBAP_MAX_FRAME_SIZE) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    /* Encode MBAP header (big-endian) */
    out_buffer[0] = (uint8_t)(header->transaction_id >> 8);
    out_buffer[1] = (uint8_t)(header->transaction_id & 0xFFU);
    out_buffer[2] = (uint8_t)(header->protocol_id >> 8);
    out_buffer[3] = (uint8_t)(header->protocol_id & 0xFFU);

    /* Length field = unit_id (1 byte) + PDU length */
    const uint16_t length_field = (uint16_t)(1U + pdu_length);
    out_buffer[4] = (uint8_t)(length_field >> 8);
    out_buffer[5] = (uint8_t)(length_field & 0xFFU);

    out_buffer[6] = header->unit_id;

    /* Copy PDU data */
    memcpy(&out_buffer[7], pdu_buffer, pdu_length);

    if (out_length) {
        *out_length = total_size;
    }

    return MBC_STATUS_OK;
}

mbc_status_t mbc_mbap_decode(const uint8_t* frame_buffer, size_t frame_length,
                             mbc_mbap_header_t* out_header, const uint8_t** out_pdu,
                             size_t* out_pdu_length)
{
    if (!frame_buffer || !out_header || !out_pdu || !out_pdu_length) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (frame_length < MBC_MBAP_HEADER_SIZE) {
        return MBC_STATUS_DECODING_ERROR;
    }

    /* Decode MBAP header (big-endian) */
    out_header->transaction_id = (uint16_t)((frame_buffer[0] << 8) | frame_buffer[1]);
    out_header->protocol_id = (uint16_t)((frame_buffer[2] << 8) | frame_buffer[3]);
    out_header->length = (uint16_t)((frame_buffer[4] << 8) | frame_buffer[5]);
    out_header->unit_id = frame_buffer[6];

    /* Validate protocol ID (must be 0 for Modbus) */
    if (out_header->protocol_id != 0x0000U) {
        return MBC_STATUS_DECODING_ERROR;
    }

    /* Validate length field */
    const size_t expected_frame_length = MBC_MBAP_HEADER_SIZE - 1U + out_header->length;
    if (frame_length < expected_frame_length) {
        return MBC_STATUS_DECODING_ERROR;
    }

    /* PDU starts after MBAP header */
    *out_pdu = &frame_buffer[7];
    *out_pdu_length = out_header->length - 1U; /* Subtract unit_id byte */

    return MBC_STATUS_OK;
}

size_t mbc_mbap_expected_length(const uint8_t* buffer, size_t available)
{
    if (!buffer || available < 6U) {
        return 0U; /* Need at least 6 bytes to read length field */
    }

    /* Extract length field (bytes 4-5) */
    const uint16_t length = (uint16_t)((buffer[4] << 8) | buffer[5]);

    /* Total frame = 6 bytes (before length field) + length field value */
    return (size_t)(6U + length);
}
