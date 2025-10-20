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
#include <modbuscore/runtime/diagnostics.h>
#include <modbuscore/runtime/runtime.h>
#include <modbuscore/transport/posix_tcp.h>
#if defined(_WIN32)
#include <windows.h>
#endif
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void sleep_us(unsigned int usec)
{
#if defined(_WIN32)
    Sleep((DWORD)((usec + 999U) / 1000U));
#else
    struct timespec ts = {
        .tv_sec = usec / 1000000U,
        .tv_nsec = (long)(usec % 1000000U) * 1000L,
    };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        /* retry */
    }
#endif
}

static const char* severity_to_string(mbc_diag_severity_t severity)
{
    switch (severity) {
    case MBC_DIAG_SEVERITY_TRACE:
        return "TRACE";
    case MBC_DIAG_SEVERITY_DEBUG:
        return "DEBUG";
    case MBC_DIAG_SEVERITY_INFO:
        return "INFO";
    case MBC_DIAG_SEVERITY_WARNING:
        return "WARN";
    case MBC_DIAG_SEVERITY_ERROR:
        return "ERROR";
    case MBC_DIAG_SEVERITY_CRITICAL:
        return "CRITICAL";
    default:
        return "UNKNOWN";
    }
}

static void print_diag(void* ctx, const mbc_diag_event_t* event)
{
    (void)ctx;
    if (!event) {
        return;
    }

    fprintf(stdout, "[diag][%s][%s] %s (code=%u, ts=%llu)\n", severity_to_string(event->severity),
            event->component ? event->component : "n/a", event->message ? event->message : "",
            (unsigned)event->code, (unsigned long long)event->timestamp_ms);

    for (size_t i = 0; i < event->field_count; ++i) {
        const mbc_diag_kv_t* field = &event->fields[i];
        if (field->key && field->value) {
            fprintf(stdout, "    - %s: %s\n", field->key, field->value);
        }
    }
}

static void print_usage(const char* prog)
{
    fprintf(stderr, "Usage: %s [host] [port]\n", prog);
    fprintf(stderr,
            "Example: %s 127.0.0.1 1502\n\n"
            "The program attempts a Modbus TCP request and streams diagnostics telemetry.\n",
            prog);
}

static mbc_status_t submit_simple_read(mbc_engine_t* engine, uint16_t transaction_id)
{
    mbc_pdu_t request = {0};
    mbc_status_t status = mbc_pdu_build_read_holding_request(&request, 1U, 0U, 2U);
    if (!mbc_status_is_ok(status)) {
        return status;
    }

    uint8_t pdu_buffer[256];
    pdu_buffer[0] = request.function;
    memcpy(&pdu_buffer[1], request.payload, request.payload_length);
    const size_t pdu_length = 1U + request.payload_length;

    mbc_mbap_header_t header = {
        .transaction_id = transaction_id,
        .protocol_id = 0U,
        .length = (uint16_t)(pdu_length + 1U),
        .unit_id = request.unit_id,
    };

    uint8_t frame[256];
    size_t frame_length = 0U;
    status = mbc_mbap_encode(&header, pdu_buffer, pdu_length, frame, sizeof(frame), &frame_length);
    if (!mbc_status_is_ok(status)) {
        return status;
    }

    return mbc_engine_submit_request(engine, frame, frame_length);
}

int main(int argc, char** argv)
{
    const char* host = (argc > 1) ? argv[1] : "127.0.0.1";
    const uint16_t port = (uint16_t)((argc > 2) ? atoi(argv[2]) : 15020);

    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }

    fprintf(stdout, "=== ModbusCore TCP Diagnostics Demo ===\n");
    fprintf(stdout, "Target: %s:%u\n", host, port);

    mbc_posix_tcp_config_t tcp_config = {
        .host = host,
        .port = port,
        .connect_timeout_ms = 3000U,
        .recv_timeout_ms = 1000U,
    };

    mbc_transport_iface_t transport = {0};
    mbc_posix_tcp_ctx_t* tcp_ctx = NULL;
    mbc_status_t status = mbc_posix_tcp_create(&tcp_config, &transport, &tcp_ctx);
    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "Failed to connect to %s:%u (status=%d)\n", host, port, status);
        return EXIT_FAILURE;
    }

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_diag_sink_iface_t diag_sink = {
        .ctx = NULL,
        .emit = print_diag,
    };
    mbc_runtime_builder_with_diag(&builder, &diag_sink);

    mbc_runtime_t runtime;
    status = mbc_runtime_builder_build(&builder, &runtime);
    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "Failed to build runtime (status=%d)\n", status);
        mbc_posix_tcp_destroy(tcp_ctx);
        return EXIT_FAILURE;
    }

    mbc_engine_t engine;
    mbc_engine_config_t engine_config = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,
        .response_timeout_ms = 2000U,
        .event_cb = NULL,
        .event_ctx = NULL,
        .use_override = false,
    };

    status = mbc_engine_init(&engine, &engine_config);
    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "Failed to initialise engine (status=%d)\n", status);
        mbc_runtime_shutdown(&runtime);
        mbc_posix_tcp_destroy(tcp_ctx);
        return EXIT_FAILURE;
    }

    status = submit_simple_read(&engine, 1U);
    if (!mbc_status_is_ok(status)) {
        fprintf(stderr, "Submit failed (status=%d)\n", status);
    }

    fprintf(stdout, "Running engine loop (will timeout if server does not reply)...\n");
    for (int iteration = 0; iteration < 20; ++iteration) {
        status = mbc_engine_step(&engine, 256U);
        if (status == MBC_STATUS_TIMEOUT) {
            fprintf(stdout, "Engine reported timeout after waiting for response.\n");
            break;
        }
        if (!mbc_status_is_ok(status)) {
            fprintf(stdout, "Engine step error (status=%d)\n", status);
            break;
        }

        if (!mbc_posix_tcp_is_connected(tcp_ctx)) {
            fprintf(stdout, "Connection dropped while waiting for response.\n");
            break;
        }

        mbc_transport_yield(&transport);
        sleep_us(100000U); /* Allow some time for I/O */
    }

    if (mbc_posix_tcp_is_connected(tcp_ctx)) {
        fprintf(stdout, "Connection still active; shutting down gracefully.\n");
    }

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_posix_tcp_destroy(tcp_ctx);

    fprintf(stdout, "=== Diagnostics demo finished ===\n");
    return EXIT_SUCCESS;
}
