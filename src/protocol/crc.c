/**
 * @file crc.c
 * @brief Modbus RTU CRC16 calculation helpers.
 */

#include <modbuscore/protocol/crc.h>

uint16_t mbc_crc16(const uint8_t* data, size_t length)
{
    if (!data || length == 0U) {
        return 0xFFFFU;
    }

    uint16_t crc = 0xFFFFU;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x0001U) != 0U) {
                crc >>= 1U;
                crc ^= 0xA001U;
            } else {
                crc >>= 1U;
            }
        }
    }
    return crc;
}

bool mbc_crc16_validate(const uint8_t* frame, size_t length)
{
    if (!frame || length < 2U) {
        return false;
    }

    const size_t payload_len = length - 2U;
    uint16_t expected = mbc_crc16(frame, payload_len);
    uint16_t provided =
        (uint16_t)((uint16_t)frame[payload_len] | ((uint16_t)frame[payload_len + 1U] << 8));
    return expected == provided;
}
