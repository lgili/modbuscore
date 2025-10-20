/**
 * @file fuzz_mbap_decoder.c
 * @brief LibFuzzer harness for MBAP (Modbus TCP) frame decoder.
 *
 * This fuzzer tests the robustness of mbc_mbap_decode() and mbc_mbap_expected_length()
 * against malformed, truncated, and malicious MBAP frames.
 *
 * Target functions:
 * - mbc_mbap_decode(): Decodes complete MBAP header + PDU
 * - mbc_mbap_expected_length(): Determines expected frame length from partial data
 *
 * Expected behavior:
 * - No crashes, hangs, or memory errors
 * - Graceful error handling for all invalid inputs
 * - Consistent return codes (MBC_STATUS_*)
 */

#include <modbuscore/protocol/mbap.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief LibFuzzer entry point.
 *
 * @param data Fuzzer-provided input buffer
 * @param size Size of input buffer (0 to ~4KB typically)
 * @return Always 0 (non-zero would signal corpus rejection)
 */
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Test 1: mbc_mbap_expected_length() with arbitrary partial frames */
    if (size > 0) {
        size_t expected = mbc_mbap_expected_length(data, size);
        /* Expected: 0 if insufficient data, or valid frame length (7-260) */
        (void)expected; /* Suppress unused warning */
    }

    /* Test 2: mbc_mbap_decode() with various frame sizes */
    if (size >= MBC_MBAP_HEADER_SIZE) {
        mbc_mbap_header_t header = {0};
        const uint8_t* pdu = NULL;
        size_t pdu_length = 0;

        mbc_status_t status = mbc_mbap_decode(data, size, &header, &pdu, &pdu_length);

        /* Validate return code consistency */
        if (status == MBC_STATUS_OK) {
            /* On success, PDU pointer must be valid and within bounds */
            if (pdu != NULL && pdu_length > 0) {
                /* Verify pdu points within [data, data+size) */
                const uint8_t* frame_end = data + size;
                if (pdu >= data && pdu < frame_end && (pdu + pdu_length) <= frame_end) {
                    /* Valid pointer range - access first/last byte to detect ASAN issues */
                    volatile uint8_t first = pdu[0];
                    volatile uint8_t last = pdu[pdu_length - 1];
                    (void)first;
                    (void)last;
                }
            }
        }
    }

    /* Test 3: Edge case - exact header size (7 bytes, no PDU) */
    if (size == MBC_MBAP_HEADER_SIZE) {
        mbc_mbap_header_t header = {0};
        const uint8_t* pdu = NULL;
        size_t pdu_length = 0;
        (void)mbc_mbap_decode(data, size, &header, &pdu, &pdu_length);
    }

    /* Test 4: Maximum valid frame size (260 bytes) */
    if (size > MBC_MBAP_MAX_FRAME_SIZE) {
        mbc_mbap_header_t header = {0};
        const uint8_t* pdu = NULL;
        size_t pdu_length = 0;
        /* Should reject oversized frames gracefully */
        (void)mbc_mbap_decode(data, MBC_MBAP_MAX_FRAME_SIZE + 1, &header, &pdu, &pdu_length);
    }

    /* Test 5: Null pointer handling (should not crash) */
    if (size >= MBC_MBAP_HEADER_SIZE) {
        (void)mbc_mbap_decode(data, size, NULL, NULL, NULL);
    }

    return 0;
}
