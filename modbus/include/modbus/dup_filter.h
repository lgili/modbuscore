/**
 * @file dup_filter.h
 * @brief Duplicate frame filtering for robust Modbus communication
 *
 * Detects and filters duplicate frames caused by retransmissions,
 * line reflections, or network loops using lightweight ADU hashing.
 *
 * Copyright (c) 2025 ModbusCore
 * SPDX-License-Identifier: MIT
 */

#ifndef MODBUS_DUP_FILTER_H
#define MODBUS_DUP_FILTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Duplicate detection window size
 * 
 * Number of recent frame hashes to track.
 * Larger values increase RAM but catch more duplicates.
 */
#ifndef MB_DUP_WINDOW_SIZE
#define MB_DUP_WINDOW_SIZE 8
#endif

/**
 * @brief Duplicate time window (milliseconds)
 * 
 * Frames within this time window are checked for duplicates.
 * Frames older than this are aged out automatically.
 */
#ifndef MB_DUP_WINDOW_MS
#define MB_DUP_WINDOW_MS 500
#endif

/**
 * @brief ADU hash entry
 */
typedef struct {
    uint32_t hash;          /**< Frame hash value */
    uint32_t timestamp_ms;  /**< Timestamp when frame was seen */
} mb_adu_hash_entry_t;

/**
 * @brief Duplicate filter context
 */
typedef struct {
    mb_adu_hash_entry_t entries[MB_DUP_WINDOW_SIZE];  /**< Hash window */
    size_t head;                                       /**< Next write position */
    size_t count;                                      /**< Current entry count */
    uint32_t window_ms;                                /**< Time window */
    uint32_t last_added_hash;                          /**< Hash of most recent addition */
    uint32_t last_added_timestamp;                     /**< Timestamp of most recent addition */
    bool has_last_added;                               /**< Tracks whether last addition is valid */
    
    /* Statistics */
    uint32_t frames_checked;                           /**< Total frames checked */
    uint32_t duplicates_found;                         /**< Duplicates detected */
    uint32_t false_positives;                          /**< Hash collisions */
} mb_dup_filter_t;

/**
 * @brief Initialize duplicate filter
 * 
 * @param df Duplicate filter context
 * @param window_ms Time window for duplicate detection (0 = use default)
 */
void mb_dup_filter_init(mb_dup_filter_t *df, uint32_t window_ms);

/**
 * @brief Compute lightweight ADU hash
 * 
 * Computes a simple hash based on:
 * - Slave address
 * - Function code
 * - First 4 data bytes (if available)
 * 
 * This is intentionally simple and fast, optimized for embedded systems.
 * 
 * @param slave_addr Slave address
 * @param fc Function code
 * @param data Frame data (after FC)
 * @param data_len Data length
 * @return 32-bit hash value
 */
uint32_t mb_adu_hash(uint8_t slave_addr, uint8_t fc, const uint8_t *data, size_t data_len);

/**
 * @brief Check if frame is a recent duplicate
 * 
 * Searches the hash window for a matching hash within the time window.
 * 
 * @param df Duplicate filter context
 * @param hash Frame hash (from mb_adu_hash)
 * @param now_ms Current timestamp
 * @return true if duplicate found, false otherwise
 */
bool mb_dup_filter_check(mb_dup_filter_t *df, uint32_t hash, uint32_t now_ms);

/**
 * @brief Add frame hash to filter
 * 
 * Records this frame's hash so future duplicates can be detected.
 * Automatically evicts oldest entries when window is full.
 * 
 * @param df Duplicate filter context
 * @param hash Frame hash
 * @param now_ms Current timestamp
 */
void mb_dup_filter_add(mb_dup_filter_t *df, uint32_t hash, uint32_t now_ms);

/**
 * @brief Age out old entries
 * 
 * Removes entries older than the time window.
 * Called automatically by check/add, but can be called manually
 * for periodic cleanup.
 * 
 * @param df Duplicate filter context
 * @param now_ms Current timestamp
 * @return Number of entries aged out
 */
size_t mb_dup_filter_age_out(mb_dup_filter_t *df, uint32_t now_ms);

/**
 * @brief Get duplicate filter statistics
 * 
 * @param df Duplicate filter context
 * @param checked Output: total frames checked
 * @param duplicates Output: duplicates found
 * @param false_pos Output: false positives (hash collisions)
 */
void mb_dup_filter_get_stats(const mb_dup_filter_t *df,
                             uint32_t *checked,
                             uint32_t *duplicates,
                             uint32_t *false_pos);

/**
 * @brief Reset duplicate filter statistics
 * 
 * @param df Duplicate filter context
 */
void mb_dup_filter_reset_stats(mb_dup_filter_t *df);

/**
 * @brief Clear all entries from filter
 * 
 * Removes all hash entries. Useful after communication errors
 * or when switching to a different slave.
 * 
 * @param df Duplicate filter context
 */
void mb_dup_filter_clear(mb_dup_filter_t *df);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_DUP_FILTER_H */
