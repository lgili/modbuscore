/**
 * @file modbus_transport.h
 * @brief Abstraction layer for Modbus transport operations.
 *
 * This header defines the structures and function pointers required to abstract
 * the underlying transport layer used by the Modbus library. The goal is to
 * allow different hardware or platform implementations (e.g., UART, TCP) to be
 * plugged into the Modbus stack.
 *
 * Both Master and Slave implementations can use this interface to send and
 * receive frames, as well as to manage timing functions (for timeouts and
 * inter-character delays).
 *
 * Users of the library must provide an implementation that matches these
 * function pointers and set them into the Modbus context before starting
 * the protocol operations.
 * 
 * Example functions that must be provided by the user:
 *  - read():  Blocking or non-blocking read of a specified number of bytes.
 *  - write(): Write a specified number of bytes.
 *  - get_reference_msec(): Get a timestamp (in ms) reference.
 *  - measure_time_msec():  Measure elapsed time from a reference.
 *  - optional: change_baudrate(), restart_uart(), write_gpio() for RS485 DE/RE control.
 *
 * @author Luiz Carlos Gili
 * @date 2024-12-20
 *
 * @addtogroup ModbusTransport
 * @{
 */

#ifndef MODBUS_TRANSPORT_H
#define MODBUS_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif 

/**
 * @brief Modbus transport type.
 *
 * Currently supports RTU and TCP. Other transports could be added in the future.
 */
typedef enum {
    MODBUS_TRANSPORT_RTU = 1,
    MODBUS_TRANSPORT_TCP = 2
} modbus_transport_type_t;

/**
 * @brief Structure holding platform-specific function pointers for I/O and timing.
 *
 * Users must fill this structure with appropriate functions that implement the
 * read, write, timing, and (optionally) UART control operations. These functions
 * are used by both Master and Slave instances of the Modbus stack.
 */
typedef struct {
    modbus_transport_type_t transport; /**< Transport type (RTU or TCP). */

    /**
     * @brief Read function pointer.
     *
     * Reads `count` bytes into `buf`. Should return:
     * - The number of bytes actually read if successful.
     * - 0 to `count-1` if a timeout occurred (partial read).
     * - <0 if a transport error occurred.
     *
     * @param buf   Buffer to store read data.
     * @param count Number of bytes to read.
     * @return int32_t Number of bytes read or error code.
     */
    int32_t (*read)(uint8_t *buf, uint16_t count);

    /**
     * @brief Write function pointer.
     *
     * Writes `count` bytes from `buf`. Should return:
     * - The number of bytes actually written if successful.
     * - <0 if a transport error occurred.
     *
     * @param buf   Buffer with the data to write.
     * @param count Number of bytes to write.
     * @return int32_t Number of bytes written or error code.
     */
    int32_t (*write)(const uint8_t *buf, uint16_t count);

    /**
     * @brief Retrieves a reference timestamp in milliseconds.
     *
     * Typically returns a millisecond counter since startup. Used as a reference
     * for measuring intervals and timeouts.
     *
     * @return uint16_t Reference time in milliseconds.
     */
    uint16_t (*get_reference_msec)(void);

    /**
     * @brief Measures elapsed time in milliseconds from a given reference.
     *
     * Given a previously acquired timestamp from get_reference_msec(), returns
     * how many milliseconds have passed since that reference.
     *
     * @param ref Reference timestamp obtained from get_reference_msec().
     * @return uint16_t Elapsed time in milliseconds.
     */
    uint16_t (*measure_time_msec)(uint16_t ref);

    /**
     * @brief Optional function to change the baud rate.
     *
     * Only relevant for RTU. If not applicable, set to NULL.
     *
     * @param baudrate New baudrate to set.
     * @return uint16_t The actual baudrate set.
     */
    uint16_t (*change_baudrate)(uint16_t baudrate);

    /**
     * @brief Optional function to restart the UART or underlying interface.
     *
     * If not applicable, set to NULL.
     */
    void (*restart_uart)(void);

    /**
     * @brief Optional function to write a GPIO pin (for DE/RE control in RS485).
     *
     * If not needed, set to NULL.
     *
     * @param gpio  GPIO pin identifier.
     * @param value Value to write to the pin (0 or 1).
     * @return uint8_t status code (0 = success, else error).
     */
    uint8_t (*write_gpio)(uint8_t gpio, uint8_t value);

    /**
     * @brief Optional function to parse a bootloader request, if applicable.
     *
     * This is specific to certain implementations. If not applicable, set to NULL.
     *
     * @param buffer      Pointer to the data buffer.
     * @param buffer_size Pointer to the buffer size (in/out).
     * @return uint8_t status code (0 = success, else error).
     */
    uint8_t (*parse_bootloader_request)(uint8_t *buffer, uint16_t *buffer_size);

    /**
     * @brief User-defined argument pointer.
     *
     * Can be used to store platform-specific data or context that the read/write
     * functions need.
     */
    void* arg;
} modbus_transport_t;

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_TRANSPORT_H */

/** @} */
