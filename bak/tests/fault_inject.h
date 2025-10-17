/**
 * @file fault_inject.h
 * @brief Fault injection framework for robustness testing
 *
 * Provides systematic fault injection for testing Modbus robustness:
 * - Bit flips (random bit errors)
 * - Truncation (partial frames)
 * - Phantom bytes (garbage insertion)
 * - Frame duplication
 * - Frame merging (T3.5 violations)
 *
 * Copyright (c) 2025 ModbusCore
 * SPDX-License-Identifier: MIT
 */

#ifndef MODBUS_FAULT_INJECT_H
#define MODBUS_FAULT_INJECT_H

#include "modbus/transport.h"
#include "modbus/mb_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fault injection types
 */
typedef enum {
    FAULT_NONE = 0,           /**< No fault */
    FAULT_BIT_FLIP,           /**< Random bit flip in frame */
    FAULT_TRUNCATE,           /**< Drop last N bytes */
    FAULT_PHANTOM_BYTES,      /**< Insert random bytes */
    FAULT_DUPLICATE,          /**< Duplicate entire frame */
    FAULT_MERGE_FRAMES,       /**< Concatenate frames without T3.5 */
    FAULT_DROP_FRAME,         /**< Drop entire frame */
    FAULT_DELAY,              /**< Add random delay */
    FAULT_TYPE_COUNT
} mb_fault_type_t;

/**
 * @brief Fault injection configuration
 */
typedef struct {
    mb_fault_type_t type;     /**< Fault type */
    float probability;        /**< 0.0 - 1.0 */
    uint32_t seed;            /**< RNG seed for reproducibility */
    uint32_t count;           /**< How many faults injected */
    uint32_t param;           /**< Type-specific parameter */
} mb_fault_config_t;

/**
 * @brief Fault injection statistics
 */
typedef struct {
    uint32_t total_frames;         /**< Total frames processed */
    uint32_t total_injected;       /**< Total faults injected */
    uint32_t bit_flips;            /**< Bit flip faults */
    uint32_t truncations;          /**< Truncation faults */
    uint32_t phantom_insertions;   /**< Phantom byte faults */
    uint32_t duplications;         /**< Frame duplication faults */
    uint32_t merges;               /**< Frame merge faults */
    uint32_t drops;                /**< Frame drop faults */
    uint32_t delays;               /**< Delay faults */
} mb_fault_stats_t;

/**
 * @brief Fault injection transport wrapper
 */
typedef struct {
    const mb_transport_if_t *inner;       /**< Wrapped transport */
    mb_fault_config_t faults[8];          /**< Up to 8 fault rules */
    size_t fault_count;                   /**< Number of active rules */
    
    /* Statistics */
    mb_fault_stats_t stats;
    
    /* Internal state */
    uint32_t rng_state;                   /**< RNG state */
    uint8_t pending_frame[512];           /**< Pending duplicate/merge */
    size_t pending_len;                   /**< Pending frame length */
    bool has_pending;                     /**< Has pending frame */
    
    /* Transport interface (exported) */
    mb_transport_if_t transport_if;
} mb_fault_inject_transport_t;

/**
 * @brief Create fault injection wrapper around transport
 * 
 * @param fit Fault injection transport to initialize
 * @param inner_transport Transport to wrap
 * @param seed RNG seed (0 = use default)
 * @return MB_OK on success
 */
mb_err_t mb_fault_inject_create(
    mb_fault_inject_transport_t *fit,
    const mb_transport_if_t *inner_transport,
    uint32_t seed
);

/**
 * @brief Add fault injection rule
 * 
 * @param fit Fault injection transport
 * @param type Fault type
 * @param probability Injection probability (0.0 - 1.0)
 * @param param Type-specific parameter (e.g., max bytes for truncate)
 * @return MB_OK on success
 */
mb_err_t mb_fault_inject_add_rule(
    mb_fault_inject_transport_t *fit,
    mb_fault_type_t type,
    float probability,
    uint32_t param
);

/**
 * @brief Inject random bit flip
 * 
 * Flips random bits in data with given probability per bit.
 * 
 * @param data Data buffer to modify
 * @param len Data length
 * @param probability Bit flip probability (0.0 - 1.0)
 * @param rng_state RNG state (updated)
 * @return Number of bits flipped
 */
size_t mb_fault_inject_bit_flip(
    uint8_t *data,
    size_t len,
    float probability,
    uint32_t *rng_state
);

/**
 * @brief Inject truncation (drop last N bytes)
 * 
 * @param data Data buffer
 * @param len Original length
 * @param max_drop Maximum bytes to drop
 * @param rng_state RNG state (updated)
 * @return New length after truncation
 */
size_t mb_fault_inject_truncate(
    uint8_t *data,
    size_t len,
    size_t max_drop,
    uint32_t *rng_state
);

/**
 * @brief Inject phantom bytes
 * 
 * Inserts random garbage bytes at random position.
 * 
 * @param data Data buffer
 * @param len Original length
 * @param max_len Maximum buffer size
 * @param max_insert Maximum bytes to insert
 * @param rng_state RNG state (updated)
 * @return New length after insertion
 */
size_t mb_fault_inject_phantom(
    uint8_t *data,
    size_t len,
    size_t max_len,
    size_t max_insert,
    uint32_t *rng_state
);

/**
 * @brief Get fault injection statistics
 * 
 * @param fit Fault injection transport
 * @param stats Output statistics
 */
void mb_fault_inject_get_stats(
    const mb_fault_inject_transport_t *fit,
    mb_fault_stats_t *stats
);

/**
 * @brief Reset fault injection statistics
 * 
 * @param fit Fault injection transport
 */
void mb_fault_inject_reset_stats(mb_fault_inject_transport_t *fit);

/**
 * @brief Enable/disable fault injection
 * 
 * @param fit Fault injection transport
 * @param enabled true to enable, false to disable
 */
void mb_fault_inject_set_enabled(mb_fault_inject_transport_t *fit, bool enabled);

/**
 * @brief Simple pseudo-random number generator
 * 
 * Fast PRNG suitable for embedded systems (xorshift32).
 * 
 * @param state RNG state (updated)
 * @return Random uint32_t
 */
static inline uint32_t mb_fault_prng(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/**
 * @brief Random float between 0.0 and 1.0
 * 
 * @param state RNG state (updated)
 * @return Random float [0.0, 1.0)
 */
static inline float mb_fault_prng_float(uint32_t *state) {
    return (float)mb_fault_prng(state) / (float)UINT32_MAX;
}

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_FAULT_INJECT_H */
