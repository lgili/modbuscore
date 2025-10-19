#ifndef MODBUSCORE_RUNTIME_DIAGNOSTICS_H
#define MODBUSCORE_RUNTIME_DIAGNOSTICS_H

/**
 * @file diagnostics.h
 * @brief Structured diagnostics interfaces for the runtime.
 *
 * Diagnostics extend the classic logger with structured payloads that can be
 * forwarded to tracing pipelines, telemetry collectors or simple in-memory
 * buffers. A sink receives immutable events carrying a severity, component
 * identifier, human-readable message and optional key/value metadata.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Severity levels for diagnostics events.
 */
typedef enum mbc_diag_severity {
    MBC_DIAG_SEVERITY_TRACE,   /**< Detailed tracing */
    MBC_DIAG_SEVERITY_DEBUG,   /**< Debug information */
    MBC_DIAG_SEVERITY_INFO,    /**< Informational message */
    MBC_DIAG_SEVERITY_WARNING, /**< Warning about potential issue */
    MBC_DIAG_SEVERITY_ERROR,   /**< Recoverable error */
    MBC_DIAG_SEVERITY_CRITICAL /**< Non-recoverable failure */
} mbc_diag_severity_t;

/**
 * @brief Key/value metadata attached to a diagnostics event.
 *
 * Values are immutable strings owned by the producer. The sink must copy data
 * if it needs to retain it beyond the callback scope.
 */
typedef struct mbc_diag_kv {
    const char* key;   /**< Metadata key */
    const char* value; /**< Metadata value */
} mbc_diag_kv_t;

/**
 * @brief Structured diagnostics event.
 */
typedef struct mbc_diag_event {
    mbc_diag_severity_t severity; /**< Event severity */
    const char* component;        /**< Component or subsystem identifier */
    const char* message;          /**< Human-readable message */
    const mbc_diag_kv_t* fields;  /**< Optional metadata array */
    size_t field_count;           /**< Number of metadata entries */
    uint32_t code;                /**< Optional numeric code (0 = none) */
    uint64_t timestamp_ms;        /**< Event timestamp (milliseconds) */
} mbc_diag_event_t;

/**
 * @brief Diagnostics sink interface.
 */
typedef struct mbc_diag_sink_iface {
    void* ctx;                                              /**< User context passed to emit */
    void (*emit)(void* ctx, const mbc_diag_event_t* event); /**< Emit diagnostics event */
} mbc_diag_sink_iface_t;

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_RUNTIME_DIAGNOSTICS_H */
