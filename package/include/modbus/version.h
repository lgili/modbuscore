#ifndef MODBUS_VERSION_H
#define MODBUS_VERSION_H

/**
 * @file version.h
 * @brief Public accessor for the generated ModbusCore version macros.
 *
 * The build system generates ``modbus/version_config.h`` during configuration.
 * Including this wrapper keeps the public path stable while exposing the
 * compile-time major/minor/patch constants.
 */

#include <modbus/version_config.h>

#endif /* MODBUS_VERSION_H */
