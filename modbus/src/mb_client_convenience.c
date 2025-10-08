/**
 * @file mb_client_convenience.c
 * @brief High-level convenience functions for common Modbus operations.
 *
 * This module provides function-code-specific helpers that build PDUs and
 * submit requests via mb_client_submit(). These functions are used by both
 * async embedded applications and blocking desktop wrappers (mb_quick).
 *
 * All functions return immediately after submitting the request. Callers
 * must poll the client FSM and inspect the returned transaction to get results.
 */

// Suppress easily-swappable-parameters warnings for this file
// Parameters follow Modbus standard ordering (unit_id, address, etc.)
// NOLINTBEGIN(bugprone-easily-swappable-parameters)

#include <modbus/client.h>
#include <modbus/pdu.h>
#include <modbus/mb_err.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                              Read Operations                               */
/* -------------------------------------------------------------------------- */

#if MB_CONF_ENABLE_FC03
mb_err_t mb_client_read_holding_registers(mb_client_t *client,
                                          uint8_t unit_id,
                                          uint16_t start_addr,
                                          uint16_t quantity,
                                          mb_client_txn_t **out_txn)
{
    if (client == NULL || out_txn == NULL || quantity == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Build PDU
    uint8_t pdu[5];
    mb_err_t err = mb_pdu_build_read_holding_request(pdu, sizeof(pdu), start_addr, quantity);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Build request
    mb_client_request_t req;
    memset(&req, 0, sizeof(req));
    req.request.unit_id = unit_id;
    req.request.function = pdu[0];
    req.request.payload = (pdu[1] || pdu[2] || pdu[3] || pdu[4]) ? &pdu[1] : NULL;
    req.request.payload_len = 4;
    req.timeout_ms = 1000;
    req.max_retries = 0;

    return mb_client_submit(client, &req, out_txn);
}
#endif

#if MB_CONF_ENABLE_FC04
mb_err_t mb_client_read_input_registers(mb_client_t *client,
                                        uint8_t unit_id,
                                        uint16_t start_addr,
                                        uint16_t quantity,
                                        mb_client_txn_t **out_txn)
{
    if (client == NULL || out_txn == NULL || quantity == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Build PDU
    uint8_t pdu[5];
    mb_err_t err = mb_pdu_build_read_input_request(pdu, sizeof(pdu), start_addr, quantity);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Build request
    mb_client_request_t req;
    memset(&req, 0, sizeof(req));
    req.request.unit_id = unit_id;
    req.request.function = pdu[0];
    req.request.payload = &pdu[1];
    req.request.payload_len = 4;
    req.timeout_ms = 1000;
    req.max_retries = 0;

    return mb_client_submit(client, &req, out_txn);
}
#endif

#if MB_CONF_ENABLE_FC01
mb_err_t mb_client_read_coils(mb_client_t *client,
                              uint8_t unit_id,
                              uint16_t start_addr,
                              uint16_t quantity,
                              mb_client_txn_t **out_txn)
{
    if (client == NULL || out_txn == NULL || quantity == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Build PDU
    uint8_t pdu[5];
    mb_err_t err = mb_pdu_build_read_coils_request(pdu, sizeof(pdu), start_addr, quantity);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Build request
    mb_client_request_t req;
    memset(&req, 0, sizeof(req));
    req.request.unit_id = unit_id;
    req.request.function = pdu[0];
    req.request.payload = &pdu[1];
    req.request.payload_len = 4;
    req.timeout_ms = 1000;
    req.max_retries = 0;

    return mb_client_submit(client, &req, out_txn);
}
#endif

#if MB_CONF_ENABLE_FC02
mb_err_t mb_client_read_discrete_inputs(mb_client_t *client,
                                        uint8_t unit_id,
                                        uint16_t start_addr,
                                        uint16_t quantity,
                                        mb_client_txn_t **out_txn)
{
    if (client == NULL || out_txn == NULL || quantity == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Build PDU
    uint8_t pdu[5];
    mb_err_t err = mb_pdu_build_read_discrete_inputs_request(pdu, sizeof(pdu), start_addr, quantity);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Build request
    mb_client_request_t req;
    memset(&req, 0, sizeof(req));
    req.request.unit_id = unit_id;
    req.request.function = pdu[0];
    req.request.payload = &pdu[1];
    req.request.payload_len = 4;
    req.timeout_ms = 1000;
    req.max_retries = 0;

    return mb_client_submit(client, &req, out_txn);
}
#endif

/* -------------------------------------------------------------------------- */
/*                              Write Operations                              */
/* -------------------------------------------------------------------------- */

#if MB_CONF_ENABLE_FC06
mb_err_t mb_client_write_single_register(mb_client_t *client,
                                         uint8_t unit_id,
                                         uint16_t address,
                                         uint16_t value,
                                         mb_client_txn_t **out_txn)
{
    if (client == NULL || out_txn == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Build PDU
    uint8_t pdu[5];
    mb_err_t err = mb_pdu_build_write_single_request(pdu, sizeof(pdu), address, value);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Build request
    mb_client_request_t req;
    memset(&req, 0, sizeof(req));
    req.request.unit_id = unit_id;
    req.request.function = pdu[0];
    req.request.payload = &pdu[1];
    req.request.payload_len = 4;
    req.timeout_ms = 1000;
    req.max_retries = 0;

    return mb_client_submit(client, &req, out_txn);
}
#endif

#if MB_CONF_ENABLE_FC05
mb_err_t mb_client_write_single_coil(mb_client_t *client,
                                     uint8_t unit_id,
                                     uint16_t address,
                                     bool value,
                                     mb_client_txn_t **out_txn)
{
    if (client == NULL || out_txn == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Build PDU
    uint8_t pdu[5];
    mb_err_t err = mb_pdu_build_write_single_coil_request(pdu, sizeof(pdu), address, value);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Build request
    mb_client_request_t req;
    memset(&req, 0, sizeof(req));
    req.request.unit_id = unit_id;
    req.request.function = pdu[0];
    req.request.payload = &pdu[1];
    req.request.payload_len = 4;
    req.timeout_ms = 1000;
    req.max_retries = 0;

    return mb_client_submit(client, &req, out_txn);
}
#endif

#if MB_CONF_ENABLE_FC10
mb_err_t mb_client_write_multiple_registers(mb_client_t *client,
                                            uint8_t unit_id,
                                            uint16_t start_addr,
                                            uint16_t quantity,
                                            const uint16_t *values,
                                            mb_client_txn_t **out_txn)
{
    if (client == NULL || out_txn == NULL || values == NULL || quantity == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Build PDU (max 123 registers per request)
    uint8_t pdu[256];
    mb_err_t err = mb_pdu_build_write_multiple_request(pdu, sizeof(pdu), start_addr, values, quantity);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // PDU length = FC(1) + StartAddr(2) + Quantity(2) + ByteCount(1) + Data(quantity*2)
    size_t pdu_len = 1 + 2 + 2 + 1 + (quantity * 2);

    // Build request
    mb_client_request_t req;
    memset(&req, 0, sizeof(req));
    req.request.unit_id = unit_id;
    req.request.function = pdu[0];
    req.request.payload = &pdu[1];
    req.request.payload_len = pdu_len - 1;
    req.timeout_ms = 1000;
    req.max_retries = 0;

    return mb_client_submit(client, &req, out_txn);
}
#endif

#if MB_CONF_ENABLE_FC0F
mb_err_t mb_client_write_multiple_coils(mb_client_t *client,
                                        uint8_t unit_id,
                                        uint16_t start_addr,
                                        uint16_t quantity,
                                        const uint8_t *values,
                                        mb_client_txn_t **out_txn)
{
    if (client == NULL || out_txn == NULL || values == NULL || quantity == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Build PDU (max 1968 coils per request)
    // NOTE: mb_pdu_build_write_multiple_coils_request expects bool array,
    // but we receive packed bits (uint8_t array). Cast for now.
    uint8_t pdu[256];
    mb_err_t err = mb_pdu_build_write_multiple_coils_request(pdu, sizeof(pdu), start_addr, 
                                                             (const bool *)values, quantity);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // PDU length = FC(1) + StartAddr(2) + Quantity(2) + ByteCount(1) + Data((quantity+7)/8)
    size_t byte_count = (quantity + 7) / 8;
    size_t pdu_len = 1 + 2 + 2 + 1 + byte_count;

    // Build request
    mb_client_request_t req;
    memset(&req, 0, sizeof(req));
    req.request.unit_id = unit_id;
    req.request.function = pdu[0];
    req.request.payload = &pdu[1];
    req.request.payload_len = pdu_len - 1;
    req.timeout_ms = 1000;
    req.max_retries = 0;

    return mb_client_submit(client, &req, out_txn);
}
#endif

// NOLINTEND(bugprone-easily-swappable-parameters)
