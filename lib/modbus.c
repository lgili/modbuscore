/******************************************************************************
 * The information contained herein is confidential property of Embraco. The
 * user, copying, transfer or disclosure of such information is prohibited
 * except by express written agreement with Embraco.
 ******************************************************************************/

/**
 * @file    modbus_protocol.c
 * @brief   Mobus Protocol driver module.
 * @author  Luiz Carlos Gili
 * @date    2024-09-23
 * @addtogroup Modbus
 * @{
 */

/******************************************************************************
 * Include section
 ******************************************************************************/
// #include "modbus_serial.h"
#include "modbus.h"
#include "helpers.h"
#ifdef MODBUS_CLIENT
#include "client.h"
#else
#include "server.h"
#endif
/******************************************************************************
 * Defines section
 * Add all #defines here
 *
 ******************************************************************************/
/******************************************************************************
 * Definitions
 ******************************************************************************/

// /******************************************************************************
//  * Public Functions Implementation
//  ******************************************************************************/

void modbus_platform_conf_create(modbus_platform_conf* platform_conf) {
    memset(platform_conf, 0, sizeof(modbus_platform_conf));
    platform_conf->crc_calc = modbus_crc_with_table;
}

