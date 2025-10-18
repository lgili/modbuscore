/**
 * @file example_tcp_client_fc03.c
 * @brief End-to-end example: Modbus TCP client reading holding registers (FC03).
 *
 * This example demonstrates complete integration of all layers:
 * - POSIX TCP driver (transport)
 * - Runtime with dependency injection
 * - Protocol engine (FSM)
 * - PDU builders/parsers
 * - MBAP framing for Modbus TCP
 *
 * To test:
 * 1. Run a local Modbus TCP server on port 5502 (e.g., modpoll, pymodbus, or simple_tcp_server.py)
 * 2. Compile: gcc example_tcp_client_fc03.c -L../build -lmodbuscore -I../include -o client
 * 3. Execute: ./client
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <modbuscore/transport/posix_tcp.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 5502
#define UNIT_ID 1
#define START_ADDRESS 0
#define REGISTER_COUNT 10

static void print_registers(const uint8_t *data, size_t count)
{
    printf("  Registers read:\n");
    for (size_t i = 0; i < count; ++i) {
        uint16_t value = (uint16_t)((data[i * 2] << 8) | data[i * 2 + 1]);
        printf("    [%zu] = 0x%04X (%u)\n", i, value, value);
    }
}

int main(void)
{
    printf("=== ModbusCore v3.0 - TCP Client Example (FC03) ===\n\n");

    /* ========================================================================
     * Step 1: Create POSIX TCP transport driver
     * ====================================================================== */
    printf("Step 1: Creating TCP transport...\n");

    mbc_posix_tcp_config_t tcp_config = {
        .host = SERVER_HOST,
        .port = SERVER_PORT,
        .connect_timeout_ms = 5000,
        .recv_timeout_ms = 1000,
    };

    mbc_transport_iface_t transport;
    mbc_posix_tcp_ctx_t *tcp_ctx = NULL;

    mbc_status_t status = mbc_posix_tcp_create(&tcp_config, &transport, &tcp_ctx);
    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "✗ Failed to connect to %s:%u (status=%d)\n",
                SERVER_HOST, SERVER_PORT, status);
        fprintf(stderr, "  Make sure a Modbus TCP server is running.\n");
        return 1;
    }

    printf("✓ Connected to %s:%u\n\n", SERVER_HOST, SERVER_PORT);

    /* ========================================================================
     * Step 2: Create runtime with dependency injection
     * ====================================================================== */
    printf("Step 2: Building runtime with DI...\n");

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_runtime_t runtime;
    status = mbc_runtime_builder_build(&builder, &runtime);
    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "✗ Failed to build runtime (status=%d)\n", status);
        mbc_posix_tcp_destroy(tcp_ctx);
        return 1;
    }

    printf("✓ Runtime initialized\n\n");

    /* ========================================================================
     * Step 3: Create protocol engine (client mode)
     * ====================================================================== */
    printf("Step 3: Initializing protocol engine (client mode)...\n");

    mbc_engine_t engine;
    mbc_engine_config_t engine_config = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,  /* Use TCP (MBAP) framing */
        .use_override = false,
        .response_timeout_ms = 3000,
        .event_cb = NULL,  /* No telemetry in this example */
        .event_ctx = NULL,
    };

    status = mbc_engine_init(&engine, &engine_config);
    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "✗ Failed to initialize engine (status=%d)\n", status);
        mbc_runtime_shutdown(&runtime);
        mbc_posix_tcp_destroy(tcp_ctx);
        return 1;
    }

    printf("✓ Engine ready\n\n");

    /* ========================================================================
     * Step 4: Build and send FC03 request (Read Holding Registers)
     * ====================================================================== */
    printf("Step 4: Building FC03 request (unit=%u, addr=%u, count=%u)...\n",
           UNIT_ID, START_ADDRESS, REGISTER_COUNT);

    mbc_pdu_t request_pdu;
    status = mbc_pdu_build_read_holding_request(&request_pdu, UNIT_ID,
                                                START_ADDRESS, REGISTER_COUNT);
    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "✗ Failed to build request PDU (status=%d)\n", status);
        goto cleanup;
    }

    /* Serialize PDU to bytes (Function + Data only, without Unit ID) */
    uint8_t pdu_buffer[256];
    pdu_buffer[0] = request_pdu.function;
    memcpy(&pdu_buffer[1], request_pdu.payload, request_pdu.payload_length);
    size_t pdu_length = 1 + request_pdu.payload_length;

    /* Wrap PDU with MBAP header for Modbus TCP */
    mbc_mbap_header_t mbap_header = {
        .transaction_id = 1,      /* Transaction ID (can be incremented) */
        .protocol_id = 0,         /* Always 0 for Modbus */
        .length = 0,              /* Will be set by mbc_mbap_encode */
        .unit_id = UNIT_ID
    };

    uint8_t request_buffer[256];
    size_t request_length = 0;
    status = mbc_mbap_encode(&mbap_header, pdu_buffer, pdu_length,
                             request_buffer, sizeof(request_buffer), &request_length);
    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "✗ Failed to encode MBAP frame (status=%d)\n", status);
        goto cleanup;
    }

    printf("✓ MBAP frame encoded (%zu bytes: 7 MBAP + %zu PDU)\n", request_length, pdu_length);

    /* Debug: print hex dump of request */
    printf("  Request bytes: ");
    for (size_t i = 0; i < request_length; i++) {
        printf("%02X ", request_buffer[i]);
    }
    printf("\n");
    printf("  Sending request...\n");

    status = mbc_engine_submit_request(&engine, request_buffer, request_length);
    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "✗ Failed to submit request (status=%d)\n", status);
        goto cleanup;
    }

    printf("✓ Request submitted\n\n");

    /* Give TCP stack time to actually transmit the data */
    usleep(10000);  /* 10ms delay to ensure send completes */

    /* ========================================================================
     * Step 5: Poll until complete response is received
     * ====================================================================== */
    printf("Step 5: Polling for response (max 100 iterations)...\n");

    bool response_received = false;
    size_t total_bytes_received = 0;
    for (int i = 0; i < 100; ++i) {
        status = mbc_engine_step(&engine, 10);  /* Budget = 10 bytes per iteration */

        /* Debug: show status every 10 iterations */
        if (i % 10 == 0 || i < 5) {
            printf("  [iter %d] engine_step() returned status=%d\n", i, status);
        }

        /* Debug: check internal state (hack for debugging) */
        if (i == 0) {
            printf("  Engine initial state: state=%d, rx_length=%zu\n",
                   engine.state, engine.rx_length);
        }

        if (status == MBC_STATUS_TIMEOUT) {
            fprintf(stderr, "✗ Timeout waiting for response (iteration %d)\n", i);
            goto cleanup;
        }

        if (!mbc_status_is_ok(status) && status != MBC_STATUS_OK) {
            fprintf(stderr, "✗ Engine step error (status=%d, iteration=%d)\n", status, i);
            goto cleanup;
        }

        /* Check if we received a complete PDU */
        mbc_pdu_t response_pdu;
        if (mbc_engine_take_pdu(&engine, &response_pdu)) {
            printf("✓ Response received after %d iterations!\n\n", i);

            /* Check if it's an exception */
            if (response_pdu.function & 0x80U) {
                uint8_t original_fc, exception_code;
                mbc_pdu_parse_exception(&response_pdu, &original_fc, &exception_code);
                fprintf(stderr, "✗ Server returned exception: FC=0x%02X, Code=0x%02X\n",
                        original_fc, exception_code);
                goto cleanup;
            }

            /* Parse FC03 response */
            const uint8_t *register_data = NULL;
            size_t register_count = 0;

            status = mbc_pdu_parse_read_holding_response(&response_pdu,
                                                         &register_data,
                                                         &register_count);
            if (!mbc_status_is_ok(status)) {
                fprintf(stderr, "✗ Failed to parse response (status=%d)\n", status);
                goto cleanup;
            }

            printf("Step 6: Parsing response...\n");
            print_registers(register_data, register_count);

            response_received = true;
            break;
        }

        /* No PDU yet, continue polling */
    }

    if (!response_received) {
        fprintf(stderr, "✗ No response after 100 iterations\n");
        goto cleanup;
    }

    printf("\n=== SUCCESS ===\n");

cleanup:
    /* Cleanup in reverse order of creation */
    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_posix_tcp_destroy(tcp_ctx);

    return response_received ? 0 : 1;
}
