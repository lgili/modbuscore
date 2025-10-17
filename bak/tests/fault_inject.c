/**
 * @file fault_inject.c
 * @brief Fault injection framework implementation
 *
 * Copyright (c) 2025 ModbusCore
 * SPDX-License-Identifier: MIT
 */

#include "fault_inject.h"
#include <string.h>

/* Forward declarations of transport interface functions */
static mb_err_t fault_inject_send(void *ctx, const uint8_t *data, size_t len);
static mb_err_t fault_inject_recv(void *ctx, uint8_t *buf, size_t buf_size, size_t *received);
static mb_err_t fault_inject_poll(void *ctx);
static void fault_inject_reset(void *ctx);

mb_err_t mb_fault_inject_create(
    mb_fault_inject_transport_t *fit,
    const mb_transport_if_t *inner_transport,
    uint32_t seed
) {
    if (!fit || !inner_transport) {
        return MB_ERR_NULL_PTR;
    }
    
    memset(fit, 0, sizeof(*fit));
    
    fit->inner = inner_transport;
    fit->fault_count = 0;
    fit->rng_state = (seed != 0) ? seed : 0x12345678;
    fit->has_pending = false;
    fit->pending_len = 0;
    
    /* Setup transport interface */
    fit->transport_if.ctx = fit;
    fit->transport_if.send = fault_inject_send;
    fit->transport_if.recv = fault_inject_recv;
    fit->transport_if.poll = fault_inject_poll;
    fit->transport_if.reset = fault_inject_reset;
    
    return MB_OK;
}

mb_err_t mb_fault_inject_add_rule(
    mb_fault_inject_transport_t *fit,
    mb_fault_type_t type,
    float probability,
    uint32_t param
) {
    if (!fit) {
        return MB_ERR_NULL_PTR;
    }
    
    if (fit->fault_count >= 8) {
        return MB_ERR_NO_MEM;  /* Max 8 rules */
    }
    
    /* Clamp probability */
    if (probability < 0.0f) probability = 0.0f;
    if (probability > 1.0f) probability = 1.0f;
    
    mb_fault_config_t *rule = &fit->faults[fit->fault_count++];
    rule->type = type;
    rule->probability = probability;
    rule->param = param;
    rule->count = 0;
    
    return MB_OK;
}

size_t mb_fault_inject_bit_flip(
    uint8_t *data,
    size_t len,
    float probability,
    uint32_t *rng_state
) {
    if (!data || !rng_state || len == 0) return 0;
    
    size_t flips = 0;
    
    for (size_t i = 0; i < len; i++) {
        for (int bit = 0; bit < 8; bit++) {
            if (mb_fault_prng_float(rng_state) < probability) {
                data[i] ^= (1 << bit);
                flips++;
            }
        }
    }
    
    return flips;
}

size_t mb_fault_inject_truncate(
    uint8_t *data,
    size_t len,
    size_t max_drop,
    uint32_t *rng_state
) {
    (void)data;  /* Not modified, just truncated */
    
    if (!rng_state || len == 0 || max_drop == 0) return len;
    
    /* Drop 1 to max_drop bytes */
    size_t drop = (mb_fault_prng(rng_state) % max_drop) + 1;
    if (drop > len) drop = len;
    
    return len - drop;
}

size_t mb_fault_inject_phantom(
    uint8_t *data,
    size_t len,
    size_t max_len,
    size_t max_insert,
    uint32_t *rng_state
) {
    if (!data || !rng_state || len >= max_len || max_insert == 0) return len;
    
    /* Determine how many bytes to insert */
    size_t insert = (mb_fault_prng(rng_state) % max_insert) + 1;
    if (len + insert > max_len) {
        insert = max_len - len;
    }
    if (insert == 0) return len;
    
    /* Random insertion point */
    size_t pos = mb_fault_prng(rng_state) % (len + 1);
    
    /* Shift data to make room */
    if (pos < len) {
        memmove(&data[pos + insert], &data[pos], len - pos);
    }
    
    /* Insert random bytes */
    for (size_t i = 0; i < insert; i++) {
        data[pos + i] = (uint8_t)mb_fault_prng(rng_state);
    }
    
    return len + insert;
}

/* Apply faults to outgoing data */
static size_t apply_faults(
    mb_fault_inject_transport_t *fit,
    uint8_t *data,
    size_t len,
    size_t max_len,
    bool *should_drop
) {
    *should_drop = false;
    
    for (size_t i = 0; i < fit->fault_count; i++) {
        mb_fault_config_t *rule = &fit->faults[i];
        
        /* Check if this fault should trigger */
        if (mb_fault_prng_float(&fit->rng_state) >= rule->probability) {
            continue;
        }
        
        rule->count++;
        fit->stats.total_injected++;
        
        switch (rule->type) {
            case FAULT_BIT_FLIP: {
                float bit_prob = (rule->param > 0) ? (rule->param / 100.0f) : 0.01f;
                size_t flips = mb_fault_inject_bit_flip(data, len, bit_prob, &fit->rng_state);
                if (flips > 0) fit->stats.bit_flips++;
                break;
            }
            
            case FAULT_TRUNCATE: {
                size_t max_drop = (rule->param > 0) ? rule->param : 4;
                len = mb_fault_inject_truncate(data, len, max_drop, &fit->rng_state);
                fit->stats.truncations++;
                break;
            }
            
            case FAULT_PHANTOM_BYTES: {
                size_t max_insert = (rule->param > 0) ? rule->param : 8;
                len = mb_fault_inject_phantom(data, len, max_len, max_insert, &fit->rng_state);
                fit->stats.phantom_insertions++;
                break;
            }
            
            case FAULT_DROP_FRAME:
                *should_drop = true;
                fit->stats.drops++;
                return 0;
            
            case FAULT_DUPLICATE:
                /* Store frame for duplication on next send */
                if (!fit->has_pending && len <= sizeof(fit->pending_frame)) {
                    memcpy(fit->pending_frame, data, len);
                    fit->pending_len = len;
                    fit->has_pending = true;
                    fit->stats.duplications++;
                }
                break;
            
            case FAULT_MERGE_FRAMES:
                /* Will be handled by sending without delay */
                fit->stats.merges++;
                break;
            
            case FAULT_DELAY:
                /* Delay would be handled by transport layer */
                fit->stats.delays++;
                break;
            
            default:
                break;
        }
    }
    
    return len;
}

static mb_err_t fault_inject_send(void *ctx, const uint8_t *data, size_t len) {
    mb_fault_inject_transport_t *fit = (mb_fault_inject_transport_t *)ctx;
    if (!fit || !fit->inner || !data) {
        return MB_ERR_NULL_PTR;
    }
    
    fit->stats.total_frames++;
    
    /* Copy data to mutable buffer */
    uint8_t buf[512];
    if (len > sizeof(buf)) {
        return MB_ERR_NO_MEM;
    }
    memcpy(buf, data, len);
    
    /* Apply faults */
    bool should_drop = false;
    len = apply_faults(fit, buf, len, sizeof(buf), &should_drop);
    
    if (should_drop) {
        return MB_OK;  /* Pretend success, but don't send */
    }
    
    /* Send modified/original frame */
    mb_err_t err = fit->inner->send(fit->inner->ctx, buf, len);
    
    /* If we have a pending duplicate, send it now */
    if (fit->has_pending) {
        fit->inner->send(fit->inner->ctx, fit->pending_frame, fit->pending_len);
        fit->has_pending = false;
    }
    
    return err;
}

static mb_err_t fault_inject_recv(void *ctx, uint8_t *buf, size_t buf_size, size_t *received) {
    mb_fault_inject_transport_t *fit = (mb_fault_inject_transport_t *)ctx;
    if (!fit || !fit->inner) {
        return MB_ERR_NULL_PTR;
    }
    
    /* For now, just pass through (faults mainly on TX) */
    return fit->inner->recv(fit->inner->ctx, buf, buf_size, received);
}

static mb_err_t fault_inject_poll(void *ctx) {
    mb_fault_inject_transport_t *fit = (mb_fault_inject_transport_t *)ctx;
    if (!fit || !fit->inner) {
        return MB_ERR_NULL_PTR;
    }
    
    return fit->inner->poll(fit->inner->ctx);
}

static void fault_inject_reset(void *ctx) {
    mb_fault_inject_transport_t *fit = (mb_fault_inject_transport_t *)ctx;
    if (!fit || !fit->inner) {
        return;
    }
    
    fit->has_pending = false;
    fit->pending_len = 0;
    
    if (fit->inner->reset) {
        fit->inner->reset(fit->inner->ctx);
    }
}

void mb_fault_inject_get_stats(
    const mb_fault_inject_transport_t *fit,
    mb_fault_stats_t *stats
) {
    if (!fit || !stats) return;
    
    *stats = fit->stats;
}

void mb_fault_inject_reset_stats(mb_fault_inject_transport_t *fit) {
    if (!fit) return;
    
    memset(&fit->stats, 0, sizeof(fit->stats));
    
    for (size_t i = 0; i < fit->fault_count; i++) {
        fit->faults[i].count = 0;
    }
}

void mb_fault_inject_set_enabled(mb_fault_inject_transport_t *fit, bool enabled) {
    (void)fit;
    (void)enabled;
    /* Could add enable/disable flag if needed */
}
