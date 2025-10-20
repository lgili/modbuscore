/**
 * @file fuzz_pdu_parser.c
 * @brief LibFuzzer harness for Modbus PDU parsing functions.
 *
 * This fuzzer tests all PDU encoding/decoding and parsing functions:
 * - mbc_pdu_encode() / mbc_pdu_decode()
 * - FC03: Read Holding Registers (request/response)
 * - FC06: Write Single Register (request/response)
 * - FC16: Write Multiple Registers (request/response)
 * - Exception response parsing
 *
 * Expected behavior:
 * - No crashes, hangs, or memory violations
 * - Graceful error handling for malformed PDUs
 * - Consistent return codes
 */

#include <modbuscore/protocol/pdu.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief LibFuzzer entry point.
 *
 * @param data Fuzzer-provided input buffer
 * @param size Size of input buffer
 * @return Always 0
 */
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    /* Minimum PDU size is 2 bytes: [function] [data...] */
    if (size < 2) {
        return 0;
    }

    /* Test 1: Generic PDU decode */
    mbc_pdu_t pdu = {0};
    mbc_status_t status = mbc_pdu_decode(data, size, &pdu);

    if (status != MBC_STATUS_OK) {
        /* Decoding failed - this is expected for malformed input */
        return 0;
    }

    /* Test 2: Re-encode the decoded PDU and verify consistency */
    uint8_t encoded[MBC_PDU_MAX + 2];
    size_t encoded_len = 0;
    status = mbc_pdu_encode(&pdu, encoded, sizeof(encoded), &encoded_len);

    if (status == MBC_STATUS_OK && encoded_len > 0) {
        /* Verify encoded data is within bounds */
        volatile uint8_t first = encoded[0];
        volatile uint8_t last = encoded[encoded_len - 1];
        (void)first;
        (void)last;
    }

    /* Test 3: Function-specific parsers based on function code */
    uint8_t function = pdu.function;

    if (function == 0x03) {
        /* FC03: Read Holding Registers response */
        const uint8_t* reg_data = NULL;
        size_t reg_count = 0;
        (void)mbc_pdu_parse_read_holding_response(&pdu, &reg_data, &reg_count);
    } else if (function == 0x06) {
        /* FC06: Write Single Register response */
        uint16_t address = 0;
        uint16_t value = 0;
        (void)mbc_pdu_parse_write_single_response(&pdu, &address, &value);
    } else if (function == 0x10) {
        /* FC16 (0x10): Write Multiple Registers response */
        uint16_t address = 0;
        uint16_t quantity = 0;
        (void)mbc_pdu_parse_write_multiple_response(&pdu, &address, &quantity);
    } else if (function >= 0x80) {
        /* Exception response (function code with bit 0x80 set) */
        uint8_t orig_function = 0;
        uint8_t exception_code = 0;
        (void)mbc_pdu_parse_exception(&pdu, &orig_function, &exception_code);
    }

    /* Test 4: Build and parse FC03 request (if input suggests it) */
    if (size >= 5 && data[0] == 0x03) {
        uint16_t address = (uint16_t)((data[1] << 8) | data[2]);
        uint16_t quantity = (uint16_t)((data[3] << 8) | data[4]);

        mbc_pdu_t request = {0};
        status = mbc_pdu_build_read_holding_request(&request, 1, address, quantity);

        if (status == MBC_STATUS_OK) {
            /* Encode and verify */
            uint8_t buf[256];
            size_t len = 0;
            (void)mbc_pdu_encode(&request, buf, sizeof(buf), &len);
        }
    }

    /* Test 5: Build and parse FC06 request */
    if (size >= 5 && data[0] == 0x06) {
        uint16_t address = (uint16_t)((data[1] << 8) | data[2]);
        uint16_t value = (uint16_t)((data[3] << 8) | data[4]);

        mbc_pdu_t request = {0};
        status = mbc_pdu_build_write_single_register(&request, 1, address, value);

        if (status == MBC_STATUS_OK) {
            uint8_t buf[256];
            size_t len = 0;
            (void)mbc_pdu_encode(&request, buf, sizeof(buf), &len);
        }
    }

    /* Test 6: Build and parse FC16 request */
    if (size >= 6 && data[0] == 0x10) {
        uint16_t address = (uint16_t)((data[1] << 8) | data[2]);
        uint16_t quantity = (data[3] % 123) + 1; /* Limit to valid range 1-123 */

        /* Extract register values from fuzzer input */
        uint16_t values[123];
        size_t available_values = (size - 4) / 2;
        if (available_values > quantity) {
            available_values = quantity;
        }

        for (size_t i = 0; i < available_values; ++i) {
            size_t offset = 4 + i * 2;
            if (offset + 1 < size) {
                values[i] = (uint16_t)((data[offset] << 8) | data[offset + 1]);
            } else {
                values[i] = 0;
            }
        }

        mbc_pdu_t request = {0};
        status =
            mbc_pdu_build_write_multiple_registers(&request, 1, address, values, available_values);

        if (status == MBC_STATUS_OK) {
            uint8_t buf[256];
            size_t len = 0;
            (void)mbc_pdu_encode(&request, buf, sizeof(buf), &len);
        }
    }

    /* Test 7: Null pointer handling */
    (void)mbc_pdu_decode(data, size, NULL);
    (void)mbc_pdu_encode(NULL, encoded, sizeof(encoded), NULL);

    return 0;
}
