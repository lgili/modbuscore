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

#ifndef _MODBUS_HELPER_H
#define _MODBUS_HELPER_H

#define CRC_POLYNOMIAL 0xA001U
#define CRC_TABLE_SIZE 256U

uint16_t modbus_calculate_crc(const uint8_t *data, uint16_t length);
uint16_t modbus_crc_with_table(const uint8_t *data, uint16_t length);
uint8_t calculate_checksum(const uint8_t *p_serial_data);

#endif  /*_MODBUS_HELPER_H*/
/** @} */