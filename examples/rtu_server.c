#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/iface.h>

#ifdef _WIN32
#  include <modbuscore/transport/win32_rtu.h>
#else
#  include <modbuscore/transport/posix_rtu.h>
#  include <fcntl.h>
#  include <stdlib.h>
#  include <string.h>
#  include <unistd.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define HOLDING_REG_COUNT 64

static bool encode_exception(uint8_t unit, uint8_t function, uint8_t code, mbc_pdu_t *out)
{
    if (!out) {
        return false;
    }
    out->unit_id = unit;
    out->function = (uint8_t)(function | 0x80U);
    out->payload[0] = code;
    out->payload_length = 1U;
    return true;
}

static bool handle_request(const mbc_pdu_t *request,
                           mbc_pdu_t *response,
                           uint16_t *registers,
                           size_t register_count)
{
    if (!request || !response || !registers) {
        return false;
    }

    response->unit_id = request->unit_id;

    switch (request->function) {
    case 0x03: {
        if (request->payload_length < 4U) {
            return encode_exception(request->unit_id, request->function, 0x03U, response);
        }
        uint16_t address = (uint16_t)((request->payload[0] << 8) | request->payload[1]);
        uint16_t quantity = (uint16_t)((request->payload[2] << 8) | request->payload[3]);
        if (quantity == 0U || (size_t)address + quantity > register_count) {
            return encode_exception(request->unit_id, request->function, 0x02U, response);
        }
        response->function = 0x03U;
        response->payload[0] = (uint8_t)(quantity * 2U);
        for (uint16_t i = 0; i < quantity; ++i) {
            uint16_t value = registers[address + i];
            response->payload[1 + i * 2U] = (uint8_t)(value >> 8);
            response->payload[2 + i * 2U] = (uint8_t)(value & 0xFFU);
        }
        response->payload_length = (size_t)(1U + quantity * 2U);
        return true;
    }
    case 0x06: {
        if (request->payload_length < 4U) {
            return encode_exception(request->unit_id, request->function, 0x03U, response);
        }
        uint16_t address = (uint16_t)((request->payload[0] << 8) | request->payload[1]);
        if (address >= register_count) {
            return encode_exception(request->unit_id, request->function, 0x02U, response);
        }
        uint16_t value = (uint16_t)((request->payload[2] << 8) | request->payload[3]);
        registers[address] = value;
        response->function = 0x06U;
        memcpy(response->payload, request->payload, 4U);
        response->payload_length = 4U;
        return true;
    }
    case 0x10: {
        if (request->payload_length < 5U) {
            return encode_exception(request->unit_id, request->function, 0x03U, response);
        }
        uint16_t address = (uint16_t)((request->payload[0] << 8) | request->payload[1]);
        uint16_t quantity = (uint16_t)((request->payload[2] << 8) | request->payload[3]);
        uint8_t byte_count = request->payload[4];
        if (quantity == 0U || byte_count != quantity * 2U ||
            (size_t)address + quantity > register_count ||
            request->payload_length < (size_t)(5U + byte_count)) {
            return encode_exception(request->unit_id, request->function, 0x03U, response);
        }
        for (uint16_t i = 0; i < quantity; ++i) {
            uint8_t hi = request->payload[5 + i * 2U];
            uint8_t lo = request->payload[6 + i * 2U];
            registers[address + i] = (uint16_t)((hi << 8) | lo);
        }
        response->function = 0x10U;
        memcpy(response->payload, request->payload, 4U);
        response->payload_length = 4U;
        return true;
    }
    default:
        return encode_exception(request->unit_id, request->function, 0x01U, response);
    }
}

static void usage(const char *prog)
{
#ifdef _WIN32
    printf("Usage: %s --port <COMx> [--baud <rate>] [--unit <id>]\n", prog);
#else
    printf("Usage: %s --device </dev/ttyUSB0> [--baud <rate>] [--unit <id>]\n", prog);
#endif
    printf("Default baud: 9600, unit: 0x11\n");
}

int main(int argc, char **argv)
{
    uint32_t baud_rate = 9600U;
    uint8_t unit_id = 0x11U;
#ifdef _WIN32
    const char *port_name = NULL;
#else
    const char *device_path = NULL;
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
            unit_id = (uint8_t)atoi(argv[++i]);
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
    mbc_win32_rtu_ctx_t *ctx = NULL;
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
    mbc_posix_rtu_ctx_t *ctx = NULL;
    mbc_status_t status = mbc_posix_rtu_create(&cfg, &transport, &ctx);
#endif

    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "Failed to open RTU interface (status=%d)\n", status);
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
        .role = MBC_ENGINE_ROLE_SERVER,
        .framing = MBC_FRAMING_RTU,
        .use_override = false,
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

    uint16_t holding_registers[HOLDING_REG_COUNT];
    for (size_t i = 0; i < HOLDING_REG_COUNT; ++i) {
        holding_registers[i] = (uint16_t)i;
    }

    printf("Modbus RTU server ready (unit 0x%02X, baud %u)\n", unit_id, baud_rate);
    printf("Waiting for master requests...\n");

    while (true) {
        mbc_status_t step = mbc_engine_step(&engine, 256U);
        if (step == MBC_STATUS_IO_ERROR) {
            fprintf(stderr, "I/O error (device disconnected?)\n");
            break;
        }
        if (step == MBC_STATUS_DECODING_ERROR) {
            printf("CRC or framing error detected, waiting for next request...\n");
            continue;
        }

        mbc_pdu_t request = {0};
        if (!mbc_engine_take_pdu(&engine, &request)) {
            continue;
        }

        if (request.unit_id != unit_id) {
            printf("Ignoring request for unit 0x%02X\n", request.unit_id);
            continue;
        }

        printf("Received RTU function 0x%02X\n", request.function);

        mbc_pdu_t response = {0};
        if (!handle_request(&request, &response, holding_registers, HOLDING_REG_COUNT)) {
            fprintf(stderr, "Failed to build RTU response\n");
            break;
        }

        uint8_t frame[2 + MBC_PDU_MAX];
        size_t frame_len = 0U;
        if (mbc_pdu_encode(&response, frame, sizeof(frame), &frame_len) != MBC_STATUS_OK) {
            fprintf(stderr, "Failed to encode RTU response\n");
            break;
        }

        if (!mbc_status_is_ok(mbc_engine_submit_request(&engine, frame, frame_len))) {
            fprintf(stderr, "Failed to send RTU response\n");
            break;
        }
    }

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
#ifdef _WIN32
    mbc_win32_rtu_destroy(ctx);
#else
    mbc_posix_rtu_destroy(ctx);
#endif
    return 0;
}
