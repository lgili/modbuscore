/**
 * @file rtu_resync.c
 * @brief Fast resynchronization implementation for RTU transport
 *
 * Copyright (c) 2025 ModbusCore
 * SPDX-License-Identifier: MIT
 */

#include "modbus/rtu_resync.h"
#include "modbus/utils.h"
#include <string.h>

void mb_rtu_resync_init(mb_rtu_resync_t *rs) {
    if (!rs) return;
    
    memset(rs, 0, sizeof(*rs));
    rs->head = 0;
    rs->tail = 0;
    rs->candidate_pos = 0;
}

size_t mb_rtu_resync_add_data(mb_rtu_resync_t *rs, const uint8_t *data, size_t len) {
    if (!rs || !data) return 0;
    
    size_t added = 0;
    for (size_t i = 0; i < len; i++) {
        /* Check if buffer is full */
        size_t next_head = (rs->head + 1) % MB_RESYNC_BUFFER_SIZE;
        if (next_head == rs->tail) {
            /* Buffer full, discard oldest byte */
            rs->tail = (rs->tail + 1) % MB_RESYNC_BUFFER_SIZE;
            rs->bytes_discarded++;
        }
        
        rs->buffer[rs->head] = data[i];
        rs->head = next_head;
        added++;
    }
    
    return added;
}

int mb_rtu_find_frame_start(mb_rtu_resync_t *rs) {
    if (!rs) return -1;
    
    size_t available = mb_rtu_resync_available(rs);
    if (available < 4) {  /* Minimum frame: addr + FC + 2-byte CRC */
        return -1;
    }
    
    /* Scan from candidate_pos looking for valid slave address */
    size_t scan_pos = rs->candidate_pos;
    size_t scan_count = 0;
    
    while (scan_count < available) {
        size_t abs_pos = (rs->tail + scan_pos) % MB_RESYNC_BUFFER_SIZE;
        uint8_t byte = rs->buffer[abs_pos];
        
        /* Check if this looks like a valid slave address */
        if (mb_rtu_is_valid_slave_addr(byte)) {
            /* Potential frame start found */
            rs->candidate_pos = scan_pos;
            return (int)scan_pos;
        }
        
        scan_pos++;
        scan_count++;
    }
    
    /* No candidate found */
    rs->candidate_pos = 0;
    return -1;
}

bool mb_rtu_quick_crc_check(const uint8_t *data, size_t len) {
    if (!data || len < 4) {  /* Minimum: addr + FC + 2-byte CRC */
        return false;
    }
    
    /* Calculate CRC on data (excluding last 2 bytes) */
    uint16_t crc_calc = modbus_crc_with_table(data, (uint16_t)(len - 2));
    
    /* Extract received CRC (little-endian) */
    uint16_t crc_recv = ((uint16_t)data[len-1] << 8) | data[len-2];
    
    return (crc_calc == crc_recv);
}

void mb_rtu_resync_discard(mb_rtu_resync_t *rs, size_t count) {
    if (!rs) return;
    
    size_t available = mb_rtu_resync_available(rs);
    if (count > available) {
        count = available;
    }
    
    rs->tail = (rs->tail + count) % MB_RESYNC_BUFFER_SIZE;
    if (count > 0U) {
        uint32_t increment = (count > UINT32_MAX) ? UINT32_MAX : (uint32_t)count;
        uint32_t remaining = UINT32_MAX - rs->bytes_discarded;
        if (increment > remaining) {
            rs->bytes_discarded = UINT32_MAX;
        } else {
            rs->bytes_discarded += increment;
        }
    }
    
    /* Reset candidate position */
    if (rs->candidate_pos >= count) {
        rs->candidate_pos -= count;
    } else {
        rs->candidate_pos = 0;
    }
}

size_t mb_rtu_resync_available(const mb_rtu_resync_t *rs) {
    if (!rs) return 0;
    
    if (rs->head >= rs->tail) {
        return rs->head - rs->tail;
    } else {
        return MB_RESYNC_BUFFER_SIZE - rs->tail + rs->head;
    }
}

size_t mb_rtu_resync_copy(mb_rtu_resync_t *rs, uint8_t *dest, size_t count) {
    if (!rs || !dest) return 0;
    
    size_t available = mb_rtu_resync_available(rs);
    if (count > available) {
        count = available;
    }
    
    size_t copied = 0;
    size_t pos = rs->tail;
    
    for (size_t i = 0; i < count; i++) {
        dest[i] = rs->buffer[pos];
        pos = (pos + 1) % MB_RESYNC_BUFFER_SIZE;
        copied++;
    }
    
    return copied;
}

void mb_rtu_resync_get_stats(const mb_rtu_resync_t *rs,
                             uint32_t *attempts,
                             uint32_t *discarded,
                             uint32_t *recovered) {
    if (!rs) return;
    
    if (attempts) *attempts = rs->resync_attempts;
    if (discarded) *discarded = rs->bytes_discarded;
    if (recovered) *recovered = rs->frames_recovered;
}

void mb_rtu_resync_reset_stats(mb_rtu_resync_t *rs) {
    if (!rs) return;
    
    rs->resync_attempts = 0;
    rs->bytes_discarded = 0;
    rs->frames_recovered = 0;
}
