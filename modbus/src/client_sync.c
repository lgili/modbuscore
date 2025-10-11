#include <modbus/client_sync.h>

#include <modbus/client.h>
#include <modbus/mb_err.h>
#include <modbus/pdu.h>
#include <modbus/transport_if.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

typedef struct {
    volatile bool completed;
    mb_err_t status;
    uint8_t function;
    uint8_t unit_id;
    uint8_t payload[MB_PDU_MAX];
    mb_size_t payload_len;
} mb_client_sync_state_t;

void mb_client_sync_opts_init(mb_client_sync_opts_t *opts)
{
    if (opts == NULL) {
        return;
    }
    opts->timeout_ms = MB_CLIENT_SYNC_TIMEOUT_DEFAULT;
    opts->max_retries = 0U;
    opts->retry_backoff_ms = 0U;
}

static const mb_client_sync_opts_t *mb_client_sync_resolve_opts(const mb_client_sync_opts_t *opts,
                                                                mb_client_sync_opts_t *scratch)
{
    if (opts != NULL) {
        return opts;
    }
    mb_client_sync_opts_init(scratch);
    return scratch;
}

static void mb_client_sync_callback(mb_client_t *client,
                                    const mb_client_txn_t *txn,
                                    mb_err_t status,
                                    const mb_adu_view_t *response,
                                    void *user_ctx)
{
    (void)client;
    (void)txn;
    mb_client_sync_state_t *state = (mb_client_sync_state_t *)user_ctx;
    if (state == NULL) {
        return;
    }

    state->status = status;
    state->payload_len = 0U;
    state->unit_id = 0U;
    state->function = 0U;

    if (mb_err_is_ok(status) && response != NULL) {
        state->unit_id = response->unit_id;
        state->function = response->function;
        if (response->payload != NULL && response->payload_len > 0U) {
            state->payload_len = (response->payload_len > MB_PDU_MAX)
                                     ? MB_PDU_MAX
                                     : response->payload_len;
            memcpy(state->payload, response->payload, state->payload_len);
        }
    }

    state->completed = true;
}

static mb_err_t mb_client_sync_wait(mb_client_t *client,
                                    mb_client_txn_t *txn,
                                    mb_client_sync_state_t *state,
                                    const mb_client_sync_opts_t *opts)
{
    if (client == NULL || txn == NULL || state == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_transport_if_t *iface = client->iface;
    if (iface == NULL || iface->now == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_time_ms_t timeout = (opts->timeout_ms != 0U) ? opts->timeout_ms
                                                          : MB_CLIENT_SYNC_TIMEOUT_DEFAULT;
    const mb_time_ms_t start_time = mb_transport_now(iface);
    const mb_time_ms_t deadline = start_time + timeout;

    bool cancelled = false;

    while (!state->completed) {
        mb_client_poll(client);
        if (state->completed) {
            break;
        }

        const mb_time_ms_t now = mb_transport_now(iface);
        if (!cancelled && now >= deadline) {
            mb_client_cancel(client, txn);
            cancelled = true;
        }

        mb_transport_yield(iface);
    }

    return state->status;
}

static mb_err_t mb_client_sync_submit(mb_client_t *client,
                                      mb_client_request_t *request,
                                      mb_client_sync_state_t *state,
                                      const mb_client_sync_opts_t *opts)
{
    if (client == NULL || request == NULL || state == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    request->timeout_ms = (opts->timeout_ms != 0U) ? opts->timeout_ms : 0U;
    request->max_retries = opts->max_retries;
    request->retry_backoff_ms = opts->retry_backoff_ms;
    request->callback = mb_client_sync_callback;
    request->user_ctx = state;

    mb_client_txn_t *txn = NULL;

    state->completed = false;
    state->status = MB_ERR_TIMEOUT;
    state->payload_len = 0U;

    mb_err_t status = mb_client_submit(client, request, &txn);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    return mb_client_sync_wait(client, txn, state, opts);
}

mb_err_t mb_client_read_holding_sync(mb_client_t *client,
                                     uint8_t unit_id,
                                     uint16_t address,
                                     uint16_t count,
                                     uint16_t *out_registers,
                                     const mb_client_sync_opts_t *opts)
{
    if (client == NULL || out_registers == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (count == 0U || count > MB_PDU_FC03_MAX_REGISTERS) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_client_sync_opts_t scratch_opts;
    const mb_client_sync_opts_t *resolved_opts =
        mb_client_sync_resolve_opts(opts, &scratch_opts);

    uint8_t pdu[5];
    mb_err_t status = mb_pdu_build_read_holding_request(pdu, sizeof pdu, address, count);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    mb_client_request_t request;
    memset(&request, 0, sizeof(request));
    request.request.unit_id = unit_id;
    request.request.function = pdu[0];
    request.request.payload = &pdu[1];
    request.request.payload_len = 4U;

    mb_client_sync_state_t state;
    status = mb_client_sync_submit(client, &request, &state, resolved_opts);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    if (state.payload_len == 0U || state.payload[0] != (mb_u8)(count * 2U)) {
        return MB_ERR_INVALID_REQUEST;
    }

    const mb_size_t expected_bytes = (mb_size_t)count * 2U;
    if (state.payload_len < (expected_bytes + 1U)) {
        return MB_ERR_INVALID_REQUEST;
    }

    const uint8_t *register_bytes = &state.payload[1];
    for (uint16_t i = 0; i < count; ++i) {
        out_registers[i] = ((uint16_t)register_bytes[(mb_size_t)i * 2U] << 8) |
                           ((uint16_t)register_bytes[(mb_size_t)i * 2U + 1U]);
    }

    return MB_OK;
}

mb_err_t mb_client_write_register_sync(mb_client_t *client,
                                       uint8_t unit_id,
                                       uint16_t address,
                                       uint16_t value,
                                       const mb_client_sync_opts_t *opts)
{
    if (client == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_client_sync_opts_t scratch_opts;
    const mb_client_sync_opts_t *resolved_opts =
        mb_client_sync_resolve_opts(opts, &scratch_opts);

    uint8_t pdu[5];
    mb_err_t status = mb_pdu_build_write_single_request(pdu, sizeof pdu, address, value);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    mb_client_request_t request;
    memset(&request, 0, sizeof(request));
    request.request.unit_id = unit_id;
    request.request.function = pdu[0];
    request.request.payload = &pdu[1];
    request.request.payload_len = 4U;

    mb_client_sync_state_t state;
    status = mb_client_sync_submit(client, &request, &state, resolved_opts);
    if (!mb_err_is_ok(status)) {
        return status;
    }

    if (state.payload_len < 4U) {
        return MB_ERR_INVALID_REQUEST;
    }

    const uint16_t resp_address = (uint16_t)state.payload[0] << 8 | state.payload[1];
    const uint16_t resp_value = (uint16_t)state.payload[2] << 8 | state.payload[3];

    if (resp_address != address || resp_value != value) {
        return MB_ERR_INVALID_REQUEST;
    }

    return MB_OK;
}
