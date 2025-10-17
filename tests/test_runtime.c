#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <modbuscore/runtime/runtime.h>
#include <modbuscore/runtime/builder.h>

typedef struct {
    int count;
    char last_category[32];
    char last_message[64];
} log_capture_t;

static mbc_status_t dummy_send(void *ctx,
                              const uint8_t *buffer,
                              size_t length,
                              mbc_transport_io_t *out)
{
    (void)ctx;
    (void)buffer;
    (void)length;
    if (out) {
        out->processed = length;
    }
    return MBC_STATUS_OK;
}

static mbc_status_t dummy_recv(void *ctx,
                              uint8_t *buffer,
                              size_t capacity,
                              mbc_transport_io_t *out)
{
    (void)ctx;
    (void)buffer;
    (void)capacity;
    if (out) {
        out->processed = 0U;
    }
    return MBC_STATUS_OK;
}

static uint64_t fake_now(void *ctx)
{
    uint64_t *counter = (uint64_t *)ctx;
    return (*counter)++;
}

static void fake_logger(void *ctx, const char *category, const char *message)
{
    log_capture_t *capture = (log_capture_t *)ctx;
    capture->count++;
    if (category) {
        strncpy(capture->last_category, category, sizeof(capture->last_category) - 1U);
        capture->last_category[sizeof(capture->last_category) - 1U] = '\0';
    }
    if (message) {
        strncpy(capture->last_message, message, sizeof(capture->last_message) - 1U);
        capture->last_message[sizeof(capture->last_message) - 1U] = '\0';
    }
}

static void *fake_alloc(void *ctx, size_t size)
{
    (void)ctx;
    return malloc(size);
}

static void fake_free(void *ctx, void *ptr)
{
    (void)ctx;
    free(ptr);
}

static void test_runtime_init_direct(void)
{
    mbc_runtime_t runtime = {0};
    uint64_t counter = 0U;
    log_capture_t logs = {0};

    mbc_runtime_config_t config = {
        .transport = {.ctx = NULL, .send = dummy_send, .receive = dummy_recv},
        .clock = {.ctx = &counter, .now_ms = fake_now},
        .allocator = {.ctx = NULL, .alloc = fake_alloc, .free = fake_free},
        .logger = {.ctx = &logs, .write = fake_logger},
    };

    assert(mbc_runtime_init(NULL, NULL) == MBC_STATUS_INVALID_ARGUMENT);
    assert(mbc_runtime_init(&runtime, NULL) == MBC_STATUS_INVALID_ARGUMENT);
    assert(mbc_runtime_init(&runtime, &config) == MBC_STATUS_OK);
    assert(mbc_runtime_is_ready(&runtime));

    const mbc_runtime_config_t *deps = mbc_runtime_dependencies(&runtime);
    assert(deps && deps->transport.send == dummy_send);
    uint64_t first = deps->clock.now_ms(deps->clock.ctx);
    uint64_t second = deps->clock.now_ms(deps->clock.ctx);
    assert(first == 0ULL);
    assert(second == 1ULL);

    assert(mbc_runtime_init(&runtime, &config) == MBC_STATUS_ALREADY_INITIALISED);
    mbc_runtime_shutdown(&runtime);
}

static void test_runtime_builder_with_defaults(void)
{
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);

    mbc_transport_iface_t transport = {.ctx = NULL, .send = dummy_send, .receive = dummy_recv};
    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_runtime_t runtime = {0};
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);
    assert(mbc_runtime_is_ready(&runtime));

    const mbc_runtime_config_t *deps = mbc_runtime_dependencies(&runtime);
    assert(deps != NULL);

    void *block = deps->allocator.alloc(deps->allocator.ctx, 8U);
    assert(block != NULL);
    deps->allocator.free(deps->allocator.ctx, block);

    uint64_t first = deps->clock.now_ms(deps->clock.ctx);
    uint64_t second = deps->clock.now_ms(deps->clock.ctx);
    assert(second >= first);

    deps->logger.write(deps->logger.ctx, "default", "noop");

    mbc_runtime_shutdown(&runtime);
}

static void test_runtime_builder_with_custom_components(void)
{
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);

    mbc_transport_iface_t transport = {.ctx = NULL, .send = dummy_send, .receive = dummy_recv};
    uint64_t counter = 100U;
    mbc_clock_iface_t clock = {.ctx = &counter, .now_ms = fake_now};
    log_capture_t logs = {0};
    mbc_logger_iface_t logger = {.ctx = &logs, .write = fake_logger};
    mbc_allocator_iface_t allocator = {.ctx = NULL, .alloc = fake_alloc, .free = fake_free};

    mbc_runtime_builder_with_transport(&builder, &transport);
    mbc_runtime_builder_with_clock(&builder, &clock);
    mbc_runtime_builder_with_logger(&builder, &logger);
    mbc_runtime_builder_with_allocator(&builder, &allocator);

    mbc_runtime_t runtime = {0};
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);

    const mbc_runtime_config_t *deps = mbc_runtime_dependencies(&runtime);
    assert(deps != NULL);
    assert(deps->clock.now_ms(deps->clock.ctx) == 100ULL);

    deps->logger.write(deps->logger.ctx, "io", "ready");
    assert(logs.count == 1);
    assert(strcmp(logs.last_category, "io") == 0);
    assert(strcmp(logs.last_message, "ready") == 0);

    mbc_runtime_shutdown(&runtime);
}

static void test_runtime_builder_missing_transport(void)
{
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);

    mbc_runtime_t runtime = {0};
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_INVALID_ARGUMENT);
}

int main(void)
{
    test_runtime_init_direct();
    test_runtime_builder_with_defaults();
    test_runtime_builder_with_custom_components();
    test_runtime_builder_missing_transport();
    return 0;
}
