#include "../common/demo_tcp_socket.h"

#include <modbus/client.h>
#include <modbus/client_sync.h>
#include <modbus/mb_err.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_error(const char *stage, mb_err_t err)
{
    fprintf(stderr, "%s failed: %s\n", stage, mb_err_str(err));
}

int main(int argc, char **argv)
{
    const char *endpoint = (argc > 1) ? argv[1] : "127.0.0.1";
    uint16_t port = 502;

    const char *colon = strchr(endpoint, ':');
    char host[128];
    if (colon != NULL) {
        size_t host_len = (size_t)(colon - endpoint);
        if (host_len >= sizeof(host)) {
            fprintf(stderr, "Host name too long\n");
            return EXIT_FAILURE;
        }
        memcpy(host, endpoint, host_len);
        host[host_len] = '\0';
        port = (uint16_t)atoi(colon + 1);
    } else {
        snprintf(host, sizeof(host), "%s", endpoint);
    }

    demo_tcp_socket_t socket_ctx;
    mb_err_t status = demo_tcp_socket_connect(&socket_ctx, host, port, 2000U);
    if (!mb_err_is_ok(status)) {
        print_error("connect", status);
        return EXIT_FAILURE;
    }

    const mb_transport_if_t *iface = demo_tcp_socket_iface(&socket_ctx);
    if (iface == NULL) {
        fprintf(stderr, "Failed to obtain transport interface\n");
        demo_tcp_socket_close(&socket_ctx);
        return EXIT_FAILURE;
    }

    mb_client_t client;
    mb_client_txn_t txn_pool[4];
    status = mb_client_init_tcp(&client, iface, txn_pool, (mb_size_t)4);
    if (!mb_err_is_ok(status)) {
        print_error("mb_client_init_tcp", status);
        demo_tcp_socket_close(&socket_ctx);
        return EXIT_FAILURE;
    }

    uint16_t registers[4];
    mb_err_t err = mb_client_read_holding_sync(&client, 1U, 0U, 4U, registers, NULL);
    if (!mb_err_is_ok(err)) {
        print_error("mb_client_read_holding_sync", err);
        demo_tcp_socket_close(&socket_ctx);
        return EXIT_FAILURE;
    }

    printf("Received holding registers:\n");
    for (size_t i = 0; i < 4; ++i) {
        printf("  [%zu] = %u\n", i, registers[i]);
    }

    demo_tcp_socket_close(&socket_ctx);
    return EXIT_SUCCESS;
}
