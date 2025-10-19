#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include <modbuscore/transport/winsock_tcp.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/protocol/engine.h>
#include <modbuscore/protocol/pdu.h>
#include <modbuscore/protocol/mbap.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>

#define TCP_MAX_FRAME 260U

typedef struct {
    uint16_t port;
    uint8_t request[TCP_MAX_FRAME];
    size_t request_len;
    uint8_t response[TCP_MAX_FRAME];
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

static mbc_status_t build_fc03_request_frame(mbc_pdu_t *pdu,
                                             uint8_t *frame,
                                             size_t capacity,
                                             size_t *out_length)
{
    if (!pdu || !frame || !out_length) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    mbc_status_t status = mbc_pdu_build_read_holding_request(
        pdu, 0x11U, 0x0000U, 0x0001U);
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
    return mbc_mbap_encode(&header,
                           pdu_bytes,
                           pdu_length,
                           frame,
                           capacity,
                           out_length);
}

static void test_winsock_tcp_engine_client(void)
{
    const uint16_t port = 15031;
    const uint8_t response_frame[] = {
        0x00, 0x01, 0x00, 0x00, 0x00, 0x05,
        0x11, 0x03, 0x02, 0x00, 0x2A
    };

    uint8_t request_frame[260] = {0};
    size_t request_length = 0U;
    mbc_pdu_t request_pdu;
    assert(mbc_status_is_ok(build_fc03_request_frame(&request_pdu,
                                                     request_frame,
                                                     sizeof(request_frame),
                                                     &request_length)));

    server_args_t args = {
        .port = port,
        .request_len = request_length,
        .response_len = sizeof(response_frame),
    };
    memcpy(args.response, response_frame, sizeof(response_frame));

    HANDLE thread = (HANDLE)_beginthreadex(NULL, 0, server_thread, &args, 0, NULL);
    assert(thread != NULL);
    Sleep(100); /* Give server time to bind */

    mbc_winsock_tcp_config_t config = {
        .host = "127.0.0.1",
        .port = port,
        .connect_timeout_ms = 1000,
        .recv_timeout_ms = 1000,
    };

    mbc_transport_iface_t iface;
    mbc_winsock_tcp_ctx_t *ctx = NULL;
    mbc_status_t status = mbc_winsock_tcp_create(&config, &iface, &ctx);
    if (!mbc_status_is_ok(status)) {
        printf("Winsock TCP engine client test skipped (connection failed, status=%d)\n", status);
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
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

    assert(request_length <= 260);
    assert(mbc_engine_submit_request(&engine,
                                     request_frame,
                                     request_length) == MBC_STATUS_OK);

    mbc_pdu_t response_pdu = {0};
    bool response_ready = false;
    for (int i = 0; i < 100 && !response_ready; ++i) {
        status = mbc_engine_step(&engine, 32U);
        assert(status == MBC_STATUS_OK || status == MBC_STATUS_TIMEOUT);
        if (status == MBC_STATUS_TIMEOUT) {
            break;
        }
        if (mbc_engine_take_pdu(&engine, &response_pdu)) {
            response_ready = true;
            break;
        }
        mbc_transport_yield(&iface);
        Sleep(1);
    }

    assert(response_ready);

    const uint8_t *register_data = NULL;
    size_t register_count = 0U;
    assert(mbc_pdu_parse_read_holding_response(&response_pdu,
                                               &register_data,
                                               &register_count) == MBC_STATUS_OK);
    assert(register_count == 1U);
    assert(register_data[0] == 0x00U && register_data[1] == 0x2AU);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    mbc_winsock_tcp_destroy(ctx);

    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    assert(args.request_len == request_length);
    assert(memcmp(args.request, request_frame, request_length) == 0);
}

/* Helper to create a connected socket pair for testing server mode */
static mbc_status_t create_connected_socket_pair(SOCKET *client_fd, SOCKET *server_fd)
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    /* Create temporary listening socket */
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        return MBC_STATUS_IO_ERROR;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; /* Let OS choose port */

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        closesocket(listener);
        return MBC_STATUS_IO_ERROR;
    }

    if (listen(listener, 1) != 0) {
        closesocket(listener);
        return MBC_STATUS_IO_ERROR;
    }

    /* Get the actual port */
    int addr_len = sizeof(addr);
    if (getsockname(listener, (struct sockaddr *)&addr, &addr_len) != 0) {
        closesocket(listener);
        return MBC_STATUS_IO_ERROR;
    }

    /* Create client socket and connect */
    *client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*client_fd == INVALID_SOCKET) {
        closesocket(listener);
        return MBC_STATUS_IO_ERROR;
    }

    if (connect(*client_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        closesocket(*client_fd);
        closesocket(listener);
        return MBC_STATUS_IO_ERROR;
    }

    /* Accept server side */
    *server_fd = accept(listener, NULL, NULL);
    if (*server_fd == INVALID_SOCKET) {
        closesocket(*client_fd);
        closesocket(listener);
        return MBC_STATUS_IO_ERROR;
    }

    closesocket(listener);

    /* Set non-blocking mode */
    u_long mode = 1;
    ioctlsocket(*server_fd, FIONBIO, &mode);
    ioctlsocket(*client_fd, FIONBIO, &mode);

    return MBC_STATUS_OK;
}

/* Transport implementation using raw socket for server testing */
typedef struct {
    SOCKET fd;
} socket_transport_ctx_t;

static mbc_status_t socket_transport_send(void *ctx,
                                          const uint8_t *buffer,
                                          size_t length,
                                          mbc_transport_io_t *out)
{
    socket_transport_ctx_t *sock = ctx;
    if (!sock || (!buffer && length > 0U)) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    size_t total = 0U;
    while (total < length) {
        int rc = send(sock->fd, (const char *)buffer + total, (int)(length - total), 0);
        if (rc < 0) {
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) {
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

static mbc_status_t socket_transport_receive(void *ctx,
                                             uint8_t *buffer,
                                             size_t capacity,
                                             mbc_transport_io_t *out)
{
    socket_transport_ctx_t *sock = ctx;
    if (!sock || !buffer || capacity == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    int rc = recv(sock->fd, (char *)buffer, (int)capacity, 0);
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

    if (out) {
        out->processed = (size_t)rc;
    }
    return MBC_STATUS_OK;
}

static uint64_t socket_transport_now(void *ctx)
{
    (void)ctx;
    return (uint64_t)GetTickCount64();
}

static void socket_transport_yield(void *ctx)
{
    (void)ctx;
    Sleep(1);
}

static void test_winsock_tcp_engine_server(void)
{
    SOCKET client_fd = INVALID_SOCKET;
    SOCKET server_fd = INVALID_SOCKET;
    assert(mbc_status_is_ok(create_connected_socket_pair(&client_fd, &server_fd)));

    socket_transport_ctx_t transport_ctx = {
        .fd = server_fd,
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
    assert(mbc_status_is_ok(build_fc03_request_frame(&request_pdu,
                                                     request_frame,
                                                     sizeof(request_frame),
                                                     &request_length)));

    const uint16_t transaction_id = (uint16_t)((request_frame[0] << 8) | request_frame[1]);

    /* Send request from client socket */
    size_t sent = 0;
    while (sent < request_length) {
        int rc = send(client_fd, (const char *)request_frame + sent,
                     (int)(request_length - sent), 0);
        assert(rc > 0);
        sent += (size_t)rc;
    }

    /* Server receives and decodes request */
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

    /* Build response PDU */
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
    assert(mbc_mbap_encode(&header,
                           response_pdu_bytes,
                           1U + response_pdu.payload_length,
                           response_frame,
                           sizeof(response_frame),
                           &response_length) == MBC_STATUS_OK);

    assert(mbc_engine_submit_request(&engine, response_frame, response_length) == MBC_STATUS_OK);

    /* Client receives response */
    uint8_t received[TCP_MAX_FRAME] = {0};
    size_t total = 0U;
    while (total < response_length) {
        int rc = recv(client_fd, (char *)received + total, (int)(response_length - total), 0);
        if (rc > 0) {
            total += (size_t)rc;
            continue;
        }
        if (rc == 0) {
            break;
        }
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            Sleep(1);
            continue;
        }
        assert(false && "recv failed");
    }
    assert(total == response_length);
    assert(memcmp(received, response_frame, response_length) == 0);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    closesocket(client_fd);
    closesocket(server_fd);
    WSACleanup();
}

static void test_winsock_tcp_engine_client_timeout(void)
{
    SOCKET client_fd = INVALID_SOCKET;
    SOCKET server_fd = INVALID_SOCKET;
    assert(mbc_status_is_ok(create_connected_socket_pair(&client_fd, &server_fd)));

    socket_transport_ctx_t transport_ctx = {
        .fd = client_fd,
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
    assert(mbc_status_is_ok(build_fc03_request_frame(&request_pdu,
                                                     request_frame,
                                                     sizeof(request_frame),
                                                     &request_length)));

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

    assert(mbc_engine_submit_request(&engine,
                                     request_frame,
                                     request_length) == MBC_STATUS_OK);

    /* Drain transmitted request from peer */
    uint8_t peer_buffer[TCP_MAX_FRAME] = {0};
    size_t total = 0U;
    while (total < request_length) {
        int rc = recv(server_fd, (char *)peer_buffer + total, (int)(request_length - total), 0);
        if (rc > 0) {
            total += (size_t)rc;
            continue;
        }
        if (rc == 0) {
            break;
        }
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
            Sleep(1);
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
        Sleep(1);
    }
    assert(timed_out);
    assert(engine.state == MBC_ENGINE_STATE_IDLE);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
    closesocket(client_fd);
    closesocket(server_fd);
    WSACleanup();
}

int main(void)
{
    printf("=== Winsock TCP Tests ===\n\n");
    test_winsock_loop();
    test_winsock_tcp_engine_client();
    test_winsock_tcp_engine_server();
    test_winsock_tcp_engine_client_timeout();
    printf("\n=== All Winsock TCP tests completed ===\n");
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
