#include <modbus/base.h>
#include <modbus/mb_err.h>
#include <modbus/mb_log.h>
#include <modbus/observe.h>
#include <modbus/pdu.h>
#include <modbus/server.h>

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

    if (event->source != MB_EVENT_SOURCE_SERVER) {
        return;
    }

    switch (event->type) {
    case MB_EVENT_CLIENT_STATE_ENTER:
    case MB_EVENT_CLIENT_STATE_EXIT:
    case MB_EVENT_CLIENT_TX_SUBMIT:
    case MB_EVENT_CLIENT_TX_COMPLETE:
        /* Client events are ignored by the server logger. */
        break;
    case MB_EVENT_SERVER_STATE_ENTER:
        printf("[server] state -> %u\n", (unsigned)event->data.server_state.state);
        break;
    case MB_EVENT_SERVER_STATE_EXIT:
        printf("[server] state <- %u\n", (unsigned)event->data.server_state.state);
        break;
    case MB_EVENT_SERVER_REQUEST_ACCEPT:
        printf("[server] accept fc=%u broadcast=%s\n",
               (unsigned)event->data.server_req.function,
               event->data.server_req.broadcast ? "yes" : "no");
        break;
    case MB_EVENT_SERVER_REQUEST_COMPLETE:
        printf("[server] request fc=%u status=%d\n",
               (unsigned)event->data.server_req.function,
               event->data.server_req.status);
        break;
    default:
        break;
    }
}

int main(int argc, char **argv)
{
    const char *device = NULL;
    unsigned baud = DEMO_DEFAULT_BAUD;
    mb_u8 unit_id = DEMO_DEFAULT_UNIT;
    bool enable_trace = false;

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--device") == 0 || strcmp(argv[i], "-d") == 0) && (i + 1) < argc) {
            device = argv[++i];
        } else if ((strcmp(argv[i], "--baud") == 0 || strcmp(argv[i], "-b") == 0) && (i + 1) < argc) {
            baud = (unsigned)strtoul(argv[++i], NULL, 0);
        } else if ((strcmp(argv[i], "--unit") == 0 || strcmp(argv[i], "-u") == 0) && (i + 1) < argc) {
            unit_id = (mb_u8)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--trace") == 0) {
            enable_trace = true;
        } else {
            fprintf(stderr,
                    "Usage: %s --device <path-or-com> [--baud <rate>] [--unit <id>] [--trace]\n",
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

    mb_server_t server;
    mb_server_region_t regions[2];
    mb_server_request_t request_pool[8];
    uint16_t holding_regs[32];
    memset(holding_regs, 0, sizeof(holding_regs));

    status = mb_server_init(&server,
                            iface,
                            unit_id,
                            regions,
                            MB_COUNTOF(regions),
                            request_pool,
                            MB_COUNTOF(request_pool));
    if (!mb_err_is_ok(status)) {
        fprintf(stderr, "mb_server_init failed: %s\n", mb_err_str(status));
        demo_serial_port_close(&serial);
        return EXIT_FAILURE;
    }

    status = mb_server_add_storage(&server,
                                   0x0000U,
                                   (mb_u16)MB_COUNTOF(holding_regs),
                                   false,
                                   holding_regs);
    if (!mb_err_is_ok(status)) {
        fprintf(stderr, "mb_server_add_storage failed: %s\n", mb_err_str(status));
        demo_serial_port_close(&serial);
        return EXIT_FAILURE;
    }

    mb_server_set_event_callback(&server, log_event, NULL);
    mb_server_set_trace_hex(&server, enable_trace);

    printf("Modbus RTU server listening on %s (baud=%u, unit=%u). Press Ctrl+C to stop.\n",
           device,
           baud,
           (unsigned)unit_id);

    unsigned register_tick = 0U;

    while (!g_stop) {
        holding_regs[0] = (uint16_t)register_tick;
        holding_regs[1] = (uint16_t)(register_tick >> 16);
        holding_regs[2] = (uint16_t)(time(NULL) & 0xFFFF);
        register_tick++;

        status = mb_server_poll(&server);
        if (status == MB_ERR_TIMEOUT) {
            sleep_ms(5U);
            continue;
        }

        if (!mb_err_is_ok(status)) {
            printf("[server] transport error: %s\n", mb_err_str(status));
            sleep_ms(50U);
        }
    }

    demo_serial_port_close(&serial);
    printf("Server stopped.\n");
    return EXIT_SUCCESS;
}
