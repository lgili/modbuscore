#include <assert.h>
#include <modbuscore/runtime/builder.h>
#include <modbuscore/runtime/runtime.h>
#include <modbuscore/transport/mock.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int count;
    char last_category[32];
    char last_message[64];
} log_capture_t;

typedef struct {
    int count;
    mbc_diag_severity_t last_severity;
    char last_component[32];
    char last_message[64];
    uint32_t last_code;
} diag_capture_t;

static uint64_t fake_now(void* ctx)
{
    uint64_t* counter = (uint64_t*)ctx;
    return (*counter)++;
}

static void fake_logger(void* ctx, const char* category, const char* message)
{
    log_capture_t* capture = (log_capture_t*)ctx;
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

static void* fake_alloc(void* ctx, size_t size)
{
    (void)ctx;
    return malloc(size);
}

static void fake_free(void* ctx, void* ptr)
{
    (void)ctx;
    free(ptr);
}

static void fake_diag_emit(void* ctx, const mbc_diag_event_t* event)
{
    diag_capture_t* capture = (diag_capture_t*)ctx;
    capture->count++;
    capture->last_severity = event ? event->severity : MBC_DIAG_SEVERITY_TRACE;
    capture->last_code = event ? event->code : 0U;
    if (event && event->component) {
        strncpy(capture->last_component, event->component, sizeof(capture->last_component) - 1U);
        capture->last_component[sizeof(capture->last_component) - 1U] = '\0';
    }
    if (event && event->message) {
        strncpy(capture->last_message, event->message, sizeof(capture->last_message) - 1U);
        capture->last_message[sizeof(capture->last_message) - 1U] = '\0';
    }
}

static void test_runtime_init_direct(void)
{
    printf("[test_runtime_init_direct] START\n");
    mbc_runtime_t runtime = {0};
    uint64_t counter = 0U;
    log_capture_t logs = {0};
    diag_capture_t diag = {0};
    mbc_transport_iface_t transport = {0};
    mbc_mock_transport_t* mock = NULL;
    printf("[test_runtime_init_direct] Creating mock transport...\n");
    assert(mbc_mock_transport_create(NULL, &transport, &mock) == MBC_STATUS_OK);
    printf("[test_runtime_init_direct] Mock transport created: %p\n", (void*)mock);

    printf("[test_runtime_init_direct] Building config...\n");
    mbc_runtime_config_t config = {
        .transport = transport,
        .clock = {.ctx = &counter, .now_ms = fake_now},
        .allocator = {.ctx = NULL, .alloc = fake_alloc, .free = fake_free},
        .logger = {.ctx = &logs, .write = fake_logger},
        .diag = {.ctx = &diag, .emit = fake_diag_emit},
    };
    printf("[test_runtime_init_direct] Config built, transport.ctx = %p\n", transport.ctx);

    printf("[test_runtime_init_direct] Testing NULL args...\n");
    assert(mbc_runtime_init(NULL, NULL) == MBC_STATUS_INVALID_ARGUMENT);
    assert(mbc_runtime_init(&runtime, NULL) == MBC_STATUS_INVALID_ARGUMENT);
    printf("[test_runtime_init_direct] Initializing runtime...\n");
    assert(mbc_runtime_init(&runtime, &config) == MBC_STATUS_OK);
    printf("[test_runtime_init_direct] Runtime initialized\n");
    assert(mbc_runtime_is_ready(&runtime));

    printf("[test_runtime_init_direct] Getting dependencies...\n");
    const mbc_runtime_config_t* deps = mbc_runtime_dependencies(&runtime);
    printf("[test_runtime_init_direct] Got deps: %p\n", (void*)deps);
    printf("[test_runtime_init_direct] deps->transport.ctx = %p, mock = %p\n", deps->transport.ctx,
           (void*)mock);
    assert(deps && deps->transport.ctx == mock);
    printf("[test_runtime_init_direct] Calling clock...\n");
    uint64_t first = deps->clock.now_ms(deps->clock.ctx);
    printf("[test_runtime_init_direct] First clock call: %llu\n", first);
    uint64_t second = deps->clock.now_ms(deps->clock.ctx);
    printf("[test_runtime_init_direct] Second clock call: %llu\n", second);
    assert(first == 0ULL);
    assert(second == 1ULL);

    printf("[test_runtime_init_direct] Testing double init...\n");
    assert(mbc_runtime_init(&runtime, &config) == MBC_STATUS_ALREADY_INITIALISED);
    mbc_diag_event_t diag_event = {
        .severity = MBC_DIAG_SEVERITY_INFO,
        .component = "runtime",
        .message = "initialised",
        .fields = NULL,
        .field_count = 0U,
        .code = 0U,
        .timestamp_ms = 0U,
    };
    deps->diag.emit(deps->diag.ctx, &diag_event);
    assert(diag.count == 1);
    assert(strcmp(diag.last_component, "runtime") == 0);
    assert(strcmp(diag.last_message, "initialised") == 0);
    printf("[test_runtime_init_direct] Shutting down...\n");
    mbc_runtime_shutdown(&runtime);
    printf("[test_runtime_init_direct] Destroying mock...\n");
    mbc_mock_transport_destroy(mock);
    printf("[test_runtime_init_direct] DONE\n");
}

static void test_runtime_builder_with_defaults(void)
{
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);

    mbc_transport_iface_t transport = {0};
    mbc_mock_transport_t* mock = NULL;
    assert(mbc_mock_transport_create(NULL, &transport, &mock) == MBC_STATUS_OK);
    mbc_runtime_builder_with_transport(&builder, &transport);

    mbc_runtime_t runtime = {0};
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);
    assert(mbc_runtime_is_ready(&runtime));

    const mbc_runtime_config_t* deps = mbc_runtime_dependencies(&runtime);
    assert(deps != NULL);

    void* block = deps->allocator.alloc(deps->allocator.ctx, 8U);
    assert(block != NULL);
    deps->allocator.free(deps->allocator.ctx, block);

    uint64_t first = deps->clock.now_ms(deps->clock.ctx);
    uint64_t second = deps->clock.now_ms(deps->clock.ctx);
    assert(second >= first);

    deps->logger.write(deps->logger.ctx, "default", "noop");
    mbc_diag_event_t evt = {
        .severity = MBC_DIAG_SEVERITY_TRACE,
        .component = "default",
        .message = "noop",
        .fields = NULL,
        .field_count = 0U,
        .code = 0U,
        .timestamp_ms = 0U,
    };
    deps->diag.emit(deps->diag.ctx, &evt);

    mbc_runtime_shutdown(&runtime);
    mbc_mock_transport_destroy(mock);
}

static void test_runtime_builder_with_custom_components(void)
{
    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);

    mbc_transport_iface_t transport = {0};
    mbc_mock_transport_t* mock = NULL;
    assert(mbc_mock_transport_create(NULL, &transport, &mock) == MBC_STATUS_OK);

    uint64_t counter = 100U;
    mbc_clock_iface_t clock = {.ctx = &counter, .now_ms = fake_now};
    log_capture_t logs = {0};
    mbc_logger_iface_t logger = {.ctx = &logs, .write = fake_logger};
    mbc_allocator_iface_t allocator = {.ctx = NULL, .alloc = fake_alloc, .free = fake_free};
    diag_capture_t diag = {0};
    mbc_diag_sink_iface_t diag_sink = {.ctx = &diag, .emit = fake_diag_emit};

    mbc_runtime_builder_with_transport(&builder, &transport);
    mbc_runtime_builder_with_clock(&builder, &clock);
    mbc_runtime_builder_with_logger(&builder, &logger);
    mbc_runtime_builder_with_allocator(&builder, &allocator);
    mbc_runtime_builder_with_diag(&builder, &diag_sink);

    mbc_runtime_t runtime = {0};
    assert(mbc_runtime_builder_build(&builder, &runtime) == MBC_STATUS_OK);

    const mbc_runtime_config_t* deps = mbc_runtime_dependencies(&runtime);
    assert(deps != NULL);
    assert(deps->clock.now_ms(deps->clock.ctx) == 100ULL);

    deps->logger.write(deps->logger.ctx, "io", "ready");
    assert(logs.count == 1);
    assert(strcmp(logs.last_category, "io") == 0);
    assert(strcmp(logs.last_message, "ready") == 0);
    mbc_diag_event_t diag_evt = {
        .severity = MBC_DIAG_SEVERITY_WARNING,
        .component = "io",
        .message = "ready",
        .fields = NULL,
        .field_count = 0U,
        .code = 7U,
        .timestamp_ms = 123U,
    };
    deps->diag.emit(deps->diag.ctx, &diag_evt);
    assert(diag.count == 1);
    assert(diag.last_severity == MBC_DIAG_SEVERITY_WARNING);
    assert(diag.last_code == 7U);
    assert(strcmp(diag.last_component, "io") == 0);
    assert(strcmp(diag.last_message, "ready") == 0);

    mbc_runtime_shutdown(&runtime);
    mbc_mock_transport_destroy(mock);
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
    printf("=== Runtime Tests Starting ===\n");
    fflush(stdout);
    test_runtime_init_direct();
    test_runtime_builder_with_defaults();
    test_runtime_builder_with_custom_components();
    test_runtime_builder_missing_transport();
    printf("=== All Runtime Tests Passed ===\n");
    return 0;
}
