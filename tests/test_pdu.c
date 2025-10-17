#include <assert.h>
#include <string.h>

#include <modbuscore/protocol/pdu.h>

static void test_encode_decode_roundtrip(void)
{
    mbc_pdu_t pdu = {
        .unit_id = 0x11U,
        .function = 0x03U,
        .payload = {0x00, 0x6B, 0x00, 0x03},
        .payload_length = 4U,
    };

    uint8_t buffer[8];
    size_t encoded = 0U;

    assert(mbc_pdu_encode(&pdu, buffer, sizeof(buffer), &encoded) == MBC_STATUS_OK);
    assert(encoded == 6U);
    assert(buffer[0] == 0x11U);
    assert(buffer[1] == 0x03U);

    mbc_pdu_t decoded = {0};
    assert(mbc_pdu_decode(buffer, encoded, &decoded) == MBC_STATUS_OK);
    assert(decoded.unit_id == pdu.unit_id);
    assert(decoded.function == pdu.function);
    assert(decoded.payload_length == pdu.payload_length);
    assert(memcmp(decoded.payload, pdu.payload, pdu.payload_length) == 0);
}

static void test_read_holding_build_parse(void)
{
    mbc_pdu_t request = {0};
    assert(mbc_pdu_build_read_holding_request(&request, 0x01U, 0x000A, 0x0002) == MBC_STATUS_OK);
    assert(request.function == 0x03U);
    assert(request.payload_length == 4U);

    uint8_t buffer[8];
    size_t encoded = 0U;
    assert(mbc_pdu_encode(&request, buffer, sizeof(buffer), &encoded) == MBC_STATUS_OK);

    mbc_pdu_t decoded;
    assert(mbc_pdu_decode(buffer, encoded, &decoded) == MBC_STATUS_OK);
    assert(decoded.payload_length == 4U);

    /* Monta resposta fictícia */
    mbc_pdu_t response = {
        .unit_id = 0x01U,
        .function = 0x03U,
        .payload = {0x04U, 0x00U, 0x2BU, 0x00U, 0x01U},
        .payload_length = 5U,
    };

    const uint8_t *data = NULL;
    size_t registers = 0U;
    assert(mbc_pdu_parse_read_holding_response(&response, &data, &registers) == MBC_STATUS_OK);
    assert(registers == 2U);
    assert(data[0] == 0x00U && data[1] == 0x2BU);
}

static void test_write_single_and_multiple(void)
{
    mbc_pdu_t single = {0};
    assert(mbc_pdu_build_write_single_register(&single, 0x02U, 0x0010, 0x1234) == MBC_STATUS_OK);

    uint8_t buffer[16];
    size_t encoded = 0U;
    assert(mbc_pdu_encode(&single, buffer, sizeof buffer, &encoded) == MBC_STATUS_OK);

    mbc_pdu_t decoded = {0};
    assert(mbc_pdu_decode(buffer, encoded, &decoded) == MBC_STATUS_OK);

    uint16_t addr = 0U, value = 0U;
    assert(mbc_pdu_parse_write_single_response(&decoded, &addr, &value) == MBC_STATUS_OK);
    assert(addr == 0x0010U && value == 0x1234U);

    uint16_t regs[3] = {0x1111U, 0x2222U, 0x3333U};
    mbc_pdu_t multi = {0};
    assert(mbc_pdu_build_write_multiple_registers(&multi, 0x02U, 0x0005U, regs, 3U) == MBC_STATUS_OK);
    assert(multi.payload_length == 5U + 6U);

    encoded = 0U;
    mbc_pdu_t multi_response = {
        .unit_id = 0x02U,
        .function = 0x10U,
        .payload = {0x00U, 0x05U, 0x00U, 0x03U},
        .payload_length = 4U,
    };

    uint16_t quantity = 0U;
    assert(mbc_pdu_parse_write_multiple_response(&multi_response, &addr, &quantity) == MBC_STATUS_OK);
    assert(addr == 0x0005U && quantity == 3U);
}

static void test_exception_parsing(void)
{
    mbc_pdu_t exception = {
        .unit_id = 0x01U,
        .function = (uint8_t)(0x03U | 0x80U),
        .payload = {0x02U},
        .payload_length = 1U,
    };

    uint8_t func = 0U;
    uint8_t code = 0U;
    assert(mbc_pdu_parse_exception(&exception, &func, &code) == MBC_STATUS_OK);
    assert(func == 0x03U);
    assert(code == 0x02U);
}

static void test_invalid_cases(void)
{
    mbc_pdu_t pdu = {0};
    uint8_t buffer[2] = {0};

    assert(mbc_pdu_encode(NULL, buffer, sizeof(buffer), NULL) == MBC_STATUS_INVALID_ARGUMENT);
    assert(mbc_pdu_encode(&pdu, NULL, sizeof(buffer), NULL) == MBC_STATUS_INVALID_ARGUMENT);
    pdu.payload_length = MBC_PDU_MAX + 1U;
    assert(mbc_pdu_encode(&pdu, buffer, sizeof(buffer), NULL) == MBC_STATUS_INVALID_ARGUMENT);

    assert(mbc_pdu_decode(NULL, 0U, &pdu) == MBC_STATUS_INVALID_ARGUMENT);
    assert(mbc_pdu_decode(buffer, 1U, &pdu) == MBC_STATUS_INVALID_ARGUMENT);

    mbc_pdu_t response = {
        .unit_id = 0x01U,
        .function = 0x03U,
        .payload = {0x03U, 0x00U, 0x2BU, 0x00U},
        .payload_length = 4U,
    };

    const uint8_t *data = NULL;
    size_t registers = 0U;
    assert(mbc_pdu_parse_read_holding_response(&response, &data, &registers) == MBC_STATUS_DECODING_ERROR);
}

int main(void)
{
    test_encode_decode_roundtrip();
    test_read_holding_build_parse();
    test_write_single_and_multiple();
    test_exception_parsing();
    test_invalid_cases();
    return 0;
}
