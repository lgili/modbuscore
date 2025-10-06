#include <modbus/conf.h>

#if MB_CONF_TRANSPORT_TCP

#include <modbus/transport/tcp_multi.h>

#include <string.h>

static void mb_tcp_multi_slot_callback(mb_tcp_transport_t *tcp,
                                       const mb_adu_view_t *adu,
                                       mb_u16 transaction_id,
                                       mb_err_t status,
                                       void *user)
{
    (void)tcp;
    mb_tcp_multi_slot_t *slot = (mb_tcp_multi_slot_t *)user;
    if (!slot || !slot->active || !slot->owner) {
        return;
    }

    mb_tcp_multi_transport_t *multi = slot->owner;
    if (multi->callback) {
        multi->callback(multi, slot->index, adu, transaction_id, status, multi->user_ctx);
    }
}

static mb_tcp_multi_slot_t *mb_tcp_multi_get_slot(mb_tcp_multi_transport_t *multi,
                                                  mb_size_t slot_index)
{
    if (!multi || slot_index >= MB_TCP_MAX_CONNECTIONS) {
        return NULL;
    }
    return &multi->slots[slot_index];
}

mb_err_t mb_tcp_multi_init(mb_tcp_multi_transport_t *multi,
                           mb_tcp_multi_frame_callback_t callback,
                           void *user_ctx)
{
    if (!multi) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    memset(multi, 0, sizeof(*multi));
    multi->callback = callback;
    multi->user_ctx = user_ctx;

    for (mb_size_t i = 0U; i < MB_TCP_MAX_CONNECTIONS; ++i) {
        multi->slots[i].index = i;
        multi->slots[i].owner = multi;
        multi->slots[i].active = false;
        multi->slots[i].iface = NULL;
    }

    return MB_OK;
}

mb_err_t mb_tcp_multi_add(mb_tcp_multi_transport_t *multi,
                          const mb_transport_if_t *iface,
                          mb_size_t *out_slot_index)
{
    if (!multi || !iface) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    for (mb_size_t i = 0U; i < MB_TCP_MAX_CONNECTIONS; ++i) {
        mb_tcp_multi_slot_t *slot = &multi->slots[i];
        if (slot->active) {
            continue;
        }

        mb_err_t err = mb_tcp_init(&slot->tcp, iface, mb_tcp_multi_slot_callback, slot);
        if (!mb_err_is_ok(err)) {
            return err;
        }

        slot->active = true;
        slot->iface = iface;
        multi->active_count += 1U;

        if (out_slot_index) {
            *out_slot_index = i;
        }

        return MB_OK;
    }

    return MB_ERR_NO_RESOURCES;
}

mb_err_t mb_tcp_multi_remove(mb_tcp_multi_transport_t *multi,
                             mb_size_t slot_index)
{
    mb_tcp_multi_slot_t *slot = mb_tcp_multi_get_slot(multi, slot_index);
    if (!slot || !slot->active) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_tcp_reset(&slot->tcp);
    slot->active = false;
    slot->iface = NULL;

    if (multi->active_count > 0U) {
        multi->active_count -= 1U;
    }

    return MB_OK;
}

mb_err_t mb_tcp_multi_submit(mb_tcp_multi_transport_t *multi,
                             mb_size_t slot_index,
                             const mb_adu_view_t *adu,
                             mb_u16 transaction_id)
{
    mb_tcp_multi_slot_t *slot = mb_tcp_multi_get_slot(multi, slot_index);
    if (!slot || !slot->active) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    return mb_tcp_submit(&slot->tcp, adu, transaction_id);
}

mb_err_t mb_tcp_multi_poll(mb_tcp_multi_transport_t *multi,
                           mb_size_t slot_index)
{
    mb_tcp_multi_slot_t *slot = mb_tcp_multi_get_slot(multi, slot_index);
    if (!slot || !slot->active) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_err_t status = mb_tcp_poll(&slot->tcp);
    return (status == MB_ERR_TIMEOUT) ? MB_OK : status;
}

mb_err_t mb_tcp_multi_poll_all(mb_tcp_multi_transport_t *multi)
{
    if (!multi) {
        return MB_ERR_INVALID_ARGUMENT;
    }

    mb_err_t aggregate = MB_OK;
    for (mb_size_t i = 0U; i < MB_TCP_MAX_CONNECTIONS; ++i) {
        mb_tcp_multi_slot_t *slot = &multi->slots[i];
        if (!slot->active) {
            continue;
        }

        mb_err_t status = mb_tcp_poll(&slot->tcp);
        if (status == MB_ERR_TIMEOUT) {
            continue;
        }

        if (!mb_err_is_ok(status) && aggregate == MB_OK) {
            aggregate = status;
        }
    }

    return aggregate;
}

bool mb_tcp_multi_is_active(const mb_tcp_multi_transport_t *multi,
                            mb_size_t slot_index)
{
    if (!multi || slot_index >= MB_TCP_MAX_CONNECTIONS) {
        return false;
    }
    return multi->slots[slot_index].active;
}

mb_size_t mb_tcp_multi_active_count(const mb_tcp_multi_transport_t *multi)
{
    return multi ? multi->active_count : 0U;
}

#endif /* MB_CONF_TRANSPORT_TCP */
