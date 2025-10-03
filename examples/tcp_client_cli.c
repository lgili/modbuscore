#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <modbus/client.h>
#include <modbus/mb_err.h>
#include <modbus/pdu.h>
#include <modbus/port/posix.h>

#define CLI_DEFAULT_TIMEOUT_MS 1000U
#define CLI_DEFAULT_RETRIES 1U

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s --host <hostname> --port <port> --unit <id> --register <addr> --count <n> [--expect v1,v2,...]\n",
            prog);
}

typedef struct {
    bool completed;
    mb_err_t status;
    mb_u16 registers[MB_PDU_MAX / 2];
    mb_size_t count;
} cli_result_t;

static void client_callback(mb_client_t *client,
                            const mb_client_txn_t *txn,
                            mb_err_t status,
                            const mb_adu_view_t *response,
                            void *user_ctx)
{
    (void)client;
    (void)txn;
    cli_result_t *result = (cli_result_t *)user_ctx;
    result->completed = true;
    result->status = status;

    if (status != MB_OK || response == NULL || response->payload_len < 1U) {
        result->count = 0U;
        return;
    }

    const mb_u8 byte_count = response->payload[0];
    const mb_size_t register_count = byte_count / 2U;
    if (register_count > (MB_COUNTOF(result->registers))) {
        result->count = 0U;
        result->status = MB_ERR_OTHER;
        return;
    }

    for (mb_size_t i = 0; i < register_count; ++i) {
        const mb_size_t index = 1U + (2U * i);
        if ((index + 1U) >= response->payload_len) {
            result->count = i;
            result->status = MB_ERR_OTHER;
            return;
        }
        result->registers[i] = (mb_u16)((response->payload[index] << 8) | response->payload[index + 1U]);
    }

    result->count = register_count;
}

static bool parse_expected(const char *arg, mb_u16 *out_values, mb_size_t *out_count)
{
    if (!arg || !out_values || !out_count) {
        return false;
    }

    char *copy = strdup(arg);
    if (!copy) {
        return false;
    }

    mb_size_t count = 0U;
    char *token = strtok(copy, ",");
    while (token && count < MB_PDU_MAX / 2) {
        long value = strtol(token, NULL, 0);
        if (value < 0 || value > 0xFFFF) {
            free(copy);
            return false;
        }
        out_values[count++] = (mb_u16)value;
        token = strtok(NULL, ",");
    }

    *out_count = count;
    free(copy);
    return true;
}

int main(int argc, char **argv)
{
    const char *host = NULL;
    int port = -1;
    int unit_id = -1;
    int reg_addr = -1;
    int reg_count = -1;
    mb_u16 expected[MB_PDU_MAX / 2] = {0};
    mb_size_t expected_count = 0U;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--host") == 0 && (i + 1) < argc) {
            host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && (i + 1) < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--unit") == 0 && (i + 1) < argc) {
            unit_id = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--register") == 0 && (i + 1) < argc) {
            reg_addr = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--count") == 0 && (i + 1) < argc) {
            reg_count = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--expect") == 0 && (i + 1) < argc) {
            if (!parse_expected(argv[++i], expected, &expected_count)) {
                fprintf(stderr, "Failed to parse expected values.\n");
                return EXIT_FAILURE;
            }
        } else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (!host || port <= 0 || unit_id < 0 || reg_addr < 0 || reg_count <= 0) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    mb_port_posix_socket_t tcp_sock;
    if (mb_port_posix_tcp_client(&tcp_sock, host, (uint16_t)port, CLI_DEFAULT_TIMEOUT_MS) != MB_OK) {
        fprintf(stderr, "Failed to connect to %s:%d\n", host, port);
        return EXIT_FAILURE;
    }

    const mb_transport_if_t *iface = mb_port_posix_socket_iface(&tcp_sock);
    if (!iface) {
        fprintf(stderr, "Invalid POSIX transport\n");
        mb_port_posix_socket_close(&tcp_sock);
        return EXIT_FAILURE;
    }

    mb_client_t client;
    memset(&client, 0, sizeof(client));
    mb_client_txn_t tx_pool[2];
    memset(tx_pool, 0, sizeof(tx_pool));

    mb_err_t err = mb_client_init_tcp(&client, iface, tx_pool, MB_COUNTOF(tx_pool));
    if (!mb_err_is_ok(err)) {
        fprintf(stderr, "Failed to initialize client: %s\n", mb_err_str(err));
        mb_port_posix_socket_close(&tcp_sock);
        return EXIT_FAILURE;
    }

    mb_client_set_watchdog(&client, 5000U);

    mb_u8 pdu[5];
    mb_size_t pdu_len = sizeof(pdu);
    err = mb_pdu_build_read_holding_request(pdu, pdu_len, (mb_u16)reg_addr, (mb_u16)reg_count);
    if (!mb_err_is_ok(err)) {
        fprintf(stderr, "Failed to build request: %s\n", mb_err_str(err));
        mb_port_posix_socket_close(&tcp_sock);
        return EXIT_FAILURE;
    }

    mb_client_request_t request = {
        .flags = 0U,
        .request = {
            .unit_id = (mb_u8)unit_id,
            .function = MB_PDU_FC_READ_HOLDING_REGISTERS,
            .payload = &pdu[1],
            .payload_len = pdu_len - 1U,
        },
        .timeout_ms = CLI_DEFAULT_TIMEOUT_MS,
        .max_retries = CLI_DEFAULT_RETRIES,
        .retry_backoff_ms = CLI_DEFAULT_TIMEOUT_MS / 2U,
        .callback = client_callback,
        .user_ctx = NULL,
    };

    cli_result_t result = {
        .completed = false,
        .status = MB_OK,
        .count = 0U,
    };
    request.user_ctx = &result;

    mb_client_txn_t *txn = NULL;
    err = mb_client_submit(&client, &request, &txn);
    if (!mb_err_is_ok(err)) {
        fprintf(stderr, "Failed to submit transaction: %s\n", mb_err_str(err));
        mb_port_posix_socket_close(&tcp_sock);
        return EXIT_FAILURE;
    }

    while (!result.completed) {
        err = mb_client_poll(&client);
        if (err == MB_ERR_TIMEOUT) {
            usleep(1000);
            continue;
        }
        if (!mb_err_is_ok(err)) {
            fprintf(stderr, "Polling error: %s\n", mb_err_str(err));
            mb_port_posix_socket_close(&tcp_sock);
            return EXIT_FAILURE;
        }
        usleep(1000);
    }

    mb_port_posix_socket_close(&tcp_sock);

    if (!mb_err_is_ok(result.status)) {
        fprintf(stderr, "Transaction failed: %s\n", mb_err_str(result.status));
        return EXIT_FAILURE;
    }

    printf("Read %zu holding registers starting at %d:\n", result.count, reg_addr);
    for (mb_size_t i = 0; i < result.count; ++i) {
        printf("  [%zu] = %u\n", i, result.registers[i]);
    }

    if (expected_count > 0U) {
        if (expected_count != result.count) {
            fprintf(stderr, "Expected %zu values but received %zu.\n", expected_count, result.count);
            return EXIT_FAILURE;
        }
        for (mb_size_t i = 0; i < expected_count; ++i) {
            if (expected[i] != result.registers[i]) {
                fprintf(stderr, "Mismatch at index %zu: expected %u got %u.\n",
                        i, expected[i], result.registers[i]);
                return EXIT_FAILURE;
            }
        }
    }

    return EXIT_SUCCESS;
}
