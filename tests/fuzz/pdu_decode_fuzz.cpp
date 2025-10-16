#include <cstddef>
#include <cstdint>

extern "C" {
#include <modbus/internal/pdu.h>
}

namespace {
constexpr size_t kMaxCorpusBytes = MB_PDU_MAX;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (data == nullptr || size == 0U) {
        return 0;
    }

    const size_t len = (size > kMaxCorpusBytes) ? kMaxCorpusBytes : size;
    const mb_u8 *pdu = reinterpret_cast<const mb_u8 *>(data);

    mb_u16 addr = 0U;
    mb_u16 quantity = 0U;
    const mb_u8 *payload = nullptr;
    mb_u16 out_count = 0U;
    mb_u8 byte_count = 0U;
    bool coil_state = false;
    mb_u16 read_addr = 0U;
    mb_u16 read_qty = 0U;
    mb_u16 write_addr = 0U;
    mb_u16 write_qty = 0U;

    (void)mb_pdu_parse_read_coils_request(pdu, len, &addr, &quantity);
    (void)mb_pdu_parse_read_coils_response(pdu, len, &payload, &byte_count);

    (void)mb_pdu_parse_read_discrete_inputs_request(pdu, len, &addr, &quantity);
    (void)mb_pdu_parse_read_discrete_inputs_response(pdu, len, &payload, &byte_count);

    (void)mb_pdu_parse_read_holding_request(pdu, len, &addr, &quantity);
    (void)mb_pdu_parse_read_holding_response(pdu, len, &payload, &out_count);

    (void)mb_pdu_parse_read_input_request(pdu, len, &addr, &quantity);
    (void)mb_pdu_parse_read_input_response(pdu, len, &payload, &out_count);

    (void)mb_pdu_parse_write_single_request(pdu, len, &addr, &quantity);
    (void)mb_pdu_parse_write_single_response(pdu, len, &addr, &quantity);

    (void)mb_pdu_parse_write_single_coil_request(pdu, len, &addr, &coil_state);
    (void)mb_pdu_parse_write_single_coil_response(pdu, len, &addr, &coil_state);

    (void)mb_pdu_parse_write_multiple_request(pdu, len, &addr, &out_count, &payload);
    (void)mb_pdu_parse_write_multiple_response(pdu, len, &addr, &out_count);

    (void)mb_pdu_parse_write_multiple_coils_request(pdu, len, &addr, &out_count, &byte_count, &payload);
    (void)mb_pdu_parse_write_multiple_coils_response(pdu, len, &addr, &out_count);

    (void)mb_pdu_parse_read_write_multiple_request(pdu, len, &read_addr, &read_qty, &write_addr, &write_qty, &payload);
    (void)mb_pdu_parse_read_write_multiple_response(pdu, len, &payload, &out_count);
    mb_u8 ex_function = 0U;
    mb_u8 ex_code = 0U;
    (void)mb_pdu_parse_exception(pdu, len, &ex_function, &ex_code);

    return 0;
}
