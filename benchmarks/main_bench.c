/**
 * @file main_bench.c
 * @brief Main entry point for Modbus benchmark suite.
 */

#include "bench_common.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ========================================================================== */
/*                        External Benchmark Suites                           */
/* ========================================================================== */

extern void bench_baseline_register(void);
extern void bench_crc_register(void);
extern void bench_encode_register(void);
extern void bench_poll_register(void);

// extern void bench_decode_register(void);
// extern void bench_rtu_register(void);
// extern void bench_tcp_register(void);
// Future benchmarks (Phase 3):
// extern void bench_decode_register(void);
// extern void bench_rtu_register(void);
// extern void bench_tcp_register(void);

/* ========================================================================== */
/*                           Platform Information                             */
/* ========================================================================== */

static void print_platform_info(void)
{
    printf("=== Modbus Benchmarks ===\n");
    
    // Platform
#if defined(__linux__)
    printf("Platform: Linux\n");
#elif defined(__APPLE__)
    printf("Platform: macOS\n");
#elif defined(_WIN32)
    printf("Platform: Windows\n");
#elif defined(MB_BENCH_CORTEX_M)
    printf("Platform: ARM Cortex-M\n");
#elif defined(MB_BENCH_RISCV)
    printf("Platform: RISC-V\n");
#else
    printf("Platform: Unknown\n");
#endif

    // Architecture
#if defined(__x86_64__) || defined(_M_X64)
    printf("Architecture: x86_64\n");
#elif defined(__aarch64__) || defined(_M_ARM64)
    printf("Architecture: ARM64\n");
#elif defined(__arm__) || defined(_M_ARM)
    printf("Architecture: ARM32\n");
#elif defined(__riscv)
    printf("Architecture: RISC-V\n");
#else
    printf("Architecture: Unknown\n");
#endif

    // Compiler
#if defined(__clang__)
    printf("Compiler: Clang %d.%d.%d\n", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    printf("Compiler: GCC %d.%d.%d\n", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    printf("Compiler: MSVC %d\n", _MSC_VER);
#else
    printf("Compiler: Unknown\n");
#endif

    // Timestamp
    time_t now = time(NULL);
    printf("Date: %s", ctime(&now));
    
    printf("\n");
}

/* ========================================================================== */
/*                              Main Entry Point                              */
/* ========================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // Initialize timing subsystem
    mb_bench_init();

    // Print platform info
    print_platform_info();

    // Register all benchmark suites
    printf("Registering benchmarks...\n\n");
    bench_baseline_register();
    bench_crc_register();
    bench_encode_register();
    bench_poll_register();
    // bench_decode_register();  // Future (Phase 3)
    // bench_rtu_register();      // Future (Phase 3)
    // bench_tcp_register();      // Future (Phase 3)

    // Print header
    printf("Running benchmarks...\n\n");
    printf("%-32s  %-12s  %-40s  %s\n", "Benchmark", "Average", "Range", "Status");
    printf("%-32s  %-12s  %-40s  %s\n", "----------", "-------", "-----", "------");

    // Run all benchmarks (already registered)
    // Benchmarks are run during registration via mb_bench_register()
    
    // Print individual results
    size_t count = mb_bench_get_count();
    for (size_t i = 0; i < count; i++) {
        const mb_bench_stats_t *stats = mb_bench_get_stats(i);
        if (stats) {
            mb_bench_print_stats(stats);
        }
    }
    
    // Print summary
    printf("\n");
    mb_bench_print_summary();

    // Export to JSON if requested
    const char *json_output = "benchmark_results.json";
    if (argc > 1 && strcmp(argv[1], "--json") == 0) {
        if (argc > 2) {
            json_output = argv[2];
        }
        printf("\nExporting results to %s...\n", json_output);
        if (mb_bench_export_json(json_output) == 0) {
            printf("✅ Export successful\n");
        } else {
            printf("❌ Export failed\n");
        }
    }

    // Return exit code based on failures
    int failures = mb_bench_run_all();
    return (failures == 0) ? 0 : 1;
}
