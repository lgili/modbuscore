/**
 * @file bench_encode.c
 * @brief Encoding benchmarks for Modbus function codes.
 */

#include "bench_common.h"
#include <modbus/pdu.h>
#include <modbus/mb_types.h>
#include <string.h>

/* ========================================================================== */
/*                              Test Buffers                                  */
/* ========================================================================== */

static mb_u8 tx_buffer[MB_PDU_MAX];
static mb_u16 test_registers[100];

/**
 * @brief Initialize test data.
 */
static void init_test_data(void)
{
    for (size_t i = 0; i < 100; i++) {
        test_registers[i] = (uint16_t)(i * 10 + 1000);
    }
}

/* ========================================================================== */
/*                          FC03 Encoding Benchmarks                          */
/* ========================================================================== */

/**
 * @brief Encode FC03 Read Holding Registers - 1 register.
 */
static void bench_encode_fc03_1reg_run(void *user_data)
{
    (void)user_data;
    
    mb_err_t err = mb_pdu_build_read_holding_request(tx_buffer, sizeof(tx_buffer), 100, 1);
    MB_BENCH_DONT_OPTIMIZE(err);
}

/**
 * @brief Encode FC03 Read Holding Registers - 10 registers.
 */
static void bench_encode_fc03_10regs_run(void *user_data)
{
    (void)user_data;
    
    mb_err_t err = mb_pdu_build_read_holding_request(tx_buffer, sizeof(tx_buffer), 100, 10);
    MB_BENCH_DONT_OPTIMIZE(err);
}

/**
 * @brief Encode FC03 Read Holding Registers - 100 registers.
 */
static void bench_encode_fc03_100regs_run(void *user_data)
{
    (void)user_data;
    
    mb_err_t err = mb_pdu_build_read_holding_request(tx_buffer, sizeof(tx_buffer), 100, 100);
    MB_BENCH_DONT_OPTIMIZE(err);
}

/* ========================================================================== */
/*                          FC16 Encoding Benchmarks                          */
/* ========================================================================== */

/**
 * @brief Encode FC16 Write Multiple Registers - 1 register.
 */
static void bench_encode_fc16_1reg_run(void *user_data)
{
    (void)user_data;
    
    mb_err_t err = mb_pdu_build_write_multiple_request(tx_buffer, sizeof(tx_buffer), 100, test_registers, 1);
    MB_BENCH_DONT_OPTIMIZE(err);
}

/**
 * @brief Encode FC16 Write Multiple Registers - 10 registers.
 */
static void bench_encode_fc16_10regs_run(void *user_data)
{
    (void)user_data;
    
    mb_err_t err = mb_pdu_build_write_multiple_request(tx_buffer, sizeof(tx_buffer), 100, test_registers, 10);
    MB_BENCH_DONT_OPTIMIZE(err);
}

/**
 * @brief Encode FC16 Write Multiple Registers - 100 registers.
 */
static void bench_encode_fc16_100regs_run(void *user_data)
{
    (void)user_data;
    
    mb_err_t err = mb_pdu_build_write_multiple_request(tx_buffer, sizeof(tx_buffer), 100, test_registers, 100);
    MB_BENCH_DONT_OPTIMIZE(err);
}

/* ========================================================================== */
/*                      Other Function Codes                                  */
/* ========================================================================== */

/**
 * @brief Encode FC05 Write Single Coil.
 */
static void bench_encode_fc05_run(void *user_data)
{
    (void)user_data;
    
    mb_err_t err = mb_pdu_build_write_single_coil_request(tx_buffer, sizeof(tx_buffer), 100, true);
    MB_BENCH_DONT_OPTIMIZE(err);
}

/**
 * @brief Encode FC06 Write Single Register.
 */
static void bench_encode_fc06_run(void *user_data)
{
    (void)user_data;
    
    mb_err_t err = mb_pdu_build_write_single_request(tx_buffer, sizeof(tx_buffer), 100, 0x1234);
    MB_BENCH_DONT_OPTIMIZE(err);
}

/* ========================================================================== */
/*                         Benchmark Registration                             */
/* ========================================================================== */

void bench_encode_register(void)
{
    // Initialize test data
    init_test_data();

    // FC03 - Read Holding Registers
    static mb_bench_t bench_encode_fc03_1reg = {
        .name = "bench_encode_fc03_1reg",
        .run = bench_encode_fc03_1reg_run,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 300,  // 300ns budget
    };
    mb_bench_register(&bench_encode_fc03_1reg);

    static mb_bench_t bench_encode_fc03_10regs = {
        .name = "bench_encode_fc03_10regs",
        .run = bench_encode_fc03_10regs_run,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 500,  // 500ns budget
    };
    mb_bench_register(&bench_encode_fc03_10regs);

    static mb_bench_t bench_encode_fc03_100regs = {
        .name = "bench_encode_fc03_100regs",
        .run = bench_encode_fc03_100regs_run,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 800,  // 800ns budget
    };
    mb_bench_register(&bench_encode_fc03_100regs);

    // FC16 - Write Multiple Registers
    static mb_bench_t bench_encode_fc16_1reg = {
        .name = "bench_encode_fc16_1reg",
        .run = bench_encode_fc16_1reg_run,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 400,  // 400ns budget
    };
    mb_bench_register(&bench_encode_fc16_1reg);

    static mb_bench_t bench_encode_fc16_10regs = {
        .name = "bench_encode_fc16_10regs",
        .run = bench_encode_fc16_10regs_run,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 800,  // 800ns budget
    };
    mb_bench_register(&bench_encode_fc16_10regs);

    static mb_bench_t bench_encode_fc16_100regs = {
        .name = "bench_encode_fc16_100regs",
        .run = bench_encode_fc16_100regs_run,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 5000,  // 5µs budget (lots of data to copy)
    };
    mb_bench_register(&bench_encode_fc16_100regs);

    // Simple function codes
    static mb_bench_t bench_encode_fc05 = {
        .name = "bench_encode_fc05",
        .run = bench_encode_fc05_run,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 200,  // 200ns budget
    };
    mb_bench_register(&bench_encode_fc05);

    static mb_bench_t bench_encode_fc06 = {
        .name = "bench_encode_fc06",
        .run = bench_encode_fc06_run,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 200,  // 200ns budget
    };
    mb_bench_register(&bench_encode_fc06);
}
