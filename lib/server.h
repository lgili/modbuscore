
/******************************************************************************
* The information contained herein is confidential property of Embraco. The
* user, copying, transfer or disclosure of such information is prohibited
* except by express written agreement with Embraco.
******************************************************************************/

/**
* @file server.h
* @brief Modbus server protocol header file.
*
* @author  Luiz Carlos Gili
* @date    2024-12-12
*
* @addtogroup Modbus
* @{
*/

#ifndef _MODBUS_SERVER_H
#define _MODBUS_SERVER_H

#include "modbus.h"



modbus_error_t modbus_server_create(modbus_context_t *modbus, uint16_t *device_address, uint16_t *baudrate);


#endif  /*_MODBUS_SERVER_H*/
/** @} */