#ifndef MODBUS_CLIENT_SYNC_H
#define MODBUS_CLIENT_SYNC_H

/**
 * @file client_sync.h
 * @brief Blocking helpers that wrap the non-blocking Modbus client FSM.
 *
 * These helpers build on top of the regular `mb_client_*` API to provide
 * synchronous, one-shot operations. They are intended for applications that
 * prefer a straightforward "submit and wait" workflow (desktop tooling,
 * scripts, quick prototypes) while still relying on the core client FSM.
 *
 * @note The existing asynchronous API remains fully supported. These helpers
 *       are thin convenience functions and do not introduce a second client
 *       stack.
 */

#include <modbus/client.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default timeout applied by the synchronous helpers when none is provided.
 */
#define MB_CLIENT_SYNC_TIMEOUT_DEFAULT 5000U

/**
 * @brief Options controlling a synchronous client request.
 */
typedef struct {
    /**
     * Optional timeout in milliseconds. When zero, the helper falls back to
     * @ref MB_CLIENT_SYNC_TIMEOUT_DEFAULT (in addition to the per-request
     * timeout managed by the FSM itself).
     */
    mb_time_ms_t timeout_ms;

    /**
     * Maximum number of retries issued by the client FSM. Set to zero to
     * inherit the client's default policy.
     */
    mb_u8 max_retries;

    /**
     * Backoff between retries, in milliseconds. Ignored when zero.
     */
    mb_time_ms_t retry_backoff_ms;
} mb_client_sync_opts_t;

/**
 * @brief Initialises @p opts with the default synchronous settings.
 *
 * @param opts Options structure to initialise (must not be NULL).
 */
void mb_client_sync_opts_init(mb_client_sync_opts_t *opts);

/**
 * @brief Read holding registers using a blocking workflow (FC 0x03).
 *
 * @param client       Initialised Modbus client.
 * @param unit_id      Target Modbus unit identifier (1-247).
 * @param address      Starting register address.
 * @param count        Number of registers to read (1-125).
 * @param out_registers Output buffer that can hold @p count registers.
 * @param opts         Optional synchronous options (pass NULL for defaults).
 *
 * @return MB_OK on success or an error code from @ref mb_err_t.
 */
mb_err_t mb_client_read_holding_sync(mb_client_t *client,
                                     uint8_t unit_id,
                                     uint16_t address,
                                     uint16_t count,
                                     uint16_t *out_registers,
                                     const mb_client_sync_opts_t *opts);

/**
 * @brief Write a single holding register (FC 0x06) using a blocking workflow.
 *
 * @param client  Initialised Modbus client.
 * @param unit_id Target Modbus unit identifier (1-247).
 * @param address Register address to write.
 * @param value   Value to store.
 * @param opts    Optional synchronous options (pass NULL for defaults).
 *
 * @return MB_OK on success or an error code from @ref mb_err_t.
 */
mb_err_t mb_client_write_register_sync(mb_client_t *client,
                                       uint8_t unit_id,
                                       uint16_t address,
                                       uint16_t value,
                                       const mb_client_sync_opts_t *opts);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_CLIENT_SYNC_H */
