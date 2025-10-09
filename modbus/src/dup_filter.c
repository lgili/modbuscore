/**
 * @file dup_filter.c
 * @brief Duplicate frame filtering implementation
 *
 * Copyright (c) 2025 ModbusCore
 * SPDX-License-Identifier: MIT
 */

#include "modbus/dup_filter.h"
#include <string.h>

void mb_dup_filter_init(mb_dup_filter_t *df, uint32_t window_ms) {
    if (!df) return;
    
    memset(df, 0, sizeof(*df));
    df->window_ms = (window_ms > 0) ? window_ms : MB_DUP_WINDOW_MS;
    df->head = 0;
    df->count = 0;
    df->has_last_added = false;
}

uint32_t mb_adu_hash(uint8_t slave_addr, uint8_t fc, const uint8_t *data, size_t data_len) {
    /* Simple FNV-1a hash variant optimized for embedded */
    uint32_t hash = 2166136261U;  /* FNV offset basis */
    
    /* Hash slave address */
    hash ^= slave_addr;
    hash *= 16777619U;  /* FNV prime */
    
    /* Hash function code */
    hash ^= fc;
    hash *= 16777619U;
    
    /* Hash first 4 data bytes (if available) */
    size_t hash_len = (data_len < 4) ? data_len : 4;
    for (size_t i = 0; i < hash_len; i++) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    
    return hash;
}

bool mb_dup_filter_check(mb_dup_filter_t *df, uint32_t hash, uint32_t now_ms) {
    if (!df) return false;
    
    df->frames_checked++;
    
    /* Age out old entries first */
    mb_dup_filter_age_out(df, now_ms);
    
    /* Linear search through window (acceptable for small N) */
    for (size_t i = 0; i < df->count; i++) {
        if (df->entries[i].hash == hash) {
            /* Skip the entry we just inserted for this timestamp */
            if (df->has_last_added &&
                df->entries[i].timestamp_ms == df->last_added_timestamp &&
                df->entries[i].hash == df->last_added_hash &&
                now_ms == df->last_added_timestamp) {
                df->has_last_added = false;
                continue;
            }

            /* Check if within time window */
            uint32_t age_ms = now_ms - df->entries[i].timestamp_ms;
            if (age_ms <= df->window_ms) {
                df->duplicates_found++;
                return true;  /* Duplicate found */
            }
        }
    }
    
    return false;  /* Not a duplicate */
}

void mb_dup_filter_add(mb_dup_filter_t *df, uint32_t hash, uint32_t now_ms) {
    if (!df) return;
    
    /* Age out old entries before adding new one */
    mb_dup_filter_age_out(df, now_ms);
    
    /* If window is full, overwrite oldest entry (FIFO) */
    if (df->count >= MB_DUP_WINDOW_SIZE) {
        df->head = (df->head + 1) % MB_DUP_WINDOW_SIZE;
    } else {
        df->count++;
    }
    
    /* Add new entry */
    size_t write_pos = df->head;
    df->entries[write_pos].hash = hash;
    df->entries[write_pos].timestamp_ms = now_ms;
    
    df->head = (df->head + 1) % MB_DUP_WINDOW_SIZE;
    df->last_added_hash = hash;
    df->last_added_timestamp = now_ms;
    df->has_last_added = true;
}

size_t mb_dup_filter_age_out(mb_dup_filter_t *df, uint32_t now_ms) {
    if (!df || df->count == 0) return 0;
    
    size_t aged_out = 0;
    size_t i = 0;
    
    while (i < df->count) {
        uint32_t entry_ts = df->entries[i].timestamp_ms;
        uint32_t entry_hash = df->entries[i].hash;
        uint32_t age_ms = now_ms - entry_ts;
        
        if (age_ms > df->window_ms) {
            /* Entry too old, remove it by shifting remaining entries */
            for (size_t j = i; j < df->count - 1; j++) {
                df->entries[j] = df->entries[j + 1];
            }
            df->count--;
            aged_out++;
            
            /* Adjust head pointer if needed */
            if (df->head > 0) {
                df->head--;
            }

            if (df->has_last_added &&
                entry_ts == df->last_added_timestamp &&
                entry_hash == df->last_added_hash) {
                df->has_last_added = false;
            }
            
            /* Don't increment i, check the shifted entry */
        } else {
            i++;
        }
    }

    if (df->count == 0) {
        df->has_last_added = false;
    }
    
    return aged_out;
}

void mb_dup_filter_get_stats(const mb_dup_filter_t *df,
                             uint32_t *checked,
                             uint32_t *duplicates,
                             uint32_t *false_pos) {
    if (!df) return;
    
    if (checked) *checked = df->frames_checked;
    if (duplicates) *duplicates = df->duplicates_found;
    if (false_pos) *false_pos = df->false_positives;
}

void mb_dup_filter_reset_stats(mb_dup_filter_t *df) {
    if (!df) return;
    
    df->frames_checked = 0;
    df->duplicates_found = 0;
    df->false_positives = 0;
}

void mb_dup_filter_clear(mb_dup_filter_t *df) {
    if (!df) return;
    
    df->head = 0;
    df->count = 0;
    memset(df->entries, 0, sizeof(df->entries));
    df->has_last_added = false;
    df->last_added_hash = 0;
    df->last_added_timestamp = 0;
}
