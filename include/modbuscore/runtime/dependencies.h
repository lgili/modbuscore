#ifndef MODBUSCORE_RUNTIME_DEPENDENCIES_H
#define MODBUSCORE_RUNTIME_DEPENDENCIES_H

/**
 * @file dependencies.h
 * @brief Fundamental dependency interfaces consumed by the runtime.
 *
 * Each interface is a lightweight struct bundling a context pointer and the
 * function pointers required to fulfil a specific contract (clock, allocator,
 * logging, transport). All dependencies are optional until validated by the
 * runtime builder, permitindo máxima flexibilidade nos cenários de teste.
 */

#include <stddef.h>
#include <stdint.h>

#include <modbuscore/common/status.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mbc_transport_io {
    size_t processed;
} mbc_transport_io_t;

typedef struct mbc_clock_iface {
    void *ctx;
    uint64_t (*now_ms)(void *ctx);
} mbc_clock_iface_t;

typedef struct mbc_allocator_iface {
    void *ctx;
    void *(*alloc)(void *ctx, size_t size);
    void (*free)(void *ctx, void *ptr);
} mbc_allocator_iface_t;

typedef struct mbc_logger_iface {
    void *ctx;
    void (*write)(void *ctx, const char *category, const char *message);
} mbc_logger_iface_t;

typedef struct mbc_transport_iface {
    void *ctx;
    mbc_status_t (*send)(void *ctx, const uint8_t *buffer, size_t length, mbc_transport_io_t *out);
    mbc_status_t (*receive)(void *ctx, uint8_t *buffer, size_t capacity, mbc_transport_io_t *out);
    uint64_t (*now)(void *ctx);
    void (*yield)(void *ctx);
} mbc_transport_iface_t;

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_RUNTIME_DEPENDENCIES_H */
