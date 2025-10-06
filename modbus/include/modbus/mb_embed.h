/*
 * @file mb_embed.h
 * @brief Convenience shims for integrating the Modbus client in embedded apps.
 */

#ifndef MODBUS_MB_EMBED_H
#define MODBUS_MB_EMBED_H

#include <stdbool.h>
#include <string.h>

#include <modbus/client.h>
#include <modbus/mb_err.h>
#include <modbus/mb_types.h>
#include <modbus/pdu.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Minimum sensible transaction pool depth for quickstarts. */
#define MB_EMBED_CLIENT_POOL_MIN 4U

/**
 * @brief Optional knobs exposed by the wrapper helpers.
 */
typedef struct {
    mb_time_ms_t timeout_ms;
    mb_time_ms_t retry_backoff_ms;
    mb_u8 max_retries;
    mb_u32 flags;
    mb_client_callback_t callback;
    void *user_ctx;
} mb_embed_request_opts_t;

/**
 * @brief Default options (1 s timeout, 2 retries, 500 ms backoff).
 */
#define MB_EMBED_REQUEST_OPTS_INIT                                                 \
    {                                                                              \
        .timeout_ms = MB_CLIENT_DEFAULT_TIMEOUT_MS,                                \
        .retry_backoff_ms = MB_CLIENT_DEFAULT_RETRY_BACKOFF_MS,                    \
        .max_retries = 2U,                                                         \
        .flags = 0U,                                                               \
        .callback = NULL,                                                          \
        .user_ctx = NULL                                                           \
    }

static inline mb_embed_request_opts_t mb_embed_request_opts_default(void)
{
    const mb_embed_request_opts_t tmp = MB_EMBED_REQUEST_OPTS_INIT;
    return tmp;
}

static inline void mb_embed_request_opts_apply_defaults(mb_embed_request_opts_t *opts)
{
    if (opts == NULL) {
        return;
    }

    if (opts->timeout_ms == 0U) {
        opts->timeout_ms = MB_CLIENT_DEFAULT_TIMEOUT_MS;
    }

    if (opts->retry_backoff_ms == 0U) {
        opts->retry_backoff_ms = MB_CLIENT_DEFAULT_RETRY_BACKOFF_MS;
    }
}

static inline mb_err_t mb_embed_client_submit_raw(mb_client_t *client,
                                                  mb_u8 unit_id,
                                                  const mb_u8 *pdu,
                                                  mb_size_t pdu_len,
                                                  const mb_embed_request_opts_t *opts,
                                                  mb_client_txn_t **out_txn)
{
    if (client == NULL || pdu == NULL || pdu_len == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_embed_request_opts_t local_opts = opts ? *opts : mb_embed_request_opts_default();
    mb_embed_request_opts_apply_defaults(&local_opts);

    mb_client_request_t req;
    memset(&req, 0, sizeof(req));

    req.flags = local_opts.flags;
    req.request.unit_id = unit_id;
    req.request.function = pdu[0];
    req.request.payload = (pdu_len > 1U) ? &pdu[1] : NULL;
    req.request.payload_len = (pdu_len > 1U) ? (pdu_len - 1U) : 0U;
    req.timeout_ms = local_opts.timeout_ms;
    req.retry_backoff_ms = local_opts.retry_backoff_ms;
    req.max_retries = local_opts.max_retries;
    req.callback = local_opts.callback;
    req.user_ctx = local_opts.user_ctx;

    return mb_client_submit(client, &req, out_txn);
}

/* -------------------------------------------------------------------------- */
/* Request helpers                                                            */
/* -------------------------------------------------------------------------- */

static inline mb_err_t mb_embed_submit_read_holding_registers(mb_client_t *client,
                                                              mb_u8 unit_id,
                                                              mb_u16 start_addr,
                                                              mb_u16 quantity,
                                                              const mb_embed_request_opts_t *opts,
                                                              mb_client_txn_t **out_txn)
{
    mb_u8 pdu[5];
    mb_err_t err = mb_pdu_build_read_holding_request(pdu, sizeof(pdu), start_addr, quantity);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    return mb_embed_client_submit_raw(client, unit_id, pdu, sizeof(pdu), opts, out_txn);
}

static inline mb_err_t mb_embed_submit_read_input_registers(mb_client_t *client,
                                                            mb_u8 unit_id,
                                                            mb_u16 start_addr,
                                                            mb_u16 quantity,
                                                            const mb_embed_request_opts_t *opts,
                                                            mb_client_txn_t **out_txn)
{
    mb_u8 pdu[5];
    mb_err_t err = mb_pdu_build_read_input_request(pdu, sizeof(pdu), start_addr, quantity);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    return mb_embed_client_submit_raw(client, unit_id, pdu, sizeof(pdu), opts, out_txn);
}

static inline mb_err_t mb_embed_submit_read_coils(mb_client_t *client,
                                                  mb_u8 unit_id,
                                                  mb_u16 start_addr,
                                                  mb_u16 quantity,
                                                  const mb_embed_request_opts_t *opts,
                                                  mb_client_txn_t **out_txn)
{
    mb_u8 pdu[5];
    mb_err_t err = mb_pdu_build_read_coils_request(pdu, sizeof(pdu), start_addr, quantity);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    return mb_embed_client_submit_raw(client, unit_id, pdu, sizeof(pdu), opts, out_txn);
}

static inline mb_err_t mb_embed_submit_write_single_register(mb_client_t *client,
                                                             mb_u8 unit_id,
                                                             mb_u16 address,
                                                             mb_u16 value,
                                                             const mb_embed_request_opts_t *opts,
                                                             mb_client_txn_t **out_txn)
{
    mb_u8 pdu[5];
    mb_err_t err = mb_pdu_build_write_single_request(pdu, sizeof(pdu), address, value);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    return mb_embed_client_submit_raw(client, unit_id, pdu, sizeof(pdu), opts, out_txn);
}

static inline mb_err_t mb_embed_submit_write_multiple_registers(mb_client_t *client,
                                                                mb_u8 unit_id,
                                                                mb_u16 start_addr,
                                                                const mb_u16 *values,
                                                                mb_u16 count,
                                                                const mb_embed_request_opts_t *opts,
                                                                mb_client_txn_t **out_txn)
{
    mb_u8 pdu[MB_PDU_MAX];
    mb_err_t err = mb_pdu_build_write_multiple_request(pdu, sizeof(pdu), start_addr, values, count);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    const mb_size_t len = (mb_size_t)(6U + (count * 2U));
    return mb_embed_client_submit_raw(client, unit_id, pdu, len, opts, out_txn);
}

static inline mb_err_t mb_embed_submit_write_single_coil(mb_client_t *client,
                                                         mb_u8 unit_id,
                                                         mb_u16 address,
                                                         bool coil_on,
                                                         const mb_embed_request_opts_t *opts,
                                                         mb_client_txn_t **out_txn)
{
    mb_u8 pdu[5];
    mb_err_t err = mb_pdu_build_write_single_coil_request(pdu, sizeof(pdu), address, coil_on);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    return mb_embed_client_submit_raw(client, unit_id, pdu, sizeof(pdu), opts, out_txn);
}

static inline mb_err_t mb_embed_submit_write_multiple_coils(mb_client_t *client,
                                                            mb_u8 unit_id,
                                                            mb_u16 start_addr,
                                                            const bool *coils,
                                                            mb_u16 count,
                                                            const mb_embed_request_opts_t *opts,
                                                            mb_client_txn_t **out_txn)
{
    mb_u8 pdu[MB_PDU_MAX];
    mb_err_t err = mb_pdu_build_write_multiple_coils_request(pdu, sizeof(pdu), start_addr, coils, count);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    const mb_size_t byte_count = (mb_size_t)((count + 7U) / 8U);
    const mb_size_t len = (mb_size_t)(6U + byte_count);
    return mb_embed_client_submit_raw(client, unit_id, pdu, len, opts, out_txn);
}

static inline mb_err_t mb_embed_submit_readwrite_registers(mb_client_t *client,
                                                           mb_u8 unit_id,
                                                           mb_u16 read_start,
                                                           mb_u16 read_quantity,
                                                           mb_u16 write_start,
                                                           const mb_u16 *write_values,
                                                           mb_u16 write_quantity,
                                                           const mb_embed_request_opts_t *opts,
                                                           mb_client_txn_t **out_txn)
{
    mb_u8 pdu[MB_PDU_MAX];
    mb_err_t err = mb_pdu_build_read_write_multiple_request(pdu, sizeof(pdu), read_start, read_quantity, write_start, write_values, write_quantity);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    const mb_size_t len = (mb_size_t)(10U + (write_quantity * 2U));
    return mb_embed_client_submit_raw(client, unit_id, pdu, len, opts, out_txn);
}

/* -------------------------------------------------------------------------- */
/* Response helpers                                                           */
/* -------------------------------------------------------------------------- */

static inline mb_err_t mb_embed_parse_exception_adu(const mb_adu_view_t *adu,
                                                    mb_u8 *out_code)
{
    if (adu == NULL || (adu->function & MB_PDU_EXCEPTION_BIT) == 0U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (adu->payload_len < 1U) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (out_code != NULL) {
        *out_code = adu->payload[0];
    }

    return MB_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MB_EMBED_H */
