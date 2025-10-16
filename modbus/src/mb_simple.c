/**
 * @file mb_simple.c
 * @brief Implementation of the simplified Modbus API
 *
 * This file implements the mb_simple.h interface by wrapping the existing
 * mb_host and client_sync APIs into a unified, easy-to-use interface.
 *
 * @version 2.0
 * @date 2025-01-15
 */

#include <modbus/mb_simple.h>
#include <modbus/mb_err.h>
#include <modbus/internal/mb_simple_backend.h>
#include <modbus/mb_host.h>

#include <stdlib.h>
#include <string.h>

typedef mb_simple_backend_client_t backend_client_t;

static backend_client_t *default_connect_tcp(const char *endpoint)
{
    return (backend_client_t *)mb_host_tcp_connect(endpoint);
}

static backend_client_t *default_connect_rtu(const char *device, uint32_t baudrate)
{
    return (backend_client_t *)mb_host_rtu_connect(device, baudrate);
}

static void default_disconnect(backend_client_t *client)
{
    if (client) {
        mb_host_disconnect((mb_host_client_t *)client);
    }
}

static void default_set_timeout(backend_client_t *client, uint32_t timeout_ms)
{
    if (client) {
        mb_host_set_timeout((mb_host_client_t *)client, timeout_ms);
    }
}

static void default_enable_logging(backend_client_t *client, bool enable)
{
    if (client) {
        mb_host_enable_logging((mb_host_client_t *)client, enable);
    }
}

static mb_err_t default_read_holding(backend_client_t *client,
                                     uint8_t unit_id,
                                     uint16_t address,
                                     uint16_t count,
                                     uint16_t *out_registers)
{
    return mb_host_read_holding((mb_host_client_t *)client, unit_id, address, count, out_registers);
}

static mb_err_t default_read_input(backend_client_t *client,
                                   uint8_t unit_id,
                                   uint16_t address,
                                   uint16_t count,
                                   uint16_t *out_registers)
{
    return mb_host_read_input((mb_host_client_t *)client, unit_id, address, count, out_registers);
}

static mb_err_t default_read_coils(backend_client_t *client,
                                   uint8_t unit_id,
                                   uint16_t address,
                                   uint16_t count,
                                   uint8_t *out_coils)
{
    return mb_host_read_coils((mb_host_client_t *)client, unit_id, address, count, out_coils);
}

static mb_err_t default_read_discrete(backend_client_t *client,
                                      uint8_t unit_id,
                                      uint16_t address,
                                      uint16_t count,
                                      uint8_t *out_inputs)
{
    return mb_host_read_discrete((mb_host_client_t *)client, unit_id, address, count, out_inputs);
}

static mb_err_t default_write_register(backend_client_t *client,
                                       uint8_t unit_id,
                                       uint16_t address,
                                       uint16_t value)
{
    return mb_host_write_single_register((mb_host_client_t *)client, unit_id, address, value);
}

static mb_err_t default_write_coil(backend_client_t *client,
                                   uint8_t unit_id,
                                   uint16_t address,
                                   bool value)
{
    return mb_host_write_single_coil((mb_host_client_t *)client, unit_id, address, value);
}

static mb_err_t default_write_registers(backend_client_t *client,
                                        uint8_t unit_id,
                                        uint16_t address,
                                        uint16_t count,
                                        const uint16_t *registers)
{
    return mb_host_write_multiple_registers((mb_host_client_t *)client, unit_id, address, count, registers);
}

static mb_err_t default_write_coils(backend_client_t *client,
                                    uint8_t unit_id,
                                    uint16_t address,
                                    uint16_t count,
                                    const uint8_t *coils)
{
    return mb_host_write_multiple_coils((mb_host_client_t *)client, unit_id, address, count, coils);
}

static uint8_t default_last_exception(backend_client_t *client)
{
    return mb_host_last_exception((mb_host_client_t *)client);
}

static const char *default_error_string(mb_err_t err)
{
    return mb_host_error_string(err);
}

static const mb_simple_backend_t default_backend = {
    .connect_tcp = default_connect_tcp,
    .connect_rtu = default_connect_rtu,
    .disconnect = default_disconnect,
    .set_timeout = default_set_timeout,
    .enable_logging = default_enable_logging,
    .read_holding = default_read_holding,
    .read_input = default_read_input,
    .read_coils = default_read_coils,
    .read_discrete = default_read_discrete,
    .write_register = default_write_register,
    .write_coil = default_write_coil,
    .write_registers = default_write_registers,
    .write_coils = default_write_coils,
    .last_exception = default_last_exception,
    .error_string = default_error_string,
};

static const mb_simple_backend_t *active_backend = &default_backend;

const mb_simple_backend_t *mb_simple_get_backend(void)
{
    return active_backend;
}

void mb_simple_set_backend(const mb_simple_backend_t *backend)
{
    active_backend = backend ? backend : &default_backend;
}

/* ========================================================================== */
/*                           INTERNAL STRUCTURES                              */
/* ========================================================================== */

/**
 * @brief Internal structure for mb_t handle.
 *
 * This wraps a backend-specific client handle into a unified interface.
 * By default we reuse mb_host as the backend since it already provides the
 * simple, blocking API that mb_simple exposes publicly.
 */
struct mb {
    backend_client_t *client;        /**< Backend-specific client handle */
    mb_options_t options;            /**< Saved options */
    uint8_t last_exception;          /**< Last exception code received */
    bool is_tcp;                     /**< true if TCP, false if RTU */
    char *endpoint;                  /**< Saved endpoint for reconnection */
    uint32_t rtu_baudrate;           /**< Stored baudrate for RTU reconnection */
};

static void mb_update_last_exception(mb_t *mb,
                                     const mb_simple_backend_t *backend,
                                     mb_err_t err)
{
    if (!mb) {
        return;
    }

    if (err <= 0) {
        mb->last_exception = 0U;
    } else if (backend && backend->last_exception && mb->client) {
        /* Positive values indicate Modbus exceptions in this API contract. */
        mb->last_exception = backend->last_exception(mb->client);
    }
}

/* ========================================================================== */
/*                           OPTIONS                                          */
/* ========================================================================== */

void mb_options_init(mb_options_t *opts)
{
    if (!opts) return;

    opts->timeout_ms = 1000;
    opts->max_retries = 3;
    opts->pool_size = 8;
    opts->enable_logging = false;
    opts->enable_diagnostics = true;
}

/* ========================================================================== */
/*                           CONNECTION MANAGEMENT                            */
/* ========================================================================== */

mb_t *mb_create_tcp(const char *endpoint)
{
    return mb_create_tcp_ex(endpoint, NULL);
}

mb_t *mb_create_tcp_ex(const char *endpoint, const mb_options_t *opts)
{
    if (!endpoint) {
        return NULL;
    }

    /* Allocate mb_t structure */
    mb_t *mb = (mb_t *)calloc(1, sizeof(mb_t));
    if (!mb) {
        return NULL;
    }

    /* Initialize options */
    if (opts) {
        mb->options = *opts;
    } else {
        mb_options_init(&mb->options);
    }

    /* Save endpoint for reconnection */
    mb->endpoint = strdup(endpoint);
    if (!mb->endpoint) {
        free(mb);
        return NULL;
    }

    mb->is_tcp = true;
    mb->rtu_baudrate = 0U;

    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (!backend || !backend->connect_tcp) {
        free(mb->endpoint);
        free(mb);
        return NULL;
    }

    mb->client = backend->connect_tcp(endpoint);
    if (!mb->client) {
        free(mb->endpoint);
        free(mb);
        return NULL;
    }

    /* Apply options */
    if (backend->set_timeout) {
        backend->set_timeout(mb->client, mb->options.timeout_ms);
    }
    if (backend->enable_logging) {
        backend->enable_logging(mb->client, mb->options.enable_logging);
    }

    return mb;
}

mb_t *mb_create_rtu(const char *device, uint32_t baudrate)
{
    return mb_create_rtu_ex(device, baudrate, NULL);
}

mb_t *mb_create_rtu_ex(const char *device, uint32_t baudrate, const mb_options_t *opts)
{
    if (!device) {
        return NULL;
    }

    /* Allocate mb_t structure */
    mb_t *mb = (mb_t *)calloc(1, sizeof(mb_t));
    if (!mb) {
        return NULL;
    }

    /* Initialize options */
    if (opts) {
        mb->options = *opts;
    } else {
        mb_options_init(&mb->options);
    }

    /* Save device path for reconnection */
    mb->endpoint = strdup(device);
    if (!mb->endpoint) {
        free(mb);
        return NULL;
    }

    mb->is_tcp = false;
    mb->rtu_baudrate = baudrate;

    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (!backend || !backend->connect_rtu) {
        free(mb->endpoint);
        free(mb);
        return NULL;
    }

    mb->client = backend->connect_rtu(device, baudrate);
    if (!mb->client) {
        free(mb->endpoint);
        free(mb);
        return NULL;
    }

    /* Apply options */
    if (backend->set_timeout) {
        backend->set_timeout(mb->client, mb->options.timeout_ms);
    }
    if (backend->enable_logging) {
        backend->enable_logging(mb->client, mb->options.enable_logging);
    }

    return mb;
}

void mb_destroy(mb_t *mb)
{
    if (!mb) {
        return;
    }

    /* Disconnect and free host client */
    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (mb->client && backend && backend->disconnect) {
        backend->disconnect(mb->client);
        mb->client = NULL;
    }

    /* Free saved endpoint */
    if (mb->endpoint) {
        free(mb->endpoint);
        mb->endpoint = NULL;
    }

    /* Free mb structure */
    free(mb);
}

bool mb_is_connected(const mb_t *mb)
{
    if (!mb || !mb->client) {
        return false;
    }

    /* Backend may not expose connection status; assume handle implies connection. */
    return true;
}

mb_err_t mb_reconnect(mb_t *mb)
{
    if (!mb || !mb->endpoint) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (!backend) {
        return MB_ERR_OTHER;
    }

    if (mb->client && backend->disconnect) {
        backend->disconnect(mb->client);
        mb->client = NULL;
    }

    if (mb->is_tcp) {
        if (!backend->connect_tcp) {
            return MB_ERR_TRANSPORT;
        }
        mb->client = backend->connect_tcp(mb->endpoint);
    } else {
        const uint32_t baudrate = (mb->rtu_baudrate != 0U) ? mb->rtu_baudrate : 115200U;
        if (!backend->connect_rtu) {
            return MB_ERR_TRANSPORT;
        }
        mb->client = backend->connect_rtu(mb->endpoint, baudrate);
    }

    if (!mb->client) {
        return MB_ERR_TRANSPORT;
    }

    /* Re-apply options */
    if (backend->set_timeout) {
        backend->set_timeout(mb->client, mb->options.timeout_ms);
    }
    if (backend->enable_logging) {
        backend->enable_logging(mb->client, mb->options.enable_logging);
    }

    return MB_OK;
}

/* ========================================================================== */
/*                           CLIENT OPERATIONS (READ)                         */
/* ========================================================================== */

mb_err_t mb_read_holding(mb_t *mb, uint8_t unit_id, uint16_t address,
                         uint16_t count, uint16_t *out_registers)
{
    if (!mb || !mb->client || !out_registers) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (!backend || !backend->read_holding) {
        return MB_ERR_OTHER;
    }

    mb_err_t err = backend->read_holding(mb->client, unit_id, address, count, out_registers);
    mb_update_last_exception(mb, backend, err);

    return err;
}

mb_err_t mb_read_input(mb_t *mb, uint8_t unit_id, uint16_t address,
                       uint16_t count, uint16_t *out_registers)
{
    if (!mb || !mb->client || !out_registers) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (!backend || !backend->read_input) {
        return MB_ERR_OTHER;
    }

    mb_err_t err = backend->read_input(mb->client, unit_id, address, count, out_registers);
    mb_update_last_exception(mb, backend, err);

    return err;
}

mb_err_t mb_read_coils(mb_t *mb, uint8_t unit_id, uint16_t address,
                       uint16_t count, uint8_t *out_coils)
{
    if (!mb || !mb->client || !out_coils) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (!backend || !backend->read_coils) {
        return MB_ERR_OTHER;
    }

    mb_err_t err = backend->read_coils(mb->client, unit_id, address, count, out_coils);
    mb_update_last_exception(mb, backend, err);

    return err;
}

mb_err_t mb_read_discrete(mb_t *mb, uint8_t unit_id, uint16_t address,
                          uint16_t count, uint8_t *out_inputs)
{
    if (!mb || !mb->client || !out_inputs) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (!backend || !backend->read_discrete) {
        return MB_ERR_OTHER;
    }

    mb_err_t err = backend->read_discrete(mb->client, unit_id, address, count, out_inputs);
    mb_update_last_exception(mb, backend, err);

    return err;
}

/* ========================================================================== */
/*                           CLIENT OPERATIONS (WRITE)                        */
/* ========================================================================== */

mb_err_t mb_write_register(mb_t *mb, uint8_t unit_id, uint16_t address, uint16_t value)
{
    if (!mb || !mb->client) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (!backend || !backend->write_register) {
        return MB_ERR_OTHER;
    }

    mb_err_t err = backend->write_register(mb->client, unit_id, address, value);
    mb_update_last_exception(mb, backend, err);

    return err;
}

mb_err_t mb_write_coil(mb_t *mb, uint8_t unit_id, uint16_t address, bool value)
{
    if (!mb || !mb->client) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (!backend || !backend->write_coil) {
        return MB_ERR_OTHER;
    }

    mb_err_t err = backend->write_coil(mb->client, unit_id, address, value);
    mb_update_last_exception(mb, backend, err);

    return err;
}

mb_err_t mb_write_registers(mb_t *mb, uint8_t unit_id, uint16_t address,
                            uint16_t count, const uint16_t *registers)
{
    if (!mb || !mb->client || !registers) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (!backend || !backend->write_registers) {
        return MB_ERR_OTHER;
    }

    mb_err_t err = backend->write_registers(mb->client, unit_id, address, count, registers);
    mb_update_last_exception(mb, backend, err);

    return err;
}

mb_err_t mb_write_coils(mb_t *mb, uint8_t unit_id, uint16_t address,
                        uint16_t count, const uint8_t *coils)
{
    if (!mb || !mb->client || !coils) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (!backend || !backend->write_coils) {
        return MB_ERR_OTHER;
    }

    mb_err_t err = backend->write_coils(mb->client, unit_id, address, count, coils);
    mb_update_last_exception(mb, backend, err);

    return err;
}

/* ========================================================================== */
/*                           CONFIGURATION                                    */
/* ========================================================================== */

void mb_set_timeout(mb_t *mb, uint32_t timeout_ms)
{
    if (!mb) {
        return;
    }

    mb->options.timeout_ms = timeout_ms;
    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (mb->client && backend && backend->set_timeout) {
        backend->set_timeout(mb->client, timeout_ms);
    }
}

uint32_t mb_get_timeout(const mb_t *mb)
{
    if (!mb) {
        return 0;
    }

    return mb->options.timeout_ms;
}

void mb_set_logging(mb_t *mb, bool enable)
{
    if (!mb) {
        return;
    }

    mb->options.enable_logging = enable;
    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (mb->client && backend && backend->enable_logging) {
        backend->enable_logging(mb->client, enable);
    }
}

/* ========================================================================== */
/*                           ERROR HANDLING                                   */
/* ========================================================================== */

const char *mb_error_string(mb_err_t err)
{
    const mb_simple_backend_t *backend = mb_simple_get_backend();
    if (backend && backend->error_string) {
        return backend->error_string(err);
    }
    return mb_err_str(err);
}

uint8_t mb_last_exception(const mb_t *mb)
{
    if (!mb) {
        return 0;
    }

    return mb->last_exception;
}
