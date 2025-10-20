/**
 * @file generate_corpus.c
 * @brief Generate seed corpus files for fuzzing.
 *
 * This program creates valid Modbus frames (MBAP, RTU, PDU) to seed the fuzzer.
 * A good seed corpus dramatically improves fuzzer efficiency by starting from
 * valid inputs and mutating them.
 *
 * Usage: ./generate_corpus
 * Output: Multiple .bin files in current directory
 */

#include <modbuscore/protocol/crc.h>
#include <modbuscore/protocol/mbap.h>
#include <modbuscore/protocol/pdu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_file(const char* filename, const uint8_t* data, size_t size)
{
    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Failed to create %s\n", filename);
        return;
    }
    fwrite(data, 1, size, f);
    fclose(f);
    printf("Created %s (%zu bytes)\n", filename, size);
}

int main(void)
{
    printf("=== Generating Modbus Fuzzing Corpus ===\n\n");

    /* --- MBAP Corpus --- */
    printf("Generating MBAP corpus...\n");

    /* MBAP 1: FC03 Read Holding request */
    {
        mbc_pdu_t pdu = {0};
        mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);

        uint8_t pdu_buf[256];
        pdu_buf[0] = pdu.function;
        memcpy(&pdu_buf[1], pdu.payload, pdu.payload_length);
        size_t pdu_len = 1 + pdu.payload_length;

        mbc_mbap_header_t header = {
            .transaction_id = 1,
            .protocol_id = 0,
            .length = (uint16_t)(pdu_len + 1),
            .unit_id = 1,
        };

        uint8_t frame[260];
        size_t frame_len = 0;
        mbc_mbap_encode(&header, pdu_buf, pdu_len, frame, sizeof(frame), &frame_len);
        write_file("mbap_fc03_request.bin", frame, frame_len);
    }

    /* MBAP 2: FC03 Read Holding response */
    {
        uint8_t frame[] = {
            0x00, 0x01, /* Transaction ID */
            0x00, 0x00, /* Protocol ID */
            0x00, 0x07, /* Length (7 bytes) */
            0x01,       /* Unit ID */
            0x03,       /* Function code */
            0x04,       /* Byte count (4 bytes = 2 registers) */
            0x12, 0x34, /* Register 1 value */
            0x56, 0x78  /* Register 2 value */
        };
        write_file("mbap_fc03_response.bin", frame, sizeof(frame));
    }

    /* MBAP 3: FC06 Write Single Register request */
    {
        mbc_pdu_t pdu = {0};
        mbc_pdu_build_write_single_register(&pdu, 1, 100, 0xABCD);

        uint8_t pdu_buf[256];
        pdu_buf[0] = pdu.function;
        memcpy(&pdu_buf[1], pdu.payload, pdu.payload_length);
        size_t pdu_len = 1 + pdu.payload_length;

        mbc_mbap_header_t header = {
            .transaction_id = 2,
            .protocol_id = 0,
            .length = (uint16_t)(pdu_len + 1),
            .unit_id = 1,
        };

        uint8_t frame[260];
        size_t frame_len = 0;
        mbc_mbap_encode(&header, pdu_buf, pdu_len, frame, sizeof(frame), &frame_len);
        write_file("mbap_fc06_request.bin", frame, frame_len);
    }

    /* MBAP 4: Exception response */
    {
        uint8_t frame[] = {
            0x00, 0x03, /* Transaction ID */
            0x00, 0x00, /* Protocol ID */
            0x00, 0x03, /* Length (3 bytes) */
            0x01,       /* Unit ID */
            0x83,       /* Exception: FC03 + 0x80 */
            0x02        /* Exception code: ILLEGAL_DATA_ADDRESS */
        };
        write_file("mbap_exception.bin", frame, sizeof(frame));
    }

    /* MBAP 5: Minimum valid frame (header only, empty PDU) */
    {
        uint8_t frame[] = {
            0x00, 0x00, /* Transaction ID */
            0x00, 0x00, /* Protocol ID */
            0x00, 0x01, /* Length (1 byte: just unit ID) */
            0xFF        /* Unit ID */
        };
        write_file("mbap_minimal.bin", frame, sizeof(frame));
    }

    /* --- RTU/CRC Corpus --- */
    printf("\nGenerating RTU/CRC corpus...\n");

    /* RTU 1: FC03 Read Holding request with CRC */
    {
        uint8_t frame[8] = {
            0x01,       /* Unit ID */
            0x03,       /* Function code */
            0x00, 0x00, /* Start address */
            0x00, 0x0A, /* Quantity: 10 registers */
            0x00, 0x00  /* CRC placeholder */
        };
        uint16_t crc = mbc_crc16(frame, 6);
        frame[6] = (uint8_t)(crc & 0xFF);
        frame[7] = (uint8_t)((crc >> 8) & 0xFF);
        write_file("rtu_fc03_request.bin", frame, 8);
    }

    /* RTU 2: FC03 response with CRC */
    {
        uint8_t frame[9] = {
            0x01,       /* Unit ID */
            0x03,       /* Function code */
            0x04,       /* Byte count */
            0x12, 0x34, /* Register 1 */
            0x56, 0x78, /* Register 2 */
            0x00, 0x00  /* CRC placeholder */
        };
        uint16_t crc = mbc_crc16(frame, 7);
        frame[7] = (uint8_t)(crc & 0xFF);
        frame[8] = (uint8_t)((crc >> 8) & 0xFF);
        write_file("rtu_fc03_response.bin", frame, 9);
    }

    /* RTU 3: FC06 Write Single Register with CRC */
    {
        uint8_t frame[8] = {
            0x01,       /* Unit ID */
            0x06,       /* Function code */
            0x00, 0x64, /* Address: 100 */
            0xAB, 0xCD, /* Value */
            0x00, 0x00  /* CRC placeholder */
        };
        uint16_t crc = mbc_crc16(frame, 6);
        frame[6] = (uint8_t)(crc & 0xFF);
        frame[7] = (uint8_t)((crc >> 8) & 0xFF);
        write_file("rtu_fc06_request.bin", frame, 8);
    }

    /* RTU 4: Exception with CRC */
    {
        uint8_t frame[5] = {
            0x01,      /* Unit ID */
            0x83,      /* Exception: FC03 + 0x80 */
            0x02,      /* Exception code */
            0x00, 0x00 /* CRC placeholder */
        };
        uint16_t crc = mbc_crc16(frame, 3);
        frame[3] = (uint8_t)(crc & 0xFF);
        frame[4] = (uint8_t)((crc >> 8) & 0xFF);
        write_file("rtu_exception.bin", frame, 5);
    }

    /* --- PDU Corpus --- */
    printf("\nGenerating PDU corpus...\n");

    /* PDU 1: FC03 request (no framing) */
    {
        uint8_t pdu[] = {
            0x03,       /* Function code */
            0x00, 0x00, /* Start address */
            0x00, 0x0A  /* Quantity */
        };
        write_file("pdu_fc03_request.bin", pdu, sizeof(pdu));
    }

    /* PDU 2: FC03 response */
    {
        uint8_t pdu[] = {
            0x03,       /* Function code */
            0x04,       /* Byte count */
            0x12, 0x34, /* Register 1 */
            0x56, 0x78  /* Register 2 */
        };
        write_file("pdu_fc03_response.bin", pdu, sizeof(pdu));
    }

    /* PDU 3: FC06 request */
    {
        uint8_t pdu[] = {
            0x06,       /* Function code */
            0x00, 0x64, /* Address */
            0xAB, 0xCD  /* Value */
        };
        write_file("pdu_fc06_request.bin", pdu, sizeof(pdu));
    }

    /* PDU 4: FC16 request */
    {
        uint8_t pdu[] = {
            0x10,       /* Function code */
            0x00, 0x01, /* Start address */
            0x00, 0x02, /* Quantity: 2 registers */
            0x04,       /* Byte count */
            0x11, 0x22, /* Register 1 value */
            0x33, 0x44  /* Register 2 value */
        };
        write_file("pdu_fc16_request.bin", pdu, sizeof(pdu));
    }

    /* PDU 5: Exception */
    {
        uint8_t pdu[] = {
            0x83, /* Exception: FC03 + 0x80 */
            0x02  /* Exception code */
        };
        write_file("pdu_exception.bin", pdu, sizeof(pdu));
    }

    printf("\n=== Corpus generation complete ===\n");
    printf("Total files: 15 (5 MBAP, 4 RTU, 6 PDU)\n");
    return 0;
}
