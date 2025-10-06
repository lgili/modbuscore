/**
 * @file modbus_transport.h
 * @brief Abstraction layer for Modbus transport operations.
 *
 * This header defines the structures and function pointers required to abstract
 * the underlying transport layer used by the Modbus library. The goal is to
 * allow different hardware or platform implementations (e.g., UART, TCP) to be
 * integrated seamlessly into the Modbus stack.
 *
 * Both Client and Server implementations can utilize this interface to send and
 * receive frames, as well as manage timing functions (for timeouts and
 * inter-character delays).
 *
 * Users of the library must provide implementations that match these
 * function pointers and set them into the Modbus context before initiating
 * protocol operations.
 * 
 * **Example functions that must be provided by the user:**
 *  - `read()`:  Blocking or non-blocking read of a specified number of bytes.
 *  - `write()`: Write a specified number of bytes.
 *  - `get_reference_msec()`: Get a timestamp (in ms) reference.
 *  - `measure_time_msec()`:  Measure elapsed time from a reference.
 *  - **Optional:**
 *      - `change_baudrate()`: Change the baud rate (for RTU).
 *      - `restart_uart()`: Restart the UART or underlying interface.
 *      - `write_gpio()`: Control GPIO pins (for RS485 DE/RE control).
 *
 * **Usage Example:**
 * ```c
 * #include "modbus_transport.h"
 * 
 * // Implement the required transport functions
 * int32_t uart_read(uint8_t *buf, uint16_t count) {
 *     // UART read implementation
 * }
 * 
 * int32_t uart_write(const uint8_t *buf, uint16_t count) {
 *     // UART write implementation
 * }
 * 
 * uint16_t get_current_msec(void) {
 *     // Return current timestamp in milliseconds
 * }
 * 
 * uint16_t measure_elapsed_time(uint16_t ref) {
 *     // Calculate elapsed time since reference
 * }
 * 
 * // Initialize transport
 * modbus_transport_t transport = {
 *     .transport = MODBUS_TRANSPORT_RTU,
 *     .read = uart_read,
 *     .write = uart_write,
 *     .get_reference_msec = get_current_msec,
 *     .measure_time_msec = measure_elapsed_time,
 *     .change_baudrate = NULL, // Optional
 *     .restart_uart = NULL,    // Optional
 *     .write_gpio = NULL,      // Optional
 *     .parse_bootloader_request = NULL, // Optional
 *     .arg = NULL              // User-specific data
 * };
 * ```
 * 
 * @author
 * Luiz Carlos Gili
 * 
 * @date
 * 2024-12-20
 *
 * @addtogroup ModbusTransport
 * @{
 */
 
#ifndef MODBUS_TRANSPORT_H
#define MODBUS_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

#include <modbus/mb_err.h>
#include <modbus/transport_if.h>

#ifdef __cplusplus
extern "C"{
#endif 

/**
 * @brief Enumeration of Modbus transport types.
 *
 * Currently supports RTU and TCP. Additional transport types can be added in the future.
 */
typedef enum {
    MODBUS_TRANSPORT_RTU = 1,    /**< RTU transport type */
    MODBUS_TRANSPORT_TCP = 2,    /**< TCP transport type */
    MODBUS_TRANSPORT_ASCII = 3   /**< ASCII transport type */
} modbus_transport_type_t;

/**
 * @brief Structure holding platform-specific function pointers for I/O and timing.
 *
 * Users must populate this structure with appropriate functions that implement the
 * read, write, timing, and (optionally) UART control operations. These functions
 * are utilized by both Client and Server instances of the Modbus stack.
 *
 * **Fields:**
 * - `transport`: Specifies the type of transport (RTU or TCP).
 * - `read`: Function pointer for reading data from the transport.
 * - `write`: Function pointer for writing data to the transport.
 * - `get_reference_msec`: Function pointer to retrieve a reference timestamp in milliseconds.
 * - `measure_time_msec`: Function pointer to measure elapsed time since a reference timestamp.
 * - `change_baudrate`: (Optional) Function pointer to change the baud rate (RTU only).
 * - `restart_uart`: (Optional) Function pointer to restart the UART or underlying interface.
 * - `write_gpio`: (Optional) Function pointer to control GPIO pins (e.g., RS485 DE/RE control).
 * - `parse_bootloader_request`: (Optional) Function pointer to parse bootloader requests.
 * - `arg`: User-defined argument pointer for storing platform-specific data or context.
 *
 * **Function Pointer Descriptions:**
 * - `read`: Should read up to `count` bytes into `buf`. Returns the number of bytes read, 
 *   0 to `count-1` if a timeout occurred (partial read), or a negative value if an error occurred.
 * - `write`: Should write `count` bytes from `buf`. Returns the number of bytes written or a negative value on error.
 * - `get_reference_msec`: Should return the current timestamp in milliseconds.
 * - `measure_time_msec`: Should return the elapsed time in milliseconds since the provided reference timestamp.
 * - `change_baudrate`: Should change the baud rate to the specified value. Returns the actual baud rate set.
 * - `restart_uart`: Should restart the UART or underlying interface. No return value.
 * - `write_gpio`: Should set the specified GPIO pin to the given value. Returns 0 on success, else an error code.
 * - `parse_bootloader_request`: Should parse incoming bootloader requests. Returns 0 on success, else an error code.
 *
 * **Notes:**
 * - Optional function pointers should be set to `NULL` if not applicable.
 * - The `arg` field can be used to pass additional context or state to the transport functions.
 */
typedef struct {
    modbus_transport_type_t transport; /**< Transport type (RTU or TCP). */

    /**
     * @brief Read function pointer.
     *
     * Reads `count` bytes into `buf`. Should return:
     * - The number of bytes actually read if successful.
     * - 0 to `count-1` if a timeout occurred (partial read).
     * - A negative value if a transport error occurred.
     *
     * @param buf   Buffer to store read data.
     * @param count Number of bytes to read.
     * @return int32_t Number of bytes read or error code.
     *
     * @example
     * ```c
     * int32_t uart_read(uint8_t *buf, uint16_t count) {
     *     // Implement UART read logic here
     * }
     * ```
     */
    int32_t (*read)(uint8_t *buf, uint16_t count);

    /**
     * @brief Write function pointer.
     *
     * Writes `count` bytes from `buf`. Should return:
     * - The number of bytes actually written if successful.
     * - A negative value if a transport error occurred.
     *
     * @param buf   Buffer containing data to write.
     * @param count Number of bytes to write.
     * @return int32_t Number of bytes written or error code.
     *
     * @example
     * ```c
     * int32_t uart_write(const uint8_t *buf, uint16_t count) {
     *     // Implement UART write logic here
     * }
     * ```
     */
    int32_t (*write)(const uint8_t *buf, uint16_t count);

    /**
     * @brief Retrieves a reference timestamp in milliseconds.
     *
     * Typically returns a millisecond counter since system startup. Used as a reference
     * for measuring intervals and handling timeouts.
     *
     * @return uint16_t Reference time in milliseconds.
     *
     * @example
     * ```c
     * uint16_t get_current_msec(void) {
     *     // Implement timestamp retrieval logic here
     * }
     * ```
     */
    uint16_t (*get_reference_msec)(void);

    /**
     * @brief Measures elapsed time in milliseconds from a given reference.
     *
     * Given a previously acquired timestamp from `get_reference_msec()`, returns
     * how many milliseconds have passed since that reference.
     *
     * @param ref Reference timestamp obtained from `get_reference_msec()`.
     * @return uint16_t Elapsed time in milliseconds.
     *
     * @example
     * ```c
     * uint16_t measure_elapsed_time(uint16_t ref) {
     *     // Implement elapsed time calculation logic here
     * }
     * ```
     */
    uint16_t (*measure_time_msec)(uint16_t ref);

    /**
     * @brief Optional function to change the baud rate.
     *
     * Only relevant for RTU transport. If not applicable, set to `NULL`.
     *
     * @param baudrate New baud rate to set.
     * @return uint16_t The actual baud rate set.
     *
     * @example
     * ```c
     * uint16_t change_uart_baudrate(uint16_t baudrate) {
     *     // Implement baud rate change logic here
     * }
     * ```
     */
    uint16_t (*change_baudrate)(uint16_t baudrate);

    /**
     * @brief Optional function to restart the UART or underlying interface.
     *
     * If not applicable, set to `NULL`.
     *
     * @example
     * ```c
     * void restart_uart_interface(void) {
     *     // Implement UART restart logic here
     * }
     * ```
     */
    void (*restart_uart)(void);

    /**
     * @brief Optional function to write to a GPIO pin (for RS485 DE/RE control).
     *
     * If not needed, set to `NULL`.
     *
     * @param gpio  GPIO pin identifier.
     * @param value Value to write to the pin (0 or 1).
     * @return uint8_t Status code (0 = success, else error).
     *
     * @example
     * ```c
     * uint8_t control_rs485_de_re(uint8_t gpio, uint8_t value) {
     *     // Implement GPIO control logic here
     * }
     * ```
     */
    uint8_t (*write_gpio)(uint8_t gpio, uint8_t value);

    /**
     * @brief Optional function to parse a bootloader request, if applicable.
     *
     * Specific to certain implementations. If not applicable, set to `NULL`.
     *
     * @param buffer      Pointer to the data buffer.
     * @param buffer_size Pointer to the buffer size (in/out).
     * @return uint8_t Status code (0 = success, else error).
     *
     * @example
     * ```c
     * uint8_t parse_bootloader_req(uint8_t *buffer, uint16_t *buffer_size) {
     *     // Implement bootloader request parsing logic here
     * }
     * ```
     */
    uint8_t (*parse_bootloader_request)(uint8_t *buffer, uint16_t *buffer_size);

    /**
     * @brief User-defined argument pointer.
     *
     * Can be used to store platform-specific data or context that the read/write
     * functions may require.
     *
     * @example
     * ```c
     * typedef struct {
     *     UART_HandleTypeDef uart_handle;
     *     GPIO_TypeDef *gpio_port;

     *     uint16_t gpio_pin;
     * } transport_context_t;
     * 
     * transport_context_t ctx = {
     *     .uart_handle = huart1,
     *     .gpio_port = GPIOA,
     *     .gpio_pin = GPIO_PIN_5
     * };
     * 
     * modbus_transport_t transport = {
     *     .transport = MODBUS_TRANSPORT_RTU,
     *     .read = uart_read,
     *     .write = uart_write,
     *     .get_reference_msec = get_current_msec,
     *     .measure_time_msec = measure_elapsed_time,
     *     .change_baudrate = change_uart_baudrate,
     *     .restart_uart = restart_uart_interface,
     *     .write_gpio = control_rs485_de_re,
     *     .parse_bootloader_request = NULL,
     *     .arg = &ctx
     * };
     * ```
     */
    void* arg;
} modbus_transport_t;

/**
 * @brief Binds a legacy transport descriptor to the lightweight interface.
 *
 * This helper wires the classic blocking callbacks into an ``mb_transport_if_t``
 * shim so newer code paths can rely on the non-blocking façade while existing
 * platforms keep the richer ``modbus_transport_t`` structure.
 *
 * @param[out] iface   Interface instance to populate.
 * @param[in]  legacy  Legacy transport descriptor to bridge.
 *
 * @return ``MODBUS_ERROR_NONE`` when the shim was installed successfully or an
 *         error code if the legacy descriptor is incomplete.
 */
mb_err_t modbus_transport_bind_legacy(mb_transport_if_t *iface, modbus_transport_t *legacy);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TRANSPORT_H */

/** @} */
