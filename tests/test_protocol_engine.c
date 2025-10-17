#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <modbuscore/protocol/engine.h>
#include <modbuscore/runtime/builder.h>

typedef struct {
    size_t recv_calls;
    size_t send_calls;
    size_t yield_calls;
    uint64_t now_calls;
    uint64_t timestamp;
    size_t bytes_available;
    size_t events_total;
    mbc_engine_event_type_t last_event;
    mbc_engine_event_type_t events[32];
    size_t event_count;
    const uint8_t *rx_stream;
    size_t rx_stream_len;
    size_t rx_stream_pos;
} mock_transport_ctx_t;

static mbc_status_t mock_send(void *ctx,
                              const uint8_t *buffer,
                              size_t length,
                              mbc_transport_io_t *out)
{
    mock_transport_ctx_t *mock = ctx;
    mock->send_calls++;
    if (out) {
        out->processed = length;
    }
    return buffer && length ? MBC_STATUS_OK : MBC_STATUS_INVALID_ARGUMENT;
}

static mbc_status_t mock_recv(void *ctx,
                              uint8_t *buffer,
                              size_t capacity,
                              mbc_transport_io_t *out)
{
    mock_transport_ctx_t *mock = ctx;
    mock->recv_calls++;
    if (mock->rx_stream && mock->rx_stream_pos < mock->rx_stream_len) {
        size_t remaining = mock->rx_stream_len - mock->rx_stream_pos;
        size_t to_copy = (capacity < remaining) ? capacity : remaining;
        if (buffer && to_copy > 0U) {
            memcpy(buffer, mock->rx_stream + mock->rx_stream_pos, to_copy);
        }
        if (out) {
            out->processed = to_copy;
        }
        mock->rx_stream_pos += to_copy;
        if (mock->rx_stream_pos >= mock->rx_stream_len) {
            mock->rx_stream = NULL;
            mock->rx_stream_len = 0U;
            mock->rx_stream_pos = 0U;
        }
        return MBC_STATUS_OK;
    }
    if (mock->bytes_available == 0U) {
        if (out) {
            out->processed = 0U;
        }
        return MBC_STATUS_OK;
    }

    if (buffer && capacity > 0U) {
        buffer[0] = 0x55;
        if (out) {
            out->processed = 1U;
        }
        mock->bytes_available--;
        return MBC_STATUS_OK;
    }

    return MBC_STATUS_INVALID_ARGUMENT;
}

static uint64_t mock_now(void *ctx)
{
    mock_transport_ctx_t *mock = ctx;
    mock->now_calls++;
    mock->timestamp++;
    return mock->timestamp;
}

static void mock_yield(void *ctx)
{
    mock_transport_ctx_t *mock = ctx;
    mock->yield_calls++;
}

static void mock_event(void *ctx, const mbc_engine_event_t *event)
{
    mock_transport_ctx_t *mock = ctx;
    mock->events_total++;
    mock->last_event = event->type;
    if (mock->event_count < sizeof(mock->events) / sizeof(mock->events[0])) {
        mock->events[mock->event_count++] = event->type;
    }
}

static int event_seen(const mock_transport_ctx_t *ctx, mbc_engine_event_type_t type)
{
    for (size_t i = 0; i < ctx->event_count; ++i) {
        if (ctx->events[i] == type) {
            return 1;
        }
    }
    return 0;
}

static mbc_runtime_t build_runtime(mock_transport_ctx_t *transport_ctx)
{
    mbc_transport_iface_t iface = {
        .ctx = transport_ctx,
        .send = mock_send,
        .receive = mock_recv,
        .now = mock_now,
        .yield = mock_yield,
    };

    mbc_runtime_builder_t builder;
    mbc_runtime_builder_init(&builder);
    mbc_runtime_builder_with_transport(&builder, &iface);

    mbc_runtime_t runtime;
    mbc_status_t status = mbc_runtime_builder_build(&builder, &runtime);
    assert(mbc_status_is_ok(status));
    transport_ctx->timestamp = 0U;
    transport_ctx->now_calls = 0U;
    transport_ctx->event_count = 0U;
    transport_ctx->events_total = 0U;
    transport_ctx->last_event = 0;
    transport_ctx->rx_stream = NULL;
    transport_ctx->rx_stream_len = 0U;
    transport_ctx->rx_stream_pos = 0U;
    return runtime;
}

static void test_engine_initialisation(void)
{
    mock_transport_ctx_t ctx = {0};
    mbc_runtime_t runtime = build_runtime(&ctx);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .use_override = false,
        .event_cb = mock_event,
        .event_ctx = &ctx,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);
    assert(mbc_engine_is_ready(&engine));
    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
}

static void test_engine_step_transitions(void)
{
    static const uint8_t request_fc03[] = {0x01U, 0x03U, 0x00U, 0x0AU, 0x00U, 0x02U};
    mock_transport_ctx_t ctx = {0};
    mbc_runtime_t runtime = build_runtime(&ctx);
    ctx.rx_stream = request_fc03;
    ctx.rx_stream_len = sizeof(request_fc03);
    ctx.rx_stream_pos = 0U;

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_SERVER,
        .use_override = false,
        .event_cb = mock_event,
        .event_ctx = &ctx,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);
    assert(engine.state == MBC_ENGINE_STATE_IDLE);

    ctx.event_count = 0U;
    ctx.events_total = 0U;
    assert(mbc_engine_step(&engine, sizeof(request_fc03)) == MBC_STATUS_OK);
    assert(engine.state == MBC_ENGINE_STATE_IDLE);
    assert(ctx.recv_calls >= 1U);
    assert(event_seen(&ctx, MBC_ENGINE_EVENT_RX_READY));
    assert(event_seen(&ctx, MBC_ENGINE_EVENT_PDU_READY));

    mbc_pdu_t taken;
    assert(mbc_engine_take_pdu(&engine, &taken));
    assert(taken.function == 0x03U);
    assert(taken.payload_length == 4U);
    assert(!mbc_engine_take_pdu(&engine, &taken));

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
}

static void test_engine_submit_request_client(void)
{
    mock_transport_ctx_t ctx = {0};
    mbc_runtime_t runtime = build_runtime(&ctx);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .use_override = false,
        .event_cb = mock_event,
        .event_ctx = &ctx,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);

    uint8_t frame[2] = {0x01, 0x03};
    ctx.event_count = 0U;
    ctx.events_total = 0U;
    assert(mbc_engine_submit_request(&engine, frame, sizeof(frame)) == MBC_STATUS_OK);
    assert(ctx.send_calls == 1U);
    assert(engine.state == MBC_ENGINE_STATE_WAIT_RESPONSE);
    assert(event_seen(&ctx, MBC_ENGINE_EVENT_TX_SENT));
    assert(event_seen(&ctx, MBC_ENGINE_EVENT_STATE_CHANGE));

    /* Nova submissão sem voltar ao estado IDLE deve falhar */
    assert(mbc_engine_submit_request(&engine, frame, sizeof(frame)) == MBC_STATUS_BUSY);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
}

static void test_engine_client_response_decode(void)
{
    mock_transport_ctx_t ctx = {0};
    mbc_runtime_t runtime = build_runtime(&ctx);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .use_override = false,
        .event_cb = mock_event,
        .event_ctx = &ctx,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);

    uint8_t frame[2] = {0x01U, 0x03U};
    ctx.event_count = 0U;
    ctx.events_total = 0U;
    assert(mbc_engine_submit_request(&engine, frame, sizeof(frame)) == MBC_STATUS_OK);
    assert(engine.state == MBC_ENGINE_STATE_WAIT_RESPONSE);

    static const uint8_t response_fc03[] = {0x01U, 0x03U, 0x04U, 0xDEU, 0xADU, 0xBEU, 0xEFU};
    ctx.rx_stream = response_fc03;
    ctx.rx_stream_len = sizeof(response_fc03);
    ctx.rx_stream_pos = 0U;
    ctx.event_count = 0U;
    ctx.events_total = 0U;

    assert(mbc_engine_step(&engine, sizeof(response_fc03)) == MBC_STATUS_OK);
    assert(event_seen(&ctx, MBC_ENGINE_EVENT_PDU_READY));
    mbc_pdu_t pdu;
    assert(mbc_engine_take_pdu(&engine, &pdu));
    assert(pdu.function == 0x03U);
    assert(pdu.payload_length == 5U);
    assert(pdu.payload[0] == 0x04U);
    assert(engine.state == MBC_ENGINE_STATE_IDLE);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
}

static void test_engine_timeout_client(void)
{
    mock_transport_ctx_t ctx = {0};
    mbc_runtime_t runtime = build_runtime(&ctx);

    mbc_engine_t engine;
    mbc_engine_config_t config = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .use_override = false,
        .event_cb = mock_event,
        .event_ctx = &ctx,
        .response_timeout_ms = 3U,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);

    uint8_t frame[2] = {0x01U, 0x05U};
    assert(mbc_engine_submit_request(&engine, frame, sizeof(frame)) == MBC_STATUS_OK);
    assert(engine.state == MBC_ENGINE_STATE_WAIT_RESPONSE);

    ctx.timestamp = engine.last_activity_ms + engine.response_timeout_ms;

    mbc_status_t status = mbc_engine_step(&engine, 1U);
    assert(status == MBC_STATUS_TIMEOUT);
    assert(engine.state == MBC_ENGINE_STATE_IDLE);
    assert(event_seen(&ctx, MBC_ENGINE_EVENT_TIMEOUT));

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
}

static void test_engine_invalid_inputs(void)
{
    mbc_engine_t engine;
    assert(mbc_engine_init(NULL, NULL) == MBC_STATUS_INVALID_ARGUMENT);
    assert(mbc_engine_step(NULL, 1U) == MBC_STATUS_NOT_INITIALISED);

    mock_transport_ctx_t ctx = {0};
    mbc_runtime_t runtime = build_runtime(&ctx);

    mbc_engine_config_t config = {
        .runtime = &runtime,
        .role = MBC_ENGINE_ROLE_CLIENT,
        .use_override = false,
        .event_cb = mock_event,
        .event_ctx = &ctx,
    };

    assert(mbc_engine_init(&engine, &config) == MBC_STATUS_OK);
    assert(mbc_engine_step(&engine, 0U) == MBC_STATUS_INVALID_ARGUMENT);

    mbc_engine_shutdown(&engine);
    mbc_runtime_shutdown(&runtime);
}

int main(void)
{
    test_engine_initialisation();
    test_engine_step_transitions();
    test_engine_submit_request_client();
    test_engine_client_response_decode();
    test_engine_timeout_client();
    test_engine_invalid_inputs();
    return 0;
}
