#ifndef MODBUSCORE_TRANSPORT_WINSOCK_TCP_H
#define MODBUSCORE_TRANSPORT_WINSOCK_TCP_H

#include <stdbool.h>
#include <stdint.h>

#include <modbuscore/common/status.h>
#include <modbuscore/transport/iface.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mbc_winsock_tcp_config {
    const char *host;
    uint16_t port;
    uint32_t connect_timeout_ms;
    uint32_t recv_timeout_ms;
} mbc_winsock_tcp_config_t;

typedef struct mbc_winsock_tcp_ctx mbc_winsock_tcp_ctx_t;

mbc_status_t mbc_winsock_tcp_create(const mbc_winsock_tcp_config_t *config,
                                    mbc_transport_iface_t *out_iface,
                                    mbc_winsock_tcp_ctx_t **out_ctx);

void mbc_winsock_tcp_destroy(mbc_winsock_tcp_ctx_t *ctx);

bool mbc_winsock_tcp_is_connected(const mbc_winsock_tcp_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_TRANSPORT_WINSOCK_TCP_H */
