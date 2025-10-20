#if !defined(_WIN32)
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif
#endif

#include <assert.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/mbap.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/runtime/builder.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET tcp_socket_t;
#define CLOSESOCK closesocket
static uint64_t now_ms(void) { return GetTickCount64(); }
static void sleep_ms(uint32_t ms) { Sleep(ms); }
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
typedef int tcp_socket_t;
#define INVALID_SOCKET (-1)
#define CLOSESOCK close
static uint64_t now_ms(void)
{
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
    }
#endif
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000ULL);
}
static void sleep_ms(uint32_t ms)
{
    struct timespec ts = {
        .tv_sec = ms / 1000U,
        .tv_nsec = (long)(ms % 1000U) * 1000000L,
    };
    nanosleep(&ts, NULL);
}
#endif

#define DEFAULT_PORT 15020
#define HOLDING_REG_COUNT 64

typedef struct {
    tcp_socket_t fd;
} tcp_transport_ctx_t;

static int set_nonblocking(tcp_socket_t fd)
{
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

static mbc_status_t transport_send(void* ctx, const uint8_t* buffer, size_t length,
                                   mbc_transport_io_t* out)
{
    tcp_transport_ctx_t* tcp = ctx;
    if (!tcp || (!buffer && length > 0U)) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    size_t sent_total = 0U;
    while (sent_total < length) {
#ifdef _WIN32
        int rc = send(tcp->fd, (const char*)buffer + sent_total, (int)(length - sent_total), 0);
#else
        ssize_t rc = send(tcp->fd, buffer + sent_total, length - sent_total, 0);
#endif
        if (rc < 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
                sleep_ms(1);
                continue;
            }
#else
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                sleep_ms(1);
                continue;
            }
#endif
            return MBC_STATUS_IO_ERROR;
        }
        if (rc == 0) {
            break;
        }
        sent_total += (size_t)rc;
    }

    if (out) {
        out->processed = sent_total;
    }
    return (sent_total == length) ? MBC_STATUS_OK : MBC_STATUS_IO_ERROR;
}

static mbc_status_t transport_receive(void* ctx, uint8_t* buffer, size_t capacity,
                                      mbc_transport_io_t* out)
{
    tcp_transport_ctx_t* tcp = ctx;
    if (!tcp || !buffer || capacity == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

#ifdef _WIN32
    int rc = recv(tcp->fd, (char*)buffer, (int)capacity, 0);
    if (rc < 0) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            if (out) {
                out->processed = 0U;
            }
            return MBC_STATUS_OK;
        }
        return MBC_STATUS_IO_ERROR;
    }
#else
    ssize_t rc = recv(tcp->fd, buffer, capacity, 0);
    if (rc < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            if (out) {
                out->processed = 0U;
            }
            return MBC_STATUS_OK;
        }
        return MBC_STATUS_IO_ERROR;
    }
#endif

    if (out) {
        out->processed = (size_t)rc;
    }
    return (rc == 0) ? MBC_STATUS_IO_ERROR : MBC_STATUS_OK;
}

static uint64_t transport_now(void* ctx)
{
    (void)ctx;
    return now_ms();
}

static void transport_yield(void* ctx)
{
    (void)ctx;
    sleep_ms(1);
}

static bool encode_exception(uint8_t unit, uint8_t function, uint8_t code, mbc_pdu_t* out)
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

static bool handle_request(const mbc_pdu_t* request, mbc_pdu_t* response, uint16_t* registers,
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

static void usage(const char* prog)
{
    printf("Usage: %s [--port <tcp-port>] [--unit <id>] [--max-requests <n>]\n", prog);
    printf("Default port: %d, unit id: 0x11\n", DEFAULT_PORT);
    printf("Start this server, then run the TCP client example to interact.\n");
}

int main(int argc, char** argv)
{
    uint16_t port = DEFAULT_PORT;
    uint8_t unit_id = 0x11U;
    size_t max_requests = 0U;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = (uint16_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--unit") == 0 && i + 1 < argc) {
            unit_id = (uint8_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--max-requests") == 0 && i + 1 < argc) {
            max_requests = (size_t)atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    tcp_socket_t listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == INVALID_SOCKET) {
        fprintf(stderr, "Failed to create socket\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "Bind failed on port %u\n", port);
        CLOSESOCK(listen_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (listen(listen_fd, 1) != 0) {
        fprintf(stderr, "Listen failed\n");
        CLOSESOCK(listen_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    printf("Modbus TCP server listening on port %u (unit 0x%02X)\n", port, unit_id);
    printf("Waiting for client...\n");

    tcp_socket_t client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd == INVALID_SOCKET) {
        fprintf(stderr, "Accept failed\n");
        CLOSESOCK(listen_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    CLOSESOCK(listen_fd);

    if (set_nonblocking(client_fd) != 0) {
        fprintf(stderr, "Failed to configure non-blocking socket\n");
        CLOSESOCK(client_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    tcp_transport_ctx_t transport_ctx = {
        .fd = client_fd,
    };

    mbc_transport_iface_t transport = {
        .ctx = &transport_ctx,
        .send = transport_send,
        .receive = transport_receive,
        .now = transport_now,
        .yield = transport_yield,
    };

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_runtime_t runtime;
    if (mbc_runtime_builder_build(&builder, &runtime) != MBC_STATUS_OK) {
        fprintf(stderr, "Failed to build runtime\n");
        CLOSESOCK(client_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    mbc_engine_t engine;
    mbc_engine_config_t engine_cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_SERVER,
        .framing = MBC_FRAMING_TCP,
        .use_override = false,
    };
    if (mbc_engine_init(&engine, &engine_cfg) != MBC_STATUS_OK) {
        fprintf(stderr, "Failed to initialise engine\n");
        mbc_runtime_shutdown(&runtime);
        CLOSESOCK(client_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    uint16_t holding_registers[HOLDING_REG_COUNT];
    for (size_t i = 0; i < HOLDING_REG_COUNT; ++i) {
        holding_registers[i] = (uint16_t)i;
    }

    size_t served = 0U;
    printf("Client connected. Waiting for requests...\n");

    while (max_requests == 0U || served < max_requests) {
        mbc_status_t status = mbc_engine_step(&engine, 256U);
        if (status == MBC_STATUS_IO_ERROR) {
            fprintf(stderr, "Connection closed or failed (IO error)\n");
            break;
        }

        mbc_pdu_t request = {0};
        if (!mbc_engine_take_pdu(&engine, &request)) {
            transport_yield(&transport_ctx);
            continue;
        }

        if (request.unit_id != unit_id) {
            printf("Ignoring request for unit 0x%02X\n", request.unit_id);
            continue;
        }

        printf("Received function 0x%02X\n", request.function);

        mbc_pdu_t response = {0};
        bool ok = handle_request(&request, &response, holding_registers, HOLDING_REG_COUNT);

        if (!ok) {
            fprintf(stderr, "Failed to build response\n");
            break;
        }

        uint8_t response_frame[260];
        size_t response_len = 0U;
        uint8_t response_payload[1 + MBC_PDU_MAX];
        response_payload[0] = response.function;
        memcpy(&response_payload[1], response.payload, response.payload_length);

        mbc_mbap_header_t header = {
            .transaction_id = 0,
            .protocol_id = 0,
            .length = 0,
            .unit_id = response.unit_id,
        };
        mbc_mbap_header_t last_header;
        if (mbc_engine_last_mbap_header(&engine, &last_header)) {
            header.transaction_id = last_header.transaction_id;
            header.protocol_id = last_header.protocol_id;
            header.unit_id = last_header.unit_id;
        }

        if (mbc_mbap_encode(&header, response_payload, 1U + response.payload_length, response_frame,
                            sizeof(response_frame), &response_len) != MBC_STATUS_OK) {
            fprintf(stderr, "Failed to encode MBAP frame\n");
            break;
        }

        if (!mbc_status_is_ok(mbc_engine_submit_request(&engine, response_frame, response_len))) {
            fprintf(stderr, "Failed to send response\n");
            break;
        }

        if (response.function == 0x03U) {
            printf("Responded with %u registers\n", response.payload[0] / 2U);
        } else if (response.function == 0x06U || response.function == 0x10U) {
            printf("Registers updated successfully\n");
        } else if ((response.function & 0x80U) != 0U) {
            printf("Sent exception code 0x%02X\n", response.payload[0]);
        }

        served++;
    }

    printf("Server shutting down (served %zu request(s))\n", served);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    CLOSESOCK(client_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
