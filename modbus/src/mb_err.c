#include <modbus/mb_err.h>

const char *mb_err_str(mb_err_t err)
{
    switch (err) {
    case MB_OK:
        return "OK";
    case MB_ERR_INVALID_ARGUMENT:
        return "Invalid argument";
    case MB_ERR_TIMEOUT:
        return "Timeout";
    case MB_ERR_TRANSPORT:
        return "Transport error";
    case MB_ERR_CRC:
        return "CRC error";
    case MB_ERR_INVALID_REQUEST:
        return "Invalid request";
    case MB_ERR_OTHER_REQUESTS:
        return "Other request";
    case MODBUS_OTHERS_REQUESTS:
        return "Other request";
    case MB_ERR_OTHER:
        return "Unspecified error";
    case MB_EX_ILLEGAL_FUNCTION:
        return "Illegal function";
    case MB_EX_ILLEGAL_DATA_ADDRESS:
        return "Illegal data address";
    case MB_EX_ILLEGAL_DATA_VALUE:
        return "Illegal data value";
    case MB_EX_SERVER_DEVICE_FAILURE:
        return "Server device failure";
    default:
        return "Unknown error";
    }
}
