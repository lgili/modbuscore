/**
 * @file bench_crc.c
 * @brief CRC16 benchmarks for various payload sizes.
 */

#include "bench_common.h"
#include <modbus/utils.h>
#include <string.h>

/* ========================================================================== */
/*                              Test Data                                     */
/* ========================================================================== */

static uint8_t test_data_16[16];
static uint8_t test_data_64[64];
static uint8_t test_data_256[256];

/**
 * @brief Initialize test data with pseudo-random pattern.
 */
static void init_test_data(void)
{
    // Fill with pseudo-random pattern
    for (size_t i = 0; i < 16; i++) {
        test_data_16[i] = (uint8_t)(i * 17 + 42);
    }
    for (size_t i = 0; i < 64; i++) {
        test_data_64[i] = (uint8_t)(i * 13 + 37);
    }
    for (size_t i = 0; i < 256; i++) {
        test_data_256[i] = (uint8_t)(i * 7 + 23);
    }
}

/* ========================================================================== */
/*                          CRC16 Benchmarks                                  */
/* ========================================================================== */

/**
 * @brief CRC16 on 16-byte payload.
 */
static void bench_crc16_16bytes_run(void *user_data)
{
    (void)user_data;
    volatile uint16_t crc = modbus_calculate_crc(test_data_16, 16);
    MB_BENCH_DONT_OPTIMIZE(crc);
}

/**
 * @brief CRC16 on 64-byte payload.
 */
static void bench_crc16_64bytes_run(void *user_data)
{
    (void)user_data;
    volatile uint16_t crc = modbus_calculate_crc(test_data_64, 64);
    MB_BENCH_DONT_OPTIMIZE(crc);
}

/**
 * @brief CRC16 on 256-byte payload.
 */
static void bench_crc16_256bytes_run(void *user_data)
{
    (void)user_data;
    volatile uint16_t crc = modbus_calculate_crc(test_data_256, 256);
    MB_BENCH_DONT_OPTIMIZE(crc);
}

/* ========================================================================== */
/*                         Benchmark Registration                             */
/* ========================================================================== */

void bench_crc_register(void)
{
    // Initialize test data once
    init_test_data();

    // CRC16 - 16 bytes (typical small Modbus frame)
    static mb_bench_t bench_crc16_16bytes = {
        .name = "bench_crc16_16bytes",
        .setup = NULL,
        .run = bench_crc16_16bytes_run,
        .teardown = NULL,
        .user_data = NULL,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 2000,  // 2µs budget for 16 bytes
    };
    mb_bench_register(&bench_crc16_16bytes);

    // CRC16 - 64 bytes (medium frame)
    static mb_bench_t bench_crc16_64bytes = {
        .name = "bench_crc16_64bytes",
        .setup = NULL,
        .run = bench_crc16_64bytes_run,
        .teardown = NULL,
        .user_data = NULL,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 8000,  // 8µs budget for 64 bytes
    };
    mb_bench_register(&bench_crc16_64bytes);

    // CRC16 - 256 bytes (max Modbus PDU)
    static mb_bench_t bench_crc16_256bytes = {
        .name = "bench_crc16_256bytes",
        .setup = NULL,
        .run = bench_crc16_256bytes_run,
        .teardown = NULL,
        .user_data = NULL,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 50000,  // 50µs budget for 256 bytes
    };
    mb_bench_register(&bench_crc16_256bytes);
}
