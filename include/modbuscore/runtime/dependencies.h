#ifndef MODBUSCORE_RUNTIME_DEPENDENCIES_H
#define MODBUSCORE_RUNTIME_DEPENDENCIES_H

/**
 * @file dependencies.h
 * @brief Fundamental dependency interfaces consumed by the runtime.
 *
 * Each interface is a lightweight struct bundling a context pointer and the
 * function pointers required to fulfil a specific contract (clock, allocator,
 * logging, transport). All dependencies are optional until validated by the
 * runtime builder, allowing maximum flexibility in test scenarios.
 */

#include <modbuscore/common/status.h>
#include <stddef.h>
#include <stdint.h>

#include <modbuscore/runtime/diagnostics.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Transport I/O result structure.
 */
typedef struct mbc_transport_io {
    size_t processed; /**< Number of bytes actually processed */
} mbc_transport_io_t;

/**
 * @brief Clock interface for timestamp generation.
 */
typedef struct mbc_clock_iface {
    void* ctx;                     /**< User context */
    uint64_t (*now_ms)(void* ctx); /**< Get current time in milliseconds */
} mbc_clock_iface_t;

/**
 * @brief Memory allocator interface.
 */
typedef struct mbc_allocator_iface {
    void* ctx;                              /**< User context */
    void* (*alloc)(void* ctx, size_t size); /**< Allocate memory */
    void (*free)(void* ctx, void* ptr);     /**< Free memory */
} mbc_allocator_iface_t;

/**
 * @brief Logger interface for diagnostics.
 */
typedef struct mbc_logger_iface {
    void* ctx;                                                           /**< User context */
    void (*write)(void* ctx, const char* category, const char* message); /**< Log a message */
} mbc_logger_iface_t;

/**
 * @brief Transport layer interface.
 *
 * This interface abstracts the underlying transport (TCP, RTU, mock, etc.)
 * allowing the protocol engine to operate independently of the physical layer.
 */
typedef struct mbc_transport_iface {
    void* ctx; /**< User context */
    mbc_status_t (*send)(void* ctx, const uint8_t* buffer, size_t length,
                         mbc_transport_io_t* out); /**< Send data */
    mbc_status_t (*receive)(void* ctx, uint8_t* buffer, size_t capacity,
                            mbc_transport_io_t* out); /**< Receive data */
    uint64_t (*now)(void* ctx);                       /**< Get timestamp in milliseconds */
    void (*yield)(void* ctx);                         /**< Cooperative yield (optional) */
} mbc_transport_iface_t;

#ifdef __cplusplus
}
#endif

#endif /* MODBUSCORE_RUNTIME_DEPENDENCIES_H */
