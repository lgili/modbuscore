/**
 * @file bench_poll.c
 * @brief Synthetic benchmarks for tight loop overhead measurements.
 *
 * These benchmarks measure fundamental operations WITHOUT full client/server
 * initialization. They provide insights into computational costs rather than
 * end-to-end transaction overhead.
 */

#include "bench_common.h"
#include <modbus/pdu.h>
#include <modbus/utils.h>
#include <modbus/mb_types.h>
#include <string.h>

/* ========================================================================== */
/*                       PDU Parse + Build Round-Trip                         */
/* ========================================================================== */

static mb_u8 test_pdu[MB_PDU_MAX];
static mb_u16 test_data[100];

/**
 * @brief Benchmark: Build FC03 request + parse response (simulating 10 registers).
 */
static void bench_fc03_roundtrip_run(void *user_data)
{
    (void)user_data;
    
    // Build request: (out, out_cap, start_addr, quantity)
    (void)mb_pdu_build_read_holding_request(test_pdu, MB_PDU_MAX, 0x0000, 10);
    
    // Simulate response: FC (1) + Byte Count (1) + Data (20 bytes) = 22 bytes
    mb_u8 response[32];
    response[0] = 0x03;  // FC03
    response[1] = 20;    // Byte count (10 regs * 2 bytes)
    memset(&response[2], 0xAB, 20);  // Dummy data
    
    // Parse response: (pdu, len, out_payload, out_register_count)
    const mb_u8 *payload;
    mb_u16 reg_count;
    (void)mb_pdu_parse_read_holding_response(response, 22, &payload, &reg_count);
}

/**
 * @brief Benchmark: Build FC16 request (10 registers) + CRC calculation.
 */
static void bench_fc16_with_crc_run(void *user_data)
{
    (void)user_data;
    
    // Initialize test data
    for (unsigned i = 0; i < 10; ++i) {
        test_data[i] = (mb_u16)(0x1234 + i);
    }
    
    // Build FC16 request: (out, out_cap, start_addr, values, count)
    (void)mb_pdu_build_write_multiple_request(test_pdu, MB_PDU_MAX, 0x0000, test_data, 10);
    
    // Calculate CRC (as would be done in RTU encoding)
    // PDU size for FC16: FC(1) + Start(2) + Count(2) + ByteCount(1) + Data(20) = 26 bytes
    (void)modbus_calculate_crc(test_pdu, 26);
}

/* ========================================================================== */
/*                         Memory Copy Benchmarks                             */
/* ========================================================================== */

static mb_u8 src_buffer[256];
static mb_u8 dst_buffer[256];

/**
 * @brief Benchmark: memcpy 64 bytes (typical register transaction).
 */
static void bench_memcpy_64_run(void *user_data)
{
    (void)user_data;
    memcpy(dst_buffer, src_buffer, 64);
    MB_BENCH_DONT_OPTIMIZE(dst_buffer);
}

/**
 * @brief Benchmark: memset 256 bytes (buffer clearing).
 */
static void bench_memset_256_run(void *user_data)
{
    (void)user_data;
    memset(dst_buffer, 0, 256);
    MB_BENCH_DONT_OPTIMIZE(dst_buffer);
}

/* ========================================================================== */
/*                       Bitfield Manipulation                                */
/* ========================================================================== */

static bool coil_array[256];

/**
 * @brief Benchmark: Pack 64 coils into bytes (FC01/FC05).
 */
static void bench_coil_pack_run(void *user_data)
{
    (void)user_data;
    
    mb_u8 packed[8];  // 64 coils = 8 bytes
    for (unsigned i = 0; i < 64; ++i) {
        if (coil_array[i]) {
            packed[i / 8] |= (1u << (i % 8));
        }
    }
    
    MB_BENCH_DONT_OPTIMIZE(packed);
}

/**
 * @brief Benchmark: Unpack 64 coils from bytes.
 */
static void bench_coil_unpack_run(void *user_data)
{
    (void)user_data;
    
    mb_u8 packed[8] = {0xFF, 0xAA, 0x55, 0x00, 0xFF, 0xAA, 0x55, 0x00};
    
    for (unsigned i = 0; i < 64; ++i) {
        coil_array[i] = (packed[i / 8] & (1u << (i % 8))) != 0;
    }
    
    MB_BENCH_DONT_OPTIMIZE(coil_array);
}

/* ========================================================================== */
/*                         Benchmark Registration                             */
/* ========================================================================== */

void bench_poll_register(void)
{
    // PDU round-trip benchmarks
    static mb_bench_t bench_fc03_roundtrip = {
        .name = "bench_fc03_roundtrip",
        .setup = NULL,
        .run = bench_fc03_roundtrip_run,
        .teardown = NULL,
        .user_data = NULL,
        .iterations = 1000000,
        .warmup_iters = 1000,
        .budget_ns = 500,  // 500ns budget for full round-trip
    };
    mb_bench_register(&bench_fc03_roundtrip);

    static mb_bench_t bench_fc16_with_crc = {
        .name = "bench_fc16_with_crc",
        .setup = NULL,
        .run = bench_fc16_with_crc_run,
        .teardown = NULL,
        .user_data = NULL,
        .iterations = 1000000,
        .warmup_iters = 1000,
        .budget_ns = 1500,  // 1.5µs budget (encode + CRC for 10 registers)
    };
    mb_bench_register(&bench_fc16_with_crc);

    // Memory operation benchmarks
    static mb_bench_t bench_memcpy_64 = {
        .name = "bench_memcpy_64",
        .setup = NULL,
        .run = bench_memcpy_64_run,
        .teardown = NULL,
        .user_data = NULL,
        .iterations = 10000000,
        .warmup_iters = 10000,
        .budget_ns = 50,  // 50ns budget for 64-byte copy
    };
    mb_bench_register(&bench_memcpy_64);

    static mb_bench_t bench_memset_256 = {
        .name = "bench_memset_256",
        .setup = NULL,
        .run = bench_memset_256_run,
        .teardown = NULL,
        .user_data = NULL,
        .iterations = 10000000,
        .warmup_iters = 10000,
        .budget_ns = 100,  // 100ns budget for 256-byte clear
    };
    mb_bench_register(&bench_memset_256);

    // Coil/bitfield benchmarks
    static mb_bench_t bench_coil_pack = {
        .name = "bench_coil_pack",
        .setup = NULL,
        .run = bench_coil_pack_run,
        .teardown = NULL,
        .user_data = NULL,
        .iterations = 10000000,
        .warmup_iters = 10000,
        .budget_ns = 150,  // 150ns budget for 64 coils
    };
    mb_bench_register(&bench_coil_pack);

    static mb_bench_t bench_coil_unpack = {
        .name = "bench_coil_unpack",
        .setup = NULL,
        .run = bench_coil_unpack_run,
        .teardown = NULL,
        .user_data = NULL,
        .iterations = 10000000,
        .warmup_iters = 10000,
        .budget_ns = 150,  // 150ns budget for 64 coils
    };
    mb_bench_register(&bench_coil_unpack);
}
