#ifndef MODBUSCORE_TRANSPORT_WINSOCK_TCP_H
#define MODBUSCORE_TRANSPORT_WINSOCK_TCP_H

/**
 * @file winsock_tcp.h
 * @brief Winsock TCP transport driver for Windows.
 *
 * This driver provides non-blocking TCP transport using Windows Sockets API.
 * It handles connection establishment, I/O timeouts, and proper socket cleanup.
 */

#include <modbuscore/common/status.h>
#include <modbuscore/transport/iface.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Winsock TCP configuration.
 */
typedef struct mbc_winsock_tcp_config {
    const char* host;            /**< Target hostname or IP address */
    uint16_t port;               /**< Target port number */
    uint32_t connect_timeout_ms; /**< Connection timeout in milliseconds */
    uint32_t recv_timeout_ms;    /**< Receive timeout in milliseconds */
} mbc_winsock_tcp_config_t;

/**
 * @brief Opaque Winsock TCP context.
 */
typedef struct mbc_winsock_tcp_ctx mbc_winsock_tcp_ctx_t;

/**
 * @brief Create Winsock TCP transport instance.
 *
 * @param config Configuration (must not be NULL)
 * @param out_iface Transport interface filled on output
 * @param out_ctx Allocated context (use destroy when done)
 * @return MBC_STATUS_OK on success or error code
 */
mbc_status_t mbc_winsock_tcp_create(const mbc_winsock_tcp_config_t* config,
                                    mbc_transport_iface_t* out_iface,
                                    mbc_winsock_tcp_ctx_t** out_ctx);

/**
 * @brief Destroy Winsock TCP transport, releasing resources.
 */
void mbc_winsock_tcp_destroy(mbc_winsock_tcp_ctx_t* ctx);

/**
 * @brief Check if the TCP connection is established.
 */
bool mbc_winsock_tcp_is_connected(const mbc_winsock_tcp_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_TRANSPORT_WINSOCK_TCP_H */
