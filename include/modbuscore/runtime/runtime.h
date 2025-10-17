#ifndef MODBUSCORE_RUNTIME_RUNTIME_H
#define MODBUSCORE_RUNTIME_RUNTIME_H

/**
 * @file runtime.h
 * @brief Runtime builder e guardião das dependências essenciais.
 */

#include <stdbool.h>
#include <string.h>

#include <modbuscore/common/status.h>
#include <modbuscore/runtime/dependencies.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mbc_runtime_config {
    mbc_transport_iface_t transport;
    mbc_clock_iface_t clock;
    mbc_allocator_iface_t allocator;
    mbc_logger_iface_t logger;
} mbc_runtime_config_t;

typedef struct mbc_runtime {
    bool initialised;
    mbc_runtime_config_t deps;
} mbc_runtime_t;

mbc_status_t mbc_runtime_init(mbc_runtime_t *runtime, const mbc_runtime_config_t *config);
void mbc_runtime_shutdown(mbc_runtime_t *runtime);
bool mbc_runtime_is_ready(const mbc_runtime_t *runtime);
const mbc_runtime_config_t *mbc_runtime_dependencies(const mbc_runtime_t *runtime);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_RUNTIME_RUNTIME_H */
