/**
 * @file bench_common.c
 * @brief Implementation of portable benchmark timing infrastructure.
 */

#include "bench_common.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================== */
/*                        Platform-Specific Timing                            */
/* ========================================================================== */

#if defined(MB_BENCH_POSIX)
/**
 * @brief POSIX implementation using clock_gettime(CLOCK_MONOTONIC).
 */
void mb_bench_init(void)
{
    // No initialization needed for POSIX
}

mb_bench_time_t mb_bench_now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

#elif defined(MB_BENCH_WINDOWS)
/**
 * @brief Windows implementation using QueryPerformanceCounter.
 */
static LARGE_INTEGER win_frequency;

void mb_bench_init(void)
{
    QueryPerformanceFrequency(&win_frequency);
}

mb_bench_time_t mb_bench_now(void)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    // Convert to nanoseconds
    return (uint64_t)((counter.QuadPart * 1000000000ULL) / win_frequency.QuadPart);
}

#elif defined(MB_BENCH_CORTEX_M)
/**
 * @brief ARM Cortex-M implementation using DWT cycle counter.
 *
 * Requires:
 * - CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
 * - DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
 */

// DWT registers (Data Watchpoint and Trace)
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004)
#define DEM_CR     (*(volatile uint32_t *)0xE000EDFC)
#define DEM_CR_TRCENA (1 << 24)
#define DWT_CTRL_CYCCNTENA (1 << 0)

void mb_bench_init(void)
{
    // Enable DWT
    DEM_CR |= DEM_CR_TRCENA;
    DWT_CYCCNT = 0;
    DWT_CTRL |= DWT_CTRL_CYCCNTENA;
}

mb_bench_time_t mb_bench_now(void)
{
    return (mb_bench_time_t)DWT_CYCCNT;
}

#elif defined(MB_BENCH_RISCV)
/**
 * @brief RISC-V implementation using mcycle CSR.
 */
static inline uint64_t read_mcycle(void)
{
#if __riscv_xlen == 64
    uint64_t cycles;
    asm volatile("csrr %0, mcycle" : "=r"(cycles));
    return cycles;
#else
    uint32_t lo, hi, hi2;
    do {
        asm volatile("csrr %0, mcycleh" : "=r"(hi));
        asm volatile("csrr %0, mcycle" : "=r"(lo));
        asm volatile("csrr %0, mcycleh" : "=r"(hi2));
    } while (hi != hi2);
    return ((uint64_t)hi << 32) | lo;
#endif
}

void mb_bench_init(void)
{
    // No special init needed
}

mb_bench_time_t mb_bench_now(void)
{
    return read_mcycle();
}

#else
/**
 * @brief Generic fallback (low precision, uses standard C library).
 */
#include <time.h>

void mb_bench_init(void)
{
    // No initialization
}

mb_bench_time_t mb_bench_now(void)
{
    // Fallback to microsecond precision
    return (mb_bench_time_t)clock() * 1000ULL;
}
#endif

/* ========================================================================== */
/*                           Statistics Calculation                           */
/* ========================================================================== */

static mb_bench_stats_t bench_suite[MB_BENCH_MAX_SUITES];
static size_t bench_count = 0;

/**
 * @brief Comparison function for qsort (ascending order).
 */
static int compare_uint64(const void *a, const void *b)
{
    uint64_t va = *(const uint64_t *)a;
    uint64_t vb = *(const uint64_t *)b;
    if (va < vb) return -1;
    if (va > vb) return 1;
    return 0;
}

/**
 * @brief Calculate percentile from sorted array.
 */
static uint64_t calculate_percentile(uint64_t *sorted, size_t n, double p)
{
    if (n == 0) return 0;
    if (n == 1) return sorted[0];
    
    double index = p * (n - 1);
    size_t lower = (size_t)index;
    size_t upper = lower + 1;
    
    if (upper >= n) {
        return sorted[n - 1];
    }
    
    double weight = index - lower;
    return (uint64_t)(sorted[lower] * (1.0 - weight) + sorted[upper] * weight);
}

/* ========================================================================== */
/*                           Benchmark Execution                              */
/* ========================================================================== */

int mb_bench_run(mb_bench_t *bench, mb_bench_stats_t *stats)
{
    if (bench == NULL || stats == NULL || bench->run == NULL) {
        return -1;
    }

    // Clear stats
    memset(stats, 0, sizeof(*stats));
    stats->name = bench->name;
    stats->iterations = bench->iterations;
    stats->budget_ns = bench->budget_ns;

    // Allocate samples array (limited to MB_BENCH_MAX_SAMPLES)
    size_t sample_count = (bench->iterations > MB_BENCH_MAX_SAMPLES) 
                          ? MB_BENCH_MAX_SAMPLES 
                          : bench->iterations;
    uint64_t *samples = (uint64_t *)malloc(sample_count * sizeof(uint64_t));
    if (samples == NULL) {
        fprintf(stderr, "Failed to allocate samples array\n");
        return -1;
    }

    // Setup
    if (bench->setup) {
        bench->setup(bench->user_data);
    }

    // Warmup iterations (not measured)
    for (uint64_t i = 0; i < bench->warmup_iters; i++) {
        bench->run(bench->user_data);
    }

    // Measured iterations
    stats->min_ns = UINT64_MAX;
    stats->max_ns = 0;
    uint64_t sum = 0;
    mb_bench_time_t total_start = mb_bench_now();

    for (uint64_t i = 0; i < bench->iterations; i++) {
        mb_bench_time_t start = mb_bench_now();
        bench->run(bench->user_data);
        mb_bench_time_t end = mb_bench_now();
        
        uint64_t elapsed = mb_bench_elapsed(start, end);
        
        if (elapsed < stats->min_ns) stats->min_ns = elapsed;
        if (elapsed > stats->max_ns) stats->max_ns = elapsed;
        sum += elapsed;
        
        // Store sample if within limit
        if (i < sample_count) {
            samples[i] = elapsed;
        }
    }

    mb_bench_time_t total_end = mb_bench_now();
    stats->total_ns = mb_bench_elapsed(total_start, total_end);
    stats->avg_ns = sum / bench->iterations;

    // Calculate percentiles
    qsort(samples, sample_count, sizeof(uint64_t), compare_uint64);
    stats->p50_ns = calculate_percentile(samples, sample_count, 0.50);
    stats->p95_ns = calculate_percentile(samples, sample_count, 0.95);
    stats->p99_ns = calculate_percentile(samples, sample_count, 0.99);

    free(samples);

    // Teardown
    if (bench->teardown) {
        bench->teardown(bench->user_data);
    }

    // Check budget
    if (bench->budget_ns > 0) {
        stats->passed = (stats->avg_ns <= bench->budget_ns);
    } else {
        stats->passed = true;
    }

    return 0;
}

int mb_bench_register(mb_bench_t *bench)
{
    if (bench_count >= MB_BENCH_MAX_SUITES) {
        fprintf(stderr, "Benchmark suite is full (max %d)\n", MB_BENCH_MAX_SUITES);
        return -1;
    }

    // Run and store stats
    int ret = mb_bench_run(bench, &bench_suite[bench_count]);
    if (ret == 0) {
        bench_count++;
    }
    return ret;
}

int mb_bench_run_all(void)
{
    int failures = 0;
    for (size_t i = 0; i < bench_count; i++) {
        if (!bench_suite[i].passed) {
            failures++;
        }
    }
    return failures;
}

/* ========================================================================== */
/*                              Output/Reporting                              */
/* ========================================================================== */

/**
 * @brief Format time value with appropriate unit (ns, µs, ms, s).
 */
static void format_time(char *buf, size_t bufsize, uint64_t ns)
{
    if (ns < 1000) {
        snprintf(buf, bufsize, "%lu ns", (unsigned long)ns);
    } else if (ns < 1000000) {
        snprintf(buf, bufsize, "%.2f µs", ns / 1000.0);
    } else if (ns < 1000000000) {
        snprintf(buf, bufsize, "%.2f ms", ns / 1000000.0);
    } else {
        snprintf(buf, bufsize, "%.2f s", ns / 1000000000.0);
    }
}

void mb_bench_print_stats(const mb_bench_stats_t *stats)
{
    char min_str[32], max_str[32], avg_str[32], p50_str[32], p95_str[32], p99_str[32];
    
    format_time(min_str, sizeof(min_str), stats->min_ns);
    format_time(max_str, sizeof(max_str), stats->max_ns);
    format_time(avg_str, sizeof(avg_str), stats->avg_ns);
    format_time(p50_str, sizeof(p50_str), stats->p50_ns);
    format_time(p95_str, sizeof(p95_str), stats->p95_ns);
    format_time(p99_str, sizeof(p99_str), stats->p99_ns);

    printf("  %-30s: %10s  [min: %s, max: %s, p95: %s]  %s",
           stats->name,
           avg_str,
           min_str,
           max_str,
           p95_str,
           stats->passed ? "✅ PASS" : "❌ FAIL");

    if (!stats->passed && stats->budget_ns > 0) {
        char budget_str[32];
        format_time(budget_str, sizeof(budget_str), stats->budget_ns);
        printf(" (budget: %s)", budget_str);
    }
    
    printf("\n");
}

void mb_bench_print_summary(void)
{
    printf("\n=== Benchmark Summary ===\n");
    printf("Total benchmarks: %zu\n", bench_count);
    
    size_t passed = 0, failed = 0;
    for (size_t i = 0; i < bench_count; i++) {
        if (bench_suite[i].passed) {
            passed++;
        } else {
            failed++;
        }
    }
    
    printf("Passed: %zu ✅\n", passed);
    printf("Failed: %zu\n", failed);
    
    if (failed == 0) {
        printf("\nAll performance budgets met! 🎉\n");
    } else {
        printf("\n⚠️  %zu benchmark(s) exceeded performance budget!\n", failed);
    }
}

int mb_bench_export_json(const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        fprintf(stderr, "Failed to open %s for writing\n", filename);
        return -1;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"benchmarks\": [\n");
    
    for (size_t i = 0; i < bench_count; i++) {
        const mb_bench_stats_t *s = &bench_suite[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", s->name);
        fprintf(f, "      \"iterations\": %lu,\n", (unsigned long)s->iterations);
        fprintf(f, "      \"min_ns\": %lu,\n", (unsigned long)s->min_ns);
        fprintf(f, "      \"max_ns\": %lu,\n", (unsigned long)s->max_ns);
        fprintf(f, "      \"avg_ns\": %lu,\n", (unsigned long)s->avg_ns);
        fprintf(f, "      \"p50_ns\": %lu,\n", (unsigned long)s->p50_ns);
        fprintf(f, "      \"p95_ns\": %lu,\n", (unsigned long)s->p95_ns);
        fprintf(f, "      \"p99_ns\": %lu,\n", (unsigned long)s->p99_ns);
        fprintf(f, "      \"budget_ns\": %lu,\n", (unsigned long)s->budget_ns);
        fprintf(f, "      \"passed\": %s\n", s->passed ? "true" : "false");
        fprintf(f, "    }%s\n", (i < bench_count - 1) ? "," : "");
    }
    
    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    
    fclose(f);
    return 0;
}

const mb_bench_stats_t *mb_bench_get_stats(size_t index)
{
    if (index >= bench_count) {
        return NULL;
    }
    return &bench_suite[index];
}

size_t mb_bench_get_count(void)
{
    return bench_count;
}
