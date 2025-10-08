/**
 * @file bench_baseline.c
 * @brief Baseline benchmarks to measure framework overhead.
 */

#include "bench_common.h"
#include <stdio.h>

/* ========================================================================== */
/*                           Baseline Benchmarks                              */
/* ========================================================================== */

/**
 * @brief No-op benchmark (measures pure framework overhead).
 */
static void bench_noop_run(void *user_data)
{
    (void)user_data;
    MB_BENCH_DONT_OPTIMIZE(user_data);
}

/**
 * @brief Inline no-op (compiler might optimize differently).
 */
static inline void inline_noop(void)
{
    MB_BENCH_BARRIER();
}

static void bench_inline_noop_run(void *user_data)
{
    (void)user_data;
    inline_noop();
}

/**
 * @brief Memory read benchmark.
 */
static uint8_t test_array[256];

static void bench_memory_read_run(void *user_data)
{
    (void)user_data;
    volatile uint8_t val = test_array[42];
    MB_BENCH_DONT_OPTIMIZE(val);
}

/**
 * @brief Memory write benchmark.
 */
static void bench_memory_write_run(void *user_data)
{
    (void)user_data;
    test_array[42] = 0xAA;
    MB_BENCH_BARRIER();
}

/**
 * @brief Function call overhead.
 */
static int dummy_function(int x)
{
    return x + 1;
}

static void bench_function_call_run(void *user_data)
{
    (void)user_data;
    volatile int result = dummy_function(42);
    MB_BENCH_DONT_OPTIMIZE(result);
}

/* ========================================================================== */
/*                         Benchmark Registration                             */
/* ========================================================================== */

void bench_baseline_register(void)
{
    // No-op: pure framework overhead
    static mb_bench_t bench_noop = {
        .name = "bench_noop",
        .setup = NULL,
        .run = bench_noop_run,
        .teardown = NULL,
        .user_data = NULL,
        .iterations = 1000000,
        .warmup_iters = 1000,
        .budget_ns = 0,  // No budget for baseline
    };
    mb_bench_register(&bench_noop);

    // Inline no-op
    static mb_bench_t bench_inline_noop = {
        .name = "bench_inline_noop",
        .setup = NULL,
        .run = bench_inline_noop_run,
        .teardown = NULL,
        .user_data = NULL,
        .iterations = 1000000,
        .warmup_iters = 1000,
        .budget_ns = 0,
    };
    mb_bench_register(&bench_inline_noop);

    // Memory read
    static mb_bench_t bench_memory_read = {
        .name = "bench_memory_read",
        .setup = NULL,
        .run = bench_memory_read_run,
        .teardown = NULL,
        .user_data = NULL,
        .iterations = 1000000,
        .warmup_iters = 1000,
        .budget_ns = 0,
    };
    mb_bench_register(&bench_memory_read);

    // Memory write
    static mb_bench_t bench_memory_write = {
        .name = "bench_memory_write",
        .setup = NULL,
        .run = bench_memory_write_run,
        .teardown = NULL,
        .user_data = NULL,
        .iterations = 1000000,
        .warmup_iters = 1000,
        .budget_ns = 0,
    };
    mb_bench_register(&bench_memory_write);

    // Function call
    static mb_bench_t bench_function_call = {
        .name = "bench_function_call",
        .setup = NULL,
        .run = bench_function_call_run,
        .teardown = NULL,
        .user_data = NULL,
        .iterations = 1000000,
        .warmup_iters = 1000,
        .budget_ns = 0,
    };
    mb_bench_register(&bench_function_call);
}
