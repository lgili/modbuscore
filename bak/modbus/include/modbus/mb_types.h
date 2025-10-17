/**
 * @file mb_types.h
 * @brief Common fixed-width and utility types for the Modbus library.
 *
 * This header centralises the type aliases and lightweight macros we rely on
 * across the codebase.  Keeping them in a single translation unit makes it
 * easier to reason about ABI constraints and freestanding targets that may
 * not ship with the full C library.
 */

#ifndef MODBUS_MB_TYPES_H
#define MODBUS_MB_TYPES_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Fixed width aliases                                                        */
/* -------------------------------------------------------------------------- */

typedef int8_t   mb_i8;
typedef uint8_t  mb_u8;
typedef int16_t  mb_i16;
typedef uint16_t mb_u16;
typedef int32_t  mb_i32;
typedef uint32_t mb_u32;
typedef int64_t  mb_i64;
typedef uint64_t mb_u64;

typedef size_t    mb_size_t;
typedef ptrdiff_t mb_ptrdiff_t;
typedef uintptr_t mb_uintptr_t;
typedef intptr_t  mb_intptr_t;

/* Monotonic millisecond timestamp used by the FSM and watchdog logic. */
typedef mb_u64 mb_time_ms_t;

/* -------------------------------------------------------------------------- */
/* Compile-time helpers                                                       */
/* -------------------------------------------------------------------------- */

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define MB_STATIC_ASSERT(expr, msg) _Static_assert((expr), msg)
#else
#  define MB_STATIC_ASSERT(expr, msg) typedef char mb_static_assert_[(expr) ? 1 : -1]
#endif

#define MB_COUNTOF(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Attribute helpers -------------------------------------------------------- */

#if defined(__GNUC__) || defined(__clang__)
#  define MB_PACKED __attribute__((__packed__))
#  define MB_LIKELY(x)   __builtin_expect(!!(x), 1)
#  define MB_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define MB_PACKED
#  define MB_LIKELY(x)   (x)
#  define MB_UNLIKELY(x) (x)
#endif

#define MB_IS_POWER_OF_TWO(value) (((value) != 0U) && (((value) & ((value) - 1U)) == 0U))

#define MB_ALIGN_UP(value, align) \
	(((value) + ((align) - 1U)) & ~((align) - 1U))

/* -------------------------------------------------------------------------- */
/* Cooperative poll micro-step helpers                                        */
/* -------------------------------------------------------------------------- */

typedef enum {
	MB_POLL_RX_PHASE_IDLE = 0,
	MB_POLL_RX_PHASE_HEADER,
	MB_POLL_RX_PHASE_BODY,
	MB_POLL_RX_PHASE_VALIDATE,
	MB_POLL_RX_PHASE_DISPATCH
} mb_poll_rx_phase_t;

typedef enum {
	MB_POLL_TX_PHASE_IDLE = 0,
	MB_POLL_TX_PHASE_BUILD,
	MB_POLL_TX_PHASE_SEND,
	MB_POLL_TX_PHASE_DRAIN
} mb_poll_tx_phase_t;

typedef struct {
	mb_time_ms_t last_timestamp;
	mb_time_ms_t max_delta_ms;
	mb_u64 total_delta_ms;
	mb_u32 samples;
} mb_poll_jitter_t;

/* Basic sanity checks ------------------------------------------------------ */

MB_STATIC_ASSERT(sizeof(mb_u8) == 1,  "mb_u8 must be 8 bits");
MB_STATIC_ASSERT(sizeof(mb_u16) == 2, "mb_u16 must be 16 bits");
MB_STATIC_ASSERT(sizeof(mb_u32) == 4, "mb_u32 must be 32 bits");
MB_STATIC_ASSERT(sizeof(mb_u64) == 8, "mb_u64 must be 64 bits");

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MB_TYPES_H */
