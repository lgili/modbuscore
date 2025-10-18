/**
 * @file mock.c
 * @brief Implementação do transporte mock determinístico.
 */

#include <modbuscore/transport/mock.h>

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct mock_frame {
    uint8_t *data;
    size_t length;
    size_t offset;
    uint64_t ready_at;
} mock_frame_t;

typedef struct mock_frame_list {
    mock_frame_t *items;
    size_t size;
    size_t capacity;
} mock_frame_list_t;

struct mbc_mock_transport {
    uint64_t now_ms;
    uint64_t initial_now_ms;
    uint32_t send_latency_ms;
    uint32_t recv_latency_ms;
    uint32_t yield_advance_ms;
    mock_frame_list_t rx_queue;
    mock_frame_list_t tx_queue;
    bool connected;
    mbc_status_t next_send_status;
    mbc_status_t next_receive_status;
};

/* ============================================================================
 * Helpers
 * ========================================================================== */

static void frame_release(mock_frame_t *frame)
{
    if (!frame) {
        return;
    }
    free(frame->data);
    frame->data = NULL;
    frame->length = 0U;
    frame->offset = 0U;
    frame->ready_at = 0U;
}

static void list_clear(mock_frame_list_t *list)
{
    if (!list || !list->items) {
        if (list) {
            list->size = 0U;
            list->capacity = 0U;
        }
        return;
    }

    for (size_t i = 0; i < list->size; ++i) {
        frame_release(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->size = 0U;
    list->capacity = 0U;
}

static void list_reset(mock_frame_list_t *list)
{
    if (!list) {
        return;
    }
    for (size_t i = 0; i < list->size; ++i) {
        frame_release(&list->items[i]);
    }
    list->size = 0U;
}

static mbc_status_t ensure_capacity(mock_frame_list_t *list, size_t desired)
{
    if (!list) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (desired <= list->capacity) {
        return MBC_STATUS_OK;
    }

    size_t new_capacity = (list->capacity == 0U) ? 4U : list->capacity;
    while (new_capacity < desired) {
        new_capacity *= 2U;
    }

    mock_frame_t *new_items = realloc(list->items, new_capacity * sizeof(*new_items));
    if (!new_items) {
        return MBC_STATUS_NO_RESOURCES;
    }

    /* Zero-initialize the newly allocated slots */
    size_t old_capacity = list->capacity;
    if (new_capacity > old_capacity) {
        memset(&new_items[old_capacity], 0, (new_capacity - old_capacity) * sizeof(*new_items));
    }

    list->items = new_items;
    list->capacity = new_capacity;
    return MBC_STATUS_OK;
}

static mbc_status_t list_insert_frame(mock_frame_list_t *list,
                                      uint64_t ready_at,
                                      const uint8_t *data,
                                      size_t length)
{
    if (!list || !data || length == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    mbc_status_t status = ensure_capacity(list, list->size + 1U);
    if (!mbc_status_is_ok(status)) {
        return status;
    }

    uint8_t *copy = malloc(length);
    if (!copy) {
        return MBC_STATUS_NO_RESOURCES;
    }
    memcpy(copy, data, length);

    mock_frame_t frame = {
        .data = copy,
        .length = length,
        .offset = 0U,
        .ready_at = ready_at,
    };

    /* Insere mantendo ordem crescente de ready_at para consumo previsível */
    size_t insert_pos = 0U;
    while (insert_pos < list->size && list->items[insert_pos].ready_at <= ready_at) {
        insert_pos++;
    }

    if (insert_pos < list->size) {
        memmove(&list->items[insert_pos + 1U],
                &list->items[insert_pos],
                (list->size - insert_pos) * sizeof(list->items[0]));
    }

    list->items[insert_pos] = frame;
    list->size++;
    return MBC_STATUS_OK;
}

static void list_remove_at(mock_frame_list_t *list, size_t index)
{
    if (!list || index >= list->size) {
        return;
    }

    frame_release(&list->items[index]);
    if (index + 1U < list->size) {
        memmove(&list->items[index],
                &list->items[index + 1U],
                (list->size - index - 1U) * sizeof(list->items[0]));
    }
    list->size--;
}

static mock_frame_t *list_first_ready(mock_frame_list_t *list,
                                      uint64_t now_ms,
                                      size_t *out_index)
{
    if (!list) {
        return NULL;
    }
    for (size_t i = 0; i < list->size; ++i) {
        mock_frame_t *frame = &list->items[i];
        if (frame->ready_at <= now_ms) {
            if (out_index) {
                *out_index = i;
            }
            return frame;
        }
    }
    return NULL;
}

/* ============================================================================
 * Callbacks da interface
 * ========================================================================== */

static mbc_status_t mock_send(void *ctx,
                              const uint8_t *buffer,
                              size_t length,
                              mbc_transport_io_t *out)
{
    mbc_mock_transport_t *mock = (mbc_mock_transport_t *)ctx;
    if (!mock || (!buffer && length > 0U)) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (length == 0U) {
        if (out) {
            out->processed = 0U;
        }
        return MBC_STATUS_OK;
    }

    if (!mock->connected) {
        return MBC_STATUS_IO_ERROR;
    }

    if (mock->next_send_status != MBC_STATUS_OK) {
        mbc_status_t status = mock->next_send_status;
        mock->next_send_status = MBC_STATUS_OK;
        return status;
    }

    uint64_t ready_at = mock->now_ms + (uint64_t)mock->send_latency_ms;
    mbc_status_t status = list_insert_frame(&mock->tx_queue, ready_at, buffer, length);
    if (!mbc_status_is_ok(status)) {
        return status;
    }

    if (out) {
        out->processed = length;
    }
    return MBC_STATUS_OK;
}

static mbc_status_t mock_receive(void *ctx,
                                 uint8_t *buffer,
                                 size_t capacity,
                                 mbc_transport_io_t *out)
{
    mbc_mock_transport_t *mock = (mbc_mock_transport_t *)ctx;
    if (!mock || !buffer || capacity == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (!mock->connected) {
        return MBC_STATUS_IO_ERROR;
    }

    if (mock->next_receive_status != MBC_STATUS_OK) {
        mbc_status_t status = mock->next_receive_status;
        mock->next_receive_status = MBC_STATUS_OK;
        return status;
    }

    size_t ready_index = 0U;
    mock_frame_t *frame = list_first_ready(&mock->rx_queue, mock->now_ms, &ready_index);
    if (!frame) {
        if (out) {
            out->processed = 0U;
        }
        return MBC_STATUS_OK;
    }

    size_t remaining = frame->length - frame->offset;
    size_t to_copy = (remaining > capacity) ? capacity : remaining;
    memcpy(buffer, frame->data + frame->offset, to_copy);
    frame->offset += to_copy;

    if (out) {
        out->processed = to_copy;
    }

    if (frame->offset >= frame->length) {
        list_remove_at(&mock->rx_queue, ready_index);
    }

    return MBC_STATUS_OK;
}

static uint64_t mock_now(void *ctx)
{
    mbc_mock_transport_t *mock = (mbc_mock_transport_t *)ctx;
    if (!mock) {
        return 0ULL;
    }
    return mock->now_ms;
}

static void mock_yield(void *ctx)
{
    mbc_mock_transport_t *mock = (mbc_mock_transport_t *)ctx;
    if (!mock) {
        return;
    }
    mock->now_ms += (uint64_t)mock->yield_advance_ms;
}

/* ============================================================================
 * API pública
 * ========================================================================== */

mbc_status_t mbc_mock_transport_create(const mbc_mock_transport_config_t *config,
                                       mbc_transport_iface_t *out_iface,
                                       mbc_mock_transport_t **out_ctx)
{
    if (!out_iface || !out_ctx) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    mbc_mock_transport_t *mock = calloc(1, sizeof(*mock));
    if (!mock) {
        return MBC_STATUS_NO_RESOURCES;
    }

    uint32_t initial_now = config ? config->initial_now_ms : 0U;
    mock->initial_now_ms = initial_now;
    mock->now_ms = initial_now;
    mock->send_latency_ms = config ? config->send_latency_ms : 0U;
    mock->recv_latency_ms = config ? config->recv_latency_ms : 0U;
    mock->yield_advance_ms = config ? config->yield_advance_ms : 0U;
    mock->connected = true;
    mock->next_send_status = MBC_STATUS_OK;
    mock->next_receive_status = MBC_STATUS_OK;

    *out_iface = (mbc_transport_iface_t){
        .ctx = mock,
        .send = mock_send,
        .receive = mock_receive,
        .now = mock_now,
        .yield = mock_yield,
    };

    *out_ctx = mock;
    return MBC_STATUS_OK;
}

void mbc_mock_transport_destroy(mbc_mock_transport_t *ctx)
{
    if (!ctx) {
        return;
    }

    list_clear(&ctx->rx_queue);
    list_clear(&ctx->tx_queue);
    free(ctx);
}

void mbc_mock_transport_reset(mbc_mock_transport_t *ctx)
{
    if (!ctx) {
        return;
    }

    list_reset(&ctx->rx_queue);
    list_reset(&ctx->tx_queue);
    ctx->now_ms = ctx->initial_now_ms;
    ctx->connected = true;
    ctx->next_send_status = MBC_STATUS_OK;
    ctx->next_receive_status = MBC_STATUS_OK;
}

void mbc_mock_transport_advance(mbc_mock_transport_t *ctx, uint32_t delta_ms)
{
    if (!ctx) {
        return;
    }
    ctx->now_ms += (uint64_t)delta_ms;
}

mbc_status_t mbc_mock_transport_schedule_rx(mbc_mock_transport_t *ctx,
                                            const uint8_t *data,
                                            size_t length,
                                            uint32_t delay_ms)
{
    if (!ctx || !data || length == 0U) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    uint64_t ready_at = ctx->now_ms + (uint64_t)ctx->recv_latency_ms + (uint64_t)delay_ms;
    return list_insert_frame(&ctx->rx_queue, ready_at, data, length);
}

mbc_status_t mbc_mock_transport_fetch_tx(mbc_mock_transport_t *ctx,
                                         uint8_t *buffer,
                                         size_t capacity,
                                         size_t *out_length)
{
    if (!ctx || !buffer || capacity == 0U || !out_length) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    *out_length = 0U;
    size_t ready_index = 0U;
    mock_frame_t *frame = list_first_ready(&ctx->tx_queue, ctx->now_ms, &ready_index);
    if (!frame) {
        return MBC_STATUS_OK;
    }

    if (frame->length > capacity) {
        return MBC_STATUS_NO_RESOURCES;
    }

    memcpy(buffer, frame->data, frame->length);
    *out_length = frame->length;
    list_remove_at(&ctx->tx_queue, ready_index);
    return MBC_STATUS_OK;
}

size_t mbc_mock_transport_pending_rx(const mbc_mock_transport_t *ctx)
{
    if (!ctx) {
        return 0U;
    }
    return ctx->rx_queue.size;
}

size_t mbc_mock_transport_pending_tx(const mbc_mock_transport_t *ctx)
{
    if (!ctx) {
        return 0U;
    }
    return ctx->tx_queue.size;
}

void mbc_mock_transport_set_connected(mbc_mock_transport_t *ctx, bool connected)
{
    if (!ctx) {
        return;
    }
    ctx->connected = connected;
}

void mbc_mock_transport_fail_next_send(mbc_mock_transport_t *ctx, mbc_status_t status)
{
    if (!ctx) {
        return;
    }
    ctx->next_send_status = status;
}

void mbc_mock_transport_fail_next_receive(mbc_mock_transport_t *ctx, mbc_status_t status)
{
    if (!ctx) {
        return;
    }
    ctx->next_receive_status = status;
}

mbc_status_t mbc_mock_transport_drop_next_rx(mbc_mock_transport_t *ctx)
{
    if (!ctx) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (ctx->rx_queue.size == 0U) {
        return MBC_STATUS_NO_RESOURCES;
    }

    list_remove_at(&ctx->rx_queue, 0U);
    return MBC_STATUS_OK;
}

mbc_status_t mbc_mock_transport_drop_next_tx(mbc_mock_transport_t *ctx)
{
    if (!ctx) {
        return MBC_STATUS_INVALID_ARGUMENT;
    }

    if (ctx->tx_queue.size == 0U) {
        return MBC_STATUS_NO_RESOURCES;
    }

    list_remove_at(&ctx->tx_queue, 0U);
    return MBC_STATUS_OK;
}
