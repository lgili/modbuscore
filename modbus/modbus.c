/******************************************************************************
 * The information contained herein is confidential property of Embraco. The
 * user, copying, transfer or disclosure of such information is prohibited
 * except by express written agreement with Embraco.
 ******************************************************************************/

/**
 * @file    modbus_protocol.c
 * @brief   Mobus Protocol driver module.
 * @author  Luiz Carlos Gili
 * @date    2024-09-23
 * @addtogroup Modbus
 * @{
 */

/******************************************************************************
 * Include section
 ******************************************************************************/
// #include "modbus_serial.h"
#include "modbus.h"
#include "helpers.h"
#ifdef MODBUS_CLIENT
#include "modbus_client.h"
#else
#include "modbus_server.h"
#endif
/******************************************************************************
 * Defines section
 * Add all #defines here
 *
 ******************************************************************************/
/******************************************************************************
 * Definitions
 ******************************************************************************/

/******************************************************************************
 * Private variables Declarations
 ******************************************************************************/
uint8_t g_modbus_tx_buffer[MODBUS_SEND_BUFFER_SIZE];
/******************************************************************************
 * Private Function Declarations
 ******************************************************************************/



// /******************************************************************************
//  * Public Functions Implementation
//  ******************************************************************************/

void modbus_platform_conf_create(modbus_platform_conf* platform_conf) {
    memset(platform_conf, 0, sizeof(modbus_platform_conf));
    platform_conf->crc_calc = modbus_crc_with_table;
}

void set_flow_control_pin(modbus_context_t *modbus, int8_t flow_control_pin) {
    modbus->flow_control_pin = flow_control_pin;
}




// /******************************************************************************
//  * Private Functions Implementation
//  ******************************************************************************/


void set_mode_as_transmiter(modbus_context_t *modbus){
    if(modbus->platform.write_gpio && modbus->flow_control_pin >= 0){              
        modbus->platform.write_gpio(modbus->flow_control_pin, 1);
    }
}

void set_mode_as_receiver(modbus_context_t *modbus){
    if(modbus->platform.write_gpio && modbus->flow_control_pin >= 0) {
       modbus->platform.write_gpio(modbus->flow_control_pin, 0);
    }
}

/**
 * @brief Resets the Modbus message state.
 *
 * @param modbus Pointer to the Modbus context.
 */
void modbus_reset_message(modbus_context_t *modbus)
{
    modbus->msg.slave_address = 0U;
    modbus->msg.function_code = 0U;
    modbus->msg.read_address = 0U;
    modbus->msg.read_quantity = 0U;
    modbus->msg.current_read_index = 0U;
    modbus->msg.write_address = 0U;
    modbus->msg.write_quantity = 0U;
    modbus->msg.byte_count = 0U;
    modbus->msg.error = MODBUS_ERROR_NONE;
    modbus->msg.broadcast = false;
    modbus->msg.ignored = false;

    modbus->raw_data.rx_index = 0U;
    modbus->raw_data.tx_index = 0U;

    modbus->msg.mei_type = 0U;
    modbus->msg.device_id_code = 0U;
    modbus->msg.device_obj_id = 0U;
}


/******************************************************************************
 * Modbus Send Functions
 ******************************************************************************/


/**
 * @brief Sends Modbus data via UART.
 *
 * @param modbus        Pointer to the Modbus context.
 * @param send_data     Pointer to the data to be sent.
 * @param send_data_len Length of the data to send.
 */
inline void modbus_send(modbus_context_t *modbus, const uint8_t *send_data, uint8_t send_data_len)
{
    uint32_t tx_buffer_index = 0x00U;
    uint16_t local_check_sum;

    /* Create Modbus frame (RTU) */
    g_modbus_tx_buffer[tx_buffer_index++] = modbus->msg.slave_address;
    g_modbus_tx_buffer[tx_buffer_index++] = modbus->msg.function_code;

    /* Copy data to the Modbus transmission buffer */
    memcpy(&g_modbus_tx_buffer[tx_buffer_index], send_data, send_data_len);
    tx_buffer_index += send_data_len;

    /* Calculate and append the CRC */
    local_check_sum = modbus->platform.crc_calc(g_modbus_tx_buffer, tx_buffer_index);
    g_modbus_tx_buffer[tx_buffer_index++] = GET_LOW_BYTE(local_check_sum);
    g_modbus_tx_buffer[tx_buffer_index++] = GET_HIGH_BYTE(local_check_sum);

    /* Prepare for transmission */
    modbus->raw_data.rx_count = 0U;
    modbus->raw_data.tx_ptr = (uint8_t *)g_modbus_tx_buffer;
    modbus->raw_data.tx_count = tx_buffer_index;

    modbus->platform.write(modbus->raw_data.tx_ptr, modbus->raw_data.tx_count);    
}

/**
 * @brief Sends data via UART without Modbus framing.
 *
 * @param modbus        Pointer to the Modbus context.
 * @param send_data     Pointer to the data to be sent.
 * @param send_data_len Length of the data to send.
 */
inline void uart_send(modbus_context_t *modbus, const uint8_t *send_data, uint8_t send_data_len)
{
    modbus->raw_data.rx_count = 0U;
    modbus->raw_data.tx_ptr = (uint8_t *)send_data;
    modbus->raw_data.tx_count = send_data_len;

    modbus->platform.write(modbus->raw_data.tx_ptr, modbus->raw_data.tx_count);     
}

/* @brief        Send a Modbus error frame via UART.
 *
 * This function sends an error frame to the Modbus master. If the message is addressed to the slave,
 * it constructs an error response with the function code and error code, then sends it.
 *
 * @param        modbus         Pointer to the Modbus context instance.
 * @param        error_code     The Modbus error code to be sent.
 */
inline void modbus_send_error_response(modbus_context_t *modbus)
{
    /* Do not return an error for broadcast messages */
    if (modbus->msg.slave_address == *modbus->device_info.address)
    {
        /* Set the error bit in the function code */
        modbus->msg.function_code |= MODBUS_FUNC_ERROR_FRAME_HEADER;

        /* Send the Modbus error frame */ // TODO need to build the response
        modbus_send(modbus, &modbus->msg.error, sizeof(modbus->msg.error));        
    }
    else
    {
        /* If not addressed to this slave, reset the receive counter */
        modbus->raw_data.rx_count = 0;
    }
    modbus->msg.error = MODBUS_ERROR_NONE;
}
