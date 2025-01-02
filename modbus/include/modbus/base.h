/**
 * @file modbus.h
 * @brief Common definitions for Modbus Master and Slave implementations.
 *
 * This header provides common enumerations, data structures, and type definitions
 * used by both Modbus Master and Slave implementations. It does not depend on any
 * specific hardware or protocol variant, aiming at a portable and flexible design.
 *
 * Concepts defined here:
 * - Modbus error and exception codes
 * - A generic modbus_context_t structure for holding platform configuration,
 *   transport interfaces, and user-specific data.
 * - Data structures for holding registers and their callbacks.
 * - Macros and constants related to Modbus protocol.
 *
 * Users should include this header in their Master or Slave code. Platform-dependent
 * operations (I/O, timing) and higher-level states (FSM) are defined elsewhere.
 * 
 * @author Luiz Carlos Gili
 * @date 2024-12-20
 *
 * @addtogroup ModbusCore
 * @{
 */

#ifndef MODBUS_BASE_H
#define MODBUS_BASE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

#include <modbus/conf.h>
#include <modbus/transport.h>  /* For modbus_transport_t and related defs */

/* -------------------------------------------------------------------------- */
/*                          Modbus Protocol Constants                         */
/* -------------------------------------------------------------------------- */

#define MODBUS_BROADCAST_ADDRESS      0x00U
#define MODBUS_BOOTLOADER_ADDRESS     0xA5U


/**
 * @brief Utility macros to extract high/low bytes.
 */
#define GET_LOW_BYTE(d)       ((uint8_t)((d) & 0x00FFU))
#define GET_HIGH_BYTE(d)      ((uint8_t)(((d) >> 8U) & 0x00FFU))

/* -------------------------------------------------------------------------- */
/*                               Error Codes                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Error and exception codes used by the Modbus stack.
 *
 * Negative values represent library or transport errors.
 * Positive values (1 to 4) represent Modbus exceptions as per the standard.
 */
typedef enum {
    MODBUS_ERROR_NONE = 0,                /**< No error */
    MODBUS_ERROR_INVALID_ARGUMENT = -1,   /**< Invalid argument provided */
    MODBUS_ERROR_TIMEOUT = -2,            /**< Read/write timeout occurred */
    MODBUS_ERROR_TRANSPORT = -3,          /**< Transport layer error */
    MODBUS_ERROR_CRC = -4,                /**< CRC check failed */
    MODBUS_ERROR_INVALID_REQUEST = -5,    /**< Received invalid request frame */            
    MODBUS_ERROR_OTHER_REQUESTS = -6,     
    MODBUS_OTHERS_REQUESTS = -7,
    MODBUS_ERROR_OTHER = -8,              /**< Other unspecified error */

    /* Modbus exceptions (positive values) */
    MODBUS_EXCEPTION_ILLEGAL_FUNCTION = 1,      /**< Exception 1: Illegal function */
    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS = 2,  /**< Exception 2: Illegal data address */
    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE = 3,    /**< Exception 3: Illegal data value */
    MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE = 4   /**< Exception 4: Device failure */
} modbus_error_t;

/**
 * @brief Checks if the given error is a Modbus exception (1 to 4).
 */
static inline bool modbus_error_is_exception(modbus_error_t err) {
    return (err >= MODBUS_EXCEPTION_ILLEGAL_FUNCTION && err <= MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE);
}

/* -------------------------------------------------------------------------- */
/*                                Modbus Roles                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Defines the role of this Modbus instance.
 *
 * A single codebase can support both Master and Slave roles by selecting the role
 * at runtime or compile time.
 */
typedef enum {
    MODBUS_ROLE_MASTER = 0,
    MODBUS_ROLE_SLAVE  = 1
} modbus_role_t;

/* -------------------------------------------------------------------------- */
/*                         Variable and Register Handling                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief Callback type for reading a variable (e.g. holding register).
 *
 * This callback should return the current value of the variable.
 */
typedef int16_t (*modbus_read_callback_t)(void);

/**
 * @brief Callback type for writing a variable (e.g. holding register).
 *
 * The callback receives the new value and should write it to the variable if allowed.
 * It returns the value actually written (which may differ if needed).
 */
typedef int16_t (*modbus_write_callback_t)(int16_t new_value);

/**
 * @brief Structure representing a Modbus variable (e.g. a holding register).
 *
 * Each variable is identified by an address and can be read-only or read/write.
 * Optional read and write callbacks can be provided for custom logic.
 */
typedef struct {
    int16_t *variable_ptr;               /**< Pointer to the variable in memory */
    modbus_read_callback_t read_callback; /**< Optional callback for reading the variable */
    modbus_write_callback_t write_callback; /**< Optional callback for writing the variable */
    bool read_only;                      /**< True if the variable is read-only */
    uint16_t address;                    /**< Modbus address of this variable */
} variable_modbus_t;


/* -------------------------------------------------------------------------- */
/*                           Modbus Context Structure                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief The Modbus context structure holds all necessary data for both Master and Slave.
 *
 * Fields included:
 * - transport configuration (I/O, timing, etc.)
 * - runtime information (e.g., buffers, message parsing details)
 * - a place for user_data if needed
 *
 * Specific code for Master or Slave will build upon this context,
 * adding logic for frame parsing, state machines, etc.
 */
typedef struct {
    modbus_transport_t transport; /**< Platform-specific I/O and timing functions */

    modbus_role_t role;           /**< Master or Slave role */

    // You can add shared buffers, timers, and other data here.
    // For example, RX/TX buffers and indexes:
    uint8_t rx_buffer[64];
    uint16_t rx_count;
    uint16_t rx_index;

    uint8_t tx_raw_buffer[64];
    // uint16_t tx_raw_count;
    uint16_t tx_raw_index;

    uint8_t tx_buffer[64];
    // uint16_t tx_count;
    uint16_t tx_index;

    
    uint16_t rx_reference_time; /**< Timestamp for receiving data, used in timeouts. */
    uint16_t tx_reference_time; /**< Timestamp for transmitting data, used in timeouts. */
    uint16_t error_timer;

    // User data pointer for application-specific context
    void *user_data;
} modbus_context_t;

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_BASE_H */

/** @} */
