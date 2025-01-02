/**
 * @file modbus_core.c
 * @brief Core Modbus protocol implementation for both Master and Slave.
 *
 * This file implements the functions declared in modbus_core.h.
 * It provides core functionality for constructing, parsing, sending,
 * and receiving Modbus RTU frames.
 *
 * Key functionalities:
 * - Adding CRC to frames (modbus_build_rtu_frame)
 * - Parsing and validating received frames (modbus_parse_rtu_frame)
 * - Abstracted send/receive functions using the transport interface
 * - Handling of Modbus exceptions
 * - Resetting RX/TX buffers
 * 
 * It leaves higher-level logic (e.g., deciding which registers to read or how to respond
 * to specific function codes) to the Master or Slave specific code.
 * 
 * The timeouts and exact non-blocking behavior depend on the transport's configuration.
 * For instance, if the transport's read function is non-blocking, modbus_receive_frame()
 * may need to be called repeatedly until a complete frame is obtained or a timeout occurs.
 *
 * @author Luiz Carlos Gili
 * @date 2024-12-20
 */

#include <modbus/core.h>
#include <string.h>

/* Internal defines for timeouts, if needed */
#ifndef MODBUS_INTERFRAME_TIMEOUT_MS
#define MODBUS_INTERFRAME_TIMEOUT_MS 100
#endif

#ifndef MODBUS_BYTE_TIMEOUT_MS
#define MODBUS_BYTE_TIMEOUT_MS 50
#endif

uint16_t modbus_build_rtu_frame(uint8_t address, uint8_t function_code,
                                const uint8_t *data, uint16_t data_length,
                                uint8_t *out_buffer, uint16_t out_buffer_size) {
    if (out_buffer_size < (data_length + 4U)) {
        // Need at least address(1) + function(1) + CRC(2)
        return 0;
    }

    uint16_t index = 0;
    out_buffer[index++] = address;
    out_buffer[index++] = function_code;

    if (data && data_length > 0) {
        if ((data_length + 4U) > out_buffer_size) {
            // Not enough space
            return 0;
        }
        memcpy(&out_buffer[index], data, data_length);
        index += data_length;
    }

    uint16_t crc = modbus_crc_with_table(out_buffer, index);
    out_buffer[index++] = GET_LOW_BYTE(crc);
    out_buffer[index++] = GET_HIGH_BYTE(crc);

    return index; // Total frame length
}

modbus_error_t modbus_parse_rtu_frame(const uint8_t *frame, uint16_t frame_length,
                                      uint8_t *address, uint8_t *function,
                                      const uint8_t **payload, uint16_t *payload_len) {
    if (frame_length < 4U) {
        return MODBUS_ERROR_INVALID_ARGUMENT; // Too short (address+function+CRC is min)
    }

    // Extract and verify CRC
    uint16_t calc_crc = modbus_crc_with_table(frame, (uint16_t)(frame_length - 2U));
    uint16_t recv_crc = (uint16_t)(frame[frame_length - 1U] << 8U) | frame[frame_length - 2U];

    if (calc_crc != recv_crc) {
        return MODBUS_ERROR_CRC;
    }

    *address = frame[0];
    *function = frame[1];

    // Payload is everything after address+function, before CRC
    *payload = &frame[2];
    *payload_len = (uint16_t)(frame_length - 4U); // subtract address(1), func(1), CRC(2)

    return MODBUS_ERROR_NONE;
}

modbus_error_t modbus_send_frame(modbus_context_t *ctx, const uint8_t *frame, uint16_t frame_len) {
    if (!ctx || !frame || frame_len == 0) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    int32_t written = ctx->transport.write(frame, frame_len);
    if (written < 0) {
        return MODBUS_ERROR_TRANSPORT;
    } else if ((uint16_t)written < frame_len) {
        // Partial write could be considered a timeout or error
        return MODBUS_ERROR_TRANSPORT;
    }

    return MODBUS_ERROR_NONE;
}

modbus_error_t modbus_receive_frame(modbus_context_t *ctx, uint8_t *out_buffer, uint16_t out_size, uint16_t *out_length) {
    if (!ctx || !out_buffer || out_size < 4U) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    // Start reading at out_buffer[0]
    uint16_t bytes_read = 0;

    uint16_t ref_time = ctx->transport.get_reference_msec();

    int32_t r = ctx->transport.read(out_buffer, (uint16_t)out_size);
    *out_length = r;

    return MODBUS_ERROR_NONE;
}

modbus_error_t modbus_exception_to_error(uint8_t exception_code) {
    switch (exception_code) {
        case 1: return MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
        case 2: return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        case 3: return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
        case 4: return MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE;
        default: return MODBUS_ERROR_OTHER; // Unknown exception code
    }
}

void modbus_reset_buffers(modbus_context_t *ctx) {
    if (!ctx) return;
    ctx->rx_count = 0;
    ctx->rx_index = 0;
    ctx->tx_index = 0;
    ctx->tx_raw_index = 0;
    memset(ctx->rx_buffer, 0, sizeof(ctx->rx_buffer));
    memset(ctx->tx_buffer, 0, sizeof(ctx->tx_buffer));
    memset(ctx->tx_raw_buffer, 0, sizeof(ctx->tx_raw_buffer));
}
