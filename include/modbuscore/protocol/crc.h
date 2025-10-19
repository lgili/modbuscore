#ifndef MODBUSCORE_PROTOCOL_CRC_H
#define MODBUSCORE_PROTOCOL_CRC_H

/**
 * @file crc.h
 * @brief Utilities for Modbus RTU CRC16 calculation and validation.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compute Modbus CRC16 over a buffer.
 *
 * @param data   Pointer to buffer (must not be NULL when length > 0)
 * @param length Number of bytes to process
 * @return Computed CRC16 value (LSB-first representation)
 */
uint16_t mbc_crc16(const uint8_t *data, size_t length);

/**
 * @brief Validate that the last two bytes of @p frame match CRC16 over the payload.
 *
 * @param frame  Pointer to frame containing payload followed by CRC16 (LSB first)
 * @param length Total frame length including CRC (must be >= 2)
 * @return true if CRC matches, false otherwise
 */
bool mbc_crc16_validate(const uint8_t *frame, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_PROTOCOL_CRC_H */
