/**
 * @file pdu.h
 * @brief Helpers to encode and decode Modbus Protocol Data Units.
 */

#ifndef MODBUS_PDU_H
#define MODBUS_PDU_H

#include <stdbool.h>
#include <stddef.h>

#include <modbus/mb_err.h>
#include <modbus/mb_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_PDU_MAX 253U

#define MB_PDU_FC_READ_COILS                    0x01U
#define MB_PDU_FC_READ_DISCRETE_INPUTS          0x02U
#define MB_PDU_FC_READ_HOLDING_REGISTERS        0x03U
#define MB_PDU_FC_READ_INPUT_REGISTERS          0x04U
#define MB_PDU_FC_WRITE_SINGLE_COIL             0x05U
#define MB_PDU_FC_WRITE_SINGLE_REGISTER         0x06U
#define MB_PDU_FC_WRITE_MULTIPLE_COILS          0x0FU
#define MB_PDU_FC_WRITE_MULTIPLE_REGISTERS      0x10U
#define MB_PDU_FC_READ_WRITE_MULTIPLE_REGISTERS 0x17U

#define MB_PDU_EXCEPTION_BIT 0x80U

#define MB_PDU_COIL_OFF_VALUE 0x0000U
#define MB_PDU_COIL_ON_VALUE  0xFF00U

#define MB_PDU_FC01_MIN_COILS   1U
#define MB_PDU_FC01_MAX_COILS   2000U
#define MB_PDU_FC02_MIN_INPUTS  1U
#define MB_PDU_FC02_MAX_INPUTS  2000U
#define MB_PDU_FC03_MIN_REGISTERS 1U
#define MB_PDU_FC03_MAX_REGISTERS 125U
#define MB_PDU_FC04_MIN_REGISTERS 1U
#define MB_PDU_FC04_MAX_REGISTERS 125U

#define MB_PDU_FC16_MIN_REGISTERS 1U
#define MB_PDU_FC16_MAX_REGISTERS 123U

#define MB_PDU_FC0F_MIN_COILS  1U
#define MB_PDU_FC0F_MAX_COILS  1968U

#define MB_PDU_FC17_MIN_READ_REGISTERS   1U
#define MB_PDU_FC17_MAX_READ_REGISTERS   125U
#define MB_PDU_FC17_MIN_WRITE_REGISTERS  1U
#define MB_PDU_FC17_MAX_WRITE_REGISTERS  121U

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
mb_err_t mb_pdu_build_read_coils_request(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, mb_u16 quantity);
mb_err_t mb_pdu_parse_read_coils_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_quantity);

mb_err_t mb_pdu_build_read_coils_response(mb_u8 *out, mb_size_t out_cap, const bool *coils, mb_u16 count);
mb_err_t mb_pdu_parse_read_coils_response(const mb_u8 *pdu, mb_size_t len, const mb_u8 **out_payload, mb_u8 *out_byte_count);

mb_err_t mb_pdu_build_read_discrete_inputs_request(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, mb_u16 quantity);
mb_err_t mb_pdu_parse_read_discrete_inputs_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_quantity);

mb_err_t mb_pdu_build_read_discrete_inputs_response(mb_u8 *out, mb_size_t out_cap, const bool *inputs, mb_u16 count);
mb_err_t mb_pdu_parse_read_discrete_inputs_response(const mb_u8 *pdu, mb_size_t len, const mb_u8 **out_payload, mb_u8 *out_byte_count);

mb_err_t mb_pdu_build_read_holding_request(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, mb_u16 quantity);
mb_err_t mb_pdu_parse_read_holding_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_quantity);

mb_err_t mb_pdu_build_read_holding_response(mb_u8 *out, mb_size_t out_cap, const mb_u16 *registers, mb_u16 count);
mb_err_t mb_pdu_parse_read_holding_response(const mb_u8 *pdu, mb_size_t len, const mb_u8 **out_payload, mb_u16 *out_register_count);

mb_err_t mb_pdu_build_read_input_request(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, mb_u16 quantity);
mb_err_t mb_pdu_parse_read_input_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_quantity);

mb_err_t mb_pdu_build_read_input_response(mb_u8 *out, mb_size_t out_cap, const mb_u16 *registers, mb_u16 count);
mb_err_t mb_pdu_parse_read_input_response(const mb_u8 *pdu, mb_size_t len, const mb_u8 **out_payload, mb_u16 *out_register_count);

mb_err_t mb_pdu_build_write_single_request(mb_u8 *out, mb_size_t out_cap, mb_u16 address, mb_u16 value);
mb_err_t mb_pdu_parse_write_single_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_address, mb_u16 *out_value);
mb_err_t mb_pdu_build_write_single_response(mb_u8 *out, mb_size_t out_cap, mb_u16 address, mb_u16 value);
mb_err_t mb_pdu_parse_write_single_response(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_address, mb_u16 *out_value);

mb_err_t mb_pdu_build_write_single_coil_request(mb_u8 *out, mb_size_t out_cap, mb_u16 address, bool coil_on);
mb_err_t mb_pdu_parse_write_single_coil_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_address, bool *out_coil_on);
mb_err_t mb_pdu_build_write_single_coil_response(mb_u8 *out, mb_size_t out_cap, mb_u16 address, bool coil_on);
mb_err_t mb_pdu_parse_write_single_coil_response(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_address, bool *out_coil_on);

mb_err_t mb_pdu_build_write_multiple_request(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, const mb_u16 *values, mb_u16 count);
mb_err_t mb_pdu_parse_write_multiple_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_count, const mb_u8 **out_payload);
mb_err_t mb_pdu_build_exception(mb_u8 *out, mb_size_t out_cap, mb_u8 function, mb_u8 exception_code);
mb_err_t mb_pdu_parse_exception(const mb_u8 *pdu, mb_size_t len, mb_u8 *out_function, mb_u8 *out_exception);


mb_err_t mb_pdu_build_write_multiple_response(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, mb_u16 count);
mb_err_t mb_pdu_parse_write_multiple_response(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_count);

mb_err_t mb_pdu_build_write_multiple_coils_request(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, const bool *coils, mb_u16 count);
mb_err_t mb_pdu_parse_write_multiple_coils_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_count, mb_u8 *out_byte_count, const mb_u8 **out_payload);
mb_err_t mb_pdu_build_write_multiple_coils_response(mb_u8 *out, mb_size_t out_cap, mb_u16 start_addr, mb_u16 count);
mb_err_t mb_pdu_parse_write_multiple_coils_response(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_addr, mb_u16 *out_count);

mb_err_t mb_pdu_build_read_write_multiple_request(mb_u8 *out, mb_size_t out_cap, mb_u16 read_start_addr, mb_u16 read_quantity, mb_u16 write_start_addr, const mb_u16 *write_values, mb_u16 write_quantity);
mb_err_t mb_pdu_parse_read_write_multiple_request(const mb_u8 *pdu, mb_size_t len, mb_u16 *out_read_addr, mb_u16 *out_read_quantity, mb_u16 *out_write_addr, mb_u16 *out_write_quantity, const mb_u8 **out_write_payload);

mb_err_t mb_pdu_build_read_write_multiple_response(mb_u8 *out, mb_size_t out_cap, const mb_u16 *read_registers, mb_u16 read_quantity);
mb_err_t mb_pdu_parse_read_write_multiple_response(const mb_u8 *pdu, mb_size_t len, const mb_u8 **out_payload, mb_u16 *out_register_count);
// NOLINTEND(bugprone-easily-swappable-parameters)

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_PDU_H */
