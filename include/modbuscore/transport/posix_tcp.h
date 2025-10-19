#ifndef MODBUSCORE_TRANSPORT_POSIX_TCP_H
#define MODBUSCORE_TRANSPORT_POSIX_TCP_H

/**
 * @file posix_tcp.h
 * @brief POSIX TCP transport driver (Linux/macOS/BSD) with non-blocking I/O.
 *
 * This driver implements the mbc_transport_iface_t interface using POSIX TCP
 * sockets in non-blocking mode. Suitable for desktop/server clients and servers.
 *
 * Current limitations (v1.0):
 * - No TLS/SSL support (to be added in Phase 7)
 * - No automatic reconnection (implement later if needed)
 * - Client-only (multi-connection server support comes later)
 */

#include <stdint.h>
#include <stdbool.h>

#include <modbuscore/common/status.h>
#include <modbuscore/transport/iface.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for creating a POSIX TCP client.
 */
typedef struct mbc_posix_tcp_config {
    const char *host;              /**< Hostname or IP address (e.g., "192.168.1.10" or "localhost"). */
    uint16_t port;                 /**< TCP port (e.g., 502 for standard Modbus TCP). */
    uint32_t connect_timeout_ms;   /**< Connection timeout in milliseconds (0 = blocking/unlimited). */
    uint32_t recv_timeout_ms;      /**< I/O timeout for send/receive polling (0 = no timeout). */
} mbc_posix_tcp_config_t;

/**
 * @brief Opaque TCP driver context (managed internally).
 */
typedef struct mbc_posix_tcp_ctx mbc_posix_tcp_ctx_t;

/**
 * @brief Create and connect a POSIX TCP client.
 *
 * @param config Connection configuration (host, port, timeouts).
 * @param out_iface Pointer to transport interface (will be filled).
 * @param out_ctx Pointer to allocated context (use for destroy later).
 * @return MBC_STATUS_OK on success, error code otherwise.
 *
 * @note The socket is configured as non-blocking after connection.
 * @note You MUST call mbc_posix_tcp_destroy() to free resources.
 *
 * @code
 * mbc_posix_tcp_config_t config = {
 *     .host = "192.168.1.100",
 *     .port = 502,
 *     .connect_timeout_ms = 5000,
 *     .recv_timeout_ms = 1000,
 * };
 *
 * mbc_transport_iface_t iface;
 * mbc_posix_tcp_ctx_t *ctx = NULL;
 *
 * if (mbc_posix_tcp_create(&config, &iface, &ctx) == MBC_STATUS_OK) {
 *     // Use iface with mbc_runtime_builder_with_transport()
 *     // ...
 *     mbc_posix_tcp_destroy(ctx);
 * }
 * @endcode
 */
mbc_status_t mbc_posix_tcp_create(const mbc_posix_tcp_config_t *config,
                                   mbc_transport_iface_t *out_iface,
                                   mbc_posix_tcp_ctx_t **out_ctx);

/**
 * @brief Destroy the TCP context and close the socket.
 *
 * @param ctx Context returned by mbc_posix_tcp_create() (can be NULL).
 */
void mbc_posix_tcp_destroy(mbc_posix_tcp_ctx_t *ctx);

/**
 * @brief Check if the TCP connection is still active.
 *
 * @param ctx TCP context.
 * @return true if connected, false otherwise.
 */
bool mbc_posix_tcp_is_connected(const mbc_posix_tcp_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_TRANSPORT_POSIX_TCP_H */
