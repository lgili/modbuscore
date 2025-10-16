#ifndef MODBUS_INTERNAL_MB_SIMPLE_BACKEND_H
#define MODBUS_INTERNAL_MB_SIMPLE_BACKEND_H

#include <modbus/mb_err.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle owned by the mb_simple backend.
 *
 * Backends are free to map this to their own representation. The default
 * implementation simply casts to mb_host_client_t.
 */
typedef struct mb_simple_backend_client mb_simple_backend_client_t;

/**
 * @brief Operations that mb_simple expects from its backend.
 *
 * These callbacks allow the simplified API to remain agnostic of the underlying
 * implementation (legacy mb_host, test fakes, future rewrites, etc.).
 */
typedef struct mb_simple_backend {
    mb_simple_backend_client_t *(*connect_tcp)(const char *endpoint);
    mb_simple_backend_client_t *(*connect_rtu)(const char *device, uint32_t baudrate);
    void (*disconnect)(mb_simple_backend_client_t *client);

    void (*set_timeout)(mb_simple_backend_client_t *client, uint32_t timeout_ms);
    void (*enable_logging)(mb_simple_backend_client_t *client, bool enable);

    mb_err_t (*read_holding)(mb_simple_backend_client_t *client,
                             uint8_t unit_id,
                             uint16_t address,
                             uint16_t count,
                             uint16_t *out_registers);
    mb_err_t (*read_input)(mb_simple_backend_client_t *client,
                           uint8_t unit_id,
                           uint16_t address,
                           uint16_t count,
                           uint16_t *out_registers);
    mb_err_t (*read_coils)(mb_simple_backend_client_t *client,
                           uint8_t unit_id,
                           uint16_t address,
                           uint16_t count,
                           uint8_t *out_coils);
    mb_err_t (*read_discrete)(mb_simple_backend_client_t *client,
                              uint8_t unit_id,
                              uint16_t address,
                              uint16_t count,
                              uint8_t *out_inputs);

    mb_err_t (*write_register)(mb_simple_backend_client_t *client,
                               uint8_t unit_id,
                               uint16_t address,
                               uint16_t value);
    mb_err_t (*write_coil)(mb_simple_backend_client_t *client,
                           uint8_t unit_id,
                           uint16_t address,
                           bool value);
    mb_err_t (*write_registers)(mb_simple_backend_client_t *client,
                                uint8_t unit_id,
                                uint16_t address,
                                uint16_t count,
                                const uint16_t *registers);
    mb_err_t (*write_coils)(mb_simple_backend_client_t *client,
                            uint8_t unit_id,
                            uint16_t address,
                            uint16_t count,
                            const uint8_t *coils);

    uint8_t (*last_exception)(mb_simple_backend_client_t *client);
    const char *(*error_string)(mb_err_t err);
} mb_simple_backend_t;

/**
 * @brief Replace the backend implementation used by mb_simple.
 *
 * Passing NULL restores the default backend.
 */
void mb_simple_set_backend(const mb_simple_backend_t *backend);

/**
 * @brief Retrieve the backend currently used by mb_simple.
 */
const mb_simple_backend_t *mb_simple_get_backend(void);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_INTERNAL_MB_SIMPLE_BACKEND_H */

