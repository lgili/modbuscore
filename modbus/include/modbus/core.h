/**
 * @file modbus_core.h
 * @brief Core Modbus protocol functions for both Master and Slave.
 *
 * This header defines core functions and data structures used by both Modbus Master
 * and Slave implementations. It focuses on:
 * - Building and parsing Modbus RTU frames.
 * - Performing CRC checks.
 * - Handling timeouts and inter-frame delays.
 * - Providing a generic interface for sending and receiving frames, leaving the
 *   specifics (which registers to read/write) to Master or Slave logic.
 *
 * The idea is that Modbus Master or Slave code calls these functions to handle the low-level
 * details of frame management, while the higher-level logic (e.g., deciding which registers
 * to read, handling device info requests) is implemented elsewhere.
 *
 * @author Luiz Carlos Gili
 * @date 2024-12-20
 *
 * @addtogroup ModbusCore
 * @{
 */

#ifndef MODBUS_CORE_H
#define MODBUS_CORE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

#include <modbus/base.h>        /* For modbus_context_t, modbus_error_t, etc. */
#include <modbus/utils.h>  /* For safe reads from buffers, etc. */

/** 
 * @brief Common Modbus function codes (subset).
 *
 * More can be added as needed. Both Master and Slave might reference these.
 */
typedef enum {
    MODBUS_FUNC_READ_COILS                    = 0x01U,
    MODBUS_FUNC_READ_DISCRETE_INPUTS          = 0x02U,
    MODBUS_FUNC_READ_HOLDING_REGISTERS        = 0x03U,
    MODBUS_FUNC_READ_INPUT_REGISTERS          = 0x04U,
    MODBUS_FUNC_WRITE_SINGLE_COIL             = 0x05U,
    MODBUS_FUNC_WRITE_SINGLE_REGISTER         = 0x06U,
    MODBUS_FUNC_WRITE_MULTIPLE_COILS          = 0x0FU,
    MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS      = 0x10U,
    MODBUS_FUNC_READ_WRITE_MULTIPLE_REGISTERS = 0x17U,
    MODBUS_FUNC_READ_DEVICE_INFORMATION       = 0x2BU,
    
    MODBUS_FUNC_ERROR_FRAME_HEADER            = 0x80U /**< Used to indicate an error response. */
} modbus_function_code_t;

/**
 * @brief Builds a Modbus RTU frame by adding CRC.
 *
 * This function takes the given address, function code, and data payload, and
 * appends the CRC to form a complete Modbus RTU frame in the provided buffer.
 *
 * @param address         Modbus slave address (or target device address).
 * @param function_code   Modbus function code.
 * @param data            Pointer to the data payload (without CRC).
 * @param data_length     Length of the data payload.
 * @param out_buffer      Buffer to store the complete frame (including CRC).
 * @param out_buffer_size Size of the out_buffer.
 * 
 * @return Number of bytes written to out_buffer, or 0 if there is not enough space.
 */
uint16_t modbus_build_rtu_frame(uint8_t address, uint8_t function_code,
                                const uint8_t *data, uint16_t data_length,
                                uint8_t *out_buffer, uint16_t out_buffer_size);

/**
 * @brief Parses a Modbus RTU frame, verifying CRC.
 *
 * This function checks the CRC of the given frame. If valid, it extracts the address,
 * function code, and payload. The caller can then interpret the payload according to
 * the function code.  
 *
 * @param frame        Pointer to the frame buffer (address + function + data + CRC).
 * @param frame_length Length of the frame in bytes.
 * @param address      Pointer to store the parsed address.
 * @param function     Pointer to store the parsed function code.
 * @param payload      Pointer to store the start of the payload (inside frame).
 * @param payload_len  Pointer to store the length of the payload (excluding CRC).
 *
 * @return modbus_error_t MODBUS_ERROR_NONE if successful, or an error/exception code.
 */
modbus_error_t modbus_parse_rtu_frame(const uint8_t *frame, uint16_t frame_length,
                                      uint8_t *address, uint8_t *function,
                                      const uint8_t **payload, uint16_t *payload_len);

/**
 * @brief Sends a Modbus frame using the configured transport.
 *
 * This function attempts to write the given frame to the transport. It returns
 * an error if it fails. It does not block indefinitely; the behavior depends
 * on the transport implementation.
 *
 * @param ctx         Pointer to the Modbus context.
 * @param frame       Pointer to the complete frame (including CRC).
 * @param frame_len   Length of the frame.
 *
 * @return modbus_error_t MODBUS_ERROR_NONE on success, or error code.
 */
modbus_error_t modbus_send_frame(modbus_context_t *ctx, const uint8_t *frame, uint16_t frame_len);

/**
 * @brief Receives a Modbus frame using the configured transport.
 *
 * This function tries to read a Modbus RTU frame from the transport. It may implement
 * timing logic (checking inter-character timeouts, frame timeouts).
 *
 * If a complete frame is successfully received and validated, it returns MODBUS_ERROR_NONE.
 * Otherwise, it returns an error code (e.g. MODBUS_ERROR_TIMEOUT, MODBUS_ERROR_CRC).
 *
 * @param ctx         Pointer to the Modbus context.
 * @param out_buffer  Buffer to store the received frame.
 * @param out_size    Size of out_buffer.
 * @param out_length  Pointer to store the received frame length.
 *
 * @return modbus_error_t MODBUS_ERROR_NONE on success, or error code.
 */
modbus_error_t modbus_receive_frame(modbus_context_t *ctx, uint8_t *out_buffer, uint16_t out_size, uint16_t *out_length);

/**
 * @brief Helper function to check if a given function code indicates an error response.
 *
 * If the function code >= 0x80, it's typically an exception response from the slave.
 *
 * @param function_code The function code from a received frame.
 * @return true if it is an error response, false otherwise.
 */
static inline bool modbus_is_error_response(uint8_t function_code) {
    return (function_code & MODBUS_FUNC_ERROR_FRAME_HEADER) != 0;
}

/**
 * @brief Converts a Modbus exception code to a modbus_error_t.
 *
 * For example, if the payload indicates exception code 2 (Illegal Data Address),
 * this function returns MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS.
 *
 * @param exception_code The exception code from the Modbus frame.
 * @return A corresponding modbus_error_t.
 */
modbus_error_t modbus_exception_to_error(uint8_t exception_code);

/**
 * @brief Convenience function to reset internal RX/TX counters in the context.
 *
 * This might be used when starting a new transaction or after an error.
 *
 * @param ctx Pointer to the Modbus context.
 */
void modbus_reset_buffers(modbus_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_CORE_H */

/** @} */
