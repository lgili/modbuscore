/**
 * @file modbus_server.h
 * @brief Modbus Server (Slave) state machine and interface.
 *
 * This header defines the interface for the Modbus Server (Slave) implementation.
 * It integrates with the FSM defined in `fsm.h`, utilizing states and events to handle
 * the reception and processing of Modbus RTU requests, and the sending of responses.
 *
 * **Key Features:**
 * - **FSM Integration:** Utilizes a finite state machine to manage the states involved in handling Modbus requests.
 * - **Frame Handling:** Receives, parses, processes, and responds to Modbus RTU frames.
 * - **Register Management:** Provides functions to register and manage holding registers.
 * - **Device Information:** Supports adding device-specific information for responding to device info requests.
 * - **Polling Mechanism:** Offers a polling function to be called regularly within the main loop for event processing.
 *
 * **Usage Example:**
 * @code
 * #include <modbus/modbus_server.h>
 * 
 * // Define read and write callbacks if custom handling is needed
 * bool read_callback(modbus_context_t *ctx, uint16_t address, int16_t *value) {
 *     // Custom read logic
 *     return true;
 * }
 * 
 * bool write_callback(modbus_context_t *ctx, uint16_t address, int16_t value) {
 *     // Custom write logic
 *     return true;
 * }
 * 
 * int main(void) {
 *     modbus_context_t ctx;
 *     modbus_transport_t transport = {
 *         .write = transport_write_function,    // User-defined transport write function
 *         .read = transport_read_function,      // User-defined transport read function
 *         .get_reference_msec = get_msec_function // User-defined function to get current time in ms
 *     };
 *     
 *     uint16_t device_addr = 10;
 *     uint16_t baud = 19200;
 *     
 *     ctx.transport = transport;
 *     ctx.role = MODBUS_ROLE_SLAVE;
 *     ctx.server_data.device_info.address = &device_addr;
 *     ctx.server_data.device_info.baudrate = &baud;
 *     
 *     // Initialize Modbus server
 *     modbus_error_t error = modbus_server_create(&ctx, &transport, &device_addr, &baud);
 *     if (error != MODBUS_ERROR_NONE) {
 *         // Handle initialization error
 *     }
 *     
 *     // Register holding registers
 *     static int16_t reg_value = 100;
 *     modbus_set_holding_register(&ctx, 0x0000, &reg_value, false, read_callback, write_callback);
 *     
 *     // Main loop
 *     while (1) {
 *         // Inject received bytes into FSM via modbus_server_receive_data_from_uart_event()
 *         // This should be called whenever a new byte is received from the transport layer
 *         uint8_t received_byte = get_received_byte(); // User-defined function
 *         modbus_server_receive_data_from_uart_event(&ctx.server_data.fsm, received_byte);
 *         
 *         // Poll the Modbus server FSM
 *         modbus_server_poll(&ctx);
 *         
 *         // Other application code
 *     }
 *     
 *     return 0;
 * }
 * @endcode
 *
 * **Notes:**
 * - Ensure that the transport layer functions (`write`, `read`, `get_reference_msec`) are properly implemented and initialized before using the Modbus server.
 * - Handle exceptions and error codes appropriately to maintain robust communication.
 * - The `modbus_server_poll()` function should be called regularly (e.g., within the main loop) to process incoming events.
 *
 * @author
 * Luiz Carlos Gili
 * 
 * @date
 * 2024-12-20
 *
 * @ingroup ModbusServer
 * @{
 */

#ifndef MODBUS_SERVER_H
#define MODBUS_SERVER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif 

#include <modbus/base.h>    /**< For `modbus_context_t`, `modbus_error_t`, etc. */
#include <modbus/fsm.h>     /**< For FSM integration */
#include <modbus/utils.h>   /**< For safe reads from buffers, etc. */

/* -------------------------------------------------------------------------- */
/*                             Device Information                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Structure representing a package of device information.
 *
 * This structure holds a single package of device information, including
 * the package ID, the length of the ASCII value, and the ASCII data itself.
 */
typedef struct {
    uint8_t id;                                        /**< ID of the package */
    uint8_t length;                                    /**< Length of the ASCII value */
    uint8_t value_in_ascii[MAX_DEVICE_PACKAGE_VALUES]; /**< ASCII data of the package */
} device_package_info_t;

/**
 * @brief Structure representing device-specific information for a Server device.
 *
 * This structure holds various pieces of information about the Modbus device,
 * such as its address, baud rate, MEI type, conformity level, and device packages.
 */
typedef struct {
    uint16_t *address;                  /**< Pointer to the device address (RTU) */
    uint16_t *baudrate;                 /**< Pointer to the Modbus baud rate */
    uint8_t mei_type;                   /**< MEI type for device info requests */
    uint8_t conformity_level;           /**< Conformity level of the product */
    uint8_t more_follow;                /**< Number of additional packages */
    uint8_t next_obj_id;                /**< Next object ID to send */
    device_package_info_t data[MAX_DEVICE_PACKAGES]; /**< Array of device information packages */
    uint8_t info_saved;                 /**< Number of data packages saved */
} device_identification_t;

/* -------------------------------------------------------------------------- */
/*                         Modbus Server Internal Structure                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief Internal structure holding server-specific data.
 *
 * This structure encapsulates all data related to the Modbus server, including
 * the FSM instance, device information, current message processing state, and holding registers.
 */
typedef struct {
    fsm_t fsm;                          /**< FSM instance for managing server states */
    modbus_context_t *ctx;              /**< Pointer to the Modbus context */
    
    /** 
     * @brief Device information used for responding to device info requests.
     * 
     * This includes the device address, baud rate, MEI type, and other identification data.
     */
    device_identification_t device_info;
    
    /**
     * @brief Current message being processed.
     *
     * This sub-structure holds information about the ongoing Modbus message,
     * including function codes, addresses, data buffers, and error states.
     */
    struct {
        uint8_t function_code;                  /**< Current function code being processed */
        uint8_t slave_address;                  /**< Address of the slave device */
        bool broadcast;                         /**< Indicates if the message is a broadcast */
        modbus_error_t error;                   /**< Current error state */
        uint16_t read_address;                  /**< Starting address for read operations */
        uint16_t read_quantity;                 /**< Quantity of registers/coils to read */
        uint16_t write_address;                 /**< Starting address for write operations */
        uint16_t write_quantity;                /**< Quantity of registers/coils to write */
        uint16_t write_value;                   /**< Value to write for single write operations */
        uint8_t byte_count;                     /**< Byte count for variable-length responses */
        uint8_t buffer[MODBUS_RECEIVE_BUFFER_SIZE]; /**< Buffer for receiving data */
        uint16_t current_read_index;            /**< Current read index in the buffer */
        
        uint8_t mei_type;                       /**< MEI type for device info */
        uint8_t device_id_code;                 /**< Device ID code for device info */
        uint8_t device_obj_id;                  /**< Device object ID for device info */
    } msg;
    
    /**
     * @brief Array of holding registers managed by the server.
     *
     * Each holding register is represented by a `variable_modbus_t` structure,
     * allowing for read and write operations by the client.
     */
    variable_modbus_t holding_registers[MAX_SIZE_HOLDING_REGISTERS]; /**< Array of holding registers */
    uint16_t holding_register_count;                                        /**< Number of holding registers registered */
} modbus_server_data_t;

/* -------------------------------------------------------------------------- */
/*                         Modbus Server State Definitions                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief Modbus FSM state: Idle.
 *
 * This state represents the idle condition of the Modbus FSM, where it is waiting for an event to occur.
 * In this state, the FSM will transition to the `MODBUS_SERVER_STATE_RECEIVING` state when a byte is received.
 */
extern const fsm_state_t modbus_server_state_idle;   

/**
 * @brief Modbus FSM state: Receiving.
 *
 * This state represents the process of receiving Modbus data. The FSM remains in this state
 * while bytes are being received from the transport layer.
 */
extern const fsm_state_t modbus_server_state_receiving;   

/**
 * @brief Modbus FSM state: Parsing Address.
 *
 * This state parses the incoming data to extract the slave address from the received frame.
 */
extern const fsm_state_t modbus_server_state_parsing_address; 

/**
 * @brief Modbus FSM state: Parsing Function.
 *
 * This state parses the incoming data to extract the function code from the received frame.
 */
extern const fsm_state_t modbus_server_state_parsing_function; 

/**
 * @brief Modbus FSM state: Processing.
 *
 * This state handles the processing of the received Modbus frame, executing the requested function
 * (e.g., reading or writing registers).
 */
extern const fsm_state_t modbus_server_state_processing;  

/**
 * @brief Modbus FSM state: Validating Frame.
 *
 * This state runs the CRC check on the received frame to validate its integrity.
 */
extern const fsm_state_t modbus_server_state_validating_frame; 

/**
 * @brief Modbus FSM state: Building Response.
 *
 * This state builds a response frame based on the processed request data.
 */
extern const fsm_state_t modbus_server_state_building_response; 

/**
 * @brief Modbus FSM state: Putting Data on Buffer.
 *
 * This state prepares the response data to be sent by placing it into the transmit buffer.
 */
extern const fsm_state_t modbus_server_state_putting_data_on_buffer; 

/**
 * @brief Modbus FSM state: Calculating CRC.
 *
 * This state calculates the CRC for the response data to ensure data integrity during transmission.
 */
extern const fsm_state_t modbus_server_state_calculating_crc; 

/**
 * @brief Modbus FSM state: Sending.
 *
 * This state handles the sending of the response frame after it has been built and CRC-checked.
 */
extern const fsm_state_t modbus_server_state_sending;   

/**
 * @brief Modbus FSM state: Error.
 *
 * This state represents an error condition in the Modbus FSM. The FSM can recover and transition
 * back to the idle state when a new byte is received.
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
    MODBUS_EVENT_RX_BYTE_RECEIVED = 1,      /**< A new byte was received */
    MODBUS_EVENT_PARSE_ADDRESS,             /**< Trigger parsing of slave address */
    MODBUS_EVENT_PARSE_FUNCTION,            /**< Trigger parsing of function code */
    MODBUS_EVENT_PROCESS_FRAME,             /**< Trigger processing of the request frame */
    MODBUS_EVENT_VALIDATE_FRAME,            /**< Validate frame CRC and integrity */
    MODBUS_EVENT_BUILD_RESPONSE,            /**< Build response frame */
    MODBUS_EVENT_BROADCAST_DONT_ANSWER,     /**< Broadcast request received - do not respond */
    MODBUS_EVENT_PUT_DATA_ON_BUFFER,        /**< Put response data in TX buffer */
    MODBUS_EVENT_CALCULATE_CRC,             /**< Calculate CRC for response */
    MODBUS_EVENT_SEND_RESPONSE,             /**< Send response frame */
    MODBUS_EVENT_TX_COMPLETE,               /**< Transmission completed */
    MODBUS_EVENT_ERROR_DETECTED,            /**< An error occurred (CRC, invalid request, etc.) */
    MODBUS_EVENT_ERROR_WRONG_BAUDRATE,      /**< Wrong baud rate detected (if applicable) */
    MODBUS_EVENT_TIMEOUT,                   /**< Timeout occurred waiting for frame */
    MODBUS_EVENT_BOOTLOADER,                /**< Enter bootloader mode (if implemented) */
    MODBUS_EVENT_RESTART_FROM_ERROR         /**< Restart FSM after error */
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
  * @param[in,out] ctx              Pointer to the Modbus context. The context must be allocated by the user,
  *                                   and at least the `transport` and `device_info` fields must be set before calling.
  * @param[in]     platform_conf    Pointer to the transport configuration (`modbus_transport_t`).
  * @param[in]     device_address   Pointer to the device address (RTU).
  * @param[in]     baudrate         Pointer to the Modbus baud rate.
  * 
  * @return modbus_error_t `MODBUS_ERROR_NONE` on success, or an error code if initialization fails.
  *
  * @retval MODBUS_ERROR_NONE          Server initialized successfully.
  * @retval MODBUS_ERROR_INVALID_ARGUMENT Invalid arguments provided.
  * @retval MODBUS_ERROR_MEMORY        Memory allocation failed (if applicable).
  * @retval Others                      Various error codes as defined in `modbus_error_t`.
  *
  * @warning
  * - Ensure that `ctx`, `platform_conf`, `device_address`, and `baudrate` are valid pointers.
  * - The transport layer functions (`write`, `read`, `get_reference_msec`) must be properly implemented.
  * 
  * @example
  * ```c
  * modbus_context_t ctx;
  * modbus_transport_t transport = {
  *     .write = transport_write_function,       // User-defined transport write function
  *     .read = transport_read_function,         // User-defined transport read function
  *     .get_reference_msec = get_msec_function   // User-defined function to get current time in ms
  * };
  * 
  * uint16_t device_addr = 10;
  * uint16_t baud = 19200;
  * 
  * ctx.transport = transport;
  * ctx.role = MODBUS_ROLE_SERVER;
  * ctx.server_data.device_info.address = &device_addr;
  * ctx.server_data.device_info.baudrate = &baud;
  * 
  * modbus_error_t error = modbus_server_create(&ctx, &transport, &device_addr, &baud);
  * if (error != MODBUS_ERROR_NONE) {
  *     // Handle initialization error
  * }
  * ```
  */
modbus_error_t modbus_server_create(modbus_context_t *ctx, modbus_transport_t *platform_conf, uint16_t *device_address, uint16_t *baudrate);

/**
 * @brief Polls the Modbus server state machine.
 *
 * This function should be called regularly (e.g., inside the main loop) to process pending events.
 * It will attempt to parse received data, handle requests, and send responses as needed.
 *
 * @param[in,out] ctx Pointer to the Modbus context.
 *
 * @note
 * - The polling function manages the FSM transitions based on incoming events and data.
 * - It is essential to regularly call this function to ensure timely processing of Modbus requests.
 *
 * @example
 * ```c
 * while (1) {
 *     // Poll the Modbus server FSM
 *     modbus_server_poll(&ctx);
 *     
 *     // Other application code
 * }
 * ```
 */
void modbus_server_poll(modbus_context_t *ctx);

/**
 * @brief Called when a new byte is received from UART (or another transport).
 *
 * The user should call this function whenever a new byte arrives from the transport layer.
 * This function injects a `MODBUS_EVENT_RX_BYTE_RECEIVED` event into the FSM and stores
 * the received byte in the RX buffer.
 *
 * @param[in,out] fsm  Pointer to the FSM instance associated with the Modbus server.
 * @param[in]     data  The received byte.
 *
 * @warning
 * - Ensure that `fsm` is properly initialized and points to a valid FSM instance.
 * - This function is thread-safe and can be called from interrupt service routines (ISRs) if necessary.
 *
 * @example
 * ```c
 * // Assume `received_byte` is obtained from the transport layer
 * uint8_t received_byte = get_received_byte(); // User-defined function
 * modbus_server_receive_data_from_uart_event(&ctx.server_data.fsm, received_byte);
 * ```
 */
void modbus_server_receive_data_from_uart_event(fsm_t *fsm, uint8_t data);

/**
 * @brief Registers a single holding register in the server.
 *
 * This function associates a Modbus holding register address with a variable in memory.
 * Optionally, read and write callbacks can be provided for custom logic. If no callbacks
 * are provided, the `variable_ptr` is read/written directly.
 *
 * @param[in,out] ctx          Pointer to the Modbus context.
 * @param[in]     address      Modbus address of the register.
 * @param[in]     variable     Pointer to the variable holding the register value.
 * @param[in]     read_only    `true` if the register is read-only, `false` if writable.
 * @param[in]     read_cb      Optional read callback. If `NULL`, direct memory read is used.
 * @param[in]     write_cb     Optional write callback. If `NULL`, direct memory write is used.
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` on success, or an error code if registration fails.
 *
 * @retval MODBUS_ERROR_NONE          Register added successfully.
 * @retval MODBUS_ERROR_INVALID_ARGUMENT Invalid arguments provided.
 * @retval MODBUS_ERROR_NO_MEMORY     No space available to add the register.
 * @retval Others                      Various error codes as defined in `modbus_error_t`.
 *
 * @warning
 * - Ensure that `variable` points to a valid memory location that remains valid for the lifetime of the server.
 * - Callback functions (`read_cb`, `write_cb`) must adhere to the expected signatures and handle operations correctly.
 * 
 * @example
 * ```c
 * static int16_t reg_value = 100;
 * modbus_error_t error = modbus_set_holding_register(&ctx, 0x0000, &reg_value, false, read_callback, write_callback);
 * if (error != MODBUS_ERROR_NONE) {
 *     // Handle registration error
 * }
 * ```
 */
modbus_error_t modbus_set_holding_register(modbus_context_t *ctx, uint16_t address, int16_t *variable, bool read_only,
                                           modbus_read_callback_t read_cb, modbus_write_callback_t write_cb);

/**
 * @brief Registers an array of holding registers in the server.
 *
 * Similar to `modbus_set_holding_register()`, but for multiple consecutive addresses.
 *
 * @param[in,out] ctx          Pointer to the Modbus context.
 * @param[in]     start_address Starting address of the registers.
 * @param[in]     length        Number of registers to map.
 * @param[in]     variable      Pointer to the first variable in the array.
 * @param[in]     read_only     `true` if the registers are read-only, `false` if writable.
 * @param[in]     read_cb       Optional read callback. If `NULL`, direct memory read is used.
 * @param[in]     write_cb      Optional write callback. If `NULL`, direct memory write is used.
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` on success, or an error code if registration fails.
 *
 * @retval MODBUS_ERROR_NONE          Registers added successfully.
 * @retval MODBUS_ERROR_INVALID_ARGUMENT Invalid arguments provided.
 * @retval MODBUS_ERROR_NO_MEMORY     No space available to add the registers.
 * @retval Others                      Various error codes as defined in `modbus_error_t`.
 *
 * @warning
 * - Ensure that `variable` points to a valid array of variables with at least `length` elements.
 * - Callback functions (`read_cb`, `write_cb`) must adhere to the expected signatures and handle operations correctly.
 * 
 * @example
 * ```c
 * static int16_t reg_values[10] = {0};
 * modbus_error_t error = modbus_set_array_holding_register(&ctx, 0x0010, 10, reg_values, false, NULL, NULL);
 * if (error != MODBUS_ERROR_NONE) {
 *     // Handle registration error
 * }
 * ```
 */
modbus_error_t modbus_set_array_holding_register(modbus_context_t *ctx, uint16_t start_address, uint16_t length,
                                                 int16_t *variable, bool read_only,
                                                 modbus_read_callback_t read_cb, modbus_write_callback_t write_cb);

/**
 * @brief Adds device information (e.g., vendor name, product code) to the server.
 *
 * This function is useful for responding to function code 0x2B (Read Device Information).
 * It allows the server to provide detailed information about the device, enhancing
 * interoperability and device identification.
 *
 * @param[in,out] ctx    Pointer to the Modbus context.
 * @param[in]     value  Pointer to a character array containing the ASCII value of the device information.
 * @param[in]     length Number of characters in the value.
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` on success, or an error code if addition fails.
 *
 * @retval MODBUS_ERROR_NONE          Device information added successfully.
 * @retval MODBUS_ERROR_INVALID_ARGUMENT Invalid arguments provided.
 * @retval MODBUS_ERROR_NO_MEMORY     No space available to add the device information.
 * @retval Others                      Various error codes as defined in `modbus_error_t`.
 *
 * @warning
 * - Ensure that `value` points to a valid ASCII string and that `length` does not exceed `MAX_DEVICE_PACKAGE_VALUES`.
 * - Device information should be added before starting the main loop to ensure availability during requests.
 * 
 * @example
 * ```c
 * const char vendor_name[] = "OpenAI_Modbus_Slave";
 * modbus_error_t error = modbus_server_add_device_info(&ctx, vendor_name, sizeof(vendor_name) - 1);
 * if (error != MODBUS_ERROR_NONE) {
 *     // Handle addition error
 * }
 * ```
 */
modbus_error_t modbus_server_add_device_info(modbus_context_t *ctx, const char *value, uint8_t length);

/**
 * @brief Updates the baud rate of the Modbus server.
 *
 * This function allows dynamic updating of the Modbus server's baud rate. It is useful
 * for scenarios where the baud rate needs to be changed without restarting the server.
 *
 * @param[in] baud The new baud rate to set.
 *
 * @return int16_t The updated baud rate, or an error code if the update fails.
 *
 * @retval >0  The new baud rate set successfully.
 * @retval -1  Failed to update the baud rate.
 *
 * @warning
 * - Ensure that the transport layer supports dynamic baud rate changes.
 * - Changing the baud rate may disrupt ongoing communications; handle with care.
 * 
 * @example
 * ```c
 * int16_t new_baud = 38400;
 * int16_t result = update_baudrate(new_baud);
 * if (result < 0) {
 *     // Handle baud rate update error
 * }
 * ```
 */
int16_t update_baudrate(uint16_t baud);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_SERVER_H */

/** @} */
