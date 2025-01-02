/**
 * @file modbus_server.h
 * @brief Modbus Server (Slave) state machine and interface.
 *
 * This header defines the interface for the Modbus Server (Slave) implementation.
 * It integrates with the FSM defined in fsm.h, using states and events to handle
 * the reception and processing of Modbus RTU requests, and the sending of responses.
 *
 * Key features:
 * - States representing different steps of frame reception, parsing, processing, and response.
 * - Events triggered by incoming bytes, parsed requests, and completion of transmissions.
 * - Functions to create and configure the server context, set holding registers, and add device info.
 * - A polling function (`modbus_server_poll()`) to be called regularly in the main loop.
 *
 * The server uses the core Modbus functions (modbus_core) to parse frames and handle CRC,
 * and relies on user-provided callbacks (via `modbus_set_holding_register()` and friends) to
 * read and write variables.
 *
 * The `modbus_context_t` should be properly initialized with transport and role set to Slave.
 * The FSM will handle events like `MODBUS_EVENT_RX_BYTE_RECEIVED`, `MODBUS_EVENT_PARSE_ADDRESS`,
 * `MODBUS_EVENT_PROCESS_FRAME`, etc.
 *
 * Example usage:
 * @code
 * modbus_context_t ctx;
 * modbus_transport_t transport = {...}; // Set platform functions
 * ctx.transport = transport;
 * ctx.role = MODBUS_ROLE_SLAVE;
 *
 * uint16_t device_addr = 10;
 * uint16_t baud = 19200;
 * ctx.device_info.address = &device_addr;
 * ctx.device_info.baudrate = &baud;
 *
 * // Initialize server
 * modbus_server_create(&ctx);
 *
 * // Register holding registers
 * static int16_t reg_value = 100;
 * modbus_set_holding_register(0x0000, &reg_value, false, NULL, NULL);
 *
 * // Main loop
 * while (true) {
 *    // Inject received bytes into FSM via modbus_server_receive_data_from_uart_event()
 *    modbus_server_poll(&ctx);
 * }
 * @endcode
 * 
 * @author
 * @date 2024-12-20
 *
 * @addtogroup ModbusServer
 * @{
 */

#ifndef MODBUS_SERVER_H
#define MODBUS_SERVER_H

#ifdef __cplusplus
extern "C"{
#endif 

#include <modbus/base.h>
#include <modbus/fsm.h>


/* -------------------------------------------------------------------------- */
/*                             Device Information                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Structure representing a package of device information.
 */
typedef struct {
    uint8_t id;                                        /**< ID of the package */
    uint8_t length;                                    /**< Length of the ASCII value */
    uint8_t value_in_ascii[MAX_DEVICE_PACKAGE_VALUES]; /**< ASCII data of the package */
} device_package_info_t;

/**
 * @brief Structure representing device-specific information for a Slave device.
 *
 * A Master may not need this, or may use it differently.
 */
typedef struct {
    uint16_t *address;                  /**< Pointer to the device address (RTU) */
    uint16_t *baudrate;                 /**< Pointer to the Modbus baudrate */
    uint8_t mei_type;                   /**< MEI type for device info requests */
    uint8_t conformity_level;           /**< Conformity level of the product */
    uint8_t more_follow;                /**< Number of additional packages */
    uint8_t next_obj_id;                /**< Next object ID to send */
    device_package_info_t data[MAX_DEVICE_PACKAGES];
    uint8_t info_saved;                 /**< Number of data packages saved */
} device_identification_t;


/* Estrutura interna com dados do servidor */
typedef struct {
    fsm_t fsm;
    modbus_context_t *ctx;
    // Device info (mostly used by Slave, but a Master might store info about slaves)
    device_identification_t device_info;


    struct {
        uint8_t function_code;
        uint8_t slave_address;
        bool broadcast;
        modbus_error_t error;
        uint16_t read_address;
        uint16_t read_quantity;
        uint16_t write_address;
        uint16_t write_quantity;
        uint16_t write_value;
        uint8_t byte_count;
        uint8_t buffer[MODBUS_RECEIVE_BUFFER_SIZE];
        uint16_t current_read_index;

        uint8_t mei_type;        
        uint8_t device_id_code;
        uint8_t device_obj_id;
    } msg;

    variable_modbus_t holding_registers[MAX_SIZE_HOLDING_REGISTERS];
    uint16_t holding_register_count;
} modbus_server_data_t;

/* -------------------------------------------------------------------------- */
/*                         Modbus Server State Definitions                    */
/* -------------------------------------------------------------------------- */

/** 
* @brief Modbus FSM state: Idle 
* 
* This state represents the idle condition of the Modbus FSM, where it is waiting for an event to occur. 
* In this state, the FSM will transition to the `MODBUS_STATE_RECEIVING` state when a byte is received. 
*/ 
extern const fsm_state_t modbus_server_state_idle;   

/** 
* @brief Modbus FSM state: Receiving 
* 
* This state represents the process of receiving Modbus data.:
*/ 
extern const fsm_state_t modbus_server_state_receiving;   

/** 
* @brief Modbus FSM state: Parsing Address 
* 
* This state parse the income data to get the address of the slave that should respond to master:
*/ 
extern const fsm_state_t modbus_server_state_parsing_address; 

/** 
* @brief Modbus FSM state: Parsing Function 
* 
* This state parse the income data to get the function that master is asking:
*/ 
extern const fsm_state_t modbus_server_state_parsing_function; 

/** 
* @brief Modbus FSM state: Processing 
* 
* This state handles the processing of the received Modbus frame.
*/ 
extern const fsm_state_t modbus_server_state_processing;  

/** 
* @brief Modbus FSM state: Validate Frame 
* 
* This state runs the CRC on the received frame to validate.
*/ 
extern const fsm_state_t modbus_server_state_validating_frame; 

/** 
* @brief Modbus FSM state: Building Response 
* 
* This state build a response to master, based on data received.
*/
extern const fsm_state_t modbus_server_state_building_response; 

/** 
* @brief Modbus FSM state: Put Data on Buffer 
* 
* This state put data on buffer to be send.
*/
extern const fsm_state_t modbus_server_state_putting_data_on_buffer; 

/** 
* @brief Modbus FSM state: Calculating CRC 
* 
* This state calculate a CRC for the data that will be sent.
*/
extern const fsm_state_t modbus_server_state_calculating_crc; 

/** 
* @brief Modbus FSM state: Sending 
* 
* This state handles sending a response or data after processing. Currently not declared here, but it can be used
* to handle data transmission to the master. 
*/ 
extern const fsm_state_t modbus_server_state_sending;   

/** 
* @brief Modbus FSM state: Error 
* 
* This state represents an error condition in the Modbus FSM. It can recover and transition back to idle 
* when a new byte is received. 
*/ 
extern const fsm_state_t modbus_server_state_error; 


/**
 * @brief Enumeration of states in the Modbus server FSM.
 *
 * These states represent the server's internal steps in handling a request frame.
 */
typedef enum {
    MODBUS_SERVER_STATE_IDLE = 0,            /**< Waiting for new requests */
    MODBUS_SERVER_STATE_RECEIVING,           /**< Receiving frame bytes */
    MODBUS_SERVER_STATE_PARSING_ADDRESS,     /**< Parsing the slave address */
    MODBUS_SERVER_STATE_PARSING_FUNCTION,    /**< Parsing the function code */
    MODBUS_SERVER_STATE_PROCESSING,          /**< Processing the request (read/write registers, etc.) */
    MODBUS_SERVER_STATE_VALIDATING_FRAME,    /**< Validating CRC and request frame integrity */
    MODBUS_SERVER_STATE_BUILDING_RESPONSE,   /**< Building response frame data */
    MODBUS_SERVER_STATE_PUTTING_DATA_ON_BUF, /**< Preparing data to send in response */
    MODBUS_SERVER_STATE_CALCULATING_CRC,     /**< Calculating CRC for response */
    MODBUS_SERVER_STATE_SENDING,             /**< Sending response frame */
    MODBUS_SERVER_STATE_ERROR                /**< Error state, waiting to recover */
} modbus_server_state_t;

/* -------------------------------------------------------------------------- */
/*                           Modbus Server Events                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Enumeration of events that can trigger transitions in the Modbus server FSM.
 */
typedef enum {
    MODBUS_EVENT_RX_BYTE_RECEIVED = 1,  /**< A new byte was received */
    MODBUS_EVENT_PARSE_ADDRESS,         /**< Trigger parsing of slave address */
    MODBUS_EVENT_PARSE_FUNCTION,        /**< Trigger parsing of function code */
    MODBUS_EVENT_PROCESS_FRAME,         /**< Trigger processing of the request frame */
    MODBUS_EVENT_VALIDATE_FRAME,        /**< Validate frame CRC and integrity */
    MODBUS_EVENT_BUILD_RESPONSE,        /**< Build response frame */
    MODBUS_EVENT_BROADCAST_DONT_ANSWER, /**< Broadcast request received - do not respond */
    MODBUS_EVENT_PUT_DATA_ON_BUFFER,    /**< Put response data in TX buffer */
    MODBUS_EVENT_CALCULATE_CRC,         /**< Calculate CRC for response */
    MODBUS_EVENT_SEND_RESPONSE,         /**< Send response frame */
    MODBUS_EVENT_TX_COMPLETE,           /**< Transmission completed */
    MODBUS_EVENT_ERROR_DETECTED,        /**< An error occurred (CRC, invalid request, etc.) */
    MODBUS_EVENT_ERROR_WRONG_BAUDRATE,  /**< Wrong baudrate detected (if applicable) */
    MODBUS_EVENT_TIMEOUT,               /**< Timeout occurred waiting for frame */
    MODBUS_EVENT_BOOTLOADER,            /**< Enter bootloader mode (if implemented) */
    MODBUS_EVENT_RESTART_FROM_ERROR     /**< Restart FSM after error */
} modbus_server_event_t;

/* -------------------------------------------------------------------------- */
/*                           Public API Functions                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Creates and initializes the Modbus server (slave) context.
 *
 * This function sets up the FSM with the initial state, initializes buffers, and prepares
 * the server to receive and respond to Modbus requests.
 *
 * @param ctx Pointer to the Modbus context. The context must be allocated by the user and 
 *            at least the transport and device_info fields must be set.
 * 
 * @return modbus_error_t MODBUS_ERROR_NONE on success, or an error code.
 */
// modbus_error_t modbus_server_create(modbus_context_t *ctx);
modbus_error_t modbus_server_create(modbus_context_t *modbus, modbus_transport_t *platform_conf, uint16_t *device_address, uint16_t *baudrate);

/**
 * @brief Polls the Modbus server state machine.
 *
 * This function should be called regularly (e.g., inside the main loop) to process pending events.
 * It will attempt to parse received data, handle requests, and send responses as needed.
 *
 * @param ctx Pointer to the Modbus context.
 */
void modbus_server_poll(modbus_context_t *ctx);

/**
 * @brief Called when a new byte is received from UART (or another transport).
 *
 * The user should call this function whenever a new byte arrives from the transport layer.
 * This function injects a `MODBUS_EVENT_RX_BYTE_RECEIVED` event into the FSM, and stores
 * the received byte in the RX buffer.
 *
 * @param fsm  Pointer to the FSM instance associated with the Modbus server.
 * @param data The received byte.
 */
void modbus_server_receive_data_from_uart_event(fsm_t *fsm, uint8_t data);

/**
 * @brief Registers a single holding register in the server.
 *
 * This function associates a Modbus holding register address with a variable in memory.
 * Optionally, read and write callbacks can be provided for custom logic. If no callbacks
 * are provided, the variable_ptr is read/written directly.
 *
 * @param address     Modbus address of the register.
 * @param variable    Pointer to the variable holding the register value.
 * @param read_only   True if the register is read-only, false if writable.
 * @param read_cb     Optional read callback. If NULL, direct memory read is used.
 * @param write_cb    Optional write callback. If NULL, direct memory write is used.
 *
 * @return modbus_error_t MODBUS_ERROR_NONE on success, or error code if no space or invalid args.
 */
modbus_error_t modbus_set_holding_register(modbus_context_t *ctx, uint16_t address, int16_t *variable, bool read_only,
                                           modbus_read_callback_t read_cb, modbus_write_callback_t write_cb);

/**
 * @brief Registers an array of holding registers in the server.
 *
 * Similar to modbus_set_holding_register(), but for multiple consecutive addresses.
 *
 * @param start_address Starting address of the registers.
 * @param length        Number of registers to map.
 * @param variable      Pointer to the first variable in the array.
 * @param read_only     True if read-only, false if writable.
 * @param read_cb       Optional read callback.
 * @param write_cb      Optional write callback.
 *
 * @return modbus_error_t MODBUS_ERROR_NONE on success, or error code.
 */
modbus_error_t modbus_set_array_holding_register(modbus_context_t *ctx, uint16_t start_address, uint16_t length,
                                                 int16_t *variable, bool read_only,
                                                 modbus_read_callback_t read_cb, modbus_write_callback_t write_cb);

/**
 * @brief Adds device information (e.g., vendor name, product code) to the server.
 *
 * This is useful if the server needs to respond to function 0x2B (Read Device Information).
 *
 * @param ctx    Pointer to the Modbus context.
 * @param value  Pointer to a character array containing the ASCII value.
 * @param length Number of characters in the value.
 *
 * @return modbus_error_t MODBUS_ERROR_NONE on success, or error code if no space or invalid args.
 */
modbus_error_t modbus_server_add_device_info(modbus_context_t *ctx, const char *value, uint8_t length);


int16_t update_baudrate(uint16_t baud);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_SERVER_H */

/** @} */
