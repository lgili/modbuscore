/**
 * @file winsock_tcp.c
 * @brief Implementation of Winsock TCP transport driver for Windows.
 */

#include <modbuscore/transport/winsock_tcp.h>

#ifdef _WIN32

#include <stdio.h> /* For _snprintf */
#include <stdlib.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

struct mbc_winsock_tcp_ctx {
    SOCKET socket_handle;
    bool connected;
    uint32_t recv_timeout_ms;
};

static mbc_status_t winsock_startup(void)
{
    static LONG g_init_count = 0;
    static WSADATA g_wsa_data;

    if (InterlockedIncrement(&g_init_count) == 1) {
        int rc = WSAStartup(MAKEWORD(2, 2), &g_wsa_data);
        if (rc != 0) {
            InterlockedDecrement(&g_init_count);
            return MBC_STATUS_IO_ERROR;
        }
    }
    return MBC_STATUS_OK;
}

static void winsock_cleanup(void)
{
    static LONG g_init_count = 0;
    if (InterlockedDecrement(&g_init_count) == 0) {
        WSACleanup();
    }
}

static mbc_status_t configure_socket(SOCKET sock)
{
    BOOL flag = TRUE;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (const char*)&flag, sizeof(flag));

    u_long mode = 1;
    if (ioctlsocket(sock, FIONBIO, &mode) != NO_ERROR) {
        return MBC_STATUS_IO_ERROR;
    }
    return MBC_STATUS_OK;
}

static mbc_status_t resolve_host(const char* host, uint16_t port, struct addrinfo** out)
{
    char port_buffer[6];
    _snprintf(port_buffer, sizeof(port_buffer), "%u", (unsigned)port);

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
    };

    struct addrinfo* result = NULL;
    int rc = getaddrinfo(host, port_buffer, &hints, &result);
    if (rc != 0 || !result) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    *out = result;
    return MBC_STATUS_OK;
}

static mbc_status_t wait_for_connect(SOCKET sock, uint32_t timeout_ms)
{
    fd_set write_set;
    FD_ZERO(&write_set);
    FD_SET(sock, &write_set);

    struct timeval tv = {
        .tv_sec = timeout_ms / 1000U,
        .tv_usec = (timeout_ms % 1000U) * 1000U,
    };

    int rc = select(0, NULL, &write_set, NULL, (timeout_ms > 0U) ? &tv : NULL);
    if (rc <= 0) {
        return (rc == 0) ? MBC_STATUS_TIMEOUT : MBC_STATUS_IO_ERROR;
    }

    int opt = 0;
    int opt_len = sizeof(opt);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&opt, &opt_len) != 0) {
        return MBC_STATUS_IO_ERROR;
    }

    return (opt == 0) ? MBC_STATUS_OK : MBC_STATUS_IO_ERROR;
}

static mbc_status_t winsock_send(void* ctx, const uint8_t* buffer, size_t length,
                                 mbc_transport_io_t* out)
{
    mbc_winsock_tcp_ctx_t* tcp = ctx;
    if (!tcp || (!buffer && length > 0U)) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }
    if (!tcp->connected) {
        return MBC_STATUS_IO_ERROR;
    }

    int sent = send(tcp->socket_handle, (const char*)buffer, (int)length, 0);
    if (sent == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            if (out) {
                out->processed = 0U;
            }
            return MBC_STATUS_OK;
        }
        tcp->connected = false;
        return MBC_STATUS_IO_ERROR;
    }

    if (out) {
        out->processed = (size_t)sent;
    }
    return MBC_STATUS_OK;
}

static mbc_status_t winsock_receive(void* ctx, uint8_t* buffer, size_t capacity,
                                    mbc_transport_io_t* out)
{
    mbc_winsock_tcp_ctx_t* tcp = ctx;
    if (!tcp || !buffer || capacity == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }
    if (!tcp->connected) {
        return MBC_STATUS_IO_ERROR;
    }

    int received = recv(tcp->socket_handle, (char*)buffer, (int)capacity, 0);
    if (received == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            if (out) {
                out->processed = 0U;
            }
            return MBC_STATUS_OK;
        }
        tcp->connected = false;
        return MBC_STATUS_IO_ERROR;
    }

    if (received == 0) {
        tcp->connected = false;
        return MBC_STATUS_IO_ERROR;
    }

    if (out) {
        out->processed = (size_t)received;
    }
    return MBC_STATUS_OK;
}

static uint64_t winsock_now(void* ctx)
{
    (void)ctx;
    LARGE_INTEGER freq, counter;
    if (!QueryPerformanceFrequency(&freq) || !QueryPerformanceCounter(&counter)) {
        return 0ULL;
    }
    return (uint64_t)(counter.QuadPart * 1000ULL / freq.QuadPart);
}

static void winsock_yield(void* ctx)
{
    (void)ctx;
    Sleep(1);
}

mbc_status_t mbc_winsock_tcp_create(const mbc_winsock_tcp_config_t* config,
                                    mbc_transport_iface_t* out_iface,
                                    mbc_winsock_tcp_ctx_t** out_ctx)
{
    if (!config || !config->host || config->port == 0U || !out_iface || !out_ctx) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    mbc_status_t status = winsock_startup();
    if (!mbc_status_is_ok(status)) {
        return status;
    }

    struct addrinfo* addr_list = NULL;
    status = resolve_host(config->host, config->port, &addr_list);
    if (!mbc_status_is_ok(status)) {
        winsock_cleanup();
        return status;
    }

    SOCKET sock = INVALID_SOCKET;
    struct addrinfo* entry = addr_list;
    mbc_status_t last_status = MBC_STATUS_IO_ERROR;

    for (; entry; entry = entry->ai_next) {
        sock = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
        if (sock == INVALID_SOCKET) {
            last_status = MBC_STATUS_IO_ERROR;
            continue;
        }

        if (!mbc_status_is_ok(configure_socket(sock))) {
            closesocket(sock);
            sock = INVALID_SOCKET;
            continue;
        }

        int rc = connect(sock, entry->ai_addr, (int)entry->ai_addrlen);
        if (rc == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
                closesocket(sock);
                sock = INVALID_SOCKET;
                continue;
            }

            status = wait_for_connect(sock, config->connect_timeout_ms);
            if (!mbc_status_is_ok(status)) {
                closesocket(sock);
                sock = INVALID_SOCKET;
                last_status = status;
                continue;
            }
        }

        last_status = MBC_STATUS_OK;
        break;
    }

    freeaddrinfo(addr_list);

    if (!mbc_status_is_ok(last_status) || sock == INVALID_SOCKET) {
        winsock_cleanup();
        return last_status;
    }

    mbc_winsock_tcp_ctx_t* tcp = calloc(1, sizeof(*tcp));
    if (!tcp) {
        closesocket(sock);
        winsock_cleanup();
        return MBC_STATUS_NO_RESOURCES;
    }

    tcp->socket_handle = sock;
    tcp->connected = true;
    tcp->recv_timeout_ms = config->recv_timeout_ms;

    *out_iface = (mbc_transport_iface_t){
        .ctx = tcp,
        .send = winsock_send,
        .receive = winsock_receive,
        .now = winsock_now,
        .yield = winsock_yield,
    };
    *out_ctx = tcp;

    return MBC_STATUS_OK;
}

void mbc_winsock_tcp_destroy(mbc_winsock_tcp_ctx_t* ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->socket_handle != INVALID_SOCKET) {
        closesocket(ctx->socket_handle);
        ctx->socket_handle = INVALID_SOCKET;
    }
    ctx->connected = false;
    free(ctx);
    winsock_cleanup();
}

bool mbc_winsock_tcp_is_connected(const mbc_winsock_tcp_ctx_t* ctx)
{
    return ctx && ctx->connected;
}

#else /* !_WIN32 */

mbc_status_t mbc_winsock_tcp_create(const mbc_winsock_tcp_config_t* config,
                                    mbc_transport_iface_t* out_iface,
                                    mbc_winsock_tcp_ctx_t** out_ctx)
{
    (void)config;
    (void)out_iface;
    (void)out_ctx;
    return MBC_STATUS_UNSUPPORTED;
}

void mbc_winsock_tcp_destroy(mbc_winsock_tcp_ctx_t* ctx) { (void)ctx; }

bool mbc_winsock_tcp_is_connected(const mbc_winsock_tcp_ctx_t* ctx)
{
    (void)ctx;
    return false;
}

#endif /* _WIN32 */
