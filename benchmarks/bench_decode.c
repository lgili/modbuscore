/**
 * @file bench_decode.c
 * @brief Decoding benchmarks for Modbus responses.
 */

#include "bench_common.h"
#include <modbus/pdu.h>
#include <string.h>

/* ========================================================================== */
/*                         Test Response Buffers                              */
/* ========================================================================== */

static uint8_t fc03_response_1reg[5];    // FC + ByteCount + 2 bytes
static uint8_t fc03_response_10regs[23]; // FC + ByteCount + 20 bytes
static uint8_t fc03_response_100regs[203]; // FC + ByteCount + 200 bytes
static uint8_t fc16_response[6];         // FC + Addr(2) + Qty(2)
static uint8_t fc05_response[6];         // FC + Addr(2) + Value(2)
static uint8_t fc06_response[6];         // FC + Addr(2) + Value(2)

/**
 * @brief Initialize test response buffers with valid data.
 */
static void init_test_responses(void)
{
    // FC03 - 1 register
    fc03_response_1reg[0] = MB_FC_READ_HOLDING_REGISTERS;
    fc03_response_1reg[1] = 2;  // Byte count
    fc03_response_1reg[2] = 0x12;
    fc03_response_1reg[3] = 0x34;

    // FC03 - 10 registers
    fc03_response_10regs[0] = MB_FC_READ_HOLDING_REGISTERS;
    fc03_response_10regs[1] = 20;  // Byte count
    for (size_t i = 0; i < 10; i++) {
        fc03_response_10regs[2 + i*2] = (uint8_t)((i >> 8) & 0xFF);
        fc03_response_10regs[3 + i*2] = (uint8_t)(i & 0xFF);
    }

    // FC03 - 100 registers
    fc03_response_100regs[0] = MB_FC_READ_HOLDING_REGISTERS;
    fc03_response_100regs[1] = 200;  // Byte count
    for (size_t i = 0; i < 100; i++) {
        fc03_response_100regs[2 + i*2] = (uint8_t)((i >> 8) & 0xFF);
        fc03_response_100regs[3 + i*2] = (uint8_t)(i & 0xFF);
    }

    // FC16 response
    fc16_response[0] = MB_FC_WRITE_MULTIPLE_REGISTERS;
    fc16_response[1] = 0x00;  // Start addr high
    fc16_response[2] = 0x64;  // Start addr low (100)
    fc16_response[3] = 0x00;  // Qty high
    fc16_response[4] = 0x0A;  // Qty low (10)

    // FC05 response
    fc05_response[0] = MB_FC_WRITE_SINGLE_COIL;
    fc05_response[1] = 0x00;  // Addr high
    fc05_response[2] = 0x64;  // Addr low (100)
    fc05_response[3] = 0xFF;  // Value high (ON)
    fc05_response[4] = 0x00;  // Value low

    // FC06 response
    fc06_response[0] = MB_FC_WRITE_SINGLE_REGISTER;
    fc06_response[1] = 0x00;  // Addr high
    fc06_response[2] = 0x64;  // Addr low (100)
    fc06_response[3] = 0x12;  // Value high
    fc06_response[4] = 0x34;  // Value low
}

/* ========================================================================== */
/*                          FC03 Decoding Benchmarks                          */
/* ========================================================================== */

/**
 * @brief Decode FC03 response - 1 register.
 */
static void bench_decode_fc03_1reg_run(void *user_data)
{
    (void)user_data;
    
    mb_pdu_resp_t resp;
    mb_err_t err = mb_pdu_decode_response(fc03_response_1reg, sizeof(fc03_response_1reg), &resp);
    MB_BENCH_DONT_OPTIMIZE(err);
    MB_BENCH_DONT_OPTIMIZE(resp);
}

/**
 * @brief Decode FC03 response - 10 registers.
 */
static void bench_decode_fc03_10regs_run(void *user_data)
{
    (void)user_data;
    
    mb_pdu_resp_t resp;
    mb_err_t err = mb_pdu_decode_response(fc03_response_10regs, sizeof(fc03_response_10regs), &resp);
    MB_BENCH_DONT_OPTIMIZE(err);
    MB_BENCH_DONT_OPTIMIZE(resp);
}

/**
 * @brief Decode FC03 response - 100 registers.
 */
static void bench_decode_fc03_100regs_run(void *user_data)
{
    (void)user_data;
    
    mb_pdu_resp_t resp;
    mb_err_t err = mb_pdu_decode_response(fc03_response_100regs, sizeof(fc03_response_100regs), &resp);
    MB_BENCH_DONT_OPTIMIZE(err);
    MB_BENCH_DONT_OPTIMIZE(resp);
}

/* ========================================================================== */
/*                       Other Function Code Decoding                         */
/* ========================================================================== */

/**
 * @brief Decode FC16 response.
 */
static void bench_decode_fc16_run(void *user_data)
{
    (void)user_data;
    
    mb_pdu_resp_t resp;
    mb_err_t err = mb_pdu_decode_response(fc16_response, sizeof(fc16_response), &resp);
    MB_BENCH_DONT_OPTIMIZE(err);
    MB_BENCH_DONT_OPTIMIZE(resp);
}

/**
 * @brief Decode FC05 response.
 */
static void bench_decode_fc05_run(void *user_data)
{
    (void)user_data;
    
    mb_pdu_resp_t resp;
    mb_err_t err = mb_pdu_decode_response(fc05_response, sizeof(fc05_response), &resp);
    MB_BENCH_DONT_OPTIMIZE(err);
    MB_BENCH_DONT_OPTIMIZE(resp);
}

/**
 * @brief Decode FC06 response.
 */
static void bench_decode_fc06_run(void *user_data)
{
    (void)user_data;
    
    mb_pdu_resp_t resp;
    mb_err_t err = mb_pdu_decode_response(fc06_response, sizeof(fc06_response), &resp);
    MB_BENCH_DONT_OPTIMIZE(err);
    MB_BENCH_DONT_OPTIMIZE(resp);
}

/* ========================================================================== */
/*                         Benchmark Registration                             */
/* ========================================================================== */

void bench_decode_register(void)
{
    // Initialize test responses
    init_test_responses();

    // FC03 - Read Holding Registers responses
    static mb_bench_t bench_decode_fc03_1reg = {
        .name = "bench_decode_fc03_1reg",
        .run = bench_decode_fc03_1reg_run,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 300,  // 300ns budget
    };
    mb_bench_register(&bench_decode_fc03_1reg);

    static mb_bench_t bench_decode_fc03_10regs = {
        .name = "bench_decode_fc03_10regs",
        .run = bench_decode_fc03_10regs_run,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 500,  // 500ns budget
    };
    mb_bench_register(&bench_decode_fc03_10regs);

    static mb_bench_t bench_decode_fc03_100regs = {
        .name = "bench_decode_fc03_100regs",
        .run = bench_decode_fc03_100regs_run,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 1000,  // 1µs budget
    };
    mb_bench_register(&bench_decode_fc03_100regs);

    // Other function codes
    static mb_bench_t bench_decode_fc16 = {
        .name = "bench_decode_fc16",
        .run = bench_decode_fc16_run,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 200,  // 200ns budget
    };
    mb_bench_register(&bench_decode_fc16);

    static mb_bench_t bench_decode_fc05 = {
        .name = "bench_decode_fc05",
        .run = bench_decode_fc05_run,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 200,  // 200ns budget
    };
    mb_bench_register(&bench_decode_fc05);

    static mb_bench_t bench_decode_fc06 = {
        .name = "bench_decode_fc06",
        .run = bench_decode_fc06_run,
        .iterations = 100000,
        .warmup_iters = 1000,
        .budget_ns = 200,  // 200ns budget
    };
    mb_bench_register(&bench_decode_fc06);
}
