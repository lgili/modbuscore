/**
 * @file mbap.h
 * @brief MBAP (Modbus Application Protocol) framing for Modbus TCP.
 *
 * Modbus TCP uses MBAP header format:
 * [Transaction ID: 2 bytes]
 * [Protocol ID: 2 bytes] (always 0x0000 for Modbus)
 * [Length: 2 bytes] (number of following bytes, including unit ID)
 * [Unit ID: 1 byte]
 * [PDU: N bytes]
 */

#ifndef MBC_PROTOCOL_MBAP_H
#define MBC_PROTOCOL_MBAP_H

#include <modbuscore/common/status.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** MBAP header size (7 bytes) */
#define MBC_MBAP_HEADER_SIZE 7U

/** Maximum Modbus TCP frame size (260 bytes: 7 MBAP + 253 PDU) */
#define MBC_MBAP_MAX_FRAME_SIZE 260U

/**
 * @brief MBAP header structure.
 */
typedef struct {
    uint16_t transaction_id; /**< Transaction identifier for request/response matching */
    uint16_t protocol_id;    /**< Protocol identifier (always 0 for Modbus) */
    uint16_t length;         /**< Number of following bytes (unit_id + PDU length) */
    uint8_t unit_id;         /**< Unit/slave identifier */
} mbc_mbap_header_t;

/**
 * @brief Encode MBAP header + PDU into a TCP frame.
 *
 * @param header Pointer to MBAP header to encode
 * @param pdu_buffer Pointer to PDU data (without unit ID)
 * @param pdu_length Length of PDU data
 * @param out_buffer Output buffer for complete MBAP frame
 * @param out_capacity Capacity of output buffer
 * @param out_length Pointer to store actual frame length
 * @return Status code
 */
mbc_status_t mbc_mbap_encode(const mbc_mbap_header_t* header, const uint8_t* pdu_buffer,
                             size_t pdu_length, uint8_t* out_buffer, size_t out_capacity,
                             size_t* out_length);

/**
 * @brief Decode MBAP frame into header + PDU.
 *
 * @param frame_buffer Input buffer containing complete MBAP frame
 * @param frame_length Length of frame buffer
 * @param out_header Pointer to store decoded MBAP header
 * @param out_pdu Pointer to PDU data within frame (points into frame_buffer)
 * @param out_pdu_length Pointer to store PDU length
 * @return Status code
 */
mbc_status_t mbc_mbap_decode(const uint8_t* frame_buffer, size_t frame_length,
                             mbc_mbap_header_t* out_header, const uint8_t** out_pdu,
                             size_t* out_pdu_length);

/**
 * @brief Determine expected MBAP frame length from partial buffer.
 *
 * This function examines a partial MBAP frame and determines how many
 * total bytes are expected. Returns 0 if not enough data to determine.
 *
 * @param buffer Partial frame buffer (at least 6 bytes needed)
 * @param available Number of bytes currently available
 * @return Expected total frame length, or 0 if insufficient data
 */
size_t mbc_mbap_expected_length(const uint8_t* buffer, size_t available);

#ifdef __cplusplus
}
#endif

#endif /* MBC_PROTOCOL_MBAP_H */
