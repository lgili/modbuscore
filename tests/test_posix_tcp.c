#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/mbap.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/transport/posix_tcp.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define TCP_MAX_FRAME 260U

typedef struct {
    uint16_t port;
    uint8_t request[TCP_MAX_FRAME];
    size_t request_len;
    uint8_t response[TCP_MAX_FRAME];
    size_t response_len;
} tcp_server_args_t;

static void* tcp_server_thread(void* arg)
{
    tcp_server_args_t* args = arg;

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    assert(srv >= 0);

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
    setsockopt(srv, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port = htons(args->port),
    };
    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(srv);
        return NULL;
    }
    if (listen(srv, 1) != 0) {
        close(srv);
        return NULL;
    }

    int cli = accept(srv, NULL, NULL);
    if (cli < 0) {
        close(srv);
        return NULL;
    }

    size_t total = 0;
    while (total < args->request_len) {
        ssize_t n = recv(cli, (char*)args->request + total, (int)(args->request_len - total), 0);
        assert(n >= 0);
        if (n == 0) {
            break;
        }
        total += (size_t)n;
    }
    assert(total == args->request_len);

    size_t sent = 0;
    while (sent < args->response_len) {
        ssize_t n =
            send(cli, (const char*)args->response + sent, (int)(args->response_len - sent), 0);
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
    uint8_t request[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x11, 0x03, 0x00, 0x00, 0x00, 0x01};
    uint8_t response[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x05, 0x11, 0x03, 0x02, 0x00, 0x2A};

    tcp_server_args_t args = {
        .port = port,
        .request_len = sizeof(request),
        .response_len = sizeof(response),
    };
    memcpy(args.response, response, sizeof(response));

    pthread_t server;
    assert(pthread_create(&server, NULL, tcp_server_thread, &args) == 0);

    /* Give server time to bind and listen */
    usleep(100000); /* 100ms */

    mbc_posix_tcp_config_t config = {
        .host = "127.0.0.1",
        .port = port,
        .connect_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
    };

    mbc_transport_iface_t iface;
    mbc_posix_tcp_ctx_t* ctx = NULL;
    mbc_status_t status = mbc_posix_tcp_create(&config, &iface, &ctx);
    if (!mbc_status_is_ok(status)) {
        printf("POSIX TCP loop test skipped (connection failed, status=%d)\n", status);
        pthread_join(server, NULL);
        return;
    }

    mbc_transport_io_t io = {0};
    status = mbc_transport_send(&iface, request, sizeof(request), &io);
    assert(mbc_status_is_ok(status));
    assert(io.processed == sizeof(request));

    uint8_t rx[sizeof(response)] = {0};
    size_t total = 0;
    while (total < sizeof(response)) {
        io.processed = 0;
        status = mbc_transport_receive(&iface, rx + total, sizeof(response) - total, &io);
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

typedef struct {
    int fd;
} socket_transport_ctx_t;

static mbc_status_t make_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return MBC_STATUS_IO_ERROR;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return MBC_STATUS_IO_ERROR;
    }
    return MBC_STATUS_OK;
}

static mbc_status_t socket_transport_send(void* ctx, const uint8_t* buffer, size_t length,
                                          mbc_transport_io_t* out)
{
    socket_transport_ctx_t* sock = ctx;
    if (!sock || (!buffer && length > 0U)) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    size_t total = 0U;
    while (total < length) {
        ssize_t rc = send(sock->fd, buffer + total, length - total, 0);
        if (rc < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            return MBC_STATUS_IO_ERROR;
        }
        if (rc == 0) {
            break;
        }
        total += (size_t)rc;
    }

    if (out) {
        out->processed = total;
    }

    return (total == length) ? MBC_STATUS_OK : MBC_STATUS_IO_ERROR;
}

static mbc_status_t socket_transport_receive(void* ctx, uint8_t* buffer, size_t capacity,
                                             mbc_transport_io_t* out)
{
    socket_transport_ctx_t* sock = ctx;
    if (!sock || !buffer || capacity == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    ssize_t rc = recv(sock->fd, buffer, capacity, 0);
    if (rc < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            if (out) {
                out->processed = 0U;
            }
            return MBC_STATUS_OK;
        }
        return MBC_STATUS_IO_ERROR;
    }

    if (out) {
        out->processed = (size_t)rc;
    }
    return MBC_STATUS_OK;
}

static uint64_t socket_transport_now(void* ctx)
{
    (void)ctx;
    struct timespec ts;
#ifdef CLOCK_MONOTONIC
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
    }
#endif
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000ULL);
}

static void socket_transport_yield(void* ctx)
{
    (void)ctx;
    usleep(1000);
}

static mbc_status_t build_fc03_request_frame(mbc_pdu_t* pdu, uint8_t* frame, size_t capacity,
                                             size_t* out_length)
{
    if (!pdu || !frame || !out_length) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    mbc_status_t status = mbc_pdu_build_read_holding_request(pdu, 0x11U, 0x0000U, 0x0001U);
    if (!mbc_status_is_ok(status)) {
        return status;
    }

    uint8_t pdu_bytes[1U + MBC_PDU_MAX];
    pdu_bytes[0] = pdu->function;
    memcpy(&pdu_bytes[1], pdu->payload, pdu->payload_length);

    mbc_mbap_header_t header = {
        .transaction_id = 0x0001U,
        .protocol_id = 0x0000U,
        .length = 0U,
        .unit_id = pdu->unit_id,
    };

    size_t pdu_length = 1U + pdu->payload_length;
    return mbc_mbap_encode(&header, pdu_bytes, pdu_length, frame, capacity, out_length);
}

static void test_tcp_engine_client(void)
{
    const uint16_t port = 15021;
    const uint8_t response_frame[] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x05,
                                      0x11, 0x03, 0x02, 0x00, 0x2A};

    uint8_t request_frame[TCP_MAX_FRAME] = {0};
    size_t request_length = 0U;
    mbc_pdu_t request_pdu;
    assert(mbc_status_is_ok(build_fc03_request_frame(&request_pdu, request_frame,
                                                     sizeof(request_frame), &request_length)));

    tcp_server_args_t args = {
        .port = port,
        .request_len = request_length,
        .response_len = sizeof(response_frame),
    };
    memcpy(args.response, response_frame, sizeof(response_frame));

    pthread_t server;
    assert(pthread_create(&server, NULL, tcp_server_thread, &args) == 0);

    /* Give server time to bind and listen */
    usleep(100000); /* 100ms */

    mbc_posix_tcp_config_t config = {
        .host = "127.0.0.1",
        .port = port,
        .connect_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
    };

    mbc_transport_iface_t iface;
    mbc_posix_tcp_ctx_t* ctx = NULL;
    mbc_status_t status = mbc_posix_tcp_create(&config, &iface, &ctx);
    if (!mbc_status_is_ok(status)) {
        printf("POSIX TCP engine client test skipped (connection failed, status=%d)\n", status);
        pthread_join(server, NULL);
        return;
    }

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);

    mbc_runtime_t runtime;
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);

    mbc_engine_t engine;
    mbc_engine_config_t engine_cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,
        .use_override = false,
        .response_timeout_ms = 1000,
    };
    assert(mbc_engine_init(&engine, &engine_cfg) == MBC_STATUS_OK);

    assert(request_length <= TCP_MAX_FRAME);
    assert(mbc_engine_submit_request(&engine, request_frame, request_length) == MBC_STATUS_OK);

    mbc_pdu_t response_pdu = {0};
    bool response_ready = false;
    for (int i = 0; i < 100 && !response_ready; ++i) {
        mbc_status_t status = mbc_engine_step(&engine, 32U);
        assert(status == MBC_STATUS_OK || status == MBC_STATUS_TIMEOUT);
        if (status == MBC_STATUS_TIMEOUT) {
            break;
        }
        if (mbc_engine_take_pdu(&engine, &response_pdu)) {
            response_ready = true;
            break;
        }
        mbc_transport_yield(&iface);
        usleep(1000);
    }

    assert(response_ready);

    const uint8_t* register_data = NULL;
    size_t register_count = 0U;
    assert(mbc_pdu_parse_read_holding_response(&response_pdu, &register_data, &register_count) ==
           MBC_STATUS_OK);
    assert(register_count == 1U);
    assert(register_data[0] == 0x00U && register_data[1] == 0x2AU);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_posix_tcp_destroy(ctx);

    pthread_join(server, NULL);
    assert(args.request_len == request_length);
    assert(memcmp(args.request, request_frame, request_length) == 0);
}

static void test_tcp_engine_server(void)
{
    int fds[2] = {-1, -1};
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    assert(mbc_status_is_ok(make_nonblocking(fds[0])));
    assert(mbc_status_is_ok(make_nonblocking(fds[1])));

    socket_transport_ctx_t transport_ctx = {
        .fd = fds[0],
    };

    mbc_transport_iface_t transport = {
        .ctx = &transport_ctx,
        .send = socket_transport_send,
        .receive = socket_transport_receive,
        .now = socket_transport_now,
        .yield = socket_transport_yield,
    };

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_runtime_t runtime;
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);

    mbc_engine_t engine;
    mbc_engine_config_t cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_SERVER,
        .framing = MBC_FRAMING_TCP,
        .use_override = false,
    };
    assert(mbc_engine_init(&engine, &cfg) == MBC_STATUS_OK);

    uint8_t request_frame[TCP_MAX_FRAME];
    size_t request_length = 0U;
    mbc_pdu_t request_pdu;
    assert(mbc_status_is_ok(build_fc03_request_frame(&request_pdu, request_frame,
                                                     sizeof(request_frame), &request_length)));

    const uint16_t transaction_id = (uint16_t)((request_frame[0] << 8) | request_frame[1]);

    ssize_t sent = send(fds[1], request_frame, request_length, 0);
    assert(sent == (ssize_t)request_length);

    mbc_pdu_t decoded_request = {0};
    bool has_request = false;
    for (int i = 0; i < 32 && !has_request; ++i) {
        assert(mbc_engine_step(&engine, 64U) == MBC_STATUS_OK);
        if (mbc_engine_take_pdu(&engine, &decoded_request)) {
            has_request = true;
        }
    }

    assert(has_request);
    assert(decoded_request.function == 0x03U);

    mbc_pdu_t response_pdu = {
        .unit_id = decoded_request.unit_id,
        .function = decoded_request.function,
        .payload_length = 3U,
    };
    response_pdu.payload[0] = 0x02U;
    response_pdu.payload[1] = 0x00U;
    response_pdu.payload[2] = 0x2AU;

    uint8_t response_pdu_bytes[1U + MBC_PDU_MAX];
    response_pdu_bytes[0] = response_pdu.function;
    memcpy(&response_pdu_bytes[1], response_pdu.payload, response_pdu.payload_length);

    mbc_mbap_header_t header = {
        .transaction_id = transaction_id,
        .protocol_id = 0x0000U,
        .length = 0U,
        .unit_id = response_pdu.unit_id,
    };

    uint8_t response_frame[TCP_MAX_FRAME];
    size_t response_length = 0U;
    assert(mbc_mbap_encode(&header, response_pdu_bytes, 1U + response_pdu.payload_length,
                           response_frame, sizeof(response_frame),
                           &response_length) == MBC_STATUS_OK);

    assert(mbc_engine_submit_request(&engine, response_frame, response_length) == MBC_STATUS_OK);

    uint8_t received[TCP_MAX_FRAME] = {0};
    size_t total = 0U;
    while (total < response_length) {
        ssize_t rc = recv(fds[1], received + total, response_length - total, 0);
        if (rc > 0) {
            total += (size_t)rc;
            continue;
        }
        if (rc == 0) {
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            usleep(1000);
            continue;
        }
        assert(false && "recv failed");
    }
    assert(total == response_length);
    assert(memcmp(received, response_frame, response_length) == 0);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    close(fds[0]);
    close(fds[1]);
}

static void test_tcp_engine_client_timeout(void)
{
    int fds[2] = {-1, -1};
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    assert(mbc_status_is_ok(make_nonblocking(fds[0])));
    assert(mbc_status_is_ok(make_nonblocking(fds[1])));

    socket_transport_ctx_t transport_ctx = {
        .fd = fds[0],
    };

    mbc_transport_iface_t transport = {
        .ctx = &transport_ctx,
        .send = socket_transport_send,
        .receive = socket_transport_receive,
        .now = socket_transport_now,
        .yield = socket_transport_yield,
    };

    uint8_t request_frame[TCP_MAX_FRAME] = {0};
    size_t request_length = 0U;
    mbc_pdu_t request_pdu;
    assert(mbc_status_is_ok(build_fc03_request_frame(&request_pdu, request_frame,
                                                     sizeof(request_frame), &request_length)));

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_runtime_t runtime;
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);

    mbc_engine_t engine;
    mbc_engine_config_t cfg = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .framing = MBC_FRAMING_TCP,
        .use_override = false,
        .response_timeout_ms = 20,
    };
    assert(mbc_engine_init(&engine, &cfg) == MBC_STATUS_OK);

    assert(mbc_engine_submit_request(&engine, request_frame, request_length) == MBC_STATUS_OK);

    /* Drain request from peer side */
    uint8_t peer_buffer[TCP_MAX_FRAME] = {0};
    size_t total = 0U;
    while (total < request_length) {
        ssize_t rc = recv(fds[1], peer_buffer + total, request_length - total, 0);
        if (rc > 0) {
            total += (size_t)rc;
            continue;
        }
        if (rc == 0) {
            break;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            usleep(1000);
            continue;
        }
        assert(false && "recv failed while draining client request");
    }
    assert(total == request_length);
    assert(memcmp(peer_buffer, request_frame, request_length) == 0);

    bool timed_out = false;
    for (int i = 0; i < 200 && !timed_out; ++i) {
        mbc_status_t status = mbc_engine_step(&engine, 32U);
        if (status == MBC_STATUS_TIMEOUT) {
            timed_out = true;
            break;
        }
        assert(status == MBC_STATUS_OK);
        mbc_transport_yield(&transport);
        usleep(1000);
    }
    assert(timed_out);
    assert(engine.state == MBC_ENGINE_STATE_IDLE);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    close(fds[0]);
    close(fds[1]);
}

static void test_tcp_invalid_inputs(void)
{
    mbc_transport_iface_t iface;
    mbc_posix_tcp_ctx_t* ctx = NULL;

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
    test_tcp_engine_client();
    test_tcp_engine_server();
    test_tcp_engine_client_timeout();
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
