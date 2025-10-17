#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>

#include "modbus_zephyr_quickstart.h"

LOG_MODULE_REGISTER(modbus_hello_tcp, LOG_LEVEL_INF);

static volatile bool g_request_in_flight;

static void modbus_response_cb(struct mb_client *client,
                               const struct mb_client_txn *txn,
                               mb_err_t status,
                               const mb_adu_view_t *response,
                               void *user_ctx)
{
    ARG_UNUSED(client);
    ARG_UNUSED(txn);
    ARG_UNUSED(user_ctx);

    if (mb_err_is_ok(status)) {
        LOG_INF("Received %u bytes from unit %u", response->payload_len, response->unit_id);
    } else {
        LOG_WRN("Modbus transaction failed (%d)", status);
    }

    g_request_in_flight = false;
}

static int connect_server(modbus_zephyr_client_t *client)
{
    struct sockaddr_in server = {
        .sin_family = AF_INET,
        .sin_port = htons(CONFIG_MODBUS_HELLO_SERVER_PORT),
    };

    if (zsock_inet_pton(AF_INET, CONFIG_MODBUS_HELLO_SERVER_ADDR, &server.sin_addr) != 1) {
        LOG_ERR("Invalid server address %s", CONFIG_MODBUS_HELLO_SERVER_ADDR);
        return -EINVAL;
    }

    int rc = modbus_zephyr_client_connect(client,
                                          (struct sockaddr *)&server,
                                          sizeof(server));
    if (rc != 0) {
        LOG_ERR("Failed to connect Modbus server (%d)", rc);
    }

    return rc;
}

int main(void)
{
    modbus_zephyr_client_t client;
    mb_err_t err = modbus_zephyr_client_init(&client);
    if (err != MB_OK) {
        LOG_ERR("mb_client_init failed (%d)", err);
        return 0;
    }

    if (connect_server(&client) != 0) {
        return 0;
    }

    while (1) {
        if (!g_request_in_flight) {
            err = modbus_zephyr_submit_read_holding(&client,
                                                    1,
                                                    0x0000,
                                                    2,
                                                    modbus_response_cb,
                                                    NULL);
            if (err == MB_OK) {
                g_request_in_flight = true;
                LOG_INF("Submitted FC03 read for 2 registers");
            } else {
                LOG_WRN("Queue submit failed (%d)", err);
                k_msleep(500);
            }
        }

        mb_client_poll(&client.client);
        k_msleep(CONFIG_MODBUS_HELLO_POLL_INTERVAL_MS);
    }
}
