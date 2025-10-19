#ifndef MODBUSCORE_RUNTIME_BUILDER_H
#define MODBUSCORE_RUNTIME_BUILDER_H

/**
 * @file builder.h
 * @brief Builder utility for configuring `mbc_runtime_t` with safe defaults.
 *
 * This builder pattern allows incremental configuration of runtime dependencies.
 * If optional dependencies (clock, allocator, logger) are not set, safe defaults
 * will be provided automatically when calling mbc_runtime_builder_build().
 *
 * The transport layer is mandatory and must be set explicitly.
 */

#include <modbuscore/common/status.h>
#include <modbuscore/runtime/runtime.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Runtime builder state.
 */
typedef struct mbc_runtime_builder {
    mbc_runtime_config_t config; /**< Runtime configuration being built */
    bool transport_set;          /**< True if transport has been set */
    bool clock_set;              /**< True if clock has been set */
    bool allocator_set;          /**< True if allocator has been set */
    bool logger_set;             /**< True if logger has been set */
} mbc_runtime_builder_t;

/**
 * @brief Initialize runtime builder.
 *
 * @param builder Pointer to builder structure
 */
void mbc_runtime_builder_init(mbc_runtime_builder_t* builder);

/**
 * @brief Set transport layer (required).
 *
 * @param builder Pointer to builder structure
 * @param transport Transport interface
 * @return Builder pointer for chaining
 */
mbc_runtime_builder_t* mbc_runtime_builder_with_transport(mbc_runtime_builder_t* builder,
                                                          const mbc_transport_iface_t* transport);

/**
 * @brief Set clock interface (optional, defaults to system clock).
 *
 * @param builder Pointer to builder structure
 * @param clock Clock interface
 * @return Builder pointer for chaining
 */
mbc_runtime_builder_t* mbc_runtime_builder_with_clock(mbc_runtime_builder_t* builder,
                                                      const mbc_clock_iface_t* clock);

/**
 * @brief Set allocator interface (optional, defaults to malloc/free).
 *
 * @param builder Pointer to builder structure
 * @param allocator Allocator interface
 * @return Builder pointer for chaining
 */
mbc_runtime_builder_t* mbc_runtime_builder_with_allocator(mbc_runtime_builder_t* builder,
                                                          const mbc_allocator_iface_t* allocator);

/**
 * @brief Set logger interface (optional, defaults to no-op).
 *
 * @param builder Pointer to builder structure
 * @param logger Logger interface
 * @return Builder pointer for chaining
 */
mbc_runtime_builder_t* mbc_runtime_builder_with_logger(mbc_runtime_builder_t* builder,
                                                       const mbc_logger_iface_t* logger);

/**
 * @brief Build runtime from builder configuration.
 *
 * This function fills in any missing dependencies with safe defaults and
 * initializes the runtime.
 *
 * @param builder Pointer to builder structure
 * @param runtime Pointer to runtime structure to initialize
 * @return MBC_STATUS_OK on success, error code otherwise
 */
mbc_status_t mbc_runtime_builder_build(mbc_runtime_builder_t* builder, mbc_runtime_t* runtime);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_RUNTIME_BUILDER_H */
