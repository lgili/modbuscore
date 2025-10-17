#include <modbus/base.h>
#include <modbus/mb_err.h>
#include <modbus/mb_types.h>
#include <modbus/pdu.h>
#include <modbus/transport/tcp_multi.h>

#include "common/demo_tcp_socket.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#define DEMO_MAX_ENDPOINTS MB_TCP_MAX_CONNECTIONS
#define DEMO_DEFAULT_TIMEOUT_MS 4000U
#define DEMO_DEFAULT_COUNT 4U
#define DEMO_DEFAULT_START 0U
#define DEMO_DEFAULT_UNIT 0x11U

typedef struct {
    char host[128];
    uint16_t port;
    demo_tcp_socket_t socket;
    bool connected;
    mb_size_t slot_index;
    bool response_done;
    mb_u16 last_tid;
} demo_endpoint_t;

typedef struct {
    demo_endpoint_t *endpoint;
} demo_slot_state_t;

typedef struct {
    demo_endpoint_t *endpoints;
    demo_slot_state_t *slots;
    size_t endpoint_count;
    size_t completed;
} demo_multi_context_t;

static void sleep_ms(unsigned int milliseconds)
{
#if defined(_WIN32)
    Sleep(milliseconds);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(milliseconds / 1000U);
    ts.tv_nsec = (long)((milliseconds % 1000U) * 1000000L);
    nanosleep(&ts, NULL);
#endif
}

static mb_time_ms_t monotonic_ms(void)
{
#if defined(_WIN32)
    return (mb_time_ms_t)GetTickCount64();
#else
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (mb_time_ms_t)ts.tv_sec * 1000ULL + (mb_time_ms_t)(ts.tv_nsec / 1000000ULL);
    }
#endif
    return (mb_time_ms_t)(clock() * 1000.0 / CLOCKS_PER_SEC);
#endif
}

static int find_endpoint_by_slot(demo_multi_context_t *ctx, mb_size_t slot)
{
    for (size_t i = 0; i < ctx->endpoint_count; ++i) {
        if (ctx->endpoints[i].connected && ctx->endpoints[i].slot_index == slot) {
            return (int)i;
        }
    }
    return -1;
}

static void multi_callback(mb_tcp_multi_transport_t *multi,
                           mb_size_t slot_index,
                           const mb_adu_view_t *adu,
                           mb_u16 transaction_id,
                           mb_err_t status,
                           void *user)
{
    (void)multi;
    demo_multi_context_t *ctx = (demo_multi_context_t *)user;
    const int idx = find_endpoint_by_slot(ctx, slot_index);
    if (idx < 0) {
        return;
    }

    demo_endpoint_t *endpoint = &ctx->endpoints[idx];
    endpoint->response_done = true;
    endpoint->last_tid = transaction_id;

    if (mb_err_is_ok(status) && adu != NULL && adu->payload_len >= 1U) {
        const mb_u8 byte_count = adu->payload[0];
        printf("[%s:%u] TID=0x%04X fc=%u status=OK bytes=%u\n",
               endpoint->host,
               (unsigned)endpoint->port,
               (unsigned)transaction_id,
               (unsigned)adu->function,
               (unsigned)byte_count);

        for (mb_size_t i = 0U; i + 1U < adu->payload_len; i += 2U) {
            const mb_size_t reg_index = i / 2U;
            if (reg_index == 0U) {
                continue; /* skip byte count */
            }
            const mb_size_t payload_index = i;
            const mb_u8 hi = adu->payload[payload_index];
            const mb_u8 lo = adu->payload[payload_index + 1U];
            const uint16_t value = (uint16_t)((hi << 8) | lo);
            printf("  reg[%zu] = 0x%04X (%u)\n", reg_index - 1U, value, value);
        }
    } else {
        printf("[%s:%u] TID=0x%04X error=%s\n",
               endpoint->host,
               (unsigned)endpoint->port,
               (unsigned)transaction_id,
               mb_err_str(status));
    }

    ctx->completed += 1U;
}

static bool parse_endpoint(const char *spec, char *host_out, size_t host_cap, uint16_t *port_out)
{
    const char *sep = strrchr(spec, ':');
    if (sep == NULL) {
        return false;
    }

    const size_t host_len = (size_t)(sep - spec);
    if (host_len == 0U || host_len >= host_cap) {
        return false;
    }

    memcpy(host_out, spec, host_len);
    host_out[host_len] = '\0';

    const char *port_str = sep + 1;
    char *endptr = NULL;
    long port_value = strtol(port_str, &endptr, 10);
    if (endptr == port_str || port_value <= 0 || port_value > 65535) {
        return false;
    }

    *port_out = (uint16_t)port_value;
    return true;
}

int main(int argc, char **argv)
{
    demo_endpoint_t endpoints[DEMO_MAX_ENDPOINTS];
    memset(endpoints, 0, sizeof(endpoints));

    size_t endpoint_count = 0U;
    mb_u16 start_address = DEMO_DEFAULT_START;
    mb_u16 quantity = DEMO_DEFAULT_COUNT;
    mb_u8 unit_id = DEMO_DEFAULT_UNIT;
    mb_time_ms_t timeout_ms = DEMO_DEFAULT_TIMEOUT_MS;

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--endpoint") == 0 || strcmp(argv[i], "-e") == 0) && (i + 1) < argc) {
            if (endpoint_count >= DEMO_MAX_ENDPOINTS) {
                fprintf(stderr, "Maximum number of endpoints (%u) reached.\n", (unsigned)DEMO_MAX_ENDPOINTS);
                return EXIT_FAILURE;
            }
            const char *spec = argv[++i];
            if (!parse_endpoint(spec, endpoints[endpoint_count].host, sizeof(endpoints[endpoint_count].host), &endpoints[endpoint_count].port)) {
                fprintf(stderr, "Invalid endpoint specification: %s\n", spec);
                return EXIT_FAILURE;
            }
            endpoint_count += 1U;
        } else if ((strcmp(argv[i], "--start") == 0 || strcmp(argv[i], "-s") == 0) && (i + 1) < argc) {
            start_address = (mb_u16)strtoul(argv[++i], NULL, 0);
        } else if ((strcmp(argv[i], "--count") == 0 || strcmp(argv[i], "-c") == 0) && (i + 1) < argc) {
            quantity = (mb_u16)strtoul(argv[++i], NULL, 0);
        } else if ((strcmp(argv[i], "--unit") == 0 || strcmp(argv[i], "-u") == 0) && (i + 1) < argc) {
            unit_id = (mb_u8)strtoul(argv[++i], NULL, 0);
        } else if ((strcmp(argv[i], "--timeout") == 0 || strcmp(argv[i], "-t") == 0) && (i + 1) < argc) {
            timeout_ms = (mb_time_ms_t)strtoul(argv[++i], NULL, 0);
        } else {
            fprintf(stderr, "Usage: %s --endpoint host:port [--endpoint host:port ...] [--unit id] [--start addr] [--count qty] [--timeout ms]\n",
                    argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (endpoint_count == 0U) {
        fprintf(stderr, "At least one --endpoint is required.\n");
        return EXIT_FAILURE;
    }

#if defined(_WIN32)
    if (!mb_err_is_ok(demo_tcp_socket_global_init())) {
        fprintf(stderr, "Failed to initialise Winsock.\n");
        return EXIT_FAILURE;
    }
#endif

    mb_tcp_multi_transport_t multi;
    demo_slot_state_t slot_state[MB_TCP_MAX_CONNECTIONS];
    memset(slot_state, 0, sizeof(slot_state));

    demo_multi_context_t ctx = {
        .endpoints = endpoints,
        .slots = slot_state,
        .endpoint_count = endpoint_count,
        .completed = 0U,
    };

    if (!mb_err_is_ok(mb_tcp_multi_init(&multi, multi_callback, &ctx))) {
        fprintf(stderr, "Failed to initialise multi transport.\n");
#if defined(_WIN32)
        demo_tcp_socket_global_cleanup();
#endif
        return EXIT_FAILURE;
    }

    printf("Connecting to %zu endpoints...\n", endpoint_count);

    for (size_t i = 0; i < endpoint_count; ++i) {
        demo_endpoint_t *endpoint = &endpoints[i];
        const mb_err_t status = demo_tcp_socket_connect(&endpoint->socket,
                                                        endpoint->host,
                                                        endpoint->port,
                                                        timeout_ms);
        if (!mb_err_is_ok(status)) {
            fprintf(stderr, "Failed to connect to %s:%u (%s)\n",
                    endpoint->host,
                    (unsigned)endpoint->port,
                    mb_err_str(status));
            continue;
        }

        const mb_transport_if_t *iface = demo_tcp_socket_iface(&endpoint->socket);
        if (iface == NULL) {
            fprintf(stderr, "Invalid iface for %s:%u\n", endpoint->host, (unsigned)endpoint->port);
            demo_tcp_socket_close(&endpoint->socket);
            continue;
        }

        mb_size_t slot = 0U;
        if (!mb_err_is_ok(mb_tcp_multi_add(&multi, iface, &slot))) {
            fprintf(stderr, "Failed to add slot for %s:%u\n", endpoint->host, (unsigned)endpoint->port);
            demo_tcp_socket_close(&endpoint->socket);
            continue;
        }

        endpoint->slot_index = slot;
        endpoint->connected = true;
        endpoint->response_done = false;
        endpoint->last_tid = 0U;
        slot_state[slot].endpoint = endpoint;

        printf("  [%zu] %s:%u -> slot %zu\n", i, endpoint->host, (unsigned)endpoint->port, (size_t)slot);
    }

    size_t active_connections = 0U;
    for (size_t i = 0; i < endpoint_count; ++i) {
        if (endpoints[i].connected) {
            active_connections += 1U;
        }
    }

    if (active_connections == 0U) {
        fprintf(stderr, "No endpoints connected successfully.\n");
        for (size_t i = 0; i < endpoint_count; ++i) {
            demo_tcp_socket_close(&endpoints[i].socket);
        }
#if defined(_WIN32)
        demo_tcp_socket_global_cleanup();
#endif
        return EXIT_FAILURE;
    }

    mb_u8 pdu[5];
    mb_size_t pdu_len = sizeof(pdu);
    mb_err_t pdu_status = mb_pdu_build_read_holding_request(pdu, sizeof(pdu), start_address, quantity);
    if (!mb_err_is_ok(pdu_status)) {
        fprintf(stderr, "Failed to build request PDU: %s\n", mb_err_str(pdu_status));
        for (size_t i = 0; i < endpoint_count; ++i) {
            demo_tcp_socket_close(&endpoints[i].socket);
        }
#if defined(_WIN32)
        demo_tcp_socket_global_cleanup();
#endif
        return EXIT_FAILURE;
    }

    const mb_adu_view_t adu = {
        .unit_id = unit_id,
        .function = MB_PDU_FC_READ_HOLDING_REGISTERS,
        .payload = &pdu[1],
        .payload_len = pdu_len - 1U,
    };

    printf("Submitting FC03 request (unit=%u addr=%u count=%u)\n",
           (unsigned)unit_id,
           (unsigned)start_address,
           (unsigned)quantity);

    for (size_t i = 0; i < endpoint_count; ++i) {
        demo_endpoint_t *endpoint = &endpoints[i];
        if (!endpoint->connected) {
            continue;
        }

        mb_u16 tid = (mb_u16)(0x1000U + i);
        const mb_err_t submit_status = mb_tcp_multi_submit(&multi, endpoint->slot_index, &adu, tid);
        if (!mb_err_is_ok(submit_status)) {
            fprintf(stderr, "Failed to submit request to %s:%u (%s)\n",
                    endpoint->host,
                    (unsigned)endpoint->port,
                    mb_err_str(submit_status));
            endpoint->connected = false;
            demo_tcp_socket_close(&endpoint->socket);
            active_connections -= 1U;
        }
    }

    if (active_connections == 0U) {
        fprintf(stderr, "All submissions failed.\n");
#if defined(_WIN32)
        demo_tcp_socket_global_cleanup();
#endif
        return EXIT_FAILURE;
    }

    const mb_time_ms_t started_ms = monotonic_ms();
    while (ctx.completed < active_connections) {
        mb_err_t poll_status = mb_tcp_multi_poll_all(&multi);
        if (!mb_err_is_ok(poll_status) && poll_status != MB_ERR_TIMEOUT) {
            fprintf(stderr, "Poll error: %s\n", mb_err_str(poll_status));
            break;
        }

        const mb_time_ms_t now_ms = monotonic_ms();
        if ((now_ms - started_ms) > timeout_ms) {
            fprintf(stderr, "Timed out waiting for responses.\n");
            break;
        }

        sleep_ms(20U);
    }

    for (size_t i = 0; i < endpoint_count; ++i) {
        demo_tcp_socket_close(&endpoints[i].socket);
    }

#if defined(_WIN32)
    demo_tcp_socket_global_cleanup();
#endif

    printf("Completed %zu/%zu endpoints.\n", ctx.completed, active_connections);
    return (ctx.completed == active_connections) ? EXIT_SUCCESS : EXIT_FAILURE;
}
