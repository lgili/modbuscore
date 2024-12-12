#include <server.h>

/**
 * @brief Initializes the Modbus server context.
 *
 * @param modbus         Pointer to the Modbus context.
 * @param device_address Pointer to the device address.
 * @param baudrate       Pointer to the baud rate.
 * @return modbus_error_t Error code indicating success or failure.
 */
modbus_error_t modbus_server_create(modbus_context_t *modbus, uint16_t *device_address, uint16_t *baudrate)
{
    if (modbus == NULL || device_address == NULL || baudrate == NULL)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    // // put register address array in order
    // selection_sort(g_holding_register, g_holding_register_quantity);
    
    // memset(modbus, 0, sizeof(modbus_context_t));

    // modbus->device_info.address = device_address;
    // modbus->device_info.baudrate = baudrate;
    // modbus->rx_reference_time = get_reference_in_msec();
    // modbus->tx_reference_time = get_reference_in_msec();
    // modbus->error_timer =  get_reference_in_msec();
    // g_time_to_start_modbus =  get_reference_in_msec();

    // fsm_init(&modbus->fsm, &modbus_state_idle, modbus);

    // //modbus_initialize_crc_table();
    // // fixed value for now!!!
    // modbus->device_info.conformity_level = 0x81;


    return MODBUS_ERROR_NONE;
}

