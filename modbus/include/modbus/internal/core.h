/**
 * @file modbus_core.h
 * @brief Core Modbus protocol functions for both Client and Server.
 *
 * This header defines core functions and data structures used by both Modbus Client
 * and Server implementations. It focuses on:
 * - Building and parsing Modbus RTU frames.
 * - Performing CRC checks.
 * - Handling timeouts and inter-frame delays.
 * - Providing a generic interface for sending and receiving frames, leaving the
 *   specifics (which registers to read/write) to Client or Server logic.
 *
 * The idea is that Modbus Client or Server code calls these functions to handle the low-level
 * details of frame management, while the higher-level logic (e.g., deciding which registers
 * to read, handling device info requests) is implemented elsewhere.
 *
 * **Key Features:**
 * - Construction of Modbus RTU frames with CRC.
 * - Parsing and validation of received Modbus RTU frames.
 * - Sending and receiving frames through the configured transport layer.
 * - Utility functions to map Modbus exception codes to error types.
 * - Buffer management for Modbus transactions.
 *
 * **Usage Example:**
 * @code
 * // Building a Modbus RTU frame for reading holding registers
 * uint8_t request_data[] = {0x00, 0x01, 0x00, 0x02}; // Starting address and quantity
 * uint8_t frame[256];
 * uint16_t frame_length = modbus_build_rtu_frame(0x01, MODBUS_FUNC_READ_HOLDING_REGISTERS,
 *                                               request_data, sizeof(request_data),
 *                                               frame, sizeof(frame));
 *
 * // Sending the frame
 * modbus_send_frame(&ctx, frame, frame_length);
 *
 * // Receiving a response frame
 * uint8_t response_frame[256];
 * uint16_t response_length;
 * modbus_error_t error = modbus_receive_frame(&ctx, response_frame, sizeof(response_frame), &response_length);
 *
 * if (error == MODBUS_ERROR_NONE) {
 *     uint8_t address, function;
 *     const uint8_t *payload;
 *     uint16_t payload_len;
 *     error = modbus_parse_rtu_frame(response_frame, response_length, &address, &function, &payload, &payload_len);
 *     if (error == MODBUS_ERROR_NONE) {
 *         // Process payload
 *     }
 * }
 * @endcode
 *
 * **Notes:**
 * - Ensure that the transport layer is correctly initialized before using send and receive functions.
 * - Handle exceptions and error codes appropriately to maintain robust communication.
 * - The buffer sizes should be adequately defined based on the expected frame sizes.
 *
 * @see modbus_transport.h, modbus_utils.h, modbus_fsm.h
 *
 * @ingroup ModbusCore
 * @{
 */

#ifndef MODBUS_CORE_H
#define MODBUS_CORE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

#include <modbus/conf.h>
#include <modbus/internal/base.h>        /**< For modbus_context_t, modbus_error_t, etc. */
#include <modbus/internal/utils.h>       /**< For safe reads from buffers, etc. */

/** 
 * @brief Common Modbus function codes (subset).
 *
 * More can be added as needed. Both Client and Server might reference these.
 */
typedef enum {
    MODBUS_FUNC_READ_COILS                    = 0x01U, /**< Read Coils */
    MODBUS_FUNC_READ_DISCRETE_INPUTS          = 0x02U, /**< Read Discrete Inputs */
    MODBUS_FUNC_READ_HOLDING_REGISTERS        = 0x03U, /**< Read Holding Registers */
    MODBUS_FUNC_READ_INPUT_REGISTERS          = 0x04U, /**< Read Input Registers */
    MODBUS_FUNC_WRITE_SINGLE_COIL             = 0x05U, /**< Write Single Coil */
    MODBUS_FUNC_WRITE_SINGLE_REGISTER         = 0x06U, /**< Write Single Register */
    MODBUS_FUNC_WRITE_MULTIPLE_COILS          = 0x0FU, /**< Write Multiple Coils */
    MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS      = 0x10U, /**< Write Multiple Registers */
    MODBUS_FUNC_READ_WRITE_MULTIPLE_REGISTERS = 0x17U, /**< Read/Write Multiple Registers */
    MODBUS_FUNC_READ_DEVICE_INFORMATION       = 0x2BU, /**< Read Device Information */
    
    MODBUS_FUNC_ERROR_FRAME_HEADER            = 0x80U /**< Used to indicate an error response. */
} modbus_function_code_t;

/**
 * @brief Builds a Modbus RTU frame by adding CRC.
 *
 * This function takes the given address, function code, and data payload, and
 * appends the CRC to form a complete Modbus RTU frame in the provided buffer.
 *
 * @param[in]  address         Modbus server/client address (or target device address).
 * @param[in]  function_code   Modbus function code.
 * @param[in]  data            Pointer to the data payload (without CRC).
 * @param[in]  data_length     Length of the data payload.
 * @param[out] out_buffer      Buffer to store the complete frame (including CRC).
 * @param[in]  out_buffer_size Size of the out_buffer.
 * 
 * @return Number of bytes written to out_buffer, or 0 if there is not enough space.
 *
 * @retval >0  Number of bytes written to out_buffer.
 * @retval 0   Insufficient buffer size to store the complete frame.
 *
 * @example
 * ```c
 * uint8_t payload[] = {0x00, 0x01, 0x00, 0x02};
 * uint8_t frame[256];
 * uint16_t frame_len = modbus_build_rtu_frame(0x01, MODBUS_FUNC_READ_HOLDING_REGISTERS,
 *                                           payload, sizeof(payload),
 *                                           frame, sizeof(frame));
 * if (frame_len > 0) {
 *     // Frame successfully built, proceed to send
 * }
 * ```
 */
#if MB_CONF_TRANSPORT_RTU
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
 * @param[in]  frame        Pointer to the frame buffer (address + function + data + CRC).
 * @param[in]  frame_length Length of the frame in bytes.
 * @param[out] address      Pointer to store the parsed address.
 * @param[out] function     Pointer to store the parsed function code.
 * @param[out] payload      Pointer to store the start of the payload (inside frame).
 * @param[out] payload_len  Pointer to store the length of the payload (excluding CRC).
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` if successful, or an error/exception code.
 *
 * @retval MODBUS_ERROR_NONE                  Frame parsed successfully.
 * @retval MODBUS_ERROR_INVALID_REQUEST        Frame length is too short or CRC is invalid.
 * @retval MODBUS_EXCEPTION_ILLEGAL_FUNCTION   The function code indicates an error response.
 * @retval Others                              Various error codes as defined in `modbus_error_t`.
 *
 * @example
 * ```c
 * uint8_t response_frame[] = {0x01, 0x03, 0x02, 0x00, 0x0A, 0xC4, 0x0B};
 * uint16_t response_length = sizeof(response_frame);
 * uint8_t address, function;
 * const uint8_t *payload;
 * uint16_t payload_len;
 *
 * modbus_error_t error = modbus_parse_rtu_frame(response_frame, response_length,
 *                                               &address, &function,
 *                                               &payload, &payload_len);
 * if (error == MODBUS_ERROR_NONE) {
 *     // Process payload
 * }
 * ```
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
 * @param[in]  ctx         Pointer to the Modbus context.
 * @param[in]  frame       Pointer to the complete frame (including CRC).
 * @param[in]  frame_len   Length of the frame.
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` on success, or error code.
 *
 * @retval MODBUS_ERROR_NONE          Frame sent successfully.
 * @retval MODBUS_ERROR_TRANSPORT      Transport layer error occurred.
 * @retval Others                      Various error codes as defined in `modbus_error_t`.
 *
 * @example
 * ```c
 * uint8_t frame[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x02, 0xC4, 0x0B};
 * modbus_error_t error = modbus_send_frame(&ctx, frame, sizeof(frame));
 * if (error == MODBUS_ERROR_NONE) {
 *     // Frame sent successfully
 * }
 * ```
 */
modbus_error_t modbus_send_frame(modbus_context_t *ctx, const uint8_t *frame, uint16_t frame_len);

/**
 * @brief Receives a Modbus frame using the configured transport.
 *
 * This function tries to read a Modbus RTU frame from the transport. It may implement
 * timing logic (checking inter-character timeouts, frame timeouts).
 *
 * If a complete frame is successfully received and validated, it returns `MODBUS_ERROR_NONE`.
 * Otherwise, it returns an error code (e.g., `MODBUS_ERROR_TIMEOUT`, `MODBUS_ERROR_CRC`).
 *
 * @param[in]  ctx         Pointer to the Modbus context.
 * @param[out] out_buffer  Buffer to store the received frame.
 * @param[in]  out_size    Size of `out_buffer`.
 * @param[out] out_length  Pointer to store the received frame length.
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` on success, or error code.
 *
 * @retval MODBUS_ERROR_NONE          Frame received and validated successfully.
 * @retval MODBUS_ERROR_TIMEOUT        Timeout occurred while waiting for frame.
 * @retval MODBUS_ERROR_CRC            CRC check failed.
 * @retval Others                      Various error codes as defined in `modbus_error_t`.
 *
 * @example
 * ```c
 * uint8_t response_frame[256];
 * uint16_t response_length;
 * modbus_error_t error = modbus_receive_frame(&ctx, response_frame, sizeof(response_frame), &response_length);
 * if (error == MODBUS_ERROR_NONE) {
 *     // Process received frame
 * }
 * ```
 */
modbus_error_t modbus_receive_frame(modbus_context_t *ctx, uint8_t *out_buffer, uint16_t out_size, uint16_t *out_length);
#endif /* MB_CONF_TRANSPORT_RTU */

/**
 * @brief Helper function to check if a given function code indicates an error response.
 *
 * If the function code >= 0x80, it's typically an exception response from the server.
 *
 * @param[in]  function_code The function code from a received frame.
 * @return `true` if it is an error response, `false` otherwise.
 *
 * @example
 * ```c
 * uint8_t function = received_frame[1];
 * if (modbus_is_error_response(function)) {
 *     // Handle error response
 * }
 * ```
 */
static inline bool modbus_is_error_response(uint8_t function_code) {
    return (function_code & MODBUS_FUNC_ERROR_FRAME_HEADER) != 0;
}

/**
 * @brief Converts a Modbus exception code to a `modbus_error_t`.
 *
 * For example, if the payload indicates exception code 2 (Illegal Data Address),
 * this function returns `MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS`.
 *
 * @param[in]  exception_code The exception code from the Modbus frame.
 * @return A corresponding `modbus_error_t`.
 *
 * @retval MODBUS_EXCEPTION_ILLEGAL_FUNCTION           Exception code 1: Illegal Function
 * @retval MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS       Exception code 2: Illegal Data Address
 * @retval MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE         Exception code 3: Illegal Data Value
 * @retval MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE      Exception code 4: Server Device Failure
 * @retval MODBUS_ERROR_OTHER_REQUESTS                 Other exception codes mapped to `modbus_error_t`
 *
 * @example
 * ```c
 * uint8_t exception_code = received_payload[0];
 * modbus_error_t error = modbus_exception_to_error(exception_code);
 * if (modbus_error_is_exception(error)) {
 *     // Handle the specific exception
 * }
 * ```
 */
modbus_error_t modbus_exception_to_error(uint8_t exception_code);

/**
 * @brief Convenience function to reset internal RX/TX counters in the context.
 *
 * This might be used when starting a new transaction or after an error.
 *
 * @param[in,out] ctx Pointer to the Modbus context.
 *
 * @example
 * ```c
 * modbus_reset_buffers(&ctx);
 * ```
 */
void modbus_reset_buffers(modbus_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_CORE_H */

/** @} */
