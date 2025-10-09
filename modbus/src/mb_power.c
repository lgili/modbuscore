/**
 * @file mb_power.c
 * @brief Implementation of power management and idle detection
 */

#include <modbus/mb_power.h>

#if MB_CONF_BUILD_CLIENT
#include <modbus/client.h>
#endif

#if MB_CONF_BUILD_SERVER
#include <modbus/server.h>
#endif

#include <modbus/mb_err.h>
#include <string.h>
#include <stdint.h>

/* ========================================================================== */
/*                          Client Power Management                           */
/* ========================================================================== */

#if MB_CONF_BUILD_CLIENT

mb_err_t mb_client_set_idle_callback(void *client_ptr,
                                      mb_idle_callback_t callback,
                                      void *user_ctx,
                                      mb_u32 threshold_ms)
{
    if (client_ptr == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_client_t *client = (mb_client_t *)client_ptr;

    client->idle_config.callback = callback;
    client->idle_config.user_ctx = user_ctx;
    client->idle_config.threshold_ms = threshold_ms;
    client->idle_config.enabled = (callback != NULL);

    return MB_OK;
}

mb_err_t mb_client_get_idle_config(const void *client_ptr,
                                    mb_idle_config_t *config)
{
    if (client_ptr == NULL || config == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_client_t *client = (const mb_client_t *)client_ptr;
    memcpy(config, &client->idle_config, sizeof(mb_idle_config_t));

    return MB_OK;
}

/* Note: mb_client_is_idle() is implemented in client.c */

mb_u32 mb_client_time_until_next_event(const void *client_ptr)
{
    /* TODO: Implement based on client state and pending transactions */
    (void)client_ptr;
    return UINT32_MAX;
}

/**
 * @brief Internal helper to invoke client idle callback if conditions are met
 * 
 * This function is called from mb_client_poll() to potentially invoke
 * the user's idle callback when the client has no work to do.
 * 
 * @param client_ptr Client instance
 * @return Actual sleep time in ms, or 0 if callback not invoked
 */
mb_u32 mb_client_invoke_idle_callback_internal(void *client_ptr)
{
#if MB_CONF_ENABLE_POWER_MANAGEMENT
    mb_client_t *client = (mb_client_t *)client_ptr;
    if (client == NULL || !client->idle_config.enabled || client->idle_config.callback == NULL) {
        return 0;
    }

    // Check if client is idle
    if (!mb_client_is_idle(client)) {
        return 0;
    }

    // Get time until next event
    mb_u32 sleep_ms = mb_client_time_until_next_event(client);

    // Check if sleep time exceeds threshold
    if (sleep_ms < client->idle_config.threshold_ms) {
        return 0;
    }

    // Invoke user callback
    mb_u32 actual_sleep = client->idle_config.callback(
        client->idle_config.user_ctx,
        sleep_ms
    );

    return actual_sleep;
#else
    (void)client_ptr;
    return 0;
#endif
}

#endif /* MB_CONF_BUILD_CLIENT */

/* ========================================================================== */
/*                          Server Power Management                           */
/* ========================================================================== */

#if MB_CONF_BUILD_SERVER

mb_err_t mb_server_set_idle_callback(void *server_ptr,
                                      mb_idle_callback_t callback,
                                      void *user_ctx,
                                      mb_u32 threshold_ms)
{
    if (server_ptr == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_server_t *server = (mb_server_t *)server_ptr;

    server->idle_config.callback = callback;
    server->idle_config.user_ctx = user_ctx;
    server->idle_config.threshold_ms = threshold_ms;
    server->idle_config.enabled = (callback != NULL);

    return MB_OK;
}

mb_err_t mb_server_get_idle_config(const void *server_ptr,
                                    mb_idle_config_t *config)
{
    if (server_ptr == NULL || config == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    const mb_server_t *server = (const mb_server_t *)server_ptr;
    memcpy(config, &server->idle_config, sizeof(mb_idle_config_t));

    return MB_OK;
}

/* Note: mb_server_is_idle() is implemented in server.c */

mb_u32 mb_server_time_until_next_event(const void *server_ptr)
{
    /* TODO: Implement based on server state */
    (void)server_ptr;
    return UINT32_MAX;
}

/**
 * @brief Internal helper to invoke server idle callback if conditions are met
 * 
 * This function is called from mb_server_poll() to potentially invoke
 * the user's idle callback when the server has no work to do.
 * 
 * @param server_ptr Server instance
 * @return Actual sleep time in ms, or 0 if callback not invoked
 */
mb_u32 mb_server_invoke_idle_callback_internal(void *server_ptr)
{
#if MB_CONF_ENABLE_POWER_MANAGEMENT
    mb_server_t *server = (mb_server_t *)server_ptr;
    if (server == NULL || !server->idle_config.enabled || server->idle_config.callback == NULL) {
        return 0;
    }

    // Check if server is idle
    if (!mb_server_is_idle(server)) {
        return 0;
    }

    // Get time until next event
    mb_u32 sleep_ms = mb_server_time_until_next_event(server);

    // Check if sleep time exceeds threshold
    if (sleep_ms < server->idle_config.threshold_ms) {
        return 0;
    }

    // Invoke user callback
    mb_u32 actual_sleep = server->idle_config.callback(
        server->idle_config.user_ctx,
        sleep_ms
    );

    return actual_sleep;
#else
    (void)server_ptr;
    return 0;
#endif
}

#endif /* MB_CONF_BUILD_SERVER */
