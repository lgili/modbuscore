/*
 * Migration demo showcasing the libmodbus compatibility layer.
 *
 * Original applications only need to swap the include to
 * <modbus/compat/libmodbus.h> and link against libmodbuscore.
 *
 * Usage:
 *   ./libmodbus_migration_demo 127.0.0.1 1502
 *
 * Start the bundled tcp_server_demo (or any Modbus server) before running.
 */

#include <modbus/compat/libmodbus.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void die(const char *message, int err)
{
    fprintf(stderr, "%s: %s\n", message, modbus_strerror(err));
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    const char *host = (argc > 1) ? argv[1] : "127.0.0.1";
    const int port = (argc > 2) ? atoi(argv[2]) : 1502;

    modbus_t *ctx = modbus_new_tcp(host, port);
    if (ctx == NULL) {
        die("modbus_new_tcp", errno != 0 ? errno : modbus_errno);
    }

    modbus_set_slave(ctx, 1);
    modbus_set_response_timeout(ctx, 1, 500000);

    if (modbus_connect(ctx) == -1) {
        const int err = errno != 0 ? errno : modbus_errno;
        modbus_free(ctx);
        die("modbus_connect", err);
    }

    uint16_t holding[8];
    if (modbus_read_registers(ctx, 0, 4, holding) == -1) {
        const int err = errno != 0 ? errno : modbus_errno;
        modbus_close(ctx);
        modbus_free(ctx);
        die("modbus_read_registers", err);
    }

    printf("Holding registers @0..3:");
    for (int i = 0; i < 4; ++i) {
        printf(" %u", holding[i]);
    }
    printf("\n");

    uint16_t new_value = (uint16_t)(holding[0] + 1U);
    if (modbus_write_register(ctx, 0, (int)new_value) == -1) {
        const int err = errno != 0 ? errno : modbus_errno;
        modbus_close(ctx);
        modbus_free(ctx);
        die("modbus_write_register", err);
    }

    uint16_t burst[3];
    memset(burst, 0, sizeof(burst));
    for (int i = 0; i < 3; ++i) {
        burst[i] = (uint16_t)(new_value + 10 + i);
    }
    if (modbus_write_registers(ctx, 1, 3, burst) == -1) {
        const int err = errno != 0 ? errno : modbus_errno;
        modbus_close(ctx);
        modbus_free(ctx);
        die("modbus_write_registers", err);
    }

    printf("Wrote single value %u at address 0 and 3 values starting at address 1.\n", new_value);

    modbus_close(ctx);
    modbus_free(ctx);
    return EXIT_SUCCESS;
}
