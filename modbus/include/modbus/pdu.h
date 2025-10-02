/**
 * @file pdu.h
 * @brief Helpers to encode and decode Modbus PDUs (Gate 2).
 */

#ifndef MODBUS_PDU_H
#define MODBUS_PDU_H

#include <stddef.h>

#include <modbus/mb_err.h>
#include <modbus/mb_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_PDU_MAX 253U

#define MB_PDU_FC_READ_HOLDING_REGISTERS 0x03U
#define MB_PDU_FC_WRITE_SINGLE_REGISTER  0x06U
#define MB_PDU_FC_WRITE_MULTIPLE_REGISTERS 0x10U

#define MB_PDU_FC03_MIN_REGISTERS 1U
#define MB_PDU_FC03_MAX_REGISTERS 125U

#define MB_PDU_FC16_MIN_REGISTERS 1U
#define MB_PDU_FC16_MAX_REGISTERS 123U

mb_err_t mb_pdu_build_read_holding_request(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, mb_u16 quantity);
mb_err_t mb_pdu_parse_read_holding_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_quantity);

mb_err_t mb_pdu_build_read_holding_response(mb_u8 *out, mb_size_t out_cap, const mb_u16 *registers, mb_u16 count);
mb_err_t mb_pdu_parse_read_holding_response(const mb_u8 *pdu, mb_size_t len, const mb_u8 **out_payload, mb_u16 *out_register_count);

mb_err_t mb_pdu_build_write_single_request(mb_u8 *out, mb_size_t out_cap, mb_u16 address, mb_u16 value);
mb_err_t mb_pdu_parse_write_single_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_address, mb_u16 *out_value);
mb_err_t mb_pdu_build_write_single_response(mb_u8 *out, mb_size_t out_cap, mb_u16 address, mb_u16 value);
mb_err_t mb_pdu_parse_write_single_response(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_address, mb_u16 *out_value);

mb_err_t mb_pdu_build_write_multiple_request(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, const mb_u16 *values, mb_u16 count);
mb_err_t mb_pdu_parse_write_multiple_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_count, const mb_u8 **out_payload);

mb_err_t mb_pdu_build_write_multiple_response(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, mb_u16 count);
mb_err_t mb_pdu_parse_write_multiple_response(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_count);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_PDU_H */
