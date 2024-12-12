/******************************************************************************
* The information contained herein is confidential property of Embraco. The
* user, copying, transfer or disclosure of such information is prohibited
* except by express written agreement with Embraco.
******************************************************************************/

/**
* @file modbus_protocol.h
* @brief Modbus protocol header file.
*
* @author  Luiz Carlos Gili
* @date    2024-09-23
*
* @addtogroup Modbus
* @{
*/

#ifndef _MODBUS_PROTOCOL_H
#define _MODBUS_PROTOCOL_H

/******************************************************************************
*  INCLUDES
******************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fsm.h"

/******************************************************************************
*  DEFINES
******************************************************************************/
#define MODBUS_DEVICE_IDENTIFICATION_STRING_LENGTH 10

#define MODBUS_SEND_BUFFER_SIZE    (64)
#define MODBUS_RECEIVE_BUFFER_SIZE (64)

#define MODBUS_FUNC_READ_COILS                    0x01U
#define MODBUS_FUNC_READ_DISCRETE_INPUTS          0x02U
#define MODBUS_FUNC_READ_HOLDING_REGISTERS        0x03U
#define MODBUS_FUNC_READ_INPUT_REGISTERS          0x04U
#define MODBUS_FUNC_WRITE_SINGLE_COIL             0x05U
#define MODBUS_FUNC_WRITE_SINGLE_REGISTER         0x06U
#define MODBUS_FUNC_WRITE_MULTIPLE_COILS          0x0FU
#define MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS      0x10U
#define MODBUS_FUNC_READ_WRITE_MULTIPLE_REGISTERS 0x17U
#define MODBUS_FUNC_READ_DEVICE_INFORMATION       0x2BU 
#define MODBUS_FUNC_ERROR_FRAME_HEADER            0x80U
#define MODBUS_FUNC_ERROR_CODE                    0x83U  



/** Number of register in the system */
#define MAX_SIZE_HOLDING_REGISTERS      32
#define MAX_ADDRESS_HOLDING_REGISTERS   65535
/** Maximum number of coils/registers that can be read/written at once */
#define MODBUS_MAX_READ_WRITE_SIZE 0x07D0
/** Number of coils in the system */
#define NUM_COILS 64  // Adjust as per your system's requirements
/** Size of the discrete output array in bytes */
#define DISCRETE_OUTPUT_SIZE ((NUM_COILS + 7) / 8)

#define MAX_DEVICE_PACKAGE_VALUES 8
#define MAX_DEVICE_PACKAGES 5

#define MODBUS_BAUDRATE                     19200U // BAUD_RATE_SPEED
#define MODBUS_CONVERT_CHAR_INTERVAL_TO_MS(TIME_US, BAUDRATE) \
        ((1000U * TIME_US * 11) / (BAUDRATE))

#define MODBUS_FRAME_INTERVAL               MODBUS_CONVERT_CHAR_INTERVAL_TO_MS(3.5f,BAUD_RATE_SPEED)
#define MODBUS_CHARACTER_INTERVAL           MODBUS_CONVERT_CHAR_INTERVAL_TO_MS(1.5f,BAUD_RATE_SPEED)
#define MODBUS_WAIT_TO_CONFIG_BAUDRATE      (60*1000)


/** Macro to set a bit in a byte array */
#define SET_BIT(array, bit)    ((array)[(bit) / 8] |= (1 << ((bit) % 8)))
/** Macro to clear a bit in a byte array */
#define CLEAR_BIT(array, bit)  ((array)[(bit) / 8] &= ~(1 << ((bit) % 8)))
/** Macro to read a bit from a byte array */
#define READ_BIT(array, bit)   (((array)[(bit) / 8] >> ((bit) % 8)) & 0x01)

#define MODBUS_ERROR_IS_EXCEPTION(e) ((e) > 0 && (e) < 5)

#define JOIN_SHORT_DATA(h, l) (((h) << 0x08) | (l))
#define ROUND_UP_DIV(a, b)      (((a) + (b) - 1) / (b))
#define ARRAY_LENGTH(a)         (sizeof(a) / sizeof((a)[0]))
#define GET_LOW_BYTE(d)       ((d) & 0x00FF)
#define GET_HIGH_BYTE(d)      ((d) >> 0x08)


/******************************************************************************
*  TYPES
******************************************************************************/
extern uint16_t g_modbus_time;
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
typedef enum {
    MODBUS_STATE_IDLE,              /**< FSM is idle, waiting for a new event. */
    MODBUS_STATE_RECEIVING,         /**< FSM is receiving data from the Modbus frame. */
    MODBUS_STATE_PARSING_ADDRESS,   /**< FSM is parsing slave address. */
    MODBUS_STATE_PARSING_FUNCTION,  /**< FSM is parsing function code. */
    MODBUS_STATE_PROCESSING,        /**< FSM is processing the received Modbus frame. */
    MODBUS_STATE_VALIDATING_FRAME,  /**< FSM is validating the received Modbus frame. */
    MODBUS_STATE_BUILDING_RESPONSE, /**< FSM is building a response to the master. */
    MODBUS_STATE_PUTTING_DATA_ON_BUFFER, /**< FSM is putting data on buffer to send. */
    MODBUS_STATE_CALCULATING_CRC,   /**< FSM is calculating response CRC. */
    MODBUS_STATE_SENDING,           /**< FSM is sending a response or Modbus frame. */
    MODBUS_STATE_ERROR,             /**< FSM has encountered an error state. */
    MODBUS_STATE_BOOTLOADER         /**< FSM is in bootloader mode. */
} modbus_state_t;

/**
 * @brief Modbus FSM Events
 *
 * This enum defines the possible events that trigger state transitions
 * in the Modbus finite state machine.
 */
typedef enum {
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



typedef enum modbus_error_t {
    // Library errors
    MODBUS_ERROR_WRONG_DEVICE_ID = -9,
    MODBUS_ERROR_NEED_RESTART = -8,
    MODBUS_ERROR_OTHER_REQUESTS = -7,
    MODBUS_OTHERS_REQUESTS = -6,
    MODBUS_ERROR_INVALID_REQUEST = -5,  /**< Received invalid request from client */
    MODBUS_ERROR_CRC = -4,              /**< Received invalid CRC */
    MODBUS_ERROR_TRANSPORT = -3,        /**< Transport error */
    MODBUS_ERROR_TIMEOUT = -2,          /**< Read/write timeout occurred */
    MODBUS_ERROR_INVALID_ARGUMENT = -1, /**< Invalid argument provided */
    MODBUS_ERROR_NONE = 0,              /**< No error */

    // Modbus exceptions
    MODBUS_EXCEPTION_ILLEGAL_FUNCTION = 1,      /**< Modbus exception 1 */
    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS = 2,  /**< Modbus exception 2 */
    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE = 3,    /**< Modbus exception 3 */
    MODBUS_EXCEPTION_SERVER_DEVICE_FAILURE = 4, /**< Modbus exception 4 */
} modbus_error_t;


/**
 * @brief Modbus transport type.
 */
typedef enum nmbs_transport {
    NMBS_TRANSPORT_RTU = 1,
    NMBS_TRANSPORT_TCP = 2,
} modbus_transport;

/**
 * Modbus platform configuration struct.
 * Passed to modbus_server_create() and modbus_client_create().
 *
 * read() and write() are the platform-specific methods that read/write data to/from a serial port or a TCP connection.
 *
 * Both methods should block until either:
 * - `count` bytes of data are read/written
 * - the byte timeout, with `byte_timeout_ms >= 0`, expires
 *
 * A value `< 0` for `byte_timeout_ms` means infinite timeout.
 * With a value `== 0` for `byte_timeout_ms`, the method should read/write once in a non-blocking fashion and return immediately.
 *
 *
 * Their return value should be the number of bytes actually read/written, or `< 0` in case of error.
 * A return value between `0` and `count - 1` will be treated as if a timeout occurred on the transport side. All other
 * values will be treated as transport errors.
 *
 * Additionally, an optional crc_calc() function can be defined to override the default nanoMODBUS CRC calculation function.
 *
 * These methods accept a pointer to arbitrary user-data, which is the arg member of this struct.
 * After the creation of an instance it can be changed with modbus_set_platform_arg().
 */
typedef struct modbus_platform_conf {
    modbus_transport transport; /*!< Transport type */
    int32_t (*read)(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);          /*!< Bytes read transport function pointer */
    int32_t (*write)(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);   /*!< Bytes write transport function pointer */
    uint16_t (*crc_calc)(const uint8_t* data, uint32_t length, void* arg);                      /*!< CRC calculation function pointer. Optional */
    // start timer
    // check timer
    void* arg;                       /*!< User data, will be passed to functions above */
} modbus_platform_conf;



/**
 * @brief Callback types for Modbus variable operations
 */
typedef int16_t (*modbus_read_callback_t)(void);
typedef int16_t (*modbus_write_callback_t)(uint16_t);

/**
 * @brief Structure representing a Modbus variable
 */
typedef struct {
    int16_t *variable_ptr;
    modbus_read_callback_t read_callback;
    modbus_write_callback_t write_callback;
    bool read_only;
    uint16_t address;
} variable_modbus_t;



/**
 * @brief Structure representing a variable type of device information
 */
typedef struct device_package_info_t
{
    uint8_t id;                                         /**< Id and position of variable in buffer. */
    uint8_t lenght;                                     /**< Lenght of value in ASCII. */
    uint8_t  value_in_ascii[MAX_DEVICE_PACKAGE_VALUES]; /**< Value in ASCII of variable. */
}device_package_info_t;


/**
 * @brief Structure representing a device information
 */
typedef struct device_identification_t
{
    uint16_t *address;                              /**< Device address pointer for addressing in Modbus RTU mode. */
    uint16_t *baudrate;                             /**< Modbus communication baud rate. */
    uint8_t mei_type;                               /**< MEI type of request. */
    uint8_t conformity_level;                       /**< Conformity level of the product. (Maybe is specifique of Haier board) */
    uint8_t more_follow;                            /**< Quantity of package will be sent after this one */
    uint8_t next_obj_id;                            /**< Id of the next objet that will be sent. */
    device_package_info_t data[MAX_DEVICE_PACKAGES];/**< Data Related to the variables of this device. */
    uint8_t info_saved;                             /**< Quantity of Data variables was saved. */
}device_identification_t;

/**
 * @brief Modbus context structure
 *
 * This structure holds all relevant data for managing Modbus communication.
 * It includes configuration settings, raw data buffers, Modbus message information, and timing references.
 */
typedef struct {
    fsm_t fsm;                      /**< Finite State Machine instance for Modbus protocol. */
    modbus_platform_conf platform;   
   

    /** Configuration settings for Modbus. */
    struct {
        uint16_t baudrate_index;       /**< Index for baud rate configuration. */
    } config;

    /** Raw data storage for Modbus communication. */
    struct {
        uint16_t rx_count;                             /**< UART receive data length (number of bytes received). */
        uint8_t rx_buffer[MODBUS_RECEIVE_BUFFER_SIZE]; /**< UART receive data buffer (raw bytes received). */
        uint8_t tx_buffer[MODBUS_SEND_BUFFER_SIZE];    /**< UART transmit data buffer. */
        uint8_t *tx_ptr;                               /**< Pointer to the transmit buffer (outgoing data). */
        uint16_t tx_count;                             /**< UART transmit data length (number of bytes to transmit). */
        uint16_t tx_index;                             /**< Index for transmit buffer. */
        uint16_t rx_index;                             /**< Index for receive buffer. */
    } raw_data;

    /** Parsed Modbus message information. */
    struct {
        uint8_t buffer[MODBUS_RECEIVE_BUFFER_SIZE];   /**< Buffer to store parsed Modbus message data. */
        uint8_t slave_address;                        /**< Modbus slave address. */
        uint8_t function_code;                        /**< Modbus function code indicating the requested operation. */
        uint16_t read_address;                        /**< Address to read data from (if applicable). */
        uint16_t read_quantity;                       /**< Quantity of data to read. */
        uint16_t current_read_index;
        uint16_t write_address;                       /**< Address to write data to (if applicable). */
        uint16_t write_quantity;                      /**< Quantity of data to write. */
        uint16_t write_value;
        uint8_t byte_count;                           /**< Byte count for data array, if the operation involves multiple data points. */
        modbus_error_t error;                         /**< Error code, if any errors are detected during message processing. */
        bool broadcast;                               /**< Flag indicating whether the message is a broadcast. */
        bool ignored;                                 /**< Flag indicating if the message should be ignored (e.g., an unsupported function). */

        uint8_t mei_type;
        uint8_t device_id_code;
        uint8_t device_obj_id;
    } msg;


    uint16_t rx_reference_time; /**< Timestamp for receiving data, used in timeouts. */
    uint16_t tx_reference_time; /**< Timestamp for transmitting data, used in timeouts. */
    uint16_t error_timer;

    device_identification_t device_info;
    int8_t flow_control_pin;
} modbus_context_t;



/******************************************************************************
* FUNCTIONS
******************************************************************************/
// modbus_error_t modbus_server_create(modbus_context_t* modbus, uint16_t* address_rtu, uint16_t* baudrate);
// modbus_error_t add_info_to_device(modbus_context_t *modbus, const char* value, uint8_t lenght);

// void modbus_server_poll(modbus_context_t * modbus);
// modbus_error_t modbus_set_holding_register(uint16_t address, int16_t *variable, bool read_only_flag, modbus_read_callback_t read_cb, modbus_write_callback_t write_cb);
// modbus_error_t modbus_set_array_holding_register(uint16_t address, uint16_t lenght, int16_t *variable, bool read_only, modbus_read_callback_t read_cb, modbus_write_callback_t write_cb);


// void modbus_receive_data_from_uart_event(fsm_t * fsm, uint8_t data);
// modbus_error_t modbus_send_data_to_uart_event(fsm_t *fsm );
// // void modbus_error_event(struct state_t * state, modbus_error_t error);

// int16_t update_baudrate(uint16_t baud);
// void set_flow_control_pin(modbus_context_t *modbus, int8_t flow_control_pin);

// // debug only
// void restart_uart(void);
#endif  /*_MODBUS_PROTOCOL_H*/
/** @} */