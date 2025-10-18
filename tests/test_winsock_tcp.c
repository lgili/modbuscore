#include <assert.h>
#include <string.h>

#include <modbuscore/transport/winsock_tcp.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>

typedef struct {
    uint16_t port;
    uint8_t request[12];
    size_t request_len;
    uint8_t response[12];
    size_t response_len;
} server_args_t;

static unsigned __stdcall server_thread(void *arg)
{
    server_args_t *args = arg;

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(srv != INVALID_SOCKET);

    BOOL opt = TRUE;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(args->port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    assert(bind(srv, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    assert(listen(srv, 1) == 0);

    SOCKET cli = accept(srv, NULL, NULL);
    assert(cli != INVALID_SOCKET);

    size_t total = 0;
    while (total < args->request_len) {
        int n = recv(cli, (char *)args->request + total,
                     (int)(args->request_len - total), 0);
        assert(n >= 0);
        if (n == 0) {
            break;
        }
        total += (size_t)n;
    }
    assert(total == args->request_len);

    size_t sent = 0;
    while (sent < args->response_len) {
        int n = send(cli, (const char *)args->response + sent,
                     (int)(args->response_len - sent), 0);
        assert(n >= 0);
        if (n == 0) {
            break;
        }
        sent += (size_t)n;
    }
    assert(sent == args->response_len);

    closesocket(cli);
    closesocket(srv);
    WSACleanup();
    return 0;
}

static void test_winsock_loop(void)
{
    const uint16_t port = 15030;
    uint8_t request[] = {0x00,0x02,0x00,0x00,0x00,0x06,0x11,0x03,0x00,0x00,0x00,0x02};
    uint8_t response[] = {0x00,0x02,0x00,0x00,0x00,0x05,0x11,0x03,0x02,0x00,0x2B};

    server_args_t args = {
        .port = port,
        .request_len = sizeof(request),
        .response_len = sizeof(response),
    };
    memcpy(args.response, response, sizeof(response));

    HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, server_thread, &args, 0, NULL);
    assert(thread != NULL);
    Sleep(50); /* server ready */

    mbc_winsock_tcp_config_t config = {
        .host = "127.0.0.1",
        .port = port,
        .connect_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
    };

    mbc_transport_iface_t iface;
    mbc_winsock_tcp_ctx_t *ctx = NULL;
    mbc_status_t status = mbc_winsock_tcp_create(&config, &iface, &ctx);
    assert(mbc_status_is_ok(status));

    mbc_transport_io_t io = {0};
    status = mbc_transport_send(&iface, request, sizeof(request), &io);
    assert(mbc_status_is_ok(status));
    assert(io.processed == sizeof(request));

    uint8_t rx[sizeof(response)] = {0};
    size_t total = 0;
    while (total < sizeof(response)) {
        io.processed = 0;
        status = mbc_transport_receive(&iface, rx + total,
                                       sizeof(response) - total, &io);
        assert(mbc_status_is_ok(status));
        if (io.processed == 0U) {
            iface.yield(iface.ctx);
            continue;
        }
        total += io.processed;
    }
    assert(memcmp(rx, response, sizeof(response)) == 0);

    mbc_winsock_tcp_destroy(ctx);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
}

int main(void)
{
    test_winsock_loop();
    return 0;
}
#else
int main(void)
{
    mbc_transport_iface_t iface;
    mbc_winsock_tcp_ctx_t *ctx = NULL;
    mbc_winsock_tcp_config_t cfg = {
        .host = "127.0.0.1",
        .port = 502,
    };
    mbc_status_t status = mbc_winsock_tcp_create(&cfg, &iface, &ctx);
    assert(status == MBC_STATUS_UNSUPPORTED);
    assert(ctx == NULL);
    return 0;
}
#endif
