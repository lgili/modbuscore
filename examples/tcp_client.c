#if !defined(_WIN32)
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif
#endif

#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/mbap.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/iface.h>
#ifdef _WIN32
#include <modbuscore/transport/winsock_tcp.h>
#else
#include <modbuscore/transport/posix_tcp.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <time.h>
#endif

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 15020
#define DEFAULT_UNIT 0x11U

static bool await_pdu(mbc_engine_t* engine, mbc_transport_iface_t* transport,
                      uint32_t max_iterations, mbc_pdu_t* out)
{
    for (uint32_t i = 0; i < max_iterations; ++i) {
        mbc_status_t status = mbc_engine_step(engine, 256U);
        if (status == MBC_STATUS_TIMEOUT) {
            return false;
        }
        if (!mbc_status_is_ok(status)) {
            return false;
        }
        if (mbc_engine_take_pdu(engine, out)) {
            return true;
        }
        mbc_transport_yield(transport);
#ifdef _WIN32
        Sleep(1);
#else
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 1 * 1000 * 1000};
        nanosleep(&ts, NULL);
#endif
    }
    return false;
}

static bool submit_mbap_request(mbc_engine_t* engine, const mbc_pdu_t* request,
                                uint16_t transaction_id)
{
    uint8_t payload[1 + MBC_PDU_MAX];
    payload[0] = request->function;
    memcpy(&payload[1], request->payload, request->payload_length);

    mbc_mbap_header_t header = {
        .transaction_id = transaction_id,
        .protocol_id = 0,
        .length = 0,
        .unit_id = request->unit_id,
    };

    uint8_t frame[260];
    size_t frame_length = 0U;
    if (mbc_mbap_encode(&header, payload, 1U + request->payload_length, frame, sizeof(frame),
                        &frame_length) != MBC_STATUS_OK) {
        return false;
    }

    return mbc_status_is_ok(mbc_engine_submit_request(engine, frame, frame_length));
}

static void usage(const char* prog)
{
    printf("Usage: %s [--host <addr>] [--port <tcp-port>] [--unit <id>]\n", prog);
    printf("Default host: %s, port: %d, unit: 0x%02X\n", DEFAULT_HOST, DEFAULT_PORT, DEFAULT_UNIT);
    printf("Ensure the TCP server example is running before executing this client.\n");
}

int main(int argc, char** argv)
{
    const char* host = DEFAULT_HOST;
    uint16_t port = DEFAULT_PORT;
    uint8_t unit = DEFAULT_UNIT;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--unit") == 0 && i + 1 < argc) {
            unit = (uint8_t)atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

#ifdef _WIN32
    mbc_winsock_tcp_config_t config = {
        .host = host,
        .port = port,
        .connect_timeout_ms = 2000,
        .recv_timeout_ms = 2000,
    };
    mbc_transport_iface_t transport;
    mbc_winsock_tcp_ctx_t* ctx = NULL;
    mbc_status_t status = mbc_winsock_tcp_create(&config, &transport, &ctx);
#else
    mbc_posix_tcp_config_t config = {
        .host = host,
        .port = port,
        .connect_timeout_ms = 2000,
        .recv_timeout_ms = 2000,
    };
    mbc_transport_iface_t transport;
    mbc_posix_tcp_ctx_t* ctx = NULL;
    mbc_status_t status = mbc_posix_tcp_create(&config, &transport, &ctx);
#endif
    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "Failed to connect to %s:%u (status=%d)\n", host, port, status);
        return 1;
    }

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_runtime_t runtime;
    if (mbc_runtime_builder_build(&builder, &runtime) != MBC_STATUS_OK) {
        fprintf(stderr, "Failed to build runtime\n");
#ifdef _WIN32
        mbc_winsock_tcp_destroy(ctx);
#else
        mbc_posix_tcp_destroy(ctx);
#endif
        return 1;
    }

    mbc_engine_t engine;
    mbc_engine_config_t engine_cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,
        .use_override = false,
        .response_timeout_ms = 2000,
    };

    if (mbc_engine_init(&engine, &engine_cfg) != MBC_STATUS_OK) {
        fprintf(stderr, "Failed to initialise engine\n");
        mbc_runtime_shutdown(&runtime);
#ifdef _WIN32
        mbc_winsock_tcp_destroy(ctx);
#else
        mbc_posix_tcp_destroy(ctx);
#endif
        return 1;
    }

    uint16_t transaction = 1U;

    printf("Reading holding registers (unit 0x%02X)...\n", unit);
    mbc_pdu_t read_request;
    mbc_status_t pdu_status = mbc_pdu_build_read_holding_request(&read_request, unit, 0U, 4U);
    if (!mbc_status_is_ok(pdu_status) ||
        !submit_mbap_request(&engine, &read_request, transaction++)) {
        fprintf(stderr, "Failed to send read request\n");
        goto cleanup;
    }

    mbc_pdu_t response = {0};
    if (!await_pdu(&engine, &transport, 200U, &response)) {
        fprintf(stderr, "Timed out waiting for read response\n");
        goto cleanup;
    }

    const uint8_t* register_data = NULL;
    size_t register_count = 0U;
    if (mbc_pdu_parse_read_holding_response(&response, &register_data, &register_count) ==
        MBC_STATUS_OK) {
        printf("Holding registers:\n");
        for (size_t i = 0; i < register_count; ++i) {
            uint16_t value = (uint16_t)(register_data[i * 2U] << 8) | register_data[i * 2U + 1U];
            printf("  [%zu] = 0x%04X (%u)\n", i, value, value);
        }
    } else if ((response.function & 0x80U) != 0U) {
        printf("Server returned exception 0x%02X\n", response.payload[0]);
        goto cleanup;
    } else {
        fprintf(stderr, "Unexpected response format\n");
        goto cleanup;
    }

    printf("\nWriting register 1 with value 0x1234\n");
    mbc_pdu_t write_request;
    pdu_status = mbc_pdu_build_write_single_register(&write_request, unit, 1U, 0x1234U);
    if (!mbc_status_is_ok(pdu_status) ||
        !submit_mbap_request(&engine, &write_request, transaction++)) {
        fprintf(stderr, "Failed to send write request\n");
        goto cleanup;
    }

    if (!await_pdu(&engine, &transport, 200U, &response)) {
        fprintf(stderr, "Timed out waiting for write response\n");
        goto cleanup;
    }

    if ((response.function & 0x80U) != 0U) {
        printf("Server returned exception 0x%02X on write\n", response.payload[0]);
        goto cleanup;
    }
    printf("Write confirmed by server\n\n");

    printf("Reading registers again to confirm changes...\n");
    if (!submit_mbap_request(&engine, &read_request, transaction++)) {
        fprintf(stderr, "Failed to submit follow-up read\n");
        goto cleanup;
    }
    if (!await_pdu(&engine, &transport, 200U, &response)) {
        fprintf(stderr, "Timed out waiting for follow-up read\n");
        goto cleanup;
    }
    if (mbc_pdu_parse_read_holding_response(&response, &register_data, &register_count) ==
        MBC_STATUS_OK) {
        for (size_t i = 0; i < register_count; ++i) {
            uint16_t value = (uint16_t)(register_data[i * 2U] << 8) | register_data[i * 2U + 1U];
            printf("  [%zu] = 0x%04X (%u)\n", i, value, value);
        }
        printf("Done.\n");
    } else {
        printf("Unexpected response while verifying registers.\n");
    }

cleanup:
    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
#ifdef _WIN32
    mbc_winsock_tcp_destroy(ctx);
#else
    mbc_posix_tcp_destroy(ctx);
#endif
    return 0;
}
