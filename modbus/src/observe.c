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
    case MB_EX_ILLEGAL_FUNCTION:
        return MB_DIAG_ERR_SLOT_EXCEPTION_ILLEGAL_FUNCTION;
    case MB_EX_ILLEGAL_DATA_ADDRESS:
        return MB_DIAG_ERR_SLOT_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    case MB_EX_ILLEGAL_DATA_VALUE:
        return MB_DIAG_ERR_SLOT_EXCEPTION_ILLEGAL_DATA_VALUE;
    case MB_EX_SERVER_DEVICE_FAILURE:
        return MB_DIAG_ERR_SLOT_EXCEPTION_SERVER_DEVICE_FAILURE;
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
    case MB_DIAG_ERR_SLOT_MAX:
        return "invalid";
    default:
        break;
    }
    return "unknown";
}
