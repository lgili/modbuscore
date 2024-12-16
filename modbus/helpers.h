/******************************************************************************
* The information contained herein is confidential property of Embraco. The
* user, copying, transfer or disclosure of such information is prohibited
* except by express written agreement with Embraco.
******************************************************************************/

/**
* @file helpers.h
* @brief Helper header file.
*
* @author  Luiz Carlos Gili
* @date    2024-12-12
*
* @addtogroup Modbus
* @{
*/
#include "modbus.h"

#ifndef _MODBUS_HELPER_H
#define _MODBUS_HELPER_H

#define CRC_POLYNOMIAL 0xA001U
#define CRC_TABLE_SIZE 256U

uint16_t modbus_calculate_crc(const uint8_t *data, uint16_t length);
uint16_t modbus_crc_with_table(const uint8_t *data, uint16_t length);
uint8_t calculate_checksum(const uint8_t *p_serial_data);
void selection_sort(variable_modbus_t modbus_variables[], int lenght);
int binary_search(variable_modbus_t modbus_variables[], uint16_t low, uint16_t high, uint16_t value);

bool read_uint8(const uint8_t *buffer, uint16_t *index, uint16_t buffer_size, uint8_t *value);
bool read_uint16(const uint8_t *buffer, uint16_t *index, uint16_t buffer_size, uint16_t *value);

#endif  /*_MODBUS_HELPER_H*/
/** @} */