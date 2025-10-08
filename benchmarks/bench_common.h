/**
 * @file bench_common.h
 * @brief Common infrastructure for Modbus benchmarking framework.
 *
 * Provides portable timing primitives and benchmark harness for measuring
 * performance of encoding, decoding, and end-to-end operations.
 *
 * Supports:
 * - Host platforms: Linux/macOS (clock_gettime), Windows (QueryPerformanceCounter)
 * - Embedded targets: Cortex-M (DWT cycle counter), RISC-V (mcycle), AVR (TCNT)
 *
 * Usage:
 *   MB_BENCH_DEFINE(my_benchmark) {
 *       // Code to benchmark
 *   }
 *
 *   MB_BENCH_RUN(my_benchmark, 10000);  // Run 10k iterations
 *   MB_BENCH_REPORT();
 */

#ifndef MODBUS_BENCH_COMMON_H
#define MODBUS_BENCH_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                             Platform Detection                             */
/* ========================================================================== */

#if defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    #define MB_BENCH_POSIX 1
    #include <time.h>
    #include <unistd.h>
#elif defined(_WIN32)
    #define MB_BENCH_WINDOWS 1
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_6M__)
    #define MB_BENCH_CORTEX_M 1
    // DWT (Data Watchpoint and Trace) cycle counter
#elif defined(__riscv)
    #define MB_BENCH_RISCV 1
    // mcycle CSR register
#else
    #define MB_BENCH_GENERIC 1
#endif

/* ========================================================================== */
/*                              Timing Primitives                             */
/* ========================================================================== */

/**
 * @brief Opaque timestamp type (nanoseconds or cycles).
 */
typedef uint64_t mb_bench_time_t;

/**
 * @brief Initialize benchmark timing subsystem.
 *
 * Must be called once before any benchmarks run.
 * On Cortex-M, enables DWT cycle counter.
 */
void mb_bench_init(void);

/**
 * @brief Get current timestamp.
 *
 * @return Timestamp in nanoseconds (host) or cycles (embedded).
 */
mb_bench_time_t mb_bench_now(void);

/**
 * @brief Calculate elapsed time between two timestamps.
 *
 * @param start Start timestamp from mb_bench_now()
 * @param end   End timestamp from mb_bench_now()
 * @return Elapsed time in nanoseconds (host) or cycles (embedded)
 */
static inline mb_bench_time_t mb_bench_elapsed(mb_bench_time_t start, mb_bench_time_t end)
{
    return end - start;
}

/**
 * @brief Convert cycles to nanoseconds (embedded only).
 *
 * @param cycles Number of CPU cycles
 * @param cpu_freq_hz CPU frequency in Hz
 * @return Equivalent time in nanoseconds
 */
static inline uint64_t mb_bench_cycles_to_ns(uint64_t cycles, uint64_t cpu_freq_hz)
{
    return (cycles * 1000000000ULL) / cpu_freq_hz;
}

/* ========================================================================== */
/*                           Benchmark Statistics                             */
/* ========================================================================== */

/**
 * @brief Statistics for a benchmark run.
 */
typedef struct {
    const char *name;           ///< Benchmark name
    uint64_t iterations;        ///< Number of iterations
    uint64_t min_ns;            ///< Minimum time (ns)
    uint64_t max_ns;            ///< Maximum time (ns)
    uint64_t avg_ns;            ///< Average time (ns)
    uint64_t p50_ns;            ///< Median (50th percentile)
    uint64_t p95_ns;            ///< 95th percentile
    uint64_t p99_ns;            ///< 99th percentile
    uint64_t total_ns;          ///< Total elapsed time
    bool passed;                ///< True if within budget
    uint64_t budget_ns;         ///< Performance budget (0 = no budget)
} mb_bench_stats_t;

/**
 * @brief Maximum number of benchmarks in a suite.
 */
#define MB_BENCH_MAX_SUITES 64

/**
 * @brief Maximum iterations for storing individual samples (for percentiles).
 */
#define MB_BENCH_MAX_SAMPLES 100000

/* ========================================================================== */
/*                            Benchmark Definition                            */
/* ========================================================================== */

/**
 * @brief Benchmark function signature.
 *
 * @param user_data Optional user data passed to setup
 */
typedef void (*mb_bench_func_t)(void *user_data);

/**
 * @brief Benchmark descriptor.
 */
typedef struct {
    const char *name;           ///< Benchmark name
    mb_bench_func_t setup;      ///< Setup function (called once, not timed)
    mb_bench_func_t run;        ///< Function to benchmark (called many times)
    mb_bench_func_t teardown;   ///< Cleanup function (called once, not timed)
    void *user_data;            ///< User data passed to functions
    uint64_t iterations;        ///< Number of iterations to run
    uint64_t warmup_iters;      ///< Warmup iterations (not measured)
    uint64_t budget_ns;         ///< Performance budget in ns (0 = no check)
} mb_bench_t;

/**
 * @brief Define a benchmark with setup/run/teardown.
 */
#define MB_BENCH_DEFINE(name, setup_fn, run_fn, teardown_fn, iters, warmup, budget) \
    static mb_bench_t bench_##name = {                                                \
        .name = #name,                                                                \
        .setup = setup_fn,                                                            \
        .run = run_fn,                                                                \
        .teardown = teardown_fn,                                                      \
        .user_data = NULL,                                                            \
        .iterations = iters,                                                          \
        .warmup_iters = warmup,                                                       \
        .budget_ns = budget,                                                          \
    }

/**
 * @brief Define a simple benchmark (only run function, no setup/teardown).
 */
#define MB_BENCH_SIMPLE(name, run_fn, iters) \
    MB_BENCH_DEFINE(name, NULL, run_fn, NULL, iters, 100, 0)

/* ========================================================================== */
/*                           Benchmark Execution                              */
/* ========================================================================== */

/**
 * @brief Run a benchmark and collect statistics.
 *
 * @param bench Benchmark descriptor
 * @param stats Output statistics (must be allocated by caller)
 * @return 0 on success, -1 on error
 */
int mb_bench_run(mb_bench_t *bench, mb_bench_stats_t *stats);

/**
 * @brief Register a benchmark in the global suite.
 *
 * @param bench Benchmark descriptor
 * @return 0 on success, -1 if suite is full
 */
int mb_bench_register(mb_bench_t *bench);

/**
 * @brief Run all registered benchmarks.
 *
 * @return Number of failed benchmarks (0 = all passed)
 */
int mb_bench_run_all(void);

/**
 * @brief Print statistics for a single benchmark.
 *
 * @param stats Benchmark statistics
 */
void mb_bench_print_stats(const mb_bench_stats_t *stats);

/**
 * @brief Print summary of all benchmarks.
 */
void mb_bench_print_summary(void);

/**
 * @brief Export results to JSON file.
 *
 * @param filename Output file path
 * @return 0 on success, -1 on error
 */
int mb_bench_export_json(const char *filename);

/**
 * @brief Get benchmark statistics by index.
 *
 * @param index Benchmark index (0 to count-1)
 * @return Pointer to stats, or NULL if index out of range
 */
const mb_bench_stats_t *mb_bench_get_stats(size_t index);

/**
 * @brief Get total number of registered benchmarks.
 *
 * @return Number of benchmarks
 */
size_t mb_bench_get_count(void);

/* ========================================================================== */
/*                          Utility Macros                                    */
/* ========================================================================== */

/**
 * @brief Start timing a code block.
 */
#define MB_BENCH_START() \
    mb_bench_time_t __bench_start = mb_bench_now()

/**
 * @brief End timing and return elapsed nanoseconds.
 */
#define MB_BENCH_END() \
    mb_bench_elapsed(__bench_start, mb_bench_now())

/**
 * @brief Prevent compiler from optimizing away code.
 */
#if defined(__GNUC__) || defined(__clang__)
    #define MB_BENCH_DONT_OPTIMIZE(x) \
        do { \
            __asm__ volatile("" : : "r,m"(x) : "memory"); \
        } while (0)
#else
    #define MB_BENCH_DONT_OPTIMIZE(x) \
        do { \
            volatile void *__tmp = (void*)&(x); \
            (void)__tmp; \
        } while (0)
#endif

/**
 * @brief Compiler barrier (prevents reordering).
 */
#if defined(__GNUC__) || defined(__clang__)
    #define MB_BENCH_BARRIER() \
        __asm__ volatile("" ::: "memory")
#else
    #define MB_BENCH_BARRIER() \
        do { } while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_BENCH_COMMON_H */
