#ifndef MODBUSCORE_PROTOCOL_PDU_H
#define MODBUSCORE_PROTOCOL_PDU_H

/**
 * @file pdu.h
 * @brief Utilitários para codificação/decodificação de PDUs Modbus.
 */

#include <stddef.h>
#include <stdint.h>

#include <modbuscore/common/status.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MBC_PDU_MAX 253U

typedef struct mbc_pdu {
    uint8_t unit_id;
    uint8_t function;
    uint8_t payload[MBC_PDU_MAX];
    size_t payload_length;
} mbc_pdu_t;

mbc_status_t mbc_pdu_encode(const mbc_pdu_t *pdu,
                            uint8_t *buffer,
                            size_t capacity,
                            size_t *out_length);

mbc_status_t mbc_pdu_decode(const uint8_t *buffer,
                            size_t length,
                            mbc_pdu_t *out);

mbc_status_t mbc_pdu_build_read_holding_request(mbc_pdu_t *pdu,
                                                uint8_t unit_id,
                                                uint16_t address,
                                                uint16_t quantity);

mbc_status_t mbc_pdu_build_write_single_register(mbc_pdu_t *pdu,
                                                 uint8_t unit_id,
                                                 uint16_t address,
                                                 uint16_t value);

mbc_status_t mbc_pdu_build_write_multiple_registers(mbc_pdu_t *pdu,
                                                    uint8_t unit_id,
                                                    uint16_t address,
                                                    const uint16_t *values,
                                                    size_t quantity);

mbc_status_t mbc_pdu_parse_read_holding_response(const mbc_pdu_t *pdu,
                                                 const uint8_t **out_data,
                                                 size_t *out_registers);

mbc_status_t mbc_pdu_parse_write_single_response(const mbc_pdu_t *pdu,
                                                 uint16_t *out_address,
                                                 uint16_t *out_value);

mbc_status_t mbc_pdu_parse_write_multiple_response(const mbc_pdu_t *pdu,
                                                   uint16_t *out_address,
                                                   uint16_t *out_quantity);

mbc_status_t mbc_pdu_parse_exception(const mbc_pdu_t *pdu,
                                     uint8_t *out_function,
                                     uint8_t *out_code);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_PROTOCOL_PDU_H */
