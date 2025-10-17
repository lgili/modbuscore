#ifndef MODBUS_COMPAT_LIBMODBUS_H
#define MODBUS_COMPAT_LIBMODBUS_H

#include <modbus/conf.h>

#if !MB_CONF_ENABLE_COMPAT_LIBMODBUS
#error "libmodbus compatibility layer disabled. Enable MODBUS_ENABLE_COMPAT_LIBMODBUS to use this header."
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#include <modbus/mb_err.h>
#include <modbus/compat/modbus-errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle matching libmodbus' ``modbus_t``.
 */
typedef struct modbus_compat_context modbus_t;

/**
 * @brief Global error value mirroring libmodbus' ``modbus_errno`` symbol.
 *
 * This mirrors ``errno`` but keeps Modbus-specific values (EMB*) even on
 * platforms where the global ``errno`` is thread-local.
 */
extern int modbus_errno;

/**
 * @name Context management
 * @{
 */
modbus_t *modbus_new_rtu(const char *device,
                         int baud,
                         char parity,
                         int data_bit,
                         int stop_bit);

modbus_t *modbus_new_tcp(const char *ip, int port);

void modbus_free(modbus_t *ctx);

int modbus_connect(modbus_t *ctx);

void modbus_close(modbus_t *ctx);
/** @} */

/**
 * @name Session configuration
 * @{
 */
int modbus_set_slave(modbus_t *ctx, int slave);

int modbus_get_slave(modbus_t *ctx);

int modbus_set_response_timeout(modbus_t *ctx, uint32_t seconds, uint32_t microseconds);

int modbus_get_response_timeout(modbus_t *ctx, uint32_t *seconds, uint32_t *microseconds);

int modbus_set_debug(modbus_t *ctx, int flag);
/** @} */

/**
 * @name Data access helpers (FC 03/06/16)
 * @{
 */
int modbus_read_registers(modbus_t *ctx, int address, int nb, uint16_t *dest);

int modbus_write_register(modbus_t *ctx, int address, int value);

int modbus_write_registers(modbus_t *ctx, int address, int nb, const uint16_t *data);
/** @} */

/**
 * @brief Compatibility no-op matching libmodbus ``modbus_flush``.
 */
int modbus_flush(modbus_t *ctx);

/**
 * @brief Human-readable error string for both system and Modbus specific errors.
 */
const char *modbus_strerror(int errnum);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_COMPAT_LIBMODBUS_H */
