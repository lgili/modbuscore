/**
 * @file rtu_resync.h
 * @brief Fast resynchronization mechanism for RTU transport
 *
 * Provides fast frame boundary detection and resynchronization after
 * corruption or noise on RTU links. Uses address field scanning and
 * CRC prechecking to quickly identify valid frame starts.
 *
 * Copyright (c) 2025 ModbusCore
 * SPDX-License-Identifier: MIT
 */

#ifndef MODBUS_RTU_RESYNC_H
#define MODBUS_RTU_RESYNC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Resync buffer size
 * 
 * Size of circular buffer for resynchronization scanning.
 * Larger values provide more tolerance for long noise bursts.
 */
#ifndef MB_RESYNC_BUFFER_SIZE
#define MB_RESYNC_BUFFER_SIZE 256
#endif

/**
 * @brief Valid slave address range
 */
#define MB_SLAVE_ADDR_MIN 1
#define MB_SLAVE_ADDR_MAX 247

/**
 * @brief RTU resync context
 */
typedef struct {
    uint8_t buffer[MB_RESYNC_BUFFER_SIZE];  /**< Circular buffer */
    size_t head;                             /**< Write position */
    size_t tail;                             /**< Read position */
    size_t candidate_pos;                    /**< Current scan position */
    
    /* Statistics */
    uint32_t resync_attempts;                /**< Number of resync attempts */
    uint32_t bytes_discarded;                /**< Total bytes discarded */
    uint32_t frames_recovered;               /**< Frames found via resync */
} mb_rtu_resync_t;

/**
 * @brief Initialize resync context
 * 
 * @param rs Resync context to initialize
 */
void mb_rtu_resync_init(mb_rtu_resync_t *rs);

/**
 * @brief Add received bytes to resync buffer
 * 
 * @param rs Resync context
 * @param data Received data
 * @param len Length of data
 * @return Number of bytes added
 */
size_t mb_rtu_resync_add_data(mb_rtu_resync_t *rs, const uint8_t *data, size_t len);

/**
 * @brief Find potential frame start in buffer
 * 
 * Scans buffer for valid slave address (1-247) that could indicate
 * the start of a frame.
 * 
 * @param rs Resync context
 * @return Offset to candidate frame start, or -1 if none found
 */
int mb_rtu_find_frame_start(mb_rtu_resync_t *rs);

/**
 * @brief Quick CRC validation without full parse
 * 
 * Performs fast CRC check on data to validate frame integrity
 * before attempting full parsing.
 * 
 * @param data Frame data
 * @param len Frame length (must include 2-byte CRC)
 * @return true if CRC is valid, false otherwise
 */
bool mb_rtu_quick_crc_check(const uint8_t *data, size_t len);

/**
 * @brief Validate slave address
 * 
 * @param addr Address to validate
 * @return true if address is in valid range (1-247)
 */
static inline bool mb_rtu_is_valid_slave_addr(uint8_t addr) {
    return (addr >= MB_SLAVE_ADDR_MIN && addr <= MB_SLAVE_ADDR_MAX);
}

/**
 * @brief Discard bytes from resync buffer
 * 
 * @param rs Resync context
 * @param count Number of bytes to discard
 */
void mb_rtu_resync_discard(mb_rtu_resync_t *rs, size_t count);

/**
 * @brief Get number of bytes available in resync buffer
 * 
 * @param rs Resync context
 * @return Number of bytes available
 */
size_t mb_rtu_resync_available(const mb_rtu_resync_t *rs);

/**
 * @brief Copy data from resync buffer
 * 
 * @param rs Resync context
 * @param dest Destination buffer
 * @param count Number of bytes to copy
 * @return Number of bytes copied
 */
size_t mb_rtu_resync_copy(mb_rtu_resync_t *rs, uint8_t *dest, size_t count);

/**
 * @brief Get resync statistics
 * 
 * @param rs Resync context
 * @param attempts Output: number of resync attempts
 * @param discarded Output: total bytes discarded
 * @param recovered Output: frames recovered via resync
 */
void mb_rtu_resync_get_stats(const mb_rtu_resync_t *rs,
                             uint32_t *attempts,
                             uint32_t *discarded,
                             uint32_t *recovered);

/**
 * @brief Reset resync statistics
 * 
 * @param rs Resync context
 */
void mb_rtu_resync_reset_stats(mb_rtu_resync_t *rs);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_RTU_RESYNC_H */
