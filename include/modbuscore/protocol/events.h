#ifndef MODBUSCORE_PROTOCOL_EVENTS_H
#define MODBUSCORE_PROTOCOL_EVENTS_H

/**
 * @file events.h
 * @brief Events emitted by the Modbus engine (telemetry/diagnostics).
 *
 * These events allow monitoring of the protocol engine's internal state
 * transitions and I/O operations for debugging and performance analysis.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Engine event types.
 */
typedef enum mbc_engine_event_type {
    MBC_ENGINE_EVENT_RX_READY,      /**< Data received and ready for processing */
    MBC_ENGINE_EVENT_TX_SENT,       /**< Data transmitted successfully */
    MBC_ENGINE_EVENT_STEP_BEGIN,    /**< Engine step starting */
    MBC_ENGINE_EVENT_STEP_END,      /**< Engine step completed */
    MBC_ENGINE_EVENT_STATE_CHANGE,  /**< FSM state transition */
    MBC_ENGINE_EVENT_PDU_READY,     /**< Complete PDU decoded and ready */
    MBC_ENGINE_EVENT_TIMEOUT        /**< Response timeout occurred */
} mbc_engine_event_type_t;

/**
 * @brief Engine event structure.
 */
typedef struct mbc_engine_event {
    mbc_engine_event_type_t type;   /**< Event type */
    uint64_t timestamp_ms;          /**< Event timestamp in milliseconds */
} mbc_engine_event_t;

/**
 * @brief Event callback function signature.
 *
 * @param ctx User context
 * @param event Event data
 */
typedef void (*mbc_engine_event_fn)(void *ctx, const mbc_engine_event_t *event);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_PROTOCOL_EVENTS_H */
