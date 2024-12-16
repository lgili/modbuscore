
/******************************************************************************
 * The information contained herein is confidential property of Embraco. The
 * user, copying, transfer or disclosure of such information is prohibited
 * except by express written agreement with Embraco.
 ******************************************************************************/

/**
 * @file server.h
 * @brief Modbus server protocol header file.
 *
 * @author  Luiz Carlos Gili
 * @date    2024-12-12
 *
 * @addtogroup Modbus
 * @{
 */

#ifndef _MODBUS_SERVER_H
#define _MODBUS_SERVER_H

#include "modbus.h"

/**
 * @brief Modbus FSM state: Idle
 *
 * This state represents the idle condition of the Modbus FSM, where it is waiting for an event to occur.
 * In this state, the FSM will transition to the `MODBUS_STATE_RECEIVING` state when a byte is received.
 */
extern const fsm_state_t modbus_state_idle;

/**
 * @brief Modbus FSM state: Receiving
 *
 * This state represents the process of receiving Modbus data.:
 */
extern const fsm_state_t modbus_state_receiving;

/**
 * @brief Modbus FSM state: Parsing Address
 *
 * This state parse the income data to get the address of the slave that should respond to master:
 */
extern const fsm_state_t modbus_state_parsing_address;

/**
 * @brief Modbus FSM state: Parsing Function
 *
 * This state parse the income data to get the function that master is asking:
 */
extern const fsm_state_t modbus_state_parsing_function;

/**
 * @brief Modbus FSM state: Processing
 *
 * This state handles the processing of the received Modbus frame.
 */
extern const fsm_state_t modbus_state_processing;

/**
 * @brief Modbus FSM state: Validate Frame
 *
 * This state runs the CRC on the received frame to validate.
 */
extern const fsm_state_t modbus_state_validating_frame;

/**
 * @brief Modbus FSM state: Building Response
 *
 * This state build a response to master, based on data received.
 */
extern const fsm_state_t modbus_state_building_response;

/**
 * @brief Modbus FSM state: Put Data on Buffer
 *
 * This state put data on buffer to be send.
 */
extern const fsm_state_t modbus_state_putting_data_on_buffer;

/**
 * @brief Modbus FSM state: Calculating CRC
 *
 * This state calculate a CRC for the data that will be sent.
 */
extern const fsm_state_t modbus_state_calculating_crc;

/**
 * @brief Modbus FSM state: Sending
 *
 * This state handles sending a response or data after processing. Currently not declared here, but it can be used
 * to handle data transmission to the master.
 */
extern const fsm_state_t modbus_state_sending;

/**
 * @brief Modbus FSM state: Error
 *
 * This state represents an error condition in the Modbus FSM. It can recover and transition back to idle
 * when a new byte is received.
 */
extern const fsm_state_t modbus_state_error;

/**
 * @brief Modbus FSM States
 *
 * This enum represents the possible states of the Modbus finite state machine.
 */
typedef enum
{
    MODBUS_STATE_IDLE,                   /**< FSM is idle, waiting for a new event. */
    MODBUS_STATE_RECEIVING,              /**< FSM is receiving data from the Modbus frame. */
    MODBUS_STATE_PARSING_ADDRESS,        /**< FSM is parsing slave address. */
    MODBUS_STATE_PARSING_FUNCTION,       /**< FSM is parsing function code. */
    MODBUS_STATE_PROCESSING,             /**< FSM is processing the received Modbus frame. */
    MODBUS_STATE_VALIDATING_FRAME,       /**< FSM is validating the received Modbus frame. */
    MODBUS_STATE_BUILDING_RESPONSE,      /**< FSM is building a response to the master. */
    MODBUS_STATE_PUTTING_DATA_ON_BUFFER, /**< FSM is putting data on buffer to send. */
    MODBUS_STATE_CALCULATING_CRC,        /**< FSM is calculating response CRC. */
    MODBUS_STATE_SENDING,                /**< FSM is sending a response or Modbus frame. */
    MODBUS_STATE_ERROR,                  /**< FSM has encountered an error state. */
    MODBUS_STATE_BOOTLOADER              /**< FSM is in bootloader mode. */
} modbus_state_t;

/**
 * @brief Modbus FSM Events
 *
 * This enum defines the possible events that trigger state transitions
 * in the Modbus finite state machine.
 */
typedef enum
{
    MODBUS_EVENT_RX_BYTE_RECEIVED,      /**< A byte was received and should be processed. */
    MODBUS_EVENT_PARSE_ADDRESS,         /**< Parse slave address. */
    MODBUS_EVENT_PARSE_FUNCTION,        /**< Parse function code. */
    MODBUS_EVENT_PROCESS_FRAME,         /**< Process received frame. */
    MODBUS_EVENT_VALIDATE_FRAME,        /**< Validate received frame. */
    MODBUS_EVENT_BUILD_RESPONSE,        /**< Build response. */
    MODBUS_EVENT_BROADCAST_DONT_ANSWER, /**< Broadcast menssage */
    MODBUS_EVENT_PUT_DATA_ON_BUFFER,    /**< Put data on TX buffer */
    MODBUS_EVENT_CALCULATE_CRC,         /**< Calculate CRC value to send. */
    MODBUS_EVENT_SEND_RESPONSE,         /**< The response is ready to send via UART. */
    MODBUS_EVENT_TX_COMPLETE,           /**< The transmission of a response is complete. */
    MODBUS_EVENT_ERROR_DETECTED,        /**< An error was detected in Modbus communication. */
    MODBUS_EVENT_ERROR_WRONG_BAUDRATE,  /**< The baud rate is incorrectly configured in UART. */
    MODBUS_EVENT_TIMEOUT,               /**< A timeout occurred during Modbus communication. */
    MODBUS_EVENT_BOOTLOADER,            /**< In bootloader mode. */
    MODBUS_EVENT_RESTART_FROM_ERROR     /**< Restart the FSM from an error state. */
} modbus_event_t;

modbus_error_t modbus_server_create(modbus_context_t *modbus, const modbus_platform_conf *platform_conf, uint16_t *device_address, uint16_t *baudrate);

modbus_error_t modbus_set_holding_register(uint16_t address, int16_t *variable, bool read_only, modbus_read_callback_t read_cb, modbus_write_callback_t write_cb);
modbus_error_t modbus_set_array_holding_register(uint16_t address, uint16_t lenght, int16_t *variable, bool read_only, modbus_read_callback_t read_cb, modbus_write_callback_t write_cb);
modbus_error_t add_info_to_device(modbus_context_t *modbus, const char *value, uint8_t lenght);

void modbus_server_receive_data_from_uart_event(fsm_t *fsm, uint8_t data);

#endif /*_MODBUS_SERVER_H*/
       /** @} */