#ifndef MODBUSCORE_PROTOCOL_ENGINE_H
#define MODBUSCORE_PROTOCOL_ENGINE_H

/**
 * @file engine.h
 * @brief Protocol engine core (client/server) using dependency injection.
 *
 * The protocol engine is the heart of ModbusCore, managing:
 * - Request/response state machine
 * - PDU encoding/decoding
 * - Framing (RTU vs TCP/MBAP)
 * - Timeout handling
 * - Event notifications
 */

#include <modbuscore/common/status.h>
#include <modbuscore/protocol/events.h>
#include <modbuscore/protocol/mbap.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/runtime/runtime.h>
#include <modbuscore/transport/iface.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Engine role: client or server.
 */
typedef enum mbc_engine_role {
    MBC_ENGINE_ROLE_CLIENT, /**< Client mode: sends requests, receives responses */
    MBC_ENGINE_ROLE_SERVER  /**< Server mode: receives requests, sends responses */
} mbc_engine_role_t;

/**
 * @brief Engine state machine states.
 */
typedef enum mbc_engine_state {
    MBC_ENGINE_STATE_IDLE,         /**< Idle, ready for new operation */
    MBC_ENGINE_STATE_RECEIVING,    /**< Receiving data from transport */
    MBC_ENGINE_STATE_SENDING,      /**< Sending data to transport */
    MBC_ENGINE_STATE_WAIT_RESPONSE /**< Client waiting for response */
} mbc_engine_state_t;

/**
 * @brief Framing mode: RTU or TCP.
 */
typedef enum mbc_framing_mode {
    MBC_FRAMING_RTU, /**< Modbus RTU framing: [Unit ID][FC][Data...] */
    MBC_FRAMING_TCP  /**< Modbus TCP framing: [MBAP header][FC][Data...] */
} mbc_framing_mode_t;

/**
 * @brief Engine configuration.
 */
typedef struct mbc_engine_config {
    const mbc_runtime_t* runtime; /**< Runtime with dependencies (transport, etc.) */
    mbc_transport_iface_t
        transport_override;     /**< Optional transport override (if use_override=true) */
    bool use_override;          /**< If true, use transport_override instead of runtime transport */
    mbc_engine_role_t role;     /**< Engine role: client or server */
    mbc_framing_mode_t framing; /**< Framing mode: RTU or TCP (MBAP) */
    mbc_engine_event_fn event_cb; /**< Optional event callback */
    void* event_ctx;              /**< User context for event callback */
    uint32_t response_timeout_ms; /**< Client: max time waiting for response (0 = no timeout) */
} mbc_engine_config_t;

/**
 * @brief Engine state.
 *
 * This structure should be treated as opaque. Use the provided API functions.
 */
typedef struct mbc_engine {
    mbc_engine_state_t state;           /**< Current FSM state */
    mbc_engine_role_t role;             /**< Engine role */
    mbc_framing_mode_t framing;         /**< Framing mode */
    const mbc_runtime_t* runtime;       /**< Runtime reference */
    mbc_transport_iface_t transport;    /**< Transport interface */
    bool initialised;                   /**< Initialization flag */
    mbc_engine_event_fn event_cb;       /**< Event callback */
    void* event_ctx;                    /**< Event callback context */
    uint32_t response_timeout_ms;       /**< Response timeout */
    uint64_t last_activity_ms;          /**< Last activity timestamp */
    uint8_t rx_buffer[260U];            /**< RX buffer (260 bytes for MBAP: 7 header + 253 PDU) */
    size_t rx_length;                   /**< Current RX buffer length */
    size_t expected_length;             /**< Expected frame length */
    bool pdu_ready;                     /**< PDU ready flag */
    mbc_pdu_t current_pdu;              /**< Current decoded PDU */
    mbc_mbap_header_t last_mbap_header; /**< Last decoded MBAP header (TCP only) */
    bool last_mbap_valid;               /**< Flag indicating header is valid */
} mbc_engine_t;

/**
 * @brief Initialize protocol engine.
 *
 * @param engine Pointer to engine structure to initialize
 * @param config Engine configuration
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_engine_init(mbc_engine_t* engine, const mbc_engine_config_t* config);

/**
 * @brief Shutdown protocol engine and release resources.
 *
 * @param engine Pointer to engine structure
 */
void mbc_engine_shutdown(mbc_engine_t* engine);

/**
 * @brief Check if engine is initialized and ready.
 *
 * @param engine Pointer to engine structure
 * @return true if ready, false otherwise
 */
bool mbc_engine_is_ready(const mbc_engine_t* engine);

/**
 * @brief Execute one engine step (receive and process data).
 *
 * This function should be called periodically to:
 * - Receive data from transport
 * - Decode frames and PDUs
 * - Handle timeouts
 * - Update FSM state
 *
 * @param engine Pointer to engine structure
 * @param budget Maximum number of bytes to receive in this step
 * @return MBC_STATUS_OK on success, MBC_STATUS_TIMEOUT if response timeout occurred
 */
mbc_status_t mbc_engine_step(mbc_engine_t* engine, size_t budget);

/**
 * @brief Submit a request for transmission (client mode).
 *
 * For RTU mode: buffer should contain [Unit ID][FC][Data...]
 * For TCP mode: buffer should contain complete MBAP frame
 *
 * @param engine Pointer to engine structure
 * @param buffer Request data to send
 * @param length Length of request data
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_engine_submit_request(mbc_engine_t* engine, const uint8_t* buffer, size_t length);

/**
 * @brief Take a decoded PDU from the engine.
 *
 * This function retrieves and clears the current PDU if one is ready.
 *
 * @param engine Pointer to engine structure
 * @param out Pointer to store decoded PDU
 * @return true if PDU was available and copied to out, false otherwise
 */
bool mbc_engine_take_pdu(mbc_engine_t* engine, mbc_pdu_t* out);

/**
 * @brief Retrieve the last MBAP header decoded by the engine (TCP mode).
 *
 * @param engine Engine instance
 * @param out Header destination
 * @return true if a header is available, false otherwise
 */
bool mbc_engine_last_mbap_header(const mbc_engine_t* engine, mbc_mbap_header_t* out);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_PROTOCOL_ENGINE_H */
