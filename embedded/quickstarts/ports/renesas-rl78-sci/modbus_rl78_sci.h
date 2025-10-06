// SPDX-License-Identifier: Apache-2.0
//
// Public API for the RL78 SCI Modbus RTU helper.

#ifndef MODBUS_RL78_SCI_H
#define MODBUS_RL78_SCI_H

#include "modbus_amalgamated.h"

#include <modbus/mb_embed.h>

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t (*modbus_rl78_now_fn_t)(void *user_ctx);
typedef void (*modbus_rl78_delay_fn_t)(uint32_t usec, void *user_ctx);
typedef void (*modbus_rl78_direction_fn_t)(bool is_tx, void *user_ctx);

typedef struct {
    uint32_t baudrate;
    bool parity_enabled;
    bool two_stop_bits;
    uint32_t silence_timeout_ms;
    modbus_rl78_now_fn_t now_us;
    modbus_rl78_delay_fn_t delay_us;
    modbus_rl78_direction_fn_t set_direction;
    void *user_ctx;
} modbus_rl78_sci_config_t;

mb_err_t modbus_rl78_sci_init(const modbus_rl78_sci_config_t *cfg,
                              mb_client_txn_t *txn_pool,
                              mb_size_t txn_pool_len);

mb_err_t modbus_rl78_sci_poll(void);

mb_client_t *modbus_rl78_sci_client(void);

mb_err_t modbus_rl78_sci_submit_read_inputs(uint8_t unit_id,
                                            uint16_t addr,
                                            uint16_t count,
                                            const mb_embed_request_opts_t *opts,
                                            mb_client_txn_t **out_txn);

mb_err_t modbus_rl78_sci_submit_write_single(uint8_t unit_id,
                                             uint16_t addr,
                                             uint16_t value,
                                             const mb_embed_request_opts_t *opts,
                                             mb_client_txn_t **out_txn);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_RL78_SCI_H */
