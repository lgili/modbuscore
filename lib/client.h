
/******************************************************************************
* The information contained herein is confidential property of Embraco. The
* user, copying, transfer or disclosure of such information is prohibited
* except by express written agreement with Embraco.
******************************************************************************/

/**
* @file client.h
* @brief Modbus client protocol header file.
*
* @author  Luiz Carlos Gili
* @date    2024-12-12
*
* @addtogroup Modbus
* @{
*/

#ifndef _MODBUS_CLIENT_H
#define _MODBUS_CLIENT_H

#include "modbus.h"


modbus_error_t modbus_server_create(modbus_context_t *modbus, uint16_t *device_address, uint16_t *baudrate);


#endif  /*_MODBUS_CLIENT_H*/
/** @} */