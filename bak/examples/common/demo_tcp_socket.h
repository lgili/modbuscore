#ifndef DEMO_TCP_SOCKET_H
#define DEMO_TCP_SOCKET_H

#include <stdbool.h>
#include <stdint.h>

#include <modbus/mb_err.h>
#include <modbus/mb_types.h>
#include <modbus/transport_if.h>

#if defined(_WIN32)
#include <modbus/port/win.h>
#else
#include <modbus/port/posix.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct demo_tcp_socket {
    bool active;
#if defined(_WIN32)
    mb_port_win_socket_t win;
#else
    mb_port_posix_socket_t posix;
#endif
} demo_tcp_socket_t;

#if defined(_WIN32)
mb_err_t demo_tcp_socket_global_init(void);
void demo_tcp_socket_global_cleanup(void);
#else
static inline mb_err_t demo_tcp_socket_global_init(void)
{
    return MB_OK;
}

static inline void demo_tcp_socket_global_cleanup(void)
{
}
#endif

mb_err_t demo_tcp_socket_connect(demo_tcp_socket_t *sock,
                                 const char *host,
                                 uint16_t port,
                                 mb_time_ms_t timeout_ms);

const mb_transport_if_t *demo_tcp_socket_iface(demo_tcp_socket_t *sock);

void demo_tcp_socket_close(demo_tcp_socket_t *sock);

#ifdef __cplusplus
}
#endif

#endif /* DEMO_TCP_SOCKET_H */
