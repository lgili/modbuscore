#ifndef MODBUSCORE_PROTOCOL_PDU_H
#define MODBUSCORE_PROTOCOL_PDU_H

/**
 * @file pdu.h
 * @brief Utilities for encoding/decoding Modbus PDUs.
 *
 * This module provides functions for building, parsing, and encoding Modbus
 * Protocol Data Units (PDUs) for common function codes:
 * - FC03: Read Holding Registers
 * - FC06: Write Single Register
 * - FC16: Write Multiple Registers
 * - Exception responses
 */

#include <stddef.h>
#include <stdint.h>

#include <modbuscore/common/status.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum PDU payload size (253 bytes per Modbus spec).
 */
#define MBC_PDU_MAX 253U

/**
 * @brief Modbus PDU structure.
 */
typedef struct mbc_pdu {
    uint8_t unit_id;                /**< Unit/Slave ID (0-247) */
    uint8_t function;               /**< Function code (1-127, or 0x80+ for exceptions) */
    uint8_t payload[MBC_PDU_MAX];   /**< PDU payload data */
    size_t payload_length;          /**< Length of payload in bytes */
} mbc_pdu_t;

/**
 * @brief Encode PDU to byte buffer.
 *
 * @param pdu PDU to encode
 * @param buffer Output buffer
 * @param capacity Buffer capacity
 * @param out_length Encoded length (optional, can be NULL)
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_pdu_encode(const mbc_pdu_t *pdu,
                            uint8_t *buffer,
                            size_t capacity,
                            size_t *out_length);

/**
 * @brief Decode byte buffer to PDU.
 *
 * @param buffer Input buffer
 * @param length Buffer length
 * @param out Decoded PDU
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_pdu_decode(const uint8_t *buffer,
                            size_t length,
                            mbc_pdu_t *out);

/**
 * @brief Build FC03 (Read Holding Registers) request PDU.
 *
 * @param pdu PDU to build into
 * @param unit_id Unit/Slave ID
 * @param address Starting register address
 * @param quantity Number of registers to read (1-125)
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_pdu_build_read_holding_request(mbc_pdu_t *pdu,
                                                uint8_t unit_id,
                                                uint16_t address,
                                                uint16_t quantity);

/**
 * @brief Build FC06 (Write Single Register) request PDU.
 *
 * @param pdu PDU to build into
 * @param unit_id Unit/Slave ID
 * @param address Register address
 * @param value Register value to write
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_pdu_build_write_single_register(mbc_pdu_t *pdu,
                                                 uint8_t unit_id,
                                                 uint16_t address,
                                                 uint16_t value);

/**
 * @brief Build FC16 (Write Multiple Registers) request PDU.
 *
 * @param pdu PDU to build into
 * @param unit_id Unit/Slave ID
 * @param address Starting register address
 * @param values Array of register values
 * @param quantity Number of registers to write (1-123)
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_pdu_build_write_multiple_registers(mbc_pdu_t *pdu,
                                                    uint8_t unit_id,
                                                    uint16_t address,
                                                    const uint16_t *values,
                                                    size_t quantity);

/**
 * @brief Parse FC03 (Read Holding Registers) response PDU.
 *
 * @param pdu PDU to parse
 * @param out_data Pointer to register data (big-endian uint16_t pairs)
 * @param out_registers Number of registers read
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_pdu_parse_read_holding_response(const mbc_pdu_t *pdu,
                                                 const uint8_t **out_data,
                                                 size_t *out_registers);

/**
 * @brief Parse FC06 (Write Single Register) response PDU.
 *
 * @param pdu PDU to parse
 * @param out_address Register address that was written
 * @param out_value Register value that was written
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_pdu_parse_write_single_response(const mbc_pdu_t *pdu,
                                                 uint16_t *out_address,
                                                 uint16_t *out_value);

/**
 * @brief Parse FC16 (Write Multiple Registers) response PDU.
 *
 * @param pdu PDU to parse
 * @param out_address Starting register address that was written
 * @param out_quantity Number of registers that were written
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_pdu_parse_write_multiple_response(const mbc_pdu_t *pdu,
                                                   uint16_t *out_address,
                                                   uint16_t *out_quantity);

/**
 * @brief Parse exception response PDU.
 *
 * @param pdu PDU to parse (must have function code with bit 0x80 set)
 * @param out_function Original function code that triggered the exception
 * @param out_code Exception code
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_pdu_parse_exception(const mbc_pdu_t *pdu,
                                     uint8_t *out_function,
                                     uint8_t *out_code);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_PROTOCOL_PDU_H */
