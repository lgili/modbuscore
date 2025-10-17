#include <modbus/base.h>
#include <modbus/mb_err.h>
#include <modbus/mb_log.h>
#include <modbus/observe.h>
#include <modbus/pdu.h>
#include <modbus/server.h>
#include <modbus/transport/tcp.h>

#include "common/demo_tcp_socket.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sched.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define DEMO_DEFAULT_PORT 1502U
#define DEMO_UNIT_ID      0x11U

#if defined(_WIN32)
typedef SOCKET demo_socket_t;
#define DEMO_INVALID_SOCKET INVALID_SOCKET
#else
typedef int demo_socket_t;
#define DEMO_INVALID_SOCKET (-1)
#endif

static volatile sig_atomic_t g_stop = 0;

static void handle_signal(int signum)
{
    (void)signum;
    g_stop = 1;
}

static void install_signal_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static void sleep_ms(unsigned int milliseconds)
{
#if defined(_WIN32)
    Sleep(milliseconds);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(milliseconds / 1000U);
    ts.tv_nsec = (long)((milliseconds % 1000U) * 1000000L);
    nanosleep(&ts, NULL);
#endif
}

static void log_event(const mb_event_t *event, void *ctx)
{
    (void)ctx;
    if (event == NULL) {
        return;
    }

    switch (event->type) {
    case MB_EVENT_SERVER_STATE_ENTER:
        if (event->source == MB_EVENT_SOURCE_SERVER) {
            printf("[server] state -> %u\n", (unsigned)event->data.server_state.state);
        }
        break;
    case MB_EVENT_SERVER_STATE_EXIT:
        if (event->source == MB_EVENT_SOURCE_SERVER) {
            printf("[server] state <- %u\n", (unsigned)event->data.server_state.state);
        }
        break;
    case MB_EVENT_SERVER_REQUEST_ACCEPT:
        if (event->source == MB_EVENT_SOURCE_SERVER) {
            printf("[server] accept fc=%u broadcast=%s\n",
                   (unsigned)event->data.server_req.function,
                   event->data.server_req.broadcast ? "yes" : "no");
        }
        break;
    case MB_EVENT_SERVER_REQUEST_COMPLETE:
        if (event->source == MB_EVENT_SOURCE_SERVER) {
            printf("[server] request fc=%u status=%d\n",
                   (unsigned)event->data.server_req.function,
                   event->data.server_req.status);
        }
        break;
    case MB_EVENT_CLIENT_STATE_ENTER:
    case MB_EVENT_CLIENT_STATE_EXIT:
    case MB_EVENT_CLIENT_TX_SUBMIT:
    case MB_EVENT_CLIENT_TX_COMPLETE:
        /* Client events are ignored in this server demo. */
        break;
    }
}

static bool set_socket_options(demo_socket_t sock)
{
#if defined(_WIN32)
    BOOL reuse = TRUE;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) != 0) {
        return false;
    }
#else
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        return false;
    }
#endif
    return true;
}

static demo_socket_t open_listen_socket(uint16_t port)
{
    char service[8];
    snprintf(service, sizeof(service), "%u", (unsigned)port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *res = NULL;
    if (getaddrinfo(NULL, service, &hints, &res) != 0 || res == NULL) {
        return DEMO_INVALID_SOCKET;
    }

    demo_socket_t listen_sock = DEMO_INVALID_SOCKET;
    for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next) {
        demo_socket_t handle = (demo_socket_t)socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (handle == DEMO_INVALID_SOCKET) {
            continue;
        }

        if (!set_socket_options(handle)) {
#if defined(_WIN32)
            closesocket(handle);
#else
            close(handle);
#endif
            continue;
        }

        if (bind(handle, ai->ai_addr, (socklen_t)ai->ai_addrlen) != 0) {
#if defined(_WIN32)
            closesocket(handle);
#else
            close(handle);
#endif
            continue;
        }

        if (listen(handle, 1) != 0) {
#if defined(_WIN32)
            closesocket(handle);
#else
            close(handle);
#endif
            continue;
        }

        listen_sock = handle;
        break;
    }

    freeaddrinfo(res);
    return listen_sock;
}

static void close_socket(demo_socket_t sock)
{
    if (sock == DEMO_INVALID_SOCKET) {
        return;
    }
#if defined(_WIN32)
    closesocket(sock);
#else
    close(sock);
#endif
}

static bool wait_for_socket_ready(demo_socket_t sock, unsigned timeout_ms)
{
    if (sock == DEMO_INVALID_SOCKET) {
        return false;
    }

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);

    struct timeval tv;
    tv.tv_sec = (long)(timeout_ms / 1000U);
    tv.tv_usec = (long)((timeout_ms % 1000U) * 1000U);

#if defined(_WIN32)
    int ready = select(0, &readfds, NULL, NULL, (timeout_ms == 0U) ? NULL : &tv);
#else
    int ready = select(sock + 1, &readfds, NULL, NULL, (timeout_ms == 0U) ? NULL : &tv);
#endif

    return (ready > 0) && FD_ISSET(sock, &readfds);
}

#if defined(_WIN32)
static bool adopt_client_socket(demo_socket_t client, mb_port_win_socket_t *win_sock)
{
    return mb_err_is_ok(mb_port_win_socket_init(win_sock, client, true));
}
#else
static bool adopt_client_socket(demo_socket_t client, mb_port_posix_socket_t *posix_sock)
{
    return mb_err_is_ok(mb_port_posix_socket_init(posix_sock, client, true));
}
#endif

typedef struct tcp_session_ctx {
    mb_server_t *server;
    const mb_transport_if_t *socket_iface;
    mb_u16 active_tid;
    bool has_active_tid;
    bool fatal;
    mb_err_t last_error;
} tcp_session_ctx_t;

static mb_err_t tcp_bridge_send(void *ctx, const mb_u8 *buf, mb_size_t len, mb_transport_io_result_t *out)
{
    tcp_session_ctx_t *session = (tcp_session_ctx_t *)ctx;
    if (session == NULL || session->socket_iface == NULL || buf == NULL) {
        if (session != NULL) {
            session->fatal = true;
            session->last_error = MB_ERR_INVALID_ARGUMENT;
        }
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (!session->has_active_tid) {
        session->fatal = true;
        session->last_error = MB_ERR_TRANSPORT;
        return MB_ERR_TRANSPORT;
    }

    if (len < 3U) {
        session->fatal = true;
        session->last_error = MB_ERR_INVALID_ARGUMENT;
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (out) {
        out->processed = len;
    }

    const mb_size_t pdu_len = len - 3U;
    const mb_size_t length_field = 1U + pdu_len; /* unit id + PDU */
    const mb_size_t total_len = MB_TCP_HEADER_SIZE + pdu_len;

    mb_u8 frame[MB_TCP_BUFFER_SIZE];
    frame[0] = (mb_u8)(session->active_tid >> 8);
    frame[1] = (mb_u8)(session->active_tid & 0xFFU);
    frame[2] = 0U;
    frame[3] = 0U;
    frame[4] = (mb_u8)((length_field >> 8) & 0xFFU);
    frame[5] = (mb_u8)(length_field & 0xFFU);
    frame[6] = buf[0];
    if (pdu_len > 0U) {
        memcpy(&frame[7], &buf[1], pdu_len);
    }

    mb_transport_io_result_t io = {0};
    mb_err_t status = mb_transport_send(session->socket_iface, frame, total_len, &io);
    if (!mb_err_is_ok(status)) {
        session->fatal = true;
        session->last_error = status;
        return status;
    }

    if (io.processed != total_len) {
        session->fatal = true;
        session->last_error = MB_ERR_TRANSPORT;
        return MB_ERR_TRANSPORT;
    }

    session->has_active_tid = false;
    session->last_error = MB_OK;
    return MB_OK;
}

static mb_err_t tcp_bridge_recv(void *ctx, mb_u8 *buf, mb_size_t cap, mb_transport_io_result_t *out)
{
    (void)ctx;
    (void)buf;
    (void)cap;
    if (out) {
        out->processed = 0U;
    }
    return MB_ERR_TIMEOUT;
}

static mb_time_ms_t tcp_bridge_now(void *ctx)
{
    (void)ctx;
#if defined(_WIN32)
    return (mb_time_ms_t)GetTickCount64();
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0U;
    }
    return (mb_time_ms_t)((ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL));
#endif
}

static void tcp_bridge_yield(void *ctx)
{
    (void)ctx;
#if defined(_WIN32)
    Sleep(0);
#else
    sched_yield();
#endif
}

static void tcp_session_frame_callback(mb_tcp_transport_t *tcp,
                                       const mb_adu_view_t *adu,
                                       mb_u16 transaction_id,
                                       mb_err_t status,
                                       void *user_ctx)
{
    (void)tcp;
    tcp_session_ctx_t *session = (tcp_session_ctx_t *)user_ctx;
    if (session == NULL || session->server == NULL) {
        return;
    }

    if (!mb_err_is_ok(status) || adu == NULL) {
        session->fatal = true;
        session->has_active_tid = false;
        session->last_error = mb_err_is_ok(status) ? MB_ERR_TRANSPORT : status;
        return;
    }

    session->active_tid = transaction_id;
    session->has_active_tid = true;

    mb_err_t inject = mb_server_inject_adu(session->server, adu);
    if (!mb_err_is_ok(inject)) {
        session->fatal = true;
        session->has_active_tid = false;
        session->last_error = inject;
        return;
    }

    while (mb_server_pending(session->server) > 0U || !mb_server_is_idle(session->server)) {
        mb_err_t poll_status = mb_server_poll(session->server);
        if (!mb_err_is_ok(poll_status) && poll_status != MB_ERR_TIMEOUT) {
            session->fatal = true;
            session->last_error = poll_status;
            break;
        }
    }

    if (session->has_active_tid) {
        /* Broadcast requests do not expect a response. */
        session->has_active_tid = false;
    }

    if (!session->fatal) {
        session->last_error = MB_OK;
    }
}

int main(int argc, char **argv)
{
    uint16_t port = DEMO_DEFAULT_PORT;
    mb_u8 unit_id = DEMO_UNIT_ID;
    bool enable_trace = false;

    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) && (i + 1) < argc) {
            port = (uint16_t)strtoul(argv[++i], NULL, 0);
        } else if ((strcmp(argv[i], "--unit") == 0 || strcmp(argv[i], "-u") == 0) && (i + 1) < argc) {
            unit_id = (mb_u8)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--trace") == 0) {
            enable_trace = true;
        } else {
            fprintf(stderr, "Usage: %s [--port <tcp-port>] [--unit <id>] [--trace]\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    install_signal_handlers();
    mb_log_bootstrap_defaults();

#if defined(_WIN32)
    if (!mb_err_is_ok(demo_tcp_socket_global_init())) {
        fprintf(stderr, "Failed to initialise Winsock.\n");
        return EXIT_FAILURE;
    }
#endif

    demo_socket_t listen_sock = open_listen_socket(port);
    if (listen_sock == DEMO_INVALID_SOCKET) {
        fprintf(stderr, "Failed to open listening socket on port %u\n", (unsigned)port);
#if defined(_WIN32)
        demo_tcp_socket_global_cleanup();
#endif
        return EXIT_FAILURE;
    }

    printf("Modbus TCP server listening on port %u (unit=%u). Press Ctrl+C to stop.\n",
           (unsigned)port,
           (unsigned)unit_id);

    mb_server_t server;
    mb_server_region_t regions[2];
    mb_server_request_t request_pool[8];
    uint16_t holding_regs[32];
    memset(holding_regs, 0, sizeof(holding_regs));

    bool connection_active = false;
    demo_socket_t client_sock = DEMO_INVALID_SOCKET;
#if defined(_WIN32)
    mb_port_win_socket_t win_sock;
#else
    mb_port_posix_socket_t posix_sock;
#endif
    const mb_transport_if_t *socket_iface = NULL;
    mb_tcp_transport_t tcp_transport;
    memset(&tcp_transport, 0, sizeof(tcp_transport));
    tcp_session_ctx_t session_ctx;
    memset(&session_ctx, 0, sizeof(session_ctx));
    mb_transport_if_t bridge_iface;
    memset(&bridge_iface, 0, sizeof(bridge_iface));

    unsigned register_tick = 0U;

    while (!g_stop) {
        if (!connection_active) {
            if (!wait_for_socket_ready(listen_sock, 250U)) {
                sleep_ms(50U);
                continue;
            }

            struct sockaddr_storage addr;
            socklen_t addr_len = (socklen_t)sizeof(addr);
            client_sock = (demo_socket_t)accept(listen_sock, (struct sockaddr *)&addr, &addr_len);
            if (client_sock == DEMO_INVALID_SOCKET) {
                continue;
            }

            if (!adopt_client_socket(client_sock,
#if defined(_WIN32)
                                     &win_sock
#else
                                     &posix_sock
#endif
                                     )) {
                close_socket(client_sock);
                client_sock = DEMO_INVALID_SOCKET;
                continue;
            }

#if defined(_WIN32)
            socket_iface = mb_port_win_socket_iface(&win_sock);
#else
            socket_iface = mb_port_posix_socket_iface(&posix_sock);
#endif
            if (socket_iface == NULL) {
#if defined(_WIN32)
                mb_port_win_socket_close(&win_sock);
#else
                mb_port_posix_socket_close(&posix_sock);
#endif
                client_sock = DEMO_INVALID_SOCKET;
                continue;
            }

            memset(&session_ctx, 0, sizeof(session_ctx));
            session_ctx.server = &server;
            session_ctx.socket_iface = socket_iface;
            session_ctx.active_tid = 0U;
            session_ctx.has_active_tid = false;
            session_ctx.fatal = false;
            session_ctx.last_error = MB_OK;

            bridge_iface.ctx = &session_ctx;
            bridge_iface.send = tcp_bridge_send;
            bridge_iface.recv = tcp_bridge_recv;
            bridge_iface.now = tcp_bridge_now;
            bridge_iface.yield = tcp_bridge_yield;

            mb_err_t server_status = mb_server_init(&server,
                                                    &bridge_iface,
                                                    unit_id,
                                                    regions,
                                                    MB_COUNTOF(regions),
                                                    request_pool,
                                                    MB_COUNTOF(request_pool));
            if (!mb_err_is_ok(server_status)) {
                printf("[server] mb_server_init failed: %s\n", mb_err_str(server_status));
#if defined(_WIN32)
                mb_port_win_socket_close(&win_sock);
#else
                mb_port_posix_socket_close(&posix_sock);
#endif
                client_sock = DEMO_INVALID_SOCKET;
                socket_iface = NULL;
                continue;
            }

            server_status = mb_server_add_storage(&server,
                                                  0x0000U,
                                                  (mb_u16)MB_COUNTOF(holding_regs),
                                                  false,
                                                  holding_regs);
            if (!mb_err_is_ok(server_status)) {
                printf("[server] mb_server_add_storage failed: %s\n", mb_err_str(server_status));
#if defined(_WIN32)
                mb_port_win_socket_close(&win_sock);
#else
                mb_port_posix_socket_close(&posix_sock);
#endif
                client_sock = DEMO_INVALID_SOCKET;
                socket_iface = NULL;
                continue;
            }
            mb_server_set_event_callback(&server, log_event, NULL);
            mb_server_set_trace_hex(&server, enable_trace);

            mb_err_t tcp_status = mb_tcp_init(&tcp_transport, socket_iface, tcp_session_frame_callback, &session_ctx);
            if (!mb_err_is_ok(tcp_status)) {
                printf("[server] mb_tcp_init failed: %s\n", mb_err_str(tcp_status));
#if defined(_WIN32)
                mb_port_win_socket_close(&win_sock);
#else
                mb_port_posix_socket_close(&posix_sock);
#endif
                client_sock = DEMO_INVALID_SOCKET;
                socket_iface = NULL;
                continue;
            }
            mb_tcp_reset(&tcp_transport);

            connection_active = true;
            printf("[server] client connected\n");
            continue;
        }

        holding_regs[0] = (uint16_t)register_tick;
        holding_regs[1] = (uint16_t)(register_tick >> 16);
        holding_regs[2] = (uint16_t)(time(NULL) & 0xFFFF);
        register_tick++;

        bool disconnect = false;
        mb_err_t disconnect_err = MB_OK;

        mb_err_t poll_status = mb_server_poll(&server);
        if (!mb_err_is_ok(poll_status) && poll_status != MB_ERR_TIMEOUT) {
            disconnect = true;
            disconnect_err = poll_status;
        }

        if (!disconnect) {
            if (wait_for_socket_ready(client_sock, 50U)) {
                mb_err_t tcp_status = mb_tcp_poll(&tcp_transport);
                if (!mb_err_is_ok(tcp_status) && tcp_status != MB_ERR_TIMEOUT) {
                    disconnect = true;
                    disconnect_err = tcp_status;
                }
            } else {
                sleep_ms(10U);
            }
        }

        if (!disconnect && session_ctx.fatal) {
            disconnect = true;
            disconnect_err = (session_ctx.last_error != MB_OK) ? session_ctx.last_error : MB_ERR_TRANSPORT;
        }

        if (!disconnect) {
            continue;
        }

        if (!mb_err_is_ok(disconnect_err)) {
            printf("[server] transport error: %s\n", mb_err_str(disconnect_err));
        }

        mb_tcp_reset(&tcp_transport);
#if defined(_WIN32)
        mb_port_win_socket_close(&win_sock);
#else
        mb_port_posix_socket_close(&posix_sock);
#endif
        client_sock = DEMO_INVALID_SOCKET;
        socket_iface = NULL;
        session_ctx.socket_iface = NULL;
        session_ctx.has_active_tid = false;
        session_ctx.fatal = false;
        connection_active = false;
        printf("[server] client disconnected\n");
    }

    if (connection_active) {
        mb_tcp_reset(&tcp_transport);
#if defined(_WIN32)
        mb_port_win_socket_close(&win_sock);
#else
        mb_port_posix_socket_close(&posix_sock);
#endif
    }

    close_socket(listen_sock);

#if defined(_WIN32)
    demo_tcp_socket_global_cleanup();
#endif

    printf("Server stopped.\n");
    return EXIT_SUCCESS;
}
