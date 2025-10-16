/**
 * @file mb_log.h
 * @brief Lightweight logging façade shared across the Modbus library.
 *
 * The module re-exports the underlying logging implementation through
 * Modbus-specific names so applications can configure sinks and thresholds
 * without depending on the internal headers.
 */

#ifndef MODBUS_MB_LOG_H
#define MODBUS_MB_LOG_H

#include <modbus/internal/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Public aliases                                                              */
/* -------------------------------------------------------------------------- */

typedef log_level_t mb_log_level_t;
typedef log_err_t mb_log_err_t;
typedef log_function_t mb_log_function_t;

#define MB_LOG_TRACE_LEVEL    LOG_TRACE_LEVEL
#define MB_LOG_DEBUG_LEVEL    LOG_DEBUG_LEVEL
#define MB_LOG_INFO_LEVEL     LOG_INFO_LEVEL
#define MB_LOG_WARNING_LEVEL  LOG_WARNING_LEVEL
#define MB_LOG_ERROR_LEVEL    LOG_ERROR_LEVEL
#define MB_LOG_CRITICAL_LEVEL LOG_CRITICAL_LEVEL
#define MB_LOG_ALWAYS_LEVEL   LOG_ALWAYS_LEVEL

#define MB_LOG_ERR_NONE               LOG_ERR_NONE
#define MB_LOG_ERR_SUBSCRIBERS_EXCEEDED LOG_ERR_SUBSCRIBERS_EXCEEDED
#define MB_LOG_ERR_NOT_SUBSCRIBED     LOG_ERR_NOT_SUBSCRIBED

#ifdef LOG_ENABLED
#define MB_LOG_ENABLED 1
#else
#define MB_LOG_ENABLED 0
#endif

/* -------------------------------------------------------------------------- */
/* Configuration                                                              */
/* -------------------------------------------------------------------------- */

#ifndef MB_LOG_ENABLE_STDIO
#define MB_LOG_ENABLE_STDIO 1
#endif

#ifndef MB_LOG_ENABLE_SEGGER_RTT
#define MB_LOG_ENABLE_SEGGER_RTT 0
#endif

#ifndef MB_LOG_DEFAULT_THRESHOLD
#define MB_LOG_DEFAULT_THRESHOLD MB_LOG_INFO_LEVEL
#endif

#ifndef MB_LOG_STDOUT_SYNC_FLUSH
#define MB_LOG_STDOUT_SYNC_FLUSH 1
#endif

#ifndef MB_LOG_RTT_CHANNEL
#define MB_LOG_RTT_CHANNEL 0
#endif

/* -------------------------------------------------------------------------- */
/* Logging macros (passthrough)                                               */
/* -------------------------------------------------------------------------- */

#define MB_LOG_INIT()        LOG_INIT()
#define MB_LOG_SUBSCRIBE(fn, threshold) LOG_SUBSCRIBE((fn), (threshold))
#define MB_LOG_UNSUBSCRIBE(fn)          LOG_UNSUBSCRIBE((fn))
#define MB_LOG_LEVEL_NAME(level)        LOG_LEVEL_NAME((level))

#define MB_LOG_TRACE(...)    LOG_TRACE(__VA_ARGS__)
#define MB_LOG_DEBUG(...)    LOG_DEBUG(__VA_ARGS__)
#define MB_LOG_INFO(...)     LOG_INFO(__VA_ARGS__)
#define MB_LOG_WARNING(...)  LOG_WARNING(__VA_ARGS__)
#define MB_LOG_ERROR(...)    LOG_ERROR(__VA_ARGS__)
#define MB_LOG_CRITICAL(...) LOG_CRITICAL(__VA_ARGS__)
#define MB_LOG_ALWAYS(...)   LOG_ALWAYS(__VA_ARGS__)


/* -------------------------------------------------------------------------- */
/* Convenience helpers                                                        */
/* -------------------------------------------------------------------------- */

void mb_log_bootstrap_defaults(void);

#if MB_LOG_ENABLE_STDIO
mb_log_err_t mb_log_subscribe_stdio(mb_log_level_t threshold);
#else
static inline mb_log_err_t mb_log_subscribe_stdio(mb_log_level_t threshold)
{
	(void)threshold;
	return LOG_ERR_NOT_SUBSCRIBED;
}
#endif

#if MB_LOG_ENABLE_SEGGER_RTT
mb_log_err_t mb_log_subscribe_rtt(mb_log_level_t threshold);
#else
static inline mb_log_err_t mb_log_subscribe_rtt(mb_log_level_t threshold)
{
	(void)threshold;
	return LOG_ERR_NOT_SUBSCRIBED;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MB_LOG_H */
