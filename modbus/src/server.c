#include <modbus/server.h>

#include <string.h>

static const mb_u16 kExceptionIllegalFunction = MB_EX_ILLEGAL_FUNCTION;
static const mb_u16 kExceptionIllegalDataAddress = MB_EX_ILLEGAL_DATA_ADDRESS;
static const mb_u16 kExceptionIllegalDataValue = MB_EX_ILLEGAL_DATA_VALUE;
static const mb_u16 kExceptionServerDeviceFailure = MB_EX_SERVER_DEVICE_FAILURE;

static bool ranges_overlap(mb_u16 a_start, mb_u16 a_count, mb_u16 b_start, mb_u16 b_count)
{
    const mb_u32 a_begin = a_start;
    const mb_u32 a_end = a_begin + a_count;
    const mb_u32 b_begin = b_start;
    const mb_u32 b_end = b_begin + b_count;
    return !(a_end <= b_begin || b_end <= a_begin);
}

static bool region_contains(const mb_server_region_t *region, mb_u16 addr, mb_u16 count)
{
    const mb_u32 begin = region->start;
    const mb_u32 end = begin + region->count;
    const mb_u32 req_begin = addr;
    const mb_u32 req_end = req_begin + count;
    return (req_begin >= begin) && (req_end <= end);
}

static mb_err_t region_read(const mb_server_region_t *region,
                            mb_u16 addr,
                            mb_u16 quantity,
                            mb_u16 *out_values)
{
    if (region->read_cb) {
        return region->read_cb(addr, quantity, out_values, quantity, region->user_ctx);
    }

    if (!region->storage) {
        return kExceptionServerDeviceFailure;
    }

    const mb_u16 offset = (mb_u16)(addr - region->start);
    for (mb_u16 i = 0; i < quantity; ++i) {
        out_values[i] = region->storage[offset + i];
    }

    return MB_OK;
}

static mb_err_t region_write(const mb_server_region_t *region,
                             mb_u16 addr,
                             const mb_u16 *values,
                             mb_u16 quantity)
{
    if (region->read_only) {
        return kExceptionIllegalDataAddress;
    }

    if (region->write_cb) {
        return region->write_cb(addr, values, quantity, region->user_ctx);
    }

    if (!region->storage) {
        return kExceptionServerDeviceFailure;
    }

    const mb_u16 offset = (mb_u16)(addr - region->start);
    for (mb_u16 i = 0; i < quantity; ++i) {
        region->storage[offset + i] = values[i];
    }

    return MB_OK;
}

static const mb_server_region_t *find_region(const mb_server_t *server, mb_u16 start, mb_u16 quantity)
{
    for (mb_size_t i = 0; i < server->region_count; ++i) {
        const mb_server_region_t *region = &server->regions[i];
        if (region_contains(region, start, quantity)) {
            return region;
        }
    }
    return NULL;
}

static mb_err_t handle_fc03(mb_server_t *server,
                            const mb_server_region_t *region,
                            mb_u16 start,
                            mb_u16 quantity,
                            mb_size_t *out_pdu_len)
{
    mb_u16 registers[MB_PDU_FC03_MAX_REGISTERS];
    mb_err_t err = region_read(region, start, quantity, registers);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    mb_err_t build = mb_pdu_build_read_holding_response(server->tx_buffer,
                                                        sizeof(server->tx_buffer),
                                                        registers,
                                                        quantity);
    if (!mb_err_is_ok(build)) {
        return kExceptionServerDeviceFailure;
    }

    *out_pdu_len = 2U + (mb_size_t)quantity * 2U;
    return MB_OK;
}

static mb_err_t handle_fc06(mb_server_t *server,
                            const mb_server_region_t *region,
                            mb_u16 address,
                            mb_u16 value,
                            mb_size_t *out_pdu_len)
{
    mb_err_t err = region_write(region, address, &value, 1U);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    mb_err_t build = mb_pdu_build_write_single_response(server->tx_buffer,
                                                         sizeof(server->tx_buffer),
                                                         address,
                                                         value);
    if (!mb_err_is_ok(build)) {
        return kExceptionServerDeviceFailure;
    }

    *out_pdu_len = 5U;
    return MB_OK;
}

static mb_err_t handle_fc16(mb_server_t *server,
                            const mb_server_region_t *region,
                            mb_u16 start,
                            mb_u16 quantity,
                            const mb_u8 *payload,
                            mb_size_t payload_len,
                            mb_size_t *out_pdu_len)
{
    if (payload_len < (mb_size_t)(6U + quantity * 2U)) {
        return kExceptionIllegalDataValue;
    }

    mb_u16 values[MB_PDU_FC16_MAX_REGISTERS];
    for (mb_u16 i = 0; i < quantity; ++i) {
        values[i] = (mb_u16)((payload[i * 2U] << 8) | payload[i * 2U + 1U]);
    }

    mb_err_t err = region_write(region, start, values, quantity);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    mb_err_t build = mb_pdu_build_write_multiple_response(server->tx_buffer,
                                                           sizeof(server->tx_buffer),
                                                           start,
                                                           quantity);
    if (!mb_err_is_ok(build)) {
        return kExceptionServerDeviceFailure;
    }

    *out_pdu_len = 5U;
    return MB_OK;
}

static mb_err_t map_error_to_exception(mb_err_t err)
{
    if (mb_err_is_ok(err)) {
        return MB_OK;
    }

    if (mb_err_is_exception(err)) {
        return err;
    }

    return kExceptionServerDeviceFailure;
}

static void server_send_exception(mb_server_t *server,
                                  const mb_adu_view_t *request,
                                  mb_u8 exception_code)
{
    if (!mb_err_is_ok(mb_pdu_build_exception(server->tx_buffer,
                                             sizeof(server->tx_buffer),
                                             request->function,
                                             exception_code))) {
        return;
    }

    mb_adu_view_t response = {
        .unit_id = server->unit_id,
        .function = server->tx_buffer[0],
        .payload = &server->tx_buffer[1],
        .payload_len = 1U,
    };

    (void)mb_rtu_submit(&server->rtu, &response);
}

static void mb_server_rtu_callback(mb_rtu_transport_t *rtu,
                                   const mb_adu_view_t *adu,
                                   mb_err_t status,
                                   void *user)
{
    (void)rtu;
    mb_server_t *server = (mb_server_t *)user;

    if (!mb_err_is_ok(status) || adu == NULL) {
        return;
    }

    const bool broadcast = (adu->unit_id == 0U);
    if (!broadcast && adu->unit_id != server->unit_id) {
        return;
    }

    mb_u8 request_pdu[MB_PDU_MAX];
    const mb_size_t request_len = adu->payload_len + 1U;
    if (request_len > MB_PDU_MAX) {
        if (!broadcast) {
            server_send_exception(server, adu, (mb_u8)kExceptionIllegalDataValue);
        }
        return;
    }

    request_pdu[0] = adu->function;
    if (adu->payload_len > 0U) {
        memcpy(&request_pdu[1], adu->payload, adu->payload_len);
    }

    mb_size_t response_pdu_len = 0U;
    mb_err_t err = MB_OK;

    switch (adu->function) {
    case MB_PDU_FC_READ_HOLDING_REGISTERS: {
        mb_u16 start = 0U;
        mb_u16 quantity = 0U;
        err = mb_pdu_parse_read_holding_request(request_pdu, request_len, &start, &quantity);
        if (!mb_err_is_ok(err)) {
            err = kExceptionIllegalDataValue;
            break;
        }

        const mb_server_region_t *region = find_region(server, start, quantity);
        if (!region) {
            err = kExceptionIllegalDataAddress;
            break;
        }

        err = handle_fc03(server, region, start, quantity, &response_pdu_len);
        break;
    }
    case MB_PDU_FC_WRITE_SINGLE_REGISTER: {
        mb_u16 address = 0U;
        mb_u16 value = 0U;
        err = mb_pdu_parse_write_single_request(request_pdu, request_len, &address, &value);
        if (!mb_err_is_ok(err)) {
            err = kExceptionIllegalDataValue;
            break;
        }

        const mb_server_region_t *region = find_region(server, address, 1U);
        if (!region) {
            err = kExceptionIllegalDataAddress;
            break;
        }

        err = handle_fc06(server, region, address, value, &response_pdu_len);
        break;
    }
    case MB_PDU_FC_WRITE_MULTIPLE_REGISTERS: {
        mb_u16 start = 0U;
        mb_u16 quantity = 0U;
        const mb_u8 *value_bytes = NULL;
        err = mb_pdu_parse_write_multiple_request(request_pdu, request_len, &start, &quantity, &value_bytes);
        if (!mb_err_is_ok(err)) {
            err = kExceptionIllegalDataValue;
            break;
        }

        const mb_server_region_t *region = find_region(server, start, quantity);
        if (!region) {
            err = kExceptionIllegalDataAddress;
            break;
        }

        err = handle_fc16(server, region, start, quantity, value_bytes, request_len, &response_pdu_len);
        break;
    }
    default:
        err = kExceptionIllegalFunction;
        break;
    }

    err = map_error_to_exception(err);

    if (broadcast) {
        return;
    }

    if (!mb_err_is_ok(err)) {
        server_send_exception(server, adu, (mb_u8)err);
        return;
    }

    mb_adu_view_t response = {
        .unit_id = server->unit_id,
        .function = server->tx_buffer[0],
        .payload = &server->tx_buffer[1],
        .payload_len = response_pdu_len - 1U,
    };

    (void)mb_rtu_submit(&server->rtu, &response);
}

mb_err_t mb_server_init(mb_server_t *server,
                        const mb_transport_if_t *iface,
                        mb_u8 unit_id,
                        mb_server_region_t *regions,
                        mb_size_t region_capacity)
{
    if (!server || !iface || !regions || region_capacity == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    memset(server, 0, sizeof(*server));
    server->iface = iface;
    server->unit_id = unit_id;
    server->regions = regions;
    server->region_cap = region_capacity;

    mb_err_t err = mb_rtu_init(&server->rtu, iface, mb_server_rtu_callback, server);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    return MB_OK;
}

void mb_server_reset(mb_server_t *server)
{
    if (!server) {
        return;
    }

    server->region_count = 0U;
    mb_rtu_reset(&server->rtu);
}

static mb_err_t insert_region(mb_server_t *server, const mb_server_region_t *cfg)
{
    if (server->region_count >= server->region_cap) {
        return MB_ERR_OTHER;
    }

    for (mb_size_t i = 0; i < server->region_count; ++i) {
        const mb_server_region_t *existing = &server->regions[i];
        if (ranges_overlap(existing->start, existing->count, cfg->start, cfg->count)) {
            return MB_ERR_INVALID_ARGUMENT;
        }
    }

    server->regions[server->region_count++] = *cfg;
    return MB_OK;
}

mb_err_t mb_server_add_region(mb_server_t *server,
                              mb_u16 start,
                              mb_u16 count,
                              bool read_only,
                              mb_server_read_fn read_cb,
                              mb_server_write_fn write_cb,
                              void *user_ctx)
{
    if (!server || count == 0U || (!read_cb && !write_cb)) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if ((mb_u32)start + count > 0x10000UL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_server_region_t region = {
        .start = start,
        .count = count,
        .read_only = read_only,
        .read_cb = read_cb,
        .write_cb = write_cb,
        .user_ctx = user_ctx,
        .storage = NULL,
    };

    return insert_region(server, &region);
}

mb_err_t mb_server_add_storage(mb_server_t *server,
                               mb_u16 start,
                               mb_u16 count,
                               bool read_only,
                               mb_u16 *storage)
{
    if (!server || !storage || count == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if ((mb_u32)start + count > 0x10000UL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_server_region_t region = {
        .start = start,
        .count = count,
        .read_only = read_only,
        .read_cb = NULL,
        .write_cb = NULL,
        .user_ctx = NULL,
        .storage = storage,
    };

    return insert_region(server, &region);
}

mb_err_t mb_server_poll(mb_server_t *server)
{
    if (!server) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    return mb_rtu_poll(&server->rtu);
}
