/**
 * @file posix_tcp.c
 * @brief Implementation of non-blocking POSIX TCP transport driver.
 */

/* Enable POSIX extensions for getaddrinfo, usleep, clock_gettime */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <modbuscore/transport/posix_tcp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/**
 * @brief Internal TCP context structure.
 */
struct mbc_posix_tcp_ctx {
    int sockfd;               /**< Socket file descriptor. */
    bool connected;           /**< Active connection flag. */
    uint32_t recv_timeout_ms; /**< I/O timeout used when waiting for send/receive. */
};

/* ============================================================================
 * Private Helper Functions
 * ========================================================================== */

/**
 * @brief Get timestamp in milliseconds (monotonic if available).
 */
static uint64_t get_timestamp_ms(void)
{
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (uint64_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
    }
#endif
    /* Fallback to gettimeofday */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL);
}

/**
 * @brief Configure socket as non-blocking.
 */
static mbc_status_t set_nonblocking(int sockfd)
{
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        return MBC_STATUS_IO_ERROR;
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        return MBC_STATUS_IO_ERROR;
    }
    return MBC_STATUS_OK;
}

/**
 * @brief Configure useful TCP socket options (TCP_NODELAY, keepalive).
 */
static void configure_socket_options(int sockfd)
{
    /* Disable Nagle's algorithm for low latency */
    int flag = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    /* Enable TCP keepalive (useful for detecting dead connections) */
    flag = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
}

/**
 * @brief Configure receive/send timeouts if provided (> 0).
 */
static void configure_timeout_options(int sockfd, uint32_t timeout_ms)
{
    if (timeout_ms == 0U) {
        return; /* No specific timeout (infinite) */
    }

    struct timeval tv;
    tv.tv_sec = (time_t)(timeout_ms / 1000U);
    tv.tv_usec = (suseconds_t)((timeout_ms % 1000U) * 1000U);

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

/**
 * @brief Convert timeout_ms to poll() timeout parameter.
 */
static int timeout_to_poll(uint32_t timeout_ms)
{
    if (timeout_ms == 0U) {
        return -1; /* No timeout => poll blocks until event */
    }

    if (timeout_ms > (uint32_t)INT_MAX) {
        return INT_MAX;
    }

    return (int)timeout_ms;
}

/**
 * @brief Wait for socket to become writable (for connect or send).
 */
static mbc_status_t wait_for_writable_fd(int sockfd, uint32_t timeout_ms)
{
    const int poll_timeout = timeout_to_poll(timeout_ms);

    while (true) {
        struct pollfd pfd = {
            .fd = sockfd,
            .events = POLLOUT,
            .revents = 0,
        };

        int rc = poll(&pfd, 1, poll_timeout);
        if (rc > 0) {
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                return MBC_STATUS_IO_ERROR;
            }
            if (pfd.revents & POLLOUT) {
                return MBC_STATUS_OK;
            }
            /* Unexpected events: treat as generic error */
            return MBC_STATUS_IO_ERROR;
        }

        if (rc == 0) {
            return MBC_STATUS_TIMEOUT;
        }

        if (errno == EINTR) {
            continue;
        }

        return MBC_STATUS_IO_ERROR;
    }
}

/**
 * @brief Resolve hostname using getaddrinfo (IPv4/IPv6).
 */
static mbc_status_t resolve_host(const char* host, uint16_t port, struct addrinfo** out_addrinfo)
{
    if (!host || !out_addrinfo) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    char port_str[6];
    int written = snprintf(port_str, sizeof(port_str), "%u", port);
    if (written <= 0 || written >= (int)sizeof(port_str)) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC, /* IPv4 or IPv6 */
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
    };

    struct addrinfo* result = NULL;
    int rc = getaddrinfo(host, port_str, &hints, &result);
    if (rc != 0 || !result) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    *out_addrinfo = result;
    return MBC_STATUS_OK;
}

/**
 * @brief Wait for completion of non-blocking connect operation.
 */
static mbc_status_t wait_for_connect(int sockfd, uint32_t timeout_ms)
{
    mbc_status_t status = wait_for_writable_fd(sockfd, timeout_ms);
    if (!mbc_status_is_ok(status)) {
        return status;
    }

    int err = 0;
    socklen_t len = sizeof(err);
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &len) < 0) {
        return MBC_STATUS_IO_ERROR;
    }

    if (err != 0) {
        errno = err;
        return MBC_STATUS_IO_ERROR;
    }

    return MBC_STATUS_OK;
}

/* ============================================================================
 * mbc_transport_iface_t Interface Callbacks
 * ========================================================================== */

static mbc_status_t posix_tcp_send(void* ctx, const uint8_t* buffer, size_t length,
                                   mbc_transport_io_t* out)
{
    mbc_posix_tcp_ctx_t* tcp = (mbc_posix_tcp_ctx_t*)ctx;

    if (!tcp || !buffer || length == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!tcp->connected) {
        return MBC_STATUS_IO_ERROR;
    }

    size_t total_sent = 0U;

    while (total_sent < length) {
        ssize_t sent = send(tcp->sockfd, buffer + total_sent, length - total_sent, 0);
        if (sent > 0) {
            total_sent += (size_t)sent;
            continue;
        }

        if (sent == 0) {
            /* No data sent; treat as error to avoid infinite loop */
            tcp->connected = false;
            if (out) {
                out->processed = total_sent;
            }
            return MBC_STATUS_IO_ERROR;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            mbc_status_t wait_status = wait_for_writable_fd(tcp->sockfd, tcp->recv_timeout_ms);
            if (!mbc_status_is_ok(wait_status)) {
                if (wait_status != MBC_STATUS_TIMEOUT) {
                    tcp->connected = false;
                }
                if (out) {
                    out->processed = total_sent;
                }
                return wait_status;
            }
            continue;
        }

        tcp->connected = false;
        if (out) {
            out->processed = total_sent;
        }
        return MBC_STATUS_IO_ERROR;
    }

    if (out) {
        out->processed = total_sent;
    }

    return MBC_STATUS_OK;
}

static mbc_status_t posix_tcp_receive(void* ctx, uint8_t* buffer, size_t capacity,
                                      mbc_transport_io_t* out)
{
    mbc_posix_tcp_ctx_t* tcp = (mbc_posix_tcp_ctx_t*)ctx;

    if (!tcp || !buffer || capacity == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!tcp->connected) {
        return MBC_STATUS_IO_ERROR;
    }

    ssize_t received = recv(tcp->sockfd, buffer, capacity, 0);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* No data available now */
            if (out) {
                out->processed = 0U;
            }
            return MBC_STATUS_OK;
        }

        /* Real error */
        tcp->connected = false;
        return MBC_STATUS_IO_ERROR;
    }

    if (received == 0) {
        /* Connection closed by peer */
        tcp->connected = false;
        return MBC_STATUS_IO_ERROR;
    }

    if (out) {
        out->processed = (size_t)received;
    }

    return MBC_STATUS_OK;
}

static uint64_t posix_tcp_now(void* ctx)
{
    (void)ctx;
    return get_timestamp_ms();
}

static void posix_tcp_yield(void* ctx)
{
    (void)ctx;
    /* Cooperative yield: minimal sleep to release CPU */
    struct timespec ts = {
        .tv_sec = 0,
        .tv_nsec = 1000000L,
    };
    nanosleep(&ts, NULL);
}

/* ============================================================================
 * Public API
 * ========================================================================== */

mbc_status_t mbc_posix_tcp_create(const mbc_posix_tcp_config_t* config,
                                  mbc_transport_iface_t* out_iface, mbc_posix_tcp_ctx_t** out_ctx)
{
    if (!config || !config->host || config->port == 0U || !out_iface || !out_ctx) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    struct addrinfo* addr_list = NULL;
    mbc_status_t status = resolve_host(config->host, config->port, &addr_list);
    if (!mbc_status_is_ok(status)) {
        return status;
    }

    int sockfd = -1;
    mbc_status_t last_status = MBC_STATUS_IO_ERROR;

    for (struct addrinfo* entry = addr_list; entry != NULL; entry = entry->ai_next) {
        sockfd = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
        if (sockfd < 0) {
            last_status = MBC_STATUS_IO_ERROR;
            continue;
        }

        configure_socket_options(sockfd);
        configure_timeout_options(sockfd, config->recv_timeout_ms);

        if (config->connect_timeout_ms > 0U) {
            status = set_nonblocking(sockfd);
            if (!mbc_status_is_ok(status)) {
                close(sockfd);
                sockfd = -1;
                last_status = status;
                continue;
            }
        }

        int rc = connect(sockfd, entry->ai_addr, entry->ai_addrlen);
        if (rc == 0) {
            last_status = MBC_STATUS_OK;
        } else if (rc < 0 && errno == EINPROGRESS && config->connect_timeout_ms > 0U) {
            last_status = wait_for_connect(sockfd, config->connect_timeout_ms);
        } else {
            last_status = MBC_STATUS_IO_ERROR;
        }

        if (!mbc_status_is_ok(last_status)) {
            close(sockfd);
            sockfd = -1;
            continue;
        }

        /* Ensure non-blocking mode for subsequent operations */
        status = set_nonblocking(sockfd);
        if (!mbc_status_is_ok(status)) {
            close(sockfd);
            sockfd = -1;
            last_status = status;
            continue;
        }

        break;
    }

    freeaddrinfo(addr_list);

    if (sockfd < 0) {
        return last_status;
    }

    mbc_posix_tcp_ctx_t* tcp = (mbc_posix_tcp_ctx_t*)calloc(1, sizeof(*tcp));
    if (!tcp) {
        close(sockfd);
        return MBC_STATUS_NO_RESOURCES;
    }

    tcp->sockfd = sockfd;
    tcp->connected = true;
    tcp->recv_timeout_ms = config->recv_timeout_ms;

    /* Fill transport interface */
    out_iface->ctx = tcp;
    out_iface->send = posix_tcp_send;
    out_iface->receive = posix_tcp_receive;
    out_iface->now = posix_tcp_now;
    out_iface->yield = posix_tcp_yield;

    *out_ctx = tcp;
    return MBC_STATUS_OK;
}

void mbc_posix_tcp_destroy(mbc_posix_tcp_ctx_t* ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->sockfd >= 0) {
        close(ctx->sockfd);
        ctx->sockfd = -1;
    }

    ctx->connected = false;
    free(ctx);
}

bool mbc_posix_tcp_is_connected(const mbc_posix_tcp_ctx_t* ctx) { return ctx && ctx->connected; }
