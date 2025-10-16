/**
 * @file base.h
 * @brief Common definitions for Modbus Master and Slave implementations.
 *
 * This header provides common enumerations, data structures, and type definitions
 * used by both Modbus Master and Slave implementations. It is designed to be
 * portable and flexible, without dependencies on specific hardware or protocol variants.
 *
 * **Key Features:**
 * - Defines Modbus error and exception codes.
 * - Provides a generic `modbus_context_t` structure for holding platform configuration,
 *   transport interfaces, and user-specific data.
 * - Includes data structures for managing holding registers and their callbacks.
 * - Contains utility macros and constants related to the Modbus protocol.
 *
 * **Usage:**
 * - Include this header in your Master or Slave code to access common Modbus definitions.
 * - Platform-dependent operations (I/O, timing) and higher-level states (FSM) are defined elsewhere.
 * - Example:
 *   ```c
 *   #include "base.h"
 *   
 *   void setup_modbus() {
 *       modbus_context_t ctx;
 *       // Initialize context and transport
 *       // ...
 *   }
 *   ```
 *
 * @author
 * Luiz Carlos Gili
 * 
 * @date
 * 2024-12-20
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

#include <modbus/mb_types.h>
#include <modbus/conf.h>
#include <modbus/mb_err.h>
#include <modbus/internal/transport_core.h>

/* -------------------------------------------------------------------------- */
/*                          Modbus Protocol Constants                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief Modbus broadcast address.
 *
 * This address is used to send messages to all slaves on the network.
 */
#define MODBUS_BROADCAST_ADDRESS      0x00U

/**
 * @brief Modbus bootloader address.
 *
 * This address is reserved for bootloader operations.
 */
#define MODBUS_BOOTLOADER_ADDRESS     0xA5U

/**
 * @brief Utility macro to extract the low byte from a 16-bit value.
 *
 * @param d The 16-bit value.
 * @return The low byte of the value.
 *
 * @example
 * ```c
 * uint16_t value = 0x1234;
 * uint8_t low = GET_LOW_BYTE(value); // low = 0x34
 * ```
 */
#define GET_LOW_BYTE(d)       ((uint8_t)((d) & 0x00FFU))

/**
 * @brief Utility macro to extract the high byte from a 16-bit value.
 *
 * @param d The 16-bit value.
 * @return The high byte of the value.
 *
 * @example
 * ```c
 * uint16_t value = 0x1234;
 * uint8_t high = GET_HIGH_BYTE(value); // high = 0x12
 * ```
 */
#define GET_HIGH_BYTE(d)      ((uint8_t)(((d) >> 8U) & 0x00FFU))

/* -------------------------------------------------------------------------- */
/*                                Modbus Roles                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Enumeration defining the role of a Modbus instance.
 *
 * A single codebase can support both Client and Server roles by selecting the role
 * at runtime or compile time.
 */
typedef enum {
    MODBUS_ROLE_CLIENT = 0, /**< Client role */
    MODBUS_ROLE_SERVER  = 1  /**< Server role */
} modbus_role_t;

/* -------------------------------------------------------------------------- */
/*                         Variable and Register Handling                     */
/* -------------------------------------------------------------------------- */

/**
 * @brief Callback type for reading a Modbus variable (e.g., holding register).
 *
 * This callback should return the current value of the variable.
 *
 * @return The current value of the variable as a 16-bit integer.
 *
 * @example
 * ```c
 * int16_t read_temperature(void) {
 *     return current_temperature;
 * }
 * ```
 */
typedef int16_t (*modbus_read_callback_t)(void);

/**
 * @brief Callback type for writing a Modbus variable (e.g., holding register).
 *
 * The callback receives the new value and should write it to the variable if allowed.
 * It returns the value actually written, which may differ if necessary.
 *
 * @param new_value The new value to write to the variable.
 * @return The value that was actually written.
 *
 * @example
 * ```c
 * int16_t write_temperature(int16_t new_value) {
 *     if (new_value < MIN_TEMP || new_value > MAX_TEMP) {
 *         return current_temperature; // Do not change if out of range
 *     }
 *     current_temperature = new_value;
 *     return current_temperature;
 * }
 * ```
 */
typedef int16_t (*modbus_write_callback_t)(int16_t new_value);

/**
 * @brief Structure representing a Modbus variable (e.g., holding register).
 *
 * Each variable is identified by an address and can be read-only or read/write.
 * Optional read and write callbacks can be provided for custom logic.
 */
typedef struct {
    int16_t *variable_ptr;                     /**< Pointer to the variable in memory */
    modbus_read_callback_t read_callback;      /**< Optional callback for reading the variable */
    modbus_write_callback_t write_callback;    /**< Optional callback for writing the variable */
    bool read_only;                            /**< Indicates if the variable is read-only */
    uint16_t address;                          /**< Modbus address of this variable */
} variable_modbus_t;

/* -------------------------------------------------------------------------- */
/*                           Modbus Context Structure                         */
/* -------------------------------------------------------------------------- */

/**
 * @brief The Modbus context structure holds all necessary data for both Client and Server.
 *
 * **Fields:**
 * - `transport`: Platform-specific I/O and timing functions.
 * - `role`: Specifies whether the context is for a Client or Server.
 * - `rx_buffer`: Buffer for incoming data.
 * - `rx_count`: Number of bytes currently in the receive buffer.
 * - `rx_index`: Current index in the receive buffer.
 * - `tx_raw_buffer`: Raw buffer for outgoing data.
 * - `tx_index`: Current index in the transmit buffer.
 * - `tx_buffer`: Processed buffer for outgoing data.
 * - `rx_reference_time`: Timestamp for receiving data, used for handling timeouts.
 * - `tx_reference_time`: Timestamp for transmitting data, used for handling timeouts.
 * - `error_timer`: Timer for tracking errors.
 * - `user_data`: Pointer for user-specific context or data.
 *
 * **Usage:**
 * - Initialize this structure before using Modbus Client or Server functions.
 * - Populate `transport` with appropriate I/O and timing functions.
 * - Set `role` to `MODBUS_ROLE_MASTER` or `MODBUS_ROLE_SLAVE` based on usage.
 *
 * **Example:**
 * ```c
 * modbus_context_t ctx;
 * modbus_transport_init_mock(&ctx.transport);
 * ctx.role = MODBUS_ROLE_CLIENT;
 * ctx.user_data = &application_data;
 * ```
 */
typedef struct {
    modbus_transport_t transport;      /**< Platform-specific I/O and timing functions */
    mb_transport_if_t transport_iface; /**< Lightweight, non-blocking transport shim */

    modbus_role_t role;           /**< Client or Server role */

    uint8_t *rx_buffer;           /**< Buffer for incoming data */
    uint16_t rx_capacity;         /**< Capacity of the RX buffer */
    uint16_t rx_count;            /**< Number of bytes in the receive buffer */
    uint16_t rx_index;            /**< Current index in the receive buffer */

    uint8_t *rx_raw_buffer;       /**< Buffer for raw RX bytes */
    uint16_t rx_raw_capacity;     /**< Capacity of the raw RX buffer */

    uint8_t *tx_raw_buffer;       /**< Raw buffer for outgoing data */
    uint16_t tx_raw_capacity;     /**< Capacity of the raw TX buffer */
    uint16_t tx_raw_index;        /**< Current index in the raw transmit buffer */

    uint8_t *tx_buffer;           /**< Processed buffer for outgoing data */
    uint16_t tx_capacity;         /**< Capacity of the processed TX buffer */
    uint16_t tx_index;            /**< Current index in the processed buffer */

    mb_time_ms_t rx_reference_time;   /**< Timestamp for receiving data, used in timeouts */
    mb_time_ms_t tx_reference_time;   /**< Timestamp for transmitting data, used in timeouts */
    mb_time_ms_t error_timer;         /**< Timer for tracking errors */

    void *user_data;              /**< Pointer for user-specific context */

    struct {
        uint8_t rx[MODBUS_RECEIVE_BUFFER_SIZE];       /**< Default RX buffer storage */
        uint8_t rx_raw[MODBUS_RECEIVE_BUFFER_SIZE];   /**< Default raw RX buffer storage */
        uint8_t tx_raw[MODBUS_SEND_BUFFER_SIZE];      /**< Default raw TX buffer storage */
        uint8_t tx[MODBUS_SEND_BUFFER_SIZE];          /**< Default processed TX buffer storage */
    } internal_buffers;
} modbus_context_t;

static inline void modbus_context_use_internal_buffers(modbus_context_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    if (ctx->rx_buffer == NULL) {
        ctx->rx_buffer = ctx->internal_buffers.rx;
        ctx->rx_capacity = (uint16_t)MB_COUNTOF(ctx->internal_buffers.rx);
    }

    if (ctx->rx_raw_buffer == NULL) {
        ctx->rx_raw_buffer = ctx->internal_buffers.rx_raw;
        ctx->rx_raw_capacity = (uint16_t)MB_COUNTOF(ctx->internal_buffers.rx_raw);
    }

    if (ctx->tx_raw_buffer == NULL) {
        ctx->tx_raw_buffer = ctx->internal_buffers.tx_raw;
        ctx->tx_raw_capacity = (uint16_t)MB_COUNTOF(ctx->internal_buffers.tx_raw);
    }

    if (ctx->tx_buffer == NULL) {
        ctx->tx_buffer = ctx->internal_buffers.tx;
        ctx->tx_capacity = (uint16_t)MB_COUNTOF(ctx->internal_buffers.tx);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_BASE_H */

/** @} */
