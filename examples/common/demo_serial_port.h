#ifndef DEMO_SERIAL_PORT_H
#define DEMO_SERIAL_PORT_H

#include <stdbool.h>
#include <modbus/mb_err.h>
#include <modbus/mb_types.h>
#include <modbus/transport_if.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct demo_serial_port {
    bool active;
#if defined(_WIN32)
    void *handle;
#else
    int fd;
#endif
    mb_transport_if_t iface;
} demo_serial_port_t;

mb_err_t demo_serial_port_open(demo_serial_port_t *port,
                               const char *device,
                               unsigned baudrate);

void demo_serial_port_close(demo_serial_port_t *port);

const mb_transport_if_t *demo_serial_port_iface(demo_serial_port_t *port);

#ifdef __cplusplus
}
#endif

#endif /* DEMO_SERIAL_PORT_H */
