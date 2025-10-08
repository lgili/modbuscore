#include <modbus/observe.h>

#include <string.h>

mb_diag_err_slot_t mb_diag_slot_from_error(mb_err_t err)
{
    switch (err) {
    case MB_OK:
        return MB_DIAG_ERR_SLOT_OK;
    case MB_ERR_INVALID_ARGUMENT:
        return MB_DIAG_ERR_SLOT_INVALID_ARGUMENT;
    case MB_ERR_TIMEOUT:
        return MB_DIAG_ERR_SLOT_TIMEOUT;
    case MB_ERR_TRANSPORT:
        return MB_DIAG_ERR_SLOT_TRANSPORT;
    case MB_ERR_CRC:
        return MB_DIAG_ERR_SLOT_CRC;
    case MB_ERR_INVALID_REQUEST:
        return MB_DIAG_ERR_SLOT_INVALID_REQUEST;
    case MB_ERR_OTHER_REQUESTS:
    case MODBUS_OTHERS_REQUESTS:
        return MB_DIAG_ERR_SLOT_OTHER_REQUESTS;
    case MB_ERR_OTHER:
        return MB_DIAG_ERR_SLOT_OTHER;
    case MB_ERR_CANCELLED:
        return MB_DIAG_ERR_SLOT_CANCELLED;
    case MB_ERR_NO_RESOURCES:
        return MB_DIAG_ERR_SLOT_NO_RESOURCES;
    case MB_ERR_BUSY:
        return MB_DIAG_ERR_SLOT_OTHER; /* Map to OTHER as BUSY is a transient state */
    case MB_EX_ILLEGAL_FUNCTION:
        return MB_DIAG_ERR_SLOT_EXCEPTION_ILLEGAL_FUNCTION;
    case MB_EX_ILLEGAL_DATA_ADDRESS:
        return MB_DIAG_ERR_SLOT_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    case MB_EX_ILLEGAL_DATA_VALUE:
        return MB_DIAG_ERR_SLOT_EXCEPTION_ILLEGAL_DATA_VALUE;
    case MB_EX_SERVER_DEVICE_FAILURE:
        return MB_DIAG_ERR_SLOT_EXCEPTION_SERVER_DEVICE_FAILURE;
    case MB_EX_ACKNOWLEDGE:
        return MB_DIAG_ERR_SLOT_EXCEPTION_ACKNOWLEDGE;
    case MB_EX_SERVER_DEVICE_BUSY:
        return MB_DIAG_ERR_SLOT_EXCEPTION_SERVER_DEVICE_BUSY;
    case MB_EX_NEGATIVE_ACKNOWLEDGE:
        return MB_DIAG_ERR_SLOT_EXCEPTION_NEGATIVE_ACKNOWLEDGE;
    case MB_EX_MEMORY_PARITY_ERROR:
        return MB_DIAG_ERR_SLOT_EXCEPTION_MEMORY_PARITY_ERROR;
    case MB_EX_GATEWAY_PATH_UNAVAILABLE:
        return MB_DIAG_ERR_SLOT_EXCEPTION_GATEWAY_PATH_UNAVAILABLE;
    case MB_EX_GATEWAY_TARGET_FAILED:
        return MB_DIAG_ERR_SLOT_EXCEPTION_GATEWAY_TARGET_DEVICE_FAILED;
    default:
        break;
    }

    return MB_DIAG_ERR_SLOT_OTHER;
}

void mb_diag_reset(mb_diag_counters_t *diag)
{
    if (diag == NULL) {
        return;
    }
    memset(diag, 0, sizeof(*diag));
}

void mb_diag_record_fc(mb_diag_counters_t *diag, mb_u8 function)
{
    if (diag == NULL) {
        return;
    }
    diag->function[function] += 1U;
}

void mb_diag_record_error(mb_diag_counters_t *diag, mb_err_t err)
{
    if (diag == NULL) {
        return;
    }
    const mb_diag_err_slot_t slot = mb_diag_slot_from_error(err);
    if (slot < MB_DIAG_ERR_SLOT_MAX) {
        diag->error[slot] += 1U;
    }
}

const char *mb_diag_err_slot_str(mb_diag_err_slot_t slot)
{
    switch (slot) {
    case MB_DIAG_ERR_SLOT_OK:
        return "ok";
    case MB_DIAG_ERR_SLOT_INVALID_ARGUMENT:
        return "invalid-argument";
    case MB_DIAG_ERR_SLOT_TIMEOUT:
        return "timeout";
    case MB_DIAG_ERR_SLOT_TRANSPORT:
        return "transport";
    case MB_DIAG_ERR_SLOT_CRC:
        return "crc";
    case MB_DIAG_ERR_SLOT_INVALID_REQUEST:
        return "invalid-request";
    case MB_DIAG_ERR_SLOT_OTHER_REQUESTS:
        return "other-requests";
    case MB_DIAG_ERR_SLOT_OTHER:
        return "other";
    case MB_DIAG_ERR_SLOT_CANCELLED:
        return "cancelled";
    case MB_DIAG_ERR_SLOT_NO_RESOURCES:
        return "no-resources";
    case MB_DIAG_ERR_SLOT_EXCEPTION_ILLEGAL_FUNCTION:
        return "ex-illegal-function";
    case MB_DIAG_ERR_SLOT_EXCEPTION_ILLEGAL_DATA_ADDRESS:
        return "ex-illegal-data-address";
    case MB_DIAG_ERR_SLOT_EXCEPTION_ILLEGAL_DATA_VALUE:
        return "ex-illegal-data-value";
    case MB_DIAG_ERR_SLOT_EXCEPTION_SERVER_DEVICE_FAILURE:
        return "ex-server-device-failure";
    case MB_DIAG_ERR_SLOT_EXCEPTION_ACKNOWLEDGE:
        return "ex-acknowledge";
    case MB_DIAG_ERR_SLOT_EXCEPTION_SERVER_DEVICE_BUSY:
        return "ex-server-device-busy";
    case MB_DIAG_ERR_SLOT_EXCEPTION_NEGATIVE_ACKNOWLEDGE:
        return "ex-negative-acknowledge";
    case MB_DIAG_ERR_SLOT_EXCEPTION_MEMORY_PARITY_ERROR:
        return "ex-memory-parity-error";
    case MB_DIAG_ERR_SLOT_EXCEPTION_GATEWAY_PATH_UNAVAILABLE:
        return "ex-gateway-path-unavailable";
    case MB_DIAG_ERR_SLOT_EXCEPTION_GATEWAY_TARGET_DEVICE_FAILED:
        return "ex-gateway-target-device-failed";
    case MB_DIAG_ERR_SLOT_MAX:
        return "invalid";
    default:
        break;
    }
    return "unknown";
}

void mb_diag_state_init(mb_diag_state_t *state)
{
    if (state == NULL) {
        return;
    }

#if MB_CONF_DIAG_ENABLE_COUNTERS
    mb_diag_reset(&state->counters);
#endif

#if MB_CONF_DIAG_ENABLE_TRACE
    state->trace.head = 0U;
    state->trace.count = 0U;
#endif
}

void mb_diag_state_reset(mb_diag_state_t *state)
{
    mb_diag_state_init(state);
}

void mb_diag_state_record_fc(mb_diag_state_t *state, mb_u8 function)
{
#if MB_CONF_DIAG_ENABLE_COUNTERS
    if (state == NULL) {
        return;
    }
    mb_diag_record_fc(&state->counters, function);
#else
    (void)state;
    (void)function;
#endif
}

void mb_diag_state_record_error(mb_diag_state_t *state, mb_err_t err)
{
#if MB_CONF_DIAG_ENABLE_COUNTERS
    if (state == NULL) {
        return;
    }
    mb_diag_record_error(&state->counters, err);
#else
    (void)state;
    (void)err;
#endif
}

void mb_diag_state_capture_event(mb_diag_state_t *state, const mb_event_t *event)
{
#if MB_CONF_DIAG_ENABLE_TRACE
    if (state == NULL || event == NULL || MB_CONF_DIAG_TRACE_DEPTH == 0) {
        return;
    }

    mb_diag_trace_buffer_t *trace = &state->trace;
    const mb_u16 capacity = (mb_u16)MB_CONF_DIAG_TRACE_DEPTH;

    mb_u16 write_index;
    if (trace->count >= capacity) {
        write_index = trace->head;
        trace->head = (mb_u16)((trace->head + 1U) % capacity);
    } else {
        write_index = (mb_u16)((trace->head + trace->count) % capacity);
        trace->count = (mb_u16)(trace->count + 1U);
    }

    trace->entries[write_index].timestamp = event->timestamp;
    trace->entries[write_index].source = event->source;
    trace->entries[write_index].type = event->type;
    trace->entries[write_index].function = 0U;
    trace->entries[write_index].status = MB_OK;

    switch (event->type) {
    case MB_EVENT_CLIENT_TX_SUBMIT:
    case MB_EVENT_CLIENT_TX_COMPLETE:
        trace->entries[write_index].function = event->data.client_txn.function;
        trace->entries[write_index].status = event->data.client_txn.status;
        break;
    case MB_EVENT_SERVER_REQUEST_ACCEPT:
    case MB_EVENT_SERVER_REQUEST_COMPLETE:
        trace->entries[write_index].function = event->data.server_req.function;
        trace->entries[write_index].status = event->data.server_req.status;
        break;
    default:
        break;
    }
#else
    (void)state;
    (void)event;
#endif
}

void mb_diag_snapshot(const mb_diag_state_t *state, mb_diag_snapshot_t *out_snapshot)
{
    if (state == NULL || out_snapshot == NULL) {
        return;
    }

    memset(out_snapshot, 0, sizeof(*out_snapshot));

#if MB_CONF_DIAG_ENABLE_COUNTERS
    out_snapshot->counters = state->counters;
#else
    (void)state;
#endif

#if MB_CONF_DIAG_ENABLE_TRACE
    const mb_u16 capacity = (mb_u16)MB_CONF_DIAG_TRACE_DEPTH;
    mb_u16 count = state->trace.count;
    if (count > capacity) {
        count = capacity;
    }
    out_snapshot->trace_len = count;
    for (mb_u16 i = 0U; i < count; ++i) {
        const mb_u16 index = (mb_u16)((state->trace.head + i) % capacity);
        out_snapshot->trace[i] = state->trace.entries[index];
    }
    for (mb_u16 i = count; i < capacity; ++i) {
        out_snapshot->trace[i] = (mb_diag_trace_entry_t){0};
    }
#endif
}
