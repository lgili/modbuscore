/**
 * @file modbus_conf.h
 * @brief Configuration header for the Modbus library.
 *
 * This file contains compile-time configuration options, macros, and default
 * values that can be adjusted for different environments or constraints.
 * Users or integrators may modify these definitions before building the library
 * to customize its behavior.
 *
 * These settings should be included before any other modbus headers, ensuring
 * all modules see the same configuration.
 *
 * Author:
 * Date: 2024-12-20
 */

#ifndef MODBUS_CONF_H
#define MODBUS_CONF_H

#define LOG_ENABLED
/**
 * @brief Maximum size of holding registers array in the server.
 */
#ifndef MAX_SIZE_HOLDING_REGISTERS
#define MAX_SIZE_HOLDING_REGISTERS 64
#endif

/**
 * @brief Maximum address for holding registers.
 */
#ifndef MAX_ADDRESS_HOLDING_REGISTERS
#define MAX_ADDRESS_HOLDING_REGISTERS 65535
#endif

/**
 * @brief Maximum number of registers that can be read or written at once.
 */
#ifndef MODBUS_MAX_READ_WRITE_SIZE
#define MODBUS_MAX_READ_WRITE_SIZE 0x07D0
#endif

/**
 * @brief Maximum device info packages in server mode.
 */
#ifndef MAX_DEVICE_PACKAGES
#define MAX_DEVICE_PACKAGES 5
#endif

/**
 * @brief Maximum length of each device info package.
 */
#ifndef MAX_DEVICE_PACKAGE_VALUES
#define MAX_DEVICE_PACKAGE_VALUES 8
#endif

/**
 * @brief Default Modbus baudrate (for RTU).
 */
#ifndef MODBUS_BAUDRATE
#define MODBUS_BAUDRATE 19200
#endif

/**
 * @brief Size of receive and transmit buffers.
 */
#ifndef MODBUS_RECEIVE_BUFFER_SIZE
#define MODBUS_RECEIVE_BUFFER_SIZE 256
#endif

#ifndef MODBUS_SEND_BUFFER_SIZE
#define MODBUS_SEND_BUFFER_SIZE 256
#endif

#ifndef MASTER_DEFAULT_TIMEOUT_MS
#define MASTER_DEFAULT_TIMEOUT_MS  1000
#endif

/**
 * @brief If you want to enable or disable certain functions, you can define
 * macros here. For example:
 * #define ENABLE_BOOTLOADER 1
 */

/* Add more configurations as needed */

#endif /* MODBUS_CONF_H */
