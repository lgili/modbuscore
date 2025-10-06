/**
 * @file modbus_core.c
 * @brief Implementation of core Modbus protocol functions for both Client and Server.
 *
 * This source file implements the functions declared in `modbus_core.h`.
 * It provides core functionality for constructing, parsing, sending,
 * and receiving Modbus RTU frames.
 *
 * **Key Functionalities:**
 * - **Building Frames:** Constructs Modbus RTU frames by appending CRC.
 * - **Parsing Frames:** Parses and validates received Modbus RTU frames.
 * - **Sending Frames:** Abstracted send functions using the transport interface.
 * - **Receiving Frames:** Abstracted receive functions with basic timeout handling.
 * - **Exception Handling:** Maps Modbus exception codes to `modbus_error_t`.
 * - **Buffer Management:** Resets RX/TX buffers for new transactions or after errors.
 *
 * **Notes:**
 * - Ensure that the transport layer is correctly initialized before using send and receive functions.
 * - Handle exceptions and error codes appropriately to maintain robust communication.
 * - The buffer sizes should be adequately defined based on the expected frame sizes.
 * - Consider enhancing `modbus_receive_frame()` to include comprehensive timeout and inter-frame delay handling.
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
 * modbus_error_t send_error = modbus_send_frame(&ctx, frame, frame_length);
 * if (send_error != MODBUS_ERROR_NONE) {
 *     // Handle send error
 * }
 *
 * // Receiving a response frame
 * uint8_t response_frame[256];
 * uint16_t response_length;
 * modbus_error_t recv_error = modbus_receive_frame(&ctx, response_frame, sizeof(response_frame), &response_length);
 *
 * if (recv_error == MODBUS_ERROR_NONE) {
 *     uint8_t address, function;
 *     const uint8_t *payload;
 *     uint16_t payload_len;
 *     modbus_error_t parse_error = modbus_parse_rtu_frame(response_frame, response_length, &address, &function, &payload, &payload_len);
 *     if (parse_error == MODBUS_ERROR_NONE) {
 *         // Process payload
 *     } else {
 *         // Handle parse error
 *     }
 * } else {
 *     // Handle receive error
 * }
 * @endcode
 *
 * @see modbus_core.h, modbus_transport.h, modbus_utils.h
 *
 * @ingroup ModbusCore
 * @{
 */

#include <string.h>

#include <modbus/conf.h>
#include <modbus/core.h>
#include <modbus/frame.h>
#include <modbus/transport_if.h>

/* Internal defines for timeouts, if needed */
#ifndef MODBUS_INTERFRAME_TIMEOUT_MS
#define MODBUS_INTERFRAME_TIMEOUT_MS 100 /**< Time in milliseconds to wait between frames */
#endif

#ifndef MODBUS_BYTE_TIMEOUT_MS
#define MODBUS_BYTE_TIMEOUT_MS 50 /**< Time in milliseconds to wait between bytes */
#endif

/**
 * @brief Builds a Modbus RTU frame by adding CRC.
 *
 * This function constructs a complete Modbus RTU frame by appending the CRC
 * to the provided address, function code, and data payload. It ensures that
 * the output buffer has sufficient space to hold the entire frame, including
 * the CRC.
 *
 * @param[in]  address         Modbus server/client address (or target device address).
 * @param[in]  function_code   Modbus function code.
 * @param[in]  data            Pointer to the data payload (excluding CRC).
 * @param[in]  data_length     Length of the data payload in bytes.
 * @param[out] out_buffer      Buffer to store the complete frame (including CRC).
 * @param[in]  out_buffer_size Size of the `out_buffer` in bytes.
 * 
 * @return Number of bytes written to `out_buffer`, or 0 if there is not enough space.
 *
 * @retval >0  Number of bytes written to `out_buffer`.
 * @retval 0   Insufficient buffer size to store the complete frame.
 *
 * @warning
 * - Ensure that `out_buffer` has enough space to accommodate `address` (1 byte),
 *   `function_code` (1 byte), `data` (`data_length` bytes), and CRC (2 bytes).
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
                                uint8_t *out_buffer, uint16_t out_buffer_size) {
    const mb_adu_view_t adu = {
        .unit_id = address,
        .function = function_code,
        .payload = data,
        .payload_len = data_length
    };

    mb_size_t produced = 0U;
    if (mb_frame_rtu_encode(&adu, out_buffer, out_buffer_size, &produced) != MODBUS_ERROR_NONE) {
        return 0U;
    }

    return (uint16_t)produced;
}

/**
 * @brief Parses a Modbus RTU frame, verifying CRC.
 *
 * This function validates the CRC of the provided Modbus RTU frame. If the CRC is
 * correct, it extracts the address, function code, and payload from the frame.
 * The payload can then be interpreted based on the function code.
 *
 * @param[in]  frame        Pointer to the frame buffer (address + function + data + CRC).
 * @param[in]  frame_length Length of the frame in bytes.
 * @param[out] address      Pointer to store the parsed address.
 * @param[out] function     Pointer to store the parsed function code.
 * @param[out] payload      Pointer to store the start of the payload within the frame.
 * @param[out] payload_len  Pointer to store the length of the payload (excluding CRC).
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` if successful, or an error/exception code.
 *
 * @retval MODBUS_ERROR_NONE                  Frame parsed and CRC validated successfully.
 * @retval MODBUS_ERROR_INVALID_ARGUMENT       Frame length is too short or pointers are `NULL`.
 * @retval MODBUS_ERROR_CRC                    CRC check failed.
 * @retval MODBUS_EXCEPTION_ILLEGAL_FUNCTION   The function code indicates an error response.
 * @retval MODBUS_ERROR_OTHER                  Other error codes as defined in `modbus_error_t`.
 *
 * @warning
 * - The frame must contain at least 4 bytes: address (1 byte) + function code (1 byte) + CRC (2 bytes).
 * - Ensure that `payload` and `payload_len` are valid pointers before calling this function.
 * 
 * @example
 * ```c
 * uint8_t response_frame[] = {0x01, 0x03, 0x02, 0x00, 0x0A, 0xC4, 0x0B};
 * uint16_t response_length = sizeof(response_frame);
 * uint8_t address, function;
 * const uint8_t *payload;
 * uint16_t payload_len;
 *
 * modbus_error_t error = modbus_parse_rtu_frame(response_frame, response_length, &address, &function, &payload, &payload_len);
 * if (error == MODBUS_ERROR_NONE) {
 *     // Process payload
 * }
 * ```
 */
modbus_error_t modbus_parse_rtu_frame(const uint8_t *frame, uint16_t frame_length,
                                      uint8_t *address, uint8_t *function,
                                      const uint8_t **payload, uint16_t *payload_len) {
    if (!frame || !address || !function || !payload || !payload_len) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    mb_adu_view_t adu = {0};
    modbus_error_t status = mb_frame_rtu_decode(frame, frame_length, &adu);
    if (status != MODBUS_ERROR_NONE) {
        return status;
    }

    *address = adu.unit_id;
    *function = adu.function;

    /* Check if the function code indicates an error response */
    if (modbus_is_error_response(*function)) {
        if (adu.payload_len == 0U || !adu.payload) {
            return MODBUS_ERROR_INVALID_ARGUMENT;
        }
        uint8_t exception_code = adu.payload[0];
        return modbus_exception_to_error(exception_code);
    }

    /* Extract payload */
    *payload = adu.payload;
    *payload_len = (uint16_t)adu.payload_len;

    return MODBUS_ERROR_NONE;
}

/**
 * @brief Sends a Modbus frame using the configured transport.
 *
 * This function attempts to send the provided Modbus RTU frame through the
 * configured transport interface. It ensures that the entire frame is sent.
 * The behavior is non-blocking and depends on the transport implementation.
 *
 * @param[in]  ctx        Pointer to the Modbus context.
 * @param[in]  frame      Pointer to the complete Modbus RTU frame (including CRC).
 * @param[in]  frame_len  Length of the frame in bytes.
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` on successful send, or an error code.
 *
 * @retval MODBUS_ERROR_NONE          Frame sent successfully.
 * @retval MODBUS_ERROR_TRANSPORT     Transport layer encountered an error.
 * @retval MODBUS_ERROR_INVALID_ARGUMENT Invalid context or frame parameters.
 * @retval MODBUS_ERROR_PARTIAL_WRITE Partial frame was written (if transport returns fewer bytes).
 * @retval Others                      Various error codes as defined in `modbus_error_t`.
 *
 * @warning
 * - Ensure that the transport's write function is properly implemented and initialized.
 * - The function does not handle retries; implement retry logic if necessary.
 * 
 * @example
 * ```c
 * uint8_t frame[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x02, 0xC4, 0x0B};
 * modbus_error_t error = modbus_send_frame(&ctx, frame, sizeof(frame));
 * if (error != MODBUS_ERROR_NONE) {
 *     // Handle send error
 * }
 * ```
 */
modbus_error_t modbus_send_frame(modbus_context_t *ctx, const uint8_t *frame, uint16_t frame_len) {
    if (!ctx || !frame || frame_len == 0U) {
        return MODBUS_ERROR_INVALID_ARGUMENT; // Invalid arguments
    }

    const mb_transport_if_t *iface = &ctx->transport_iface;
    mb_transport_io_result_t io = {0};
    const mb_err_t status = mb_transport_send(iface, frame, (mb_size_t)frame_len, &io);
    if (status != MODBUS_ERROR_NONE) {
        return status;
    }

    if (io.processed == (mb_size_t)frame_len) {
        ctx->tx_reference_time = mb_transport_now(iface);
    }

    return (io.processed == (mb_size_t)frame_len) ? MODBUS_ERROR_NONE : MODBUS_ERROR_TRANSPORT;
}

/**
 * @brief Receives a Modbus frame using the configured transport.
 *
 * This function attempts to receive a complete Modbus RTU frame from the transport.
 * It reads data into the provided buffer and performs basic timeout handling.
 * The exact behavior depends on the transport's read implementation.
 *
 * @param[in]  ctx         Pointer to the Modbus context.
 * @param[out] out_buffer  Buffer to store the received Modbus RTU frame.
 * @param[in]  out_size    Size of the `out_buffer` in bytes.
 * @param[out] out_length  Pointer to store the length of the received frame.
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` if a complete and valid frame is received, or an error code.
 *
 * @retval MODBUS_ERROR_NONE          Frame received and validated successfully.
 * @retval MODBUS_ERROR_TIMEOUT       Timeout occurred while waiting for frame.
 * @retval MODBUS_ERROR_CRC           CRC check failed.
 * @retval MODBUS_ERROR_TRANSPORT     Transport layer encountered an error.
 * @retval MODBUS_ERROR_INVALID_ARGUMENT Invalid context or buffer parameters.
 * @retval Others                      Various error codes as defined in `modbus_error_t`.
 *
 * @warning
 * - Ensure that `out_buffer` has enough space to hold the expected frame size.
 * - The function relies on the transport's read implementation for timeout and blocking behavior.
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
modbus_error_t modbus_receive_frame(modbus_context_t *ctx, uint8_t *out_buffer, uint16_t out_size, uint16_t *out_length) {
    if (!ctx || !out_buffer || !out_length || out_size < 4U) {
        return MODBUS_ERROR_INVALID_ARGUMENT; // Invalid arguments
    }

    const mb_transport_if_t *iface = &ctx->transport_iface;
    mb_size_t bytes_read = 0U;
    uint16_t expected_length = 0U;
    uint16_t last_activity_tick = (uint16_t)mb_transport_now(iface);
    const uint16_t frame_origin_tick = last_activity_tick;

    while (bytes_read < (mb_size_t)out_size) {
        mb_transport_io_result_t io = {0};
        const mb_err_t status = mb_transport_recv(iface,
                                                  &out_buffer[bytes_read],
                                                  (mb_size_t)(out_size - (uint16_t)bytes_read),
                                                  &io);

        if (status == MODBUS_ERROR_NONE && io.processed > 0U) {
            bytes_read += io.processed;
            last_activity_tick = (uint16_t)mb_transport_now(iface);

            if (bytes_read >= 3U && expected_length == 0U) {
                const uint8_t function_code = out_buffer[1];
                if ((function_code & MODBUS_FUNC_ERROR_FRAME_HEADER) != 0U) {
                    expected_length = 1U + 1U + 1U + 2U;
                } else {
                    switch (function_code) {
                    case MODBUS_FUNC_READ_COILS:
                    case MODBUS_FUNC_READ_DISCRETE_INPUTS:
                    case MODBUS_FUNC_READ_HOLDING_REGISTERS:
                    case MODBUS_FUNC_READ_INPUT_REGISTERS:
                    case MODBUS_FUNC_WRITE_MULTIPLE_COILS:
                    case MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS:
                    case MODBUS_FUNC_READ_WRITE_MULTIPLE_REGISTERS: {
                        const uint8_t byte_count = out_buffer[2];
                        expected_length = (uint16_t)(3U + byte_count + 2U);
                        break;
                    }
                    case MODBUS_FUNC_WRITE_SINGLE_COIL:
                    case MODBUS_FUNC_WRITE_SINGLE_REGISTER:
                        expected_length = 1U + 1U + 2U + 2U;
                        break;
                    case MODBUS_FUNC_READ_DEVICE_INFORMATION:
                        expected_length = 1U + 1U + 1U + 2U;
                        break;
                    default:
                        expected_length = 1U + 1U + 2U;
                        break;
                    }
                }
            }

            if (expected_length > 0U && bytes_read >= expected_length) {
                break;
            }

            continue;
        }

        if (status == MODBUS_ERROR_TIMEOUT || (status == MODBUS_ERROR_NONE && io.processed == 0U)) {
            const uint16_t now_tick = (uint16_t)mb_transport_now(iface);
            if (((uint16_t)(now_tick - last_activity_tick) > MODBUS_BYTE_TIMEOUT_MS) ||
                ((uint16_t)(now_tick - frame_origin_tick) > MODBUS_INTERFRAME_TIMEOUT_MS)) {
                return MODBUS_ERROR_TIMEOUT;
            }
            mb_transport_yield(iface);
            continue;
        }

        return status;
    }

    if (bytes_read == 0U) {
        return MODBUS_ERROR_TIMEOUT; // No data received
    }

    *out_length = (uint16_t)bytes_read;
    return MODBUS_ERROR_NONE;
}
#endif /* MB_CONF_TRANSPORT_RTU */

/**
 * @brief Converts a Modbus exception code to a `modbus_error_t`.
 *
 * This function maps Modbus exception codes to the corresponding `modbus_error_t`
 * enumeration values. It facilitates the interpretation of error responses from
 * Modbus devices.
 *
 * @param[in]  exception_code The exception code from the Modbus frame.
 * @return A corresponding `modbus_error_t`.
 *
 * @retval MODBUS_EXCEPTION_ILLEGAL_FUNCTION         Exception code 1: Illegal Function
 * @retval MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS     Exception code 2: Illegal Data Address
 * @retval MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE       Exception code 3: Illegal Data Value
 * @retval MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE    Exception code 4: Server Device Failure
 * @retval MODBUS_ERROR_OTHER                        Unknown or unsupported exception codes
 *
 * @warning
 * - Ensure that all relevant exception codes are handled appropriately.
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
modbus_error_t modbus_exception_to_error(uint8_t exception_code) {
    switch (exception_code) {
        case 1U:
            return MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
        case 2U:
            return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        case 3U:
            return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        case 4U:
            return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
        default:
            return MODBUS_ERROR_OTHER; // Unknown exception code
    }
}

/**
 * @brief Convenience function to reset internal RX/TX buffers in the context.
 *
 * This function clears the receive and transmit buffers within the Modbus context,
 * effectively resetting the state of any ongoing or previous transactions. It can be
 * used when starting a new transaction or after encountering an error to ensure that
 * residual data does not interfere with subsequent communications.
 *
 * @param[in,out] ctx Pointer to the Modbus context.
 *
 * @note
 * - This function assumes that the buffers are part of the `modbus_context_t` structure.
 * - After resetting, ensure that any necessary initialization for new transactions is performed.
 * 
 * @example
 * ```c
 * // Reset buffers before starting a new transaction
 * modbus_reset_buffers(&ctx);
 * ```
 */
void modbus_reset_buffers(modbus_context_t *ctx) {
    if (!ctx) {
        return; // Invalid context pointer
    }

    /* Reset RX buffer indices and counters */
    ctx->rx_count = 0U;
    ctx->rx_index = 0U;

    /* Reset TX buffer indices and counters */
    ctx->tx_index = 0U;
    ctx->tx_raw_index = 0U;

    /* Clear RX and TX buffers */
    if (ctx->rx_buffer != NULL && ctx->rx_capacity > 0U) {
        memset(ctx->rx_buffer, 0, ctx->rx_capacity);
    }

    if (ctx->rx_raw_buffer != NULL && ctx->rx_raw_capacity > 0U) {
        memset(ctx->rx_raw_buffer, 0, ctx->rx_raw_capacity);
    }

    if (ctx->tx_buffer != NULL && ctx->tx_capacity > 0U) {
        memset(ctx->tx_buffer, 0, ctx->tx_capacity);
    }

    if (ctx->tx_raw_buffer != NULL && ctx->tx_raw_capacity > 0U) {
        memset(ctx->tx_raw_buffer, 0, ctx->tx_raw_capacity);
    }
}

