#ifndef MODBUSCORE_RUNTIME_BUILDER_H
#define MODBUSCORE_RUNTIME_BUILDER_H

/**
 * @file builder.h
 * @brief Builder utilitário para configurar `mbc_runtime_t` com defaults seguros.
 */

#include <stdbool.h>

#include <modbuscore/common/status.h>
#include <modbuscore/runtime/runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mbc_runtime_builder {
    mbc_runtime_config_t config;
    bool transport_set;
    bool clock_set;
    bool allocator_set;
    bool logger_set;
} mbc_runtime_builder_t;

void mbc_runtime_builder_init(mbc_runtime_builder_t *builder);

mbc_runtime_builder_t *mbc_runtime_builder_with_transport(mbc_runtime_builder_t *builder,
                                                          const mbc_transport_iface_t *transport);
mbc_runtime_builder_t *mbc_runtime_builder_with_clock(mbc_runtime_builder_t *builder,
                                                      const mbc_clock_iface_t *clock);
mbc_runtime_builder_t *mbc_runtime_builder_with_allocator(mbc_runtime_builder_t *builder,
                                                          const mbc_allocator_iface_t *allocator);
mbc_runtime_builder_t *mbc_runtime_builder_with_logger(mbc_runtime_builder_t *builder,
                                                       const mbc_logger_iface_t *logger);

mbc_status_t mbc_runtime_builder_build(mbc_runtime_builder_t *builder, mbc_runtime_t *runtime);

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_RUNTIME_BUILDER_H */
