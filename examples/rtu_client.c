#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/runtime/builder.h>
#ifdef _WIN32
#include <modbuscore/transport/win32_rtu.h>
#else
#include <modbuscore/transport/posix_rtu.h>
#include <time.h>
#include <unistd.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool await_pdu(mbc_engine_t* engine, mbc_transport_iface_t* transport, uint32_t max_iters,
                      mbc_pdu_t* out)
{
    for (uint32_t i = 0; i < max_iters; ++i) {
        mbc_status_t st = mbc_engine_step(engine, 256U);
        if (st == MBC_STATUS_TIMEOUT) {
            return false;
        }
        if (!mbc_status_is_ok(st)) {
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

static bool submit_rtu(mbc_engine_t* engine, const mbc_pdu_t* pdu)
{
    uint8_t frame[2 + MBC_PDU_MAX];
    size_t frame_len = 0U;
    if (mbc_pdu_encode(pdu, frame, sizeof(frame), &frame_len) != MBC_STATUS_OK) {
        return false;
    }
    return mbc_status_is_ok(mbc_engine_submit_request(engine, frame, frame_len));
}

static void usage(const char* prog)
{
#ifdef _WIN32
    printf("Usage: %s --port <COMx> [--baud <rate>] [--unit <id>]\n", prog);
#else
    printf("Usage: %s --device </dev/ttyUSB0> [--baud <rate>] [--unit <id>]\n", prog);
#endif
    printf("Default baud: 9600, unit: 0x11\n");
}

int main(int argc, char** argv)
{
    uint32_t baud_rate = 9600U;
    uint8_t unit = 0x11U;
#ifdef _WIN32
    const char* port_name = NULL;
#else
    const char* device_path = NULL;
#endif

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
#ifdef _WIN32
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port_name = argv[++i];
#else
        } else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            device_path = argv[++i];
#endif
        } else if (strcmp(argv[i], "--baud") == 0 && i + 1 < argc) {
            baud_rate = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--unit") == 0 && i + 1 < argc) {
            unit = (uint8_t)atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

#ifdef _WIN32
    if (!port_name) {
        fprintf(stderr, "Missing --port argument (e.g., --port COM3)\n");
        return 1;
    }
    mbc_win32_rtu_config_t cfg = {
        .port_name = port_name,
        .baud_rate = baud_rate,
        .data_bits = 8,
        .parity = 'N',
        .stop_bits = 1,
        .guard_time_us = 0,
        .rx_buffer_capacity = 256,
    };
    mbc_transport_iface_t transport;
    mbc_win32_rtu_ctx_t* ctx = NULL;
    mbc_status_t status = mbc_win32_rtu_create(&cfg, &transport, &ctx);
#else
    if (!device_path) {
        fprintf(stderr, "Missing --device argument (e.g., --device /dev/ttyUSB0)\n");
        return 1;
    }
    mbc_posix_rtu_config_t cfg = {
        .device_path = device_path,
        .baud_rate = baud_rate,
        .data_bits = 8,
        .parity = 'N',
        .stop_bits = 1,
        .guard_time_us = 0,
        .rx_buffer_capacity = 256,
    };
    mbc_transport_iface_t transport;
    mbc_posix_rtu_ctx_t* ctx = NULL;
    mbc_status_t status = mbc_posix_rtu_create(&cfg, &transport, &ctx);
#endif

    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "Failed to open RTU transport (status=%d)\n", status);
        return 1;
    }

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_runtime_t runtime;
    if (mbc_runtime_builder_build(&builder, &runtime) != MBC_STATUS_OK) {
        fprintf(stderr, "Failed to build runtime\n");
#ifdef _WIN32
        mbc_win32_rtu_destroy(ctx);
#else
        mbc_posix_rtu_destroy(ctx);
#endif
        return 1;
    }

    mbc_engine_t engine;
    mbc_engine_config_t engine_cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_RTU,
        .use_override = false,
        .response_timeout_ms = 1000,
    };

    if (mbc_engine_init(&engine, &engine_cfg) != MBC_STATUS_OK) {
        fprintf(stderr, "Failed to initialise engine\n");
        mbc_runtime_shutdown(&runtime);
#ifdef _WIN32
        mbc_win32_rtu_destroy(ctx);
#else
        mbc_posix_rtu_destroy(ctx);
#endif
        return 1;
    }

    mbc_pdu_t request;
    printf("Requesting holding registers (unit 0x%02X)...\n", unit);
    if (!mbc_status_is_ok(mbc_pdu_build_read_holding_request(&request, unit, 0U, 4U)) ||
        !submit_rtu(&engine, &request)) {
        fprintf(stderr, "Failed to send RTU read request\n");
        goto cleanup;
    }

    mbc_pdu_t response = {0};
    if (!await_pdu(&engine, &transport, 200U, &response)) {
        fprintf(stderr, "Timed out waiting for response\n");
        goto cleanup;
    }

    const uint8_t* data = NULL;
    size_t count = 0U;
    if (mbc_pdu_parse_read_holding_response(&response, &data, &count) == MBC_STATUS_OK) {
        for (size_t i = 0; i < count; ++i) {
            uint16_t value = (uint16_t)(data[i * 2U] << 8) | data[i * 2U + 1U];
            printf("  [%zu] = 0x%04X (%u)\n", i, value, value);
        }
    } else {
        printf("Exception/function 0x%02X (code 0x%02X)\n", response.function, response.payload[0]);
        goto cleanup;
    }

    printf("\nWriting register 1 with value 0x4321\n");
    if (!mbc_status_is_ok(mbc_pdu_build_write_single_register(&request, unit, 1U, 0x4321U)) ||
        !submit_rtu(&engine, &request)) {
        fprintf(stderr, "Failed to send write request\n");
        goto cleanup;
    }

    if (!await_pdu(&engine, &transport, 200U, &response)) {
        fprintf(stderr, "Timed out waiting for write response\n");
        goto cleanup;
    }
    if ((response.function & 0x80U) != 0U) {
        printf("Write failed with exception 0x%02X\n", response.payload[0]);
        goto cleanup;
    }
    printf("Write confirmed by slave.\n");

cleanup:
    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
#ifdef _WIN32
    mbc_win32_rtu_destroy(ctx);
#else
    mbc_posix_rtu_destroy(ctx);
#endif
    return 0;
}
