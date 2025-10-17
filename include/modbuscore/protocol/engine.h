#ifndef MODBUSCORE_PROTOCOL_ENGINE_H
#define MODBUSCORE_PROTOCOL_ENGINE_H

/**
 * @file engine.h
 * @brief Skeleton do núcleo protocolar (cliente/servidor) usando DI.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <modbuscore/common/status.h>
#include <modbuscore/runtime/runtime.h>
#include <modbuscore/transport/iface.h>
#include <modbuscore/protocol/events.h>
#include <modbuscore/protocol/pdu.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mbc_engine_role {
    MBC_ENGINE_ROLE_CLIENT,
    MBC_ENGINE_ROLE_SERVER
} mbc_engine_role_t;

typedef enum mbc_engine_state {
    MBC_ENGINE_STATE_IDLE,
    MBC_ENGINE_STATE_RECEIVING,
    MBC_ENGINE_STATE_SENDING,
    MBC_ENGINE_STATE_WAIT_RESPONSE
} mbc_engine_state_t;

typedef struct mbc_engine_config {
    const mbc_runtime_t *runtime;
    mbc_transport_iface_t transport_override; /**< Opcional; ctx=NULL => usar runtime. */
    bool use_override;
    mbc_engine_role_t role;
    mbc_engine_event_fn event_cb;
    void *event_ctx;
    uint32_t response_timeout_ms; /**< Cliente: tempo máximo aguardando resposta. */
} mbc_engine_config_t;

typedef struct mbc_engine {
    mbc_engine_state_t state;
    mbc_engine_role_t role;
    const mbc_runtime_t *runtime;
    mbc_transport_iface_t transport;
    bool initialised;
    mbc_engine_event_fn event_cb;
    void *event_ctx;
    uint32_t response_timeout_ms;
    uint64_t last_activity_ms;
    uint8_t rx_buffer[MBC_PDU_MAX + 2U];
    size_t rx_length;
    size_t expected_length;
    bool pdu_ready;
    mbc_pdu_t current_pdu;
} mbc_engine_t;

mbc_status_t mbc_engine_init(mbc_engine_t *engine, const mbc_engine_config_t *config);
void mbc_engine_shutdown(mbc_engine_t *engine);
bool mbc_engine_is_ready(const mbc_engine_t *engine);
mbc_status_t mbc_engine_step(mbc_engine_t *engine, size_t budget);
mbc_status_t mbc_engine_submit_request(mbc_engine_t *engine, const uint8_t *buffer, size_t length);
bool mbc_engine_take_pdu(mbc_engine_t *engine, mbc_pdu_t *out);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_PROTOCOL_ENGINE_H */
