#include "modbus_client.h"

modbus_error_t modbus_client_create(modbus_context_t *modbus, const modbus_platform_conf *platform_conf, uint16_t *baudrate)
{
    if (modbus == NULL || baudrate == NULL || platform_conf == NULL)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    if (!platform_conf->read || !platform_conf->write || !platform_conf->get_reference_msec || !platform_conf->measure_time_msec)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    memset(modbus, 0, sizeof(modbus_context_t));

    modbus->device_info.address = 0;
    modbus->device_info.baudrate = baudrate;
    modbus->platform = *platform_conf;

    // fsm_init(&modbus->fsm, &modbus_state_idle, modbus);

    return MODBUS_ERROR_NONE;
}
