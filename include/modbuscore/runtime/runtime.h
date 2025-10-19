#ifndef MODBUSCORE_RUNTIME_RUNTIME_H
#define MODBUSCORE_RUNTIME_RUNTIME_H

/**
 * @file runtime.h
 * @brief Runtime builder and guardian of core dependencies.
 *
 * This module manages the core dependencies required by the protocol engine:
 * - Transport layer (send/receive)
 * - Clock (timestamps)
 * - Allocator (memory management)
 * - Logger (diagnostics)
 */

#include <modbuscore/common/status.h>
#include <modbuscore/runtime/dependencies.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Runtime configuration with all core dependencies.
 */
typedef struct mbc_runtime_config {
    mbc_transport_iface_t transport; /**< Transport layer interface */
    mbc_clock_iface_t clock;         /**< Clock interface for timestamps */
    mbc_allocator_iface_t allocator; /**< Memory allocator interface */
    mbc_logger_iface_t logger;       /**< Logger interface */
} mbc_runtime_config_t;

/**
 * @brief Runtime state.
 *
 * This structure should be treated as opaque. Use the provided API functions.
 */
typedef struct mbc_runtime {
    bool initialised;          /**< Initialization flag */
    mbc_runtime_config_t deps; /**< Stored dependencies */
} mbc_runtime_t;

/**
 * @brief Initialize runtime with dependencies.
 *
 * @param runtime Pointer to runtime structure
 * @param config Configuration with all dependencies
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_runtime_init(mbc_runtime_t* runtime, const mbc_runtime_config_t* config);

/**
 * @brief Shutdown runtime and release resources.
 *
 * @param runtime Pointer to runtime structure
 */
void mbc_runtime_shutdown(mbc_runtime_t* runtime);

/**
 * @brief Check if runtime is initialized and ready.
 *
 * @param runtime Pointer to runtime structure
 * @return true if ready, false otherwise
 */
bool mbc_runtime_is_ready(const mbc_runtime_t* runtime);

/**
 * @brief Get runtime dependencies.
 *
 * @param runtime Pointer to runtime structure
 * @return Pointer to runtime config, or NULL if not initialized
 */
const mbc_runtime_config_t* mbc_runtime_dependencies(const mbc_runtime_t* runtime);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_RUNTIME_RUNTIME_H */
