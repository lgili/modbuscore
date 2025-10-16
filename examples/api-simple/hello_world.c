/**
 * @file hello_world.c
 * @brief The simplest possible Modbus program - Hello World!
 *
 * This demonstrates the NEW mb_simple API - just 3 lines of code!
 *
 * Build:
 *   gcc -DMB_USE_PROFILE_SIMPLE hello_world.c -lmodbus -o hello_world
 *
 * Or with CMake:
 *   cmake --preset profile-simple
 *   cmake --build --preset profile-simple --target hello_world
 *
 * Run:
 *   ./hello_world 192.168.1.10:502
 */

#define MB_USE_PROFILE_SIMPLE
#include <modbus/mb_simple.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    const char *endpoint = (argc > 1) ? argv[1] : "127.0.0.1:502";

    /* --- THIS IS IT! Just 3 lines to read Modbus registers --- */

    mb_t *mb = mb_create_tcp(endpoint);
    uint16_t reg;
    mb_err_t err = mb_read_holding(mb, 1, 0, 1, &reg);

    /* -------------------------------------- */

    if (err == MB_OK) {
        printf("✓ Register 0: %u\n", reg);
    } else {
        printf("✗ Error: %s\n", mb_error_string(err));
    }

    mb_destroy(mb);
    return (err == MB_OK) ? 0 : 1;
}
