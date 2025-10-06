#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/net/socket.h>

#include "modbus_amalgamated.h"

#ifndef CONFIG_MODBUS_ZEPHYR_CLIENT_POOL_SIZE
#define CONFIG_MODBUS_ZEPHYR_CLIENT_POOL_SIZE 4
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct modbus_zephyr_tcp_transport {
    int sock;
    int recv_timeout_ms;
    bool connected;
};

typedef struct {
    mb_client_t client;
    mb_client_txn_t pool[CONFIG_MODBUS_ZEPHYR_CLIENT_POOL_SIZE];
    struct modbus_zephyr_tcp_transport transport;
    mb_transport_if_t iface;
} modbus_zephyr_client_t;

mb_err_t modbus_zephyr_client_init(modbus_zephyr_client_t *handle);
void modbus_zephyr_client_shutdown(modbus_zephyr_client_t *handle);
int modbus_zephyr_client_connect(modbus_zephyr_client_t *handle,
                                 const struct sockaddr *addr,
                                 socklen_t addrlen);
void modbus_zephyr_client_disconnect(modbus_zephyr_client_t *handle);

mb_err_t modbus_zephyr_submit_read_holding(modbus_zephyr_client_t *handle,
                                           uint8_t unit_id,
                                           uint16_t start_address,
                                           uint16_t quantity,
                                           mb_client_callback_t callback,
                                           void *user_ctx);

mb_err_t modbus_zephyr_submit_write_single(modbus_zephyr_client_t *handle,
                                           uint8_t unit_id,
                                           uint16_t address,
                                           uint16_t value,
                                           mb_client_callback_t callback,
                                           void *user_ctx);

#ifdef __cplusplus
}
#endif
