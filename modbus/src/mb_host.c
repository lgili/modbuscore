/**
 * @file mb_host.c
 * @brief Implementation of simplified Modbus API for host applications (desktop/Linux/macOS/Windows).
 *
 * This wrapper layer sits on top of the full mb_client_* API and provides
 * a synchronous, blocking interface suitable for desktop applications and
 * simple prototypes. It handles transport setup, blocking I/O, and error
 * translation automatically.
 */

#include <modbus/mb_host.h>
#include <modbus/client.h>
#include <modbus/transport.h>
#include <modbus/mb_err.h>
#include <modbus/mb_log.h>
#include <modbus/pdu.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    // Disable deprecation warnings for standard C functions on MSVC
    #pragma warning(disable: 4996)
#else
    #include <unistd.h>
#endif

#if MB_CONF_TRANSPORT_TCP
#include <modbus/transport/tcp.h>
#include <modbus/port/posix.h>
#endif

#if MB_CONF_TRANSPORT_RTU
#include <modbus/transport/rtu.h>
#include <modbus/port/posix.h>
#endif

/* -------------------------------------------------------------------------- */
/*                            Internal Structures                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Internal client context managed by mb_host API.
 */
struct mb_host_client {
    mb_client_t client;              /**< Core client FSM */
    mb_port_posix_socket_t socket;   /**< POSIX socket wrapper */
    
    uint32_t timeout_ms;             /**< Default timeout for blocking operations */
    bool logging_enabled;            /**< Console logging flag */
    
    uint8_t last_exception;          /**< Last exception code from server */
    mb_client_txn_t txn_pool[4];     /**< Small transaction pool for blocking ops */
};

/* -------------------------------------------------------------------------- */
/*                              Helper Functions                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Parse "host:port" string into separate host and port.
 */
static int parse_host_port(const char *host_port, char *host, size_t host_len, uint16_t *port)
{
    if (host_port == NULL || host == NULL || port == NULL) {
        return -1;
    }

    // Find colon separator
    const char *colon = strchr(host_port, ':');
    if (colon == NULL) {
        // No port specified, use default 502
        size_t len = strlen(host_port);
        if (len >= host_len) {
            return -1; // Host too long
        }
        memcpy(host, host_port, len);
        host[len] = '\0';
        *port = 502;
        return 0;
    }

    // Extract host
    size_t host_part_len = (size_t)(colon - host_port);
    if (host_part_len >= host_len) {
        return -1; // Host too long
    }
    // Use memcpy instead of strncpy to avoid MSVC warnings
    memcpy(host, host_port, host_part_len);
    host[host_part_len] = '\0';

    // Extract port
    *port = (uint16_t)atoi(colon + 1);
    if (*port == 0) {
        return -1; // Invalid port
    }

    return 0;
}

/**
 * @brief Wait for a transaction to complete with timeout.
 */
static mb_err_t wait_for_transaction(mb_host_client_t *ctx, mb_client_txn_t *txn, uint32_t timeout_ms)
{
    if (ctx == NULL || txn == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_transport_if_t *iface = mb_port_posix_socket_iface(&ctx->socket);
    if (iface == NULL || iface->now == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_time_ms_t start_time = iface->now(iface->ctx);
    const mb_time_ms_t deadline = start_time + timeout_ms;

    // Poll the client FSM until transaction completes or times out
    while (1) {
        // Poll the client FSM
        mb_client_poll(&ctx->client);

        // Check transaction state
        if (!txn->in_use || txn->cancelled) {
            // Transaction completed or was cancelled
            if (txn->completed) {
                return txn->rx_status;
            } else {
                return MB_ERR_TIMEOUT;
            }
        }

        // Check timeout
        const mb_time_ms_t now = iface->now(iface->ctx);
        if (now >= deadline) {
            // Cancel the transaction
            mb_client_cancel(&ctx->client, txn);
            return MB_ERR_TIMEOUT;
        }

        // Small sleep to avoid busy-waiting
        #ifdef _WIN32
        Sleep(1);
        #else
        usleep(1000);
        #endif
    }
}

/* -------------------------------------------------------------------------- */
/*                            Connection Management                           */
/* -------------------------------------------------------------------------- */

#if MB_CONF_TRANSPORT_TCP
mb_host_client_t *mb_host_tcp_connect(const char *host_port)
{
    if (host_port == NULL) {
        errno = EINVAL;
        return NULL;
    }

    // Parse host and port
    char host[256];
    uint16_t port;
    if (parse_host_port(host_port, host, sizeof(host), &port) != 0) {
        errno = EINVAL;
        return NULL;
    }

    // Allocate client context
    mb_host_client_t *ctx = (mb_host_client_t *)calloc(1, sizeof(mb_host_client_t));
    if (ctx == NULL) {
        return NULL;
    }

    // Connect to TCP server
    mb_err_t err = mb_port_posix_tcp_client(&ctx->socket, host, port, 5000);
    if (!mb_err_is_ok(err)) {
        free(ctx);
        errno = ECONNREFUSED;
        return NULL;
    }

    // Get transport interface
    const mb_transport_if_t *iface = mb_port_posix_socket_iface(&ctx->socket);

    // Initialize client FSM with TCP transport
    err = mb_client_init_tcp(&ctx->client, iface, ctx->txn_pool, 4);
    if (!mb_err_is_ok(err)) {
        mb_port_posix_socket_close(&ctx->socket);
        free(ctx);
        errno = EINVAL;
        return NULL;
    }

    // Set defaults
    ctx->timeout_ms = 1000; // 1 second default timeout
    ctx->logging_enabled = false;
    ctx->last_exception = 0;

    return ctx;
}
#else
mb_host_client_t *mb_host_tcp_connect(const char *host_port)
{
    (void)host_port;
    errno = ENOTSUP;
    return NULL;
}
#endif

#if MB_CONF_TRANSPORT_RTU
mb_host_client_t *mb_host_rtu_connect(const char *device, uint32_t baudrate)
{
    if (device == NULL || baudrate == 0) {
        errno = EINVAL;
        return NULL;
    }

    // Allocate client context
    mb_host_client_t *ctx = (mb_host_client_t *)calloc(1, sizeof(mb_host_client_t));
    if (ctx == NULL) {
        return NULL;
    }

    // Open serial port
    mb_err_t err = mb_port_posix_serial_open(&ctx->socket, device, baudrate, 
                                             MB_PARITY_NONE, 8, 1);
    if (!mb_err_is_ok(err)) {
        free(ctx);
        errno = ENOENT;
        return NULL;
    }

    // Get transport interface
    const mb_transport_if_t *iface = mb_port_posix_socket_iface(&ctx->socket);

    // Initialize client FSM with RTU transport
    err = mb_client_init(&ctx->client, iface, ctx->txn_pool, 4);
    if (!mb_err_is_ok(err)) {
        mb_port_posix_socket_close(&ctx->socket);
        free(ctx);
        errno = EINVAL;
        return NULL;
    }

    // Set defaults
    ctx->timeout_ms = 1000; // 1 second default timeout
    ctx->logging_enabled = false;
    ctx->last_exception = 0;

    return ctx;
}
#else
mb_host_client_t *mb_host_rtu_connect(const char *device, uint32_t baudrate)
{
    (void)device;
    (void)baudrate;
    errno = ENOTSUP;
    return NULL;
}
#endif

void mb_host_disconnect(mb_host_client_t *client)
{
    if (client == NULL) {
        return;
    }

    // Close socket (works for both TCP and RTU)
    mb_port_posix_socket_close(&client->socket);

    // Free context
    free(client);
}

/* -------------------------------------------------------------------------- */
/*                          Synchronous Read Operations                       */
/* -------------------------------------------------------------------------- */

mb_err_t mb_host_read_holding(mb_host_client_t *client,
                               uint8_t unit_id,
                               uint16_t address,
                               uint16_t count,
                               uint16_t *out_registers)
{
    if (client == NULL || out_registers == NULL || count == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Submit request using convenience API
    mb_client_txn_t *txn = NULL;
    mb_err_t err = mb_client_read_holding_registers(&client->client, unit_id, address, count, &txn);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Wait for completion
    err = wait_for_transaction(client, txn, client->timeout_ms);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Extract data from response
    if (txn->rx_status == MB_OK) {
        const uint8_t *payload = txn->rx_view.payload;
        const size_t byte_count = txn->rx_view.payload_len;
        
        if (byte_count < (size_t)(count * 2)) {  // NOLINT(bugprone-implicit-widening-of-multiplication-result)
            return MB_ERR_INVALID_REQUEST;
        }

        for (uint16_t i = 0; i < count; i++) {
            out_registers[i] = ((uint16_t)payload[(size_t)i * 2] << 8) | payload[(size_t)i * 2 + 1];  // NOLINT(bugprone-implicit-widening-of-multiplication-result)
        }
        return MB_OK;
    } else if (mb_err_is_exception(txn->rx_status)) {
        client->last_exception = txn->rx_status;
        return txn->rx_status;
    }

    return txn->rx_status;
}

mb_err_t mb_host_read_input(mb_host_client_t *client,
                             uint8_t unit_id,
                             uint16_t address,
                             uint16_t count,
                             uint16_t *out_registers)
{
    if (client == NULL || out_registers == NULL || count == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Submit request using convenience API
    mb_client_txn_t *txn = NULL;
    mb_err_t err = mb_client_read_input_registers(&client->client, unit_id, address, count, &txn);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Wait for completion
    err = wait_for_transaction(client, txn, client->timeout_ms);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Extract data from response
    if (txn->rx_status == MB_OK) {
        const uint8_t *payload = txn->rx_view.payload;
        const size_t byte_count = txn->rx_view.payload_len;
        
        if (byte_count < (size_t)(count * 2)) {  // NOLINT(bugprone-implicit-widening-of-multiplication-result)
            return MB_ERR_INVALID_REQUEST;
        }

        for (uint16_t i = 0; i < count; i++) {
            out_registers[i] = ((uint16_t)payload[(size_t)i * 2] << 8) | payload[(size_t)i * 2 + 1];  // NOLINT(bugprone-implicit-widening-of-multiplication-result)
        }
        return MB_OK;
    } else if (mb_err_is_exception(txn->rx_status)) {
        client->last_exception = txn->rx_status;
        return txn->rx_status;
    }

    return txn->rx_status;
}

mb_err_t mb_host_read_coils(mb_host_client_t *client,
                             uint8_t unit_id,
                             uint16_t address,
                             uint16_t count,
                             uint8_t *out_coils)
{
    if (client == NULL || out_coils == NULL || count == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Submit request using convenience API
    mb_client_txn_t *txn = NULL;
    mb_err_t err = mb_client_read_coils(&client->client, unit_id, address, count, &txn);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Wait for completion
    err = wait_for_transaction(client, txn, client->timeout_ms);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Extract data from response
    if (txn->rx_status == MB_OK) {
        const uint8_t *payload = txn->rx_view.payload;
        const size_t byte_count = txn->rx_view.payload_len;
        const size_t expected_bytes = (count + 7) / 8;
        
        if (byte_count < expected_bytes) {
            return MB_ERR_INVALID_REQUEST;
        }

        memcpy(out_coils, payload, expected_bytes);
        return MB_OK;
    } else if (mb_err_is_exception(txn->rx_status)) {
        client->last_exception = txn->rx_status;
        return txn->rx_status;
    }

    return txn->rx_status;
}

mb_err_t mb_host_read_discrete(mb_host_client_t *client,
                               uint8_t unit_id,
                               uint16_t address,
                               uint16_t count,
                               uint8_t *out_inputs)
{
    if (client == NULL || out_inputs == NULL || count == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Submit request using convenience API
    mb_client_txn_t *txn = NULL;
    mb_err_t err = mb_client_read_discrete_inputs(&client->client, unit_id, address, count, &txn);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Wait for completion
    err = wait_for_transaction(client, txn, client->timeout_ms);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Extract data from response
    if (txn->rx_status == MB_OK) {
        const uint8_t *payload = txn->rx_view.payload;
        const size_t byte_count = txn->rx_view.payload_len;
        const size_t expected_bytes = (count + 7) / 8;
        
        if (byte_count < expected_bytes) {
            return MB_ERR_INVALID_REQUEST;
        }

        memcpy(out_inputs, payload, expected_bytes);
        return MB_OK;
    } else if (mb_err_is_exception(txn->rx_status)) {
        client->last_exception = txn->rx_status;
        return txn->rx_status;
    }

    return txn->rx_status;
}

/* -------------------------------------------------------------------------- */
/*                          Synchronous Write Operations                      */
/* -------------------------------------------------------------------------- */

mb_err_t mb_host_write_single_register(mb_host_client_t *client,
                                       uint8_t unit_id,
                                       uint16_t address,
                                       uint16_t value)
{
    if (client == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Submit request using convenience API
    mb_client_txn_t *txn = NULL;
    mb_err_t err = mb_client_write_single_register(&client->client, unit_id, address, value, &txn);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Wait for completion
    err = wait_for_transaction(client, txn, client->timeout_ms);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    if (mb_err_is_exception(txn->rx_status)) {
        client->last_exception = txn->rx_status;
    }

    return txn->rx_status;
}

mb_err_t mb_host_write_single_coil(mb_host_client_t *client,
                                   uint8_t unit_id,
                                   uint16_t address,
                                   bool value)
{
    if (client == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Submit request using convenience API
    mb_client_txn_t *txn = NULL;
    mb_err_t err = mb_client_write_single_coil(&client->client, unit_id, address, value, &txn);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Wait for completion
    err = wait_for_transaction(client, txn, client->timeout_ms);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    if (mb_err_is_exception(txn->rx_status)) {
        client->last_exception = txn->rx_status;
    }

    return txn->rx_status;
}

mb_err_t mb_host_write_multiple_registers(mb_host_client_t *client,
                                          uint8_t unit_id,
                                          uint16_t address,
                                          uint16_t count,
                                          const uint16_t *registers)
{
    if (client == NULL || registers == NULL || count == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Submit request using convenience API
    mb_client_txn_t *txn = NULL;
    mb_err_t err = mb_client_write_multiple_registers(&client->client, unit_id, address, count, registers, &txn);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Wait for completion
    err = wait_for_transaction(client, txn, client->timeout_ms);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    if (mb_err_is_exception(txn->rx_status)) {
        client->last_exception = txn->rx_status;
    }

    return txn->rx_status;
}

mb_err_t mb_host_write_multiple_coils(mb_host_client_t *client,
                                      uint8_t unit_id,
                                      uint16_t address,
                                      uint16_t count,
                                      const uint8_t *coils)
{
    if (client == NULL || coils == NULL || count == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Submit request using convenience API
    mb_client_txn_t *txn = NULL;
    mb_err_t err = mb_client_write_multiple_coils(&client->client, unit_id, address, count, coils, &txn);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    // Wait for completion
    err = wait_for_transaction(client, txn, client->timeout_ms);
    if (!mb_err_is_ok(err)) {
        return err;
    }

    if (mb_err_is_exception(txn->rx_status)) {
        client->last_exception = txn->rx_status;
    }

    return txn->rx_status;
}

/* -------------------------------------------------------------------------- */
/*                              Configuration                                 */
/* -------------------------------------------------------------------------- */

void mb_host_set_timeout(mb_host_client_t *client, uint32_t timeout_ms)
{
    if (client != NULL) {
        client->timeout_ms = timeout_ms;
    }
}

void mb_host_enable_logging(mb_host_client_t *client, bool enable)
{
    if (client != NULL) {
        client->logging_enabled = enable;
        // TODO: Enable/disable logging in underlying transport
    }
}

/* -------------------------------------------------------------------------- */
/*                              Error Handling                                */
/* -------------------------------------------------------------------------- */

const char *mb_host_error_string(mb_err_t err)
{
    return mb_err_str(err);
}

uint8_t mb_host_last_exception(mb_host_client_t *client)
{
    if (client == NULL) {
        return 0;
    }
    return client->last_exception;
}
