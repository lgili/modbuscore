/**
 * @file modbus.h
 * @brief Single public header for the Modbus library.
 *
 * This header consolidates all the necessary includes for using the Modbus library.
 * Users only need to include this file in their application.
 *
 * Internally, it includes:
 * - modbus_conf.h: configuration and macros
 * - fsm.h: finite state machine framework
 * - modbus_crc.h: CRC calculation
 * - modbus_utils.h: utility functions (sorting, safe reads, etc.)
 * - modbus_transport.h: transport abstraction interface
 * - modbus_core.h: core functions for Modbus RTU frames and parsing
 * - modbus_master.h: Master (Client) API
 * - modbus_server.h: Server (Slave) API
 *
 * Author:
 * Date: 2024-12-20
 */

#ifndef MODBUS_H
#define MODBUS_H

/* Include configuration first */
#include <modbus/conf.h>

/* Include the rest of the library headers */
#include <modbus/mb_err.h>
#include <modbus/mb_log.h>
#include <modbus/observe.h>
#include <modbus/base.h>
#include <modbus/transport.h>
#include <modbus/transport_if.h>
#include <modbus/features.h>
#if MB_CONF_TRANSPORT_RTU
#include <modbus/transport/rtu.h>
#endif
#if MB_CONF_TRANSPORT_ASCII
#include <modbus/transport/ascii.h>
#endif
#if MB_CONF_TRANSPORT_TCP
#include <modbus/transport/tcp.h>
#include <modbus/transport/tcp_multi.h>
#endif
#include <modbus/frame.h>
#include <modbus/utils.h>
#include <modbus/fsm.h>
#include <modbus/core.h>
#if MB_CONF_BUILD_SERVER
#include <modbus/server.h>
#include <modbus/mapping.h>
#endif

#if MB_CONF_BUILD_CLIENT
#include <modbus/client.h>
#endif

#if defined(_WIN32)
#include <modbus/port/win.h>
#endif


#endif /* MODBUS_H */
