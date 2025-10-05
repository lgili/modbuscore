#include <modbus/client.h>
#include <modbus/mb_err.h>
#include <modbus/mb_log.h>
#include <modbus/observe.h>
#include <modbus/pdu.h>

#include "common/demo_serial_port.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#define DEMO_DEFAULT_BAUD 115200U
#define DEMO_DEFAULT_UNIT 0x11U

static volatile sig_atomic_t g_stop = 0;

static void handle_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

static void install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void sleep_ms(unsigned ms)
{
#if defined(_WIN32)
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000U);
    ts.tv_nsec = (long)((ms % 1000U) * 1000000L);
    nanosleep(&ts, NULL);
#endif
}

static void log_event(const mb_event_t *event, void *ctx)
{
    (void)ctx;
    if (event == NULL) {
        return;
    }

    if (event->source != MB_EVENT_SOURCE_CLIENT) {
        return;
    }

    switch (event->type) {
    case MB_EVENT_CLIENT_STATE_ENTER:
        printf("[client] state -> %u\n", (unsigned)event->data.client_state.state);
        break;
    case MB_EVENT_CLIENT_STATE_EXIT:
        printf("[client] state <- %u\n", (unsigned)event->data.client_state.state);
        break;
    case MB_EVENT_CLIENT_TX_SUBMIT:
        printf("[client] transaction submit fc=%u expect_response=%s\n",
               (unsigned)event->data.client_txn.function,
               event->data.client_txn.expect_response ? "yes" : "no");
        break;
    case MB_EVENT_CLIENT_TX_COMPLETE:
        printf("[client] transaction complete fc=%u status=%s\n",
               (unsigned)event->data.client_txn.function,
               mb_err_str(event->data.client_txn.status));
        break;
    case MB_EVENT_SERVER_STATE_ENTER:
    case MB_EVENT_SERVER_STATE_EXIT:
    case MB_EVENT_SERVER_REQUEST_ACCEPT:
    case MB_EVENT_SERVER_REQUEST_COMPLETE:
        /* Server events are ignored on the client logger. */
        break;
    default:
        break;
    }
}

typedef struct {
    bool completed;
    mb_err_t status;
    mb_u16 quantity;
    mb_u16 registers[16];
} client_result_t;

static void client_callback(mb_client_t *client,
                            const mb_client_txn_t *txn,
                            mb_err_t status,
                            const mb_adu_view_t *response,
                            void *user_ctx)
{
    (void)client;
    (void)txn;
    client_result_t *result = (client_result_t *)user_ctx;
    if (result == NULL) {
        return;
    }

    result->completed = true;
    result->status = status;
    result->quantity = 0U;

    if (!mb_err_is_ok(status) || response == NULL || response->payload_len == 0U) {
        return;
    }

    const mb_u8 byte_count = response->payload[0];
    const mb_u16 available_regs = (mb_u16)(byte_count / 2U);
    mb_u16 out_regs = available_regs;
    if (out_regs > (mb_u16)MB_COUNTOF(result->registers)) {
        out_regs = (mb_u16)MB_COUNTOF(result->registers);
    }

    if (response->payload_len < (mb_size_t)(1U + out_regs * 2U)) {
        return;
    }

    for (mb_u16 i = 0; i < out_regs; ++i) {
        const mb_size_t idx = (mb_size_t)1U + (mb_size_t)i * 2U;
        result->registers[i] = (mb_u16)((response->payload[idx] << 8) | response->payload[idx + 1U]);
    }

    result->quantity = out_regs;
}

int main(int argc, char **argv)
{
    const char *device = NULL;
    unsigned baud = DEMO_DEFAULT_BAUD;
    mb_u8 unit_id = DEMO_DEFAULT_UNIT;
    bool enable_trace = false;
    unsigned sleep_between_ms = 1000U;

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--device") == 0 || strcmp(argv[i], "-d") == 0) && (i + 1) < argc) {
            device = argv[++i];
        } else if ((strcmp(argv[i], "--baud") == 0 || strcmp(argv[i], "-b") == 0) && (i + 1) < argc) {
            baud = (unsigned)strtoul(argv[++i], NULL, 0);
        } else if ((strcmp(argv[i], "--unit") == 0 || strcmp(argv[i], "-u") == 0) && (i + 1) < argc) {
            unit_id = (mb_u8)strtoul(argv[++i], NULL, 0);
        } else if ((strcmp(argv[i], "--interval") == 0 || strcmp(argv[i], "-i") == 0) && (i + 1) < argc) {
            sleep_between_ms = (unsigned)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--trace") == 0) {
            enable_trace = true;
        } else {
            fprintf(stderr,
                    "Usage: %s --device <path-or-com> [--baud <rate>] [--unit <id>] [--interval <ms>] [--trace]\n",
                    argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (device == NULL) {
        fprintf(stderr, "Serial device is required (use --device).\n");
        return EXIT_FAILURE;
    }

    install_signal_handlers();
    mb_log_bootstrap_defaults();

    demo_serial_port_t serial;
    mb_err_t status = demo_serial_port_open(&serial, device, baud);
    if (!mb_err_is_ok(status)) {
        fprintf(stderr, "Failed to open %s (%s)\n", device, mb_err_str(status));
        return EXIT_FAILURE;
    }

    const mb_transport_if_t *iface = demo_serial_port_iface(&serial);
    if (iface == NULL) {
        fprintf(stderr, "Invalid transport for %s\n", device);
        demo_serial_port_close(&serial);
        return EXIT_FAILURE;
    }

    mb_client_t client;
    mb_client_txn_t txn_pool[4];
    memset(&client, 0, sizeof(client));
    memset(txn_pool, 0, sizeof(txn_pool));

    status = mb_client_init(&client, iface, txn_pool, MB_COUNTOF(txn_pool));
    if (!mb_err_is_ok(status)) {
        fprintf(stderr, "mb_client_init failed: %s\n", mb_err_str(status));
        demo_serial_port_close(&serial);
        return EXIT_FAILURE;
    }

    mb_client_set_watchdog(&client, 2000U);
    mb_client_set_event_callback(&client, log_event, NULL);
    mb_client_set_trace_hex(&client, enable_trace);

    printf("Modbus RTU client polling %s (baud=%u, unit=%u). Press Ctrl+C to stop.\n",
           device,
           baud,
           (unsigned)unit_id);

    while (!g_stop) {
        mb_u8 request_pdu[5];
        status = mb_pdu_build_read_holding_request(request_pdu,
                                                   sizeof(request_pdu),
                                                   0x0000U,
                                                   8U);
        if (!mb_err_is_ok(status)) {
            fprintf(stderr, "Failed to build read request: %s\n", mb_err_str(status));
            break;
        }

        client_result_t result = {
            .completed = false,
            .status = MB_OK,
            .quantity = 0U,
        };

        mb_client_request_t request = {
            .flags = 0U,
            .request = {
                .unit_id = unit_id,
                .function = request_pdu[0],
                .payload = &request_pdu[1],
                .payload_len = 4U,
            },
            .timeout_ms = 1000U,
            .max_retries = 1U,
            .retry_backoff_ms = 250U,
            .callback = client_callback,
            .user_ctx = &result,
        };

        mb_client_txn_t *txn = NULL;
        status = mb_client_submit(&client, &request, &txn);
        if (!mb_err_is_ok(status)) {
            fprintf(stderr, "mb_client_submit failed: %s\n", mb_err_str(status));
            sleep_ms(250U);
            continue;
        }

        while (!result.completed && !g_stop) {
            status = mb_client_poll(&client);
            if (status == MB_ERR_TIMEOUT) {
                sleep_ms(5U);
                continue;
            }

            if (!mb_err_is_ok(status)) {
                fprintf(stderr, "mb_client_poll error: %s\n", mb_err_str(status));
                break;
            }

            sleep_ms(1U);
        }

        if (result.completed) {
            if (mb_err_is_ok(result.status)) {
                printf("[client] read %u registers:\n", (unsigned)result.quantity);
                for (mb_u16 i = 0; i < result.quantity; ++i) {
                    printf("  R[%u] = %u (0x%04X)\n",
                           (unsigned)i,
                           result.registers[i],
                           result.registers[i]);
                }
            } else {
                printf("[client] transaction failed: %s\n", mb_err_str(result.status));
            }
        }

        if (sleep_between_ms > 0U) {
            sleep_ms(sleep_between_ms);
        }
    }

    demo_serial_port_close(&serial);
    printf("Client stopped.\n");
    return EXIT_SUCCESS;
}
