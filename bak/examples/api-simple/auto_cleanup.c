/**
 * @file auto_cleanup.c
 * @brief Demo of MB_AUTO for automatic resource cleanup (GCC/Clang)
 *
 * With MB_AUTO, you don't need to call mb_destroy() - it happens automatically
 * when the variable goes out of scope. Perfect for error handling!
 *
 * Build:
 *   gcc -DMB_USE_PROFILE_SIMPLE auto_cleanup.c -lmodbus -o auto_cleanup
 */

#define MB_USE_PROFILE_SIMPLE
#include <modbus/mb_simple.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    const char *endpoint = (argc > 1) ? argv[1] : "127.0.0.1:502";

    printf("=== Automatic Cleanup Demo ===\n\n");

    /* MB_AUTO automatically calls mb_destroy() when mb goes out of scope */
    MB_AUTO(mb, mb_create_tcp(endpoint));

    if (!mb) {
        fprintf(stderr, "Connection failed\n");
        return 1;
        /* No leak! mb_destroy() is called automatically before return */
    }

    printf("✓ Connected to %s\n\n", endpoint);

    /* Try to read some registers */
    uint16_t regs[10];
    mb_err_t err = mb_read_holding(mb, 1, 0, 10, regs);

    if (err != MB_OK) {
        fprintf(stderr, "✗ Read failed: %s\n", mb_error_string(err));
        return 1;
        /* No leak! mb_destroy() is called automatically */
    }

    printf("✓ Read successful:\n");
    for (int i = 0; i < 10; i++) {
        printf("  Register %d: %u\n", i, regs[i]);
    }

    printf("\n✓ Cleanup happens automatically when function exits\n");
    printf("  (No mb_destroy() needed!)\n");

    /* mb_destroy(mb) is called automatically here */
    return 0;
}
