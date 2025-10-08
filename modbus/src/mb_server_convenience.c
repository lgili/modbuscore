/**
 * @file mb_server_convenience.c
 * @brief Implementation of convenience helpers for Modbus server setup.
 *
 * This provides simplified server initialization by wrapping the mapping API
 * and providing sensible defaults for pool sizes.
 */

#include <modbus/mb_server_convenience.h>
#include <modbus/server.h>
#include <modbus/mapping.h>
#include <modbus/mb_err.h>

#include <string.h>
#include <stdlib.h>

#if MB_CONF_BUILD_SERVER

/* Default pool sizes */
#define DEFAULT_MAX_REGIONS  16
#define DEFAULT_MAX_REQUESTS 8

/* Internal storage for convenience wrappers */
typedef struct {
    mb_server_region_t *regions;
    mb_server_request_t *requests;
    uint16_t region_capacity;
    uint16_t request_capacity;
    const mb_transport_if_t *iface;
    void *transport_ctx;  /* TCP or RTU transport context */
} mb_server_convenience_ctx_t;

/* Static storage for single server instance (simplified) */
static mb_server_convenience_ctx_t g_server_ctx;
static bool g_server_initialized = false;

/* -------------------------------------------------------------------------- */
/*                       Server Creation (TCP)                                */
/* -------------------------------------------------------------------------- */

#if MB_CONF_TRANSPORT_TCP

mb_err_t mb_server_create_tcp(mb_server_t *server,
                               uint16_t port,
                               uint8_t unit_id)
{
    if (server == NULL || port == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_server_tcp_config_t config = {
        .port = port,
        .unit_id = unit_id,
        .max_regions = DEFAULT_MAX_REGIONS,
        .max_requests = DEFAULT_MAX_REQUESTS
    };

    return mb_server_create_tcp_ex(server, &config);
}

mb_err_t mb_server_create_tcp_ex(mb_server_t *server,
                                  const mb_server_tcp_config_t *config)
{
    if (server == NULL || config == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (g_server_initialized) {
        return MB_ERR_OTHER; /* Only one server supported currently */
    }

    /* Allocate region pool */
    g_server_ctx.region_capacity = config->max_regions;
    g_server_ctx.regions = (mb_server_region_t *)calloc(
        config->max_regions, sizeof(mb_server_region_t));
    if (g_server_ctx.regions == NULL) {
        return MB_ERR_NO_RESOURCES;
    }

    /* Allocate request pool */
    g_server_ctx.request_capacity = config->max_requests;
    g_server_ctx.requests = (mb_server_request_t *)calloc(
        config->max_requests, sizeof(mb_server_request_t));
    if (g_server_ctx.requests == NULL) {
        free(g_server_ctx.regions);
        return MB_ERR_NO_RESOURCES;
    }

    /* Note: TCP transport creation is platform-specific and complex.
     * For a complete implementation, we would need:
     * 1. Socket creation and bind
     * 2. Listen setup
     * 3. Accept loop management
     * 4. Multi-client connection tracking
     *
     * This is beyond a simple convenience wrapper and requires
     * significant platform-specific code.
     *
     * For now, we provide the structure but defer full implementation
     * to examples like tcp_server_demo.c which show proper setup.
     */
    
    /* Initialize mapping config */
    mb_server_mapping_config_t mapping_config = {
        .iface = NULL,  /* Would be set after transport creation */
        .unit_id = config->unit_id,
        .regions = g_server_ctx.regions,
        .region_capacity = g_server_ctx.region_capacity,
        .request_pool = g_server_ctx.requests,
        .request_capacity = g_server_ctx.request_capacity,
        .banks = NULL,
        .bank_count = 0
    };

    mb_err_t err = mb_server_mapping_init(server, &mapping_config);
    if (err != MB_OK) {
        free(g_server_ctx.regions);
        free(g_server_ctx.requests);
        return err;
    }

    g_server_initialized = true;
    return MB_OK;
}

#endif /* MB_CONF_TRANSPORT_TCP */

/* -------------------------------------------------------------------------- */
/*                       Server Creation (RTU)                                */
/* -------------------------------------------------------------------------- */

#if MB_CONF_TRANSPORT_RTU

mb_err_t mb_server_create_rtu(mb_server_t *server,
                               const char *device,
                               uint32_t baudrate,
                               uint8_t unit_id)
{
    if (server == NULL || device == NULL || baudrate == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_server_rtu_config_t config = {
        .device = device,
        .baudrate = baudrate,
        .unit_id = unit_id,
        .max_regions = DEFAULT_MAX_REGIONS,
        .max_requests = DEFAULT_MAX_REQUESTS
    };

    return mb_server_create_rtu_ex(server, &config);
}

mb_err_t mb_server_create_rtu_ex(mb_server_t *server,
                                  const mb_server_rtu_config_t *config)
{
    if (server == NULL || config == NULL || config->device == NULL) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    if (g_server_initialized) {
        return MB_ERR_OTHER; /* Only one server supported currently */
    }

    /* Allocate pools */
    g_server_ctx.region_capacity = config->max_regions;
    g_server_ctx.regions = (mb_server_region_t *)calloc(
        config->max_regions, sizeof(mb_server_region_t));
    if (g_server_ctx.regions == NULL) {
        return MB_ERR_NO_RESOURCES;
    }

    g_server_ctx.request_capacity = config->max_requests;
    g_server_ctx.requests = (mb_server_request_t *)calloc(
        config->max_requests, sizeof(mb_server_request_t));
    if (g_server_ctx.requests == NULL) {
        free(g_server_ctx.regions);
        return MB_ERR_NO_RESOURCES;
    }

    /* Note: RTU transport would require:
     * 1. Serial port opening (mb_port_posix_serial_open)
     * 2. RTU transport initialization
     * 3. Proper timing configuration
     *
     * Similar to TCP, this needs platform-specific implementation.
     */

    mb_server_mapping_config_t mapping_config = {
        .iface = NULL,  /* Would be set after transport creation */
        .unit_id = config->unit_id,
        .regions = g_server_ctx.regions,
        .region_capacity = g_server_ctx.region_capacity,
        .request_pool = g_server_ctx.requests,
        .request_capacity = g_server_ctx.request_capacity,
        .banks = NULL,
        .bank_count = 0
    };

    mb_err_t err = mb_server_mapping_init(server, &mapping_config);
    if (err != MB_OK) {
        free(g_server_ctx.regions);
        free(g_server_ctx.requests);
        return err;
    }

    g_server_initialized = true;
    return MB_OK;
}

#endif /* MB_CONF_TRANSPORT_RTU */

/* -------------------------------------------------------------------------- */
/*                       Data Region Registration                             */
/* -------------------------------------------------------------------------- */

#if MB_CONF_ENABLE_FC03 || MB_CONF_ENABLE_FC06 || MB_CONF_ENABLE_FC10 || MB_CONF_ENABLE_FC16
mb_err_t mb_server_add_holding(mb_server_t *server,
                                uint16_t start_addr,
                                uint16_t *registers,
                                uint16_t count)
{
    if (server == NULL || registers == NULL || count == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Create bank descriptor
    mb_server_mapping_bank_t bank = {
        .start = start_addr,
        .count = count,
        .storage = registers,
        .read_only = false
    };

    // Register single bank (wraps mb_server_mapping_apply)
    return mb_server_mapping_apply(server, &bank, 1);
}
#endif

#if MB_CONF_ENABLE_FC04
mb_err_t mb_server_add_input(mb_server_t *server,
                              uint16_t start_addr,
                              const uint16_t *registers,
                              uint16_t count)
{
    if (server == NULL || registers == NULL || count == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // TODO: The mapping API's mb_server_mapping_bank_t.storage is not const,
    // but we want read-only input registers. This needs API adjustment.
    // For now, we trust that read_only=true prevents modifications.
    
    mb_server_mapping_bank_t bank;
    bank.start = start_addr;
    bank.count = count;
    bank.read_only = true;
    
    // Copy pointer without const (safe because read_only=true)
    // NOLINTBEGIN(bugprone-multi-level-implicit-pointer-conversion)
    memcpy(&bank.storage, &registers, sizeof(void*));
    // NOLINTEND(bugprone-multi-level-implicit-pointer-conversion)

    return mb_server_mapping_apply(server, &bank, 1);
}
#endif

#if MB_CONF_ENABLE_FC01 || MB_CONF_ENABLE_FC05 || MB_CONF_ENABLE_FC0F
mb_err_t mb_server_add_coils(mb_server_t *server,
                              uint16_t start_addr,
                              uint8_t *coils,
                              uint16_t count)
{
    if (server == NULL || coils == NULL || count == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Note: Current mapping API is for registers only.
    // Real implementation would need to use mb_server_add_storage()
    // with proper data type for coils.
    (void)server;
    (void)start_addr;
    (void)coils;
    (void)count;

    // TODO: Implement when coil mapping API is available
    return MB_ERR_OTHER;
}
#endif

#if MB_CONF_ENABLE_FC02
mb_err_t mb_server_add_discrete(mb_server_t *server,
                                 uint16_t start_addr,
                                 const uint8_t *inputs,
                                 uint16_t count)
{
    if (server == NULL || inputs == NULL || count == 0) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    // Similar to coils - needs proper API extension
    (void)server;
    (void)start_addr;
    (void)inputs;
    (void)count;

    // TODO: Implement when discrete input mapping API is available
    return MB_ERR_OTHER;
}
#endif

/* -------------------------------------------------------------------------- */
/*                            Cleanup                                         */
/* -------------------------------------------------------------------------- */

void mb_server_convenience_destroy(mb_server_t *server)
{
    if (server == NULL) {
        return;
    }

    if (!g_server_initialized) {
        return;
    }

    /* Free allocated pools */
    if (g_server_ctx.regions != NULL) {
        free(g_server_ctx.regions);
        g_server_ctx.regions = NULL;
    }

    if (g_server_ctx.requests != NULL) {
        free(g_server_ctx.requests);
        g_server_ctx.requests = NULL;
    }

    /* Close transport if allocated */
    if (g_server_ctx.transport_ctx != NULL) {
        /* Transport-specific cleanup would go here */
        g_server_ctx.transport_ctx = NULL;
    }

    /* Clear server structure */
    memset(server, 0, sizeof(mb_server_t));
    
    /* Reset context */
    memset(&g_server_ctx, 0, sizeof(g_server_ctx));
    g_server_initialized = false;
}

#endif /* MB_CONF_BUILD_SERVER */
