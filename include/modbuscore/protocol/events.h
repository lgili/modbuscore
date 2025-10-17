#ifndef MODBUSCORE_PROTOCOL_EVENTS_H
#define MODBUSCORE_PROTOCOL_EVENTS_H

/**
 * @file events.h
 * @brief Eventos emitidos pelo motor Modbus (telemetria/diagnóstico).
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mbc_engine_event_type {
    MBC_ENGINE_EVENT_RX_READY,
    MBC_ENGINE_EVENT_TX_SENT,
    MBC_ENGINE_EVENT_STEP_BEGIN,
    MBC_ENGINE_EVENT_STEP_END,
    MBC_ENGINE_EVENT_STATE_CHANGE,
    MBC_ENGINE_EVENT_PDU_READY,
    MBC_ENGINE_EVENT_TIMEOUT
} mbc_engine_event_type_t;

typedef struct mbc_engine_event {
    mbc_engine_event_type_t type;
    uint64_t timestamp_ms;
} mbc_engine_event_t;

typedef void (*mbc_engine_event_fn)(void *ctx, const mbc_engine_event_t *event);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_PROTOCOL_EVENTS_H */
