/**
 * @file runtime.c
 * @brief Implementation of runtime dependency management.
 */

#include <modbuscore/runtime/runtime.h>

/**
 * @brief Validate runtime configuration.
 *
 * @param config Configuration to validate
 * @return MBC_STATUS_OK if valid, error code otherwise
 */
static mbc_status_t validate_config(const mbc_runtime_config_t* config)
{
    if (!config) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!config->transport.send || !config->transport.receive) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!config->clock.now_ms) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!config->allocator.alloc || !config->allocator.free) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!config->logger.write) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!config->diag.emit) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    return MBC_STATUS_OK;
}

mbc_status_t mbc_runtime_init(mbc_runtime_t* runtime, const mbc_runtime_config_t* config)
{
    if (!runtime) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (runtime->initialised) {
        return MBC_STATUS_ALREADY_INITIALISED;
    }

    mbc_status_t status = validate_config(config);
    if (!mbc_status_is_ok(status)) {
        return status;
    }

    runtime->deps = *config;
    runtime->initialised = true;
    return MBC_STATUS_OK;
}

void mbc_runtime_shutdown(mbc_runtime_t* runtime)
{
    if (!runtime) {
        return;
    }

    runtime->initialised = false;
    runtime->deps = (mbc_runtime_config_t){0};
}

bool mbc_runtime_is_ready(const mbc_runtime_t* runtime) { return runtime && runtime->initialised; }

const mbc_runtime_config_t* mbc_runtime_dependencies(const mbc_runtime_t* runtime)
{
    if (!mbc_runtime_is_ready(runtime)) {
        return NULL;
    }
    return &runtime->deps;
}
