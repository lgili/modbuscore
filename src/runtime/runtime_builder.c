/**
 * @file runtime_builder.c
 * @brief Implementation of runtime builder with default dependencies.
 */

#include <time.h>
#include <stdlib.h>
#include <string.h>

#include <modbuscore/runtime/builder.h>

/**
 * @brief Default clock implementation using monotonic time if available.
 *
 * @param ctx Context (unused)
 * @return Current time in milliseconds
 */
static uint64_t default_clock_now(void *ctx)
{
    (void)ctx;

#if defined(CLOCK_MONOTONIC)
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return ((uint64_t)ts.tv_sec * 1000ULL) + (uint64_t)(ts.tv_nsec / 1000000ULL);
    }
#endif

    clock_t ticks = clock();
    if (ticks == (clock_t)-1) {
        return 0ULL;
    }
    return (uint64_t)((ticks * 1000ULL) / CLOCKS_PER_SEC);
}

/**
 * @brief Default allocator using malloc.
 *
 * @param ctx Context (unused)
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
static void *default_alloc(void *ctx, size_t size)
{
    (void)ctx;
    return malloc(size);
}

/**
 * @brief Default deallocator using free.
 *
 * @param ctx Context (unused)
 * @param ptr Pointer to free
 */
static void default_free(void *ctx, void *ptr)
{
    (void)ctx;
    free(ptr);
}

/**
 * @brief Default logger (no-op implementation).
 *
 * @param ctx Context (unused)
 * @param category Log category (unused)
 * @param message Log message (unused)
 */
static void default_logger(void *ctx, const char *category, const char *message)
{
    (void)ctx;
    (void)category;
    (void)message;
}

static const mbc_clock_iface_t default_clock_iface = {
    .ctx = NULL,
    .now_ms = default_clock_now,
};

static const mbc_allocator_iface_t default_allocator_iface = {
    .ctx = NULL,
    .alloc = default_alloc,
    .free = default_free,
};

static const mbc_logger_iface_t default_logger_iface = {
    .ctx = NULL,
    .write = default_logger,
};

void mbc_runtime_builder_init(mbc_runtime_builder_t *builder)
{
    if (!builder) {
        return;
    }

    *builder = (mbc_runtime_builder_t){0};
}

mbc_runtime_builder_t *mbc_runtime_builder_with_transport(mbc_runtime_builder_t *builder,
                                                          const mbc_transport_iface_t *transport)
{
    if (!builder || !transport) {
        return builder;
    }

    builder->config.transport = *transport;
    builder->transport_set = true;
    return builder;
}

mbc_runtime_builder_t *mbc_runtime_builder_with_clock(mbc_runtime_builder_t *builder,
                                                      const mbc_clock_iface_t *clock)
{
    if (!builder || !clock) {
        return builder;
    }

    builder->config.clock = *clock;
    builder->clock_set = true;
    return builder;
}

mbc_runtime_builder_t *mbc_runtime_builder_with_allocator(mbc_runtime_builder_t *builder,
                                                          const mbc_allocator_iface_t *allocator)
{
    if (!builder || !allocator) {
        return builder;
    }

    builder->config.allocator = *allocator;
    builder->allocator_set = true;
    return builder;
}

mbc_runtime_builder_t *mbc_runtime_builder_with_logger(mbc_runtime_builder_t *builder,
                                                       const mbc_logger_iface_t *logger)
{
    if (!builder || !logger) {
        return builder;
    }

    builder->config.logger = *logger;
    builder->logger_set = true;
    return builder;
}

mbc_status_t mbc_runtime_builder_build(mbc_runtime_builder_t *builder, mbc_runtime_t *runtime)
{
    if (!builder || !runtime) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!builder->transport_set) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!builder->clock_set) {
        builder->config.clock = default_clock_iface;
        builder->clock_set = true;
    }

    if (!builder->allocator_set) {
        builder->config.allocator = default_allocator_iface;
        builder->allocator_set = true;
    }

    if (!builder->logger_set) {
        builder->config.logger = default_logger_iface;
        builder->logger_set = true;
    }

    memset(runtime, 0, sizeof(*runtime));
    return mbc_runtime_init(runtime, &builder->config);
}
