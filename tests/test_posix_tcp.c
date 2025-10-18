#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef __unix__
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <modbuscore/transport/posix_tcp.h>

typedef struct {
    uint16_t port;
    uint8_t request[12];
    size_t request_len;
    uint8_t response[12];
    size_t response_len;
} tcp_server_args_t;

static void *tcp_server_thread(void *arg)
{
    tcp_server_args_t *args = arg;

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    assert(srv >= 0);

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port = htons(args->port),
    };
    assert(bind(srv, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    assert(listen(srv, 1) == 0);

    int cli = accept(srv, NULL, NULL);
    assert(cli >= 0);

    size_t total = 0;
    while (total < args->request_len) {
        ssize_t n = recv(cli, (char *)args->request + total,
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
        ssize_t n = send(cli, (const char *)args->response + sent,
                         (int)(args->response_len - sent), 0);
        assert(n >= 0);
        if (n == 0) {
            break;
        }
        sent += (size_t)n;
    }
    assert(sent == args->response_len);

    close(cli);
    close(srv);
    return NULL;
}

static void test_tcp_loop(void)
{
    const uint16_t port = 15020;
    uint8_t request[] = {0x00,0x01,0x00,0x00,0x00,0x06,0x11,0x03,0x00,0x00,0x00,0x01};
    uint8_t response[] = {0x00,0x01,0x00,0x00,0x00,0x05,0x11,0x03,0x02,0x00,0x2A};

    tcp_server_args_t args = {
        .port = port,
        .request_len = sizeof(request),
        .response_len = sizeof(response),
    };
    memcpy(args.response, response, sizeof(response));

    pthread_t server;
    assert(pthread_create(&server, NULL, tcp_server_thread, &args) == 0);

    mbc_posix_tcp_config_t config = {
        .host = "127.0.0.1",
        .port = port,
        .connect_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
    };

    mbc_transport_iface_t iface;
    mbc_posix_tcp_ctx_t *ctx = NULL;
    mbc_status_t status = mbc_posix_tcp_create(&config, &iface, &ctx);
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

    mbc_posix_tcp_destroy(ctx);
    pthread_join(server, NULL);
}

static void test_tcp_invalid_inputs(void)
{
    mbc_transport_iface_t iface;
    mbc_posix_tcp_ctx_t *ctx = NULL;

    assert(mbc_posix_tcp_create(NULL, &iface, &ctx) == MBC_STATUS_INVALID_ARGUMENT);

    mbc_posix_tcp_config_t bad_config = {
        .host = NULL,
        .port = 502,
        .connect_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
    };
    assert(mbc_posix_tcp_create(&bad_config, &iface, &ctx) == MBC_STATUS_INVALID_ARGUMENT);

    bad_config.host = "127.0.0.1";
    bad_config.port = 0;
    assert(mbc_posix_tcp_create(&bad_config, &iface, &ctx) == MBC_STATUS_INVALID_ARGUMENT);

    bad_config.port = 502;
    assert(mbc_posix_tcp_create(&bad_config, NULL, &ctx) == MBC_STATUS_INVALID_ARGUMENT);
    assert(mbc_posix_tcp_create(&bad_config, &iface, NULL) == MBC_STATUS_INVALID_ARGUMENT);

    mbc_posix_tcp_destroy(NULL);
    assert(!mbc_posix_tcp_is_connected(NULL));
}

int main(void)
{
    printf("=== POSIX TCP Loopback Tests ===\n\n");
    test_tcp_loop();
    test_tcp_invalid_inputs();
    printf("\n=== All TCP tests completed ===\n");
    return 0;
}

#else
int main(void)
{
    printf("POSIX TCP tests skipped (non-POSIX platform).\n");
    return 0;
}
#endif
