/* Legacy Windows UART helper removed. See cross-platform demos under `examples/`. */

#if 0
#include "uart_windows.h"
#include <stdio.h>
#include <modbus/mb_log.h>

DWORD WINAPI uart_listener_thread(LPVOID lpParam);

int uart_init(uart_handle_t *uart, const char *port_name, int baud_rate) {
    (void)uart;
    (void)port_name;
    (void)baud_rate;
    return -1;
}

#endif /* LEGACY_UART_WINDOWS */
