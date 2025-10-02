/**
 * @file modbus_master.h
 * @brief Modbus Master (Client) interface and state machine definitions.
 *
 * This header defines the interface for implementing a Modbus Master (client).
 * It utilizes the same basic structures from the Modbus context (`modbus_context_t`)
 * and can optionally use a Finite State Machine (FSM) to manage requests,
 * frame transmission, and response reception.
 *
 * **Expected Functionalities of the Master:**
 * - Sending Modbus requests (e.g., reading holding registers from a slave, writing registers, etc.).
 * - Waiting for responses within configured timeouts.
 * - Handling exceptions returned by the slave.
 * - Functions to initialize the Master, create requests, and execute them.
 *
 * Similar to the Slave, the Master does not rely on dynamic memory allocation.
 * All memory is pre-allocated in the context. The user will call `modbus_client_create()`
 * to initialize the Master.
 *
 * @author
 * Luiz Carlos Gili
 * 
 * @date
 * 2024-12-20
 *
 * @addtogroup ModbusMaster
 * @{
 */

#ifndef MODBUS_MASTER_H
#define MODBUS_MASTER_H

#ifdef __cplusplus
extern "C"{
#endif

#include <modbus/base.h>
#include <modbus/fsm.h>

/* -------------------------------------------------------------------------- */
/*                           Structure Definitions                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Structure representing device-specific information for a Master device.
 *
 * This structure holds identification information relevant to the Master device,
 * such as the baud rate pointer.
 */
typedef struct {
    uint16_t *baudrate;                 /**< Pointer to the Modbus baud rate */
} client_device_identification_t;

/**
 * @brief Internal structure for Master data.
 *
 * This structure encapsulates all data related to the Modbus Master, including
 * the FSM instance, Modbus context, device information, current request details,
 * read data buffer, and timeout references.
 */
typedef struct {
    fsm_t fsm;                           /**< FSM instance for managing Master states */
    modbus_context_t *ctx;               /**< Pointer to the Modbus context */
    client_device_identification_t device_info; /**< Device-specific information */
    
    uint16_t timeout_ms;                 /**< Timeout in milliseconds for responses */
    
    /* Current request data */
    uint8_t current_slave_address;       /**< Address of the target slave device */
    uint8_t current_function;            /**< Current function code being used */
    uint16_t current_start_address;      /**< Starting address for the current request */
    uint16_t current_quantity;           /**< Quantity of registers/coils for the current request */
    
    /* Buffer to store data read from the last response */
    int16_t read_data[MODBUS_MAX_READ_WRITE_SIZE]; /**< Buffer for storing read data */
    uint16_t read_data_count;            /**< Number of registers read */
    
    /* Reference timer for handling timeouts */
    mb_time_ms_t request_time_ref;       /**< Reference time for the current request */
} modbus_client_data_t;

/* -------------------------------------------------------------------------- */
/*                           FSM State Definitions                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Modbus Master FSM state: Idle.
 *
 * This state represents the idle condition of the Modbus FSM, where it is waiting
 * for a new request to be sent.
 */
extern const fsm_state_t modbus_client_state_idle;

/**
 * @brief Modbus Master FSM state: Sending Request.
 *
 * This state handles the transmission of a Modbus request to the slave device.
 */
extern const fsm_state_t modbus_client_state_sending_request;

/**
 * @brief Modbus Master FSM state: Waiting for Response.
 *
 * This state represents the period where the Master is waiting for a response
 * from the slave device after sending a request.
 */
extern const fsm_state_t modbus_client_state_waiting_response;

/**
 * @brief Modbus Master FSM state: Processing Response.
 *
 * This state handles the processing of the received response from the slave device,
 * including parsing and validating the data.
 */
extern const fsm_state_t modbus_client_state_processing_response;

/**
 * @brief Modbus Master FSM state: Error.
 *
 * This state represents an error condition in the Modbus FSM. The FSM can recover
 * and transition back to the idle state when a new event occurs.
 */
extern const fsm_state_t modbus_client_state_error;

/* -------------------------------------------------------------------------- */
/*                           FSM State Enumerations                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Enumeration of states in the Modbus Master FSM.
 *
 * These states represent the Master's internal steps in handling a request.
 */
typedef enum {
    MODBUS_CLIENT_STATE_IDLE = 0,            /**< The Master is idle, ready to send a request */
    MODBUS_CLIENT_STATE_SENDING_REQUEST,     /**< Sending a request to the slave */
    MODBUS_CLIENT_STATE_WAITING_RESPONSE,    /**< Waiting for a response from the slave */
    MODBUS_CLIENT_STATE_PROCESSING_RESPONSE, /**< Processing the received response */
    MODBUS_CLIENT_STATE_ERROR                /**< Error state */
} modbus_client_state_t;

/**
 * @brief Enumeration of events that can trigger transitions in the Modbus Master FSM.
 *
 * These events represent actions or conditions that affect the Master's state transitions.
 */
typedef enum {
    MODBUS_CLIENT_EVENT_SEND_REQUEST = 1,     /**< Request to send a Modbus request */
    MODBUS_CLIENT_EVENT_TX_COMPLETE,          /**< Transmission of request completed */
    MODBUS_CLIENT_EVENT_RX_BYTE_RECEIVED,     /**< A byte of the response has been received */
    MODBUS_CLIENT_EVENT_RESPONSE_COMPLETE,    /**< The complete response has been received */
    MODBUS_CLIENT_EVENT_TIMEOUT,              /**< Timeout occurred while waiting for a response */
    MODBUS_CLIENT_EVENT_ERROR_DETECTED,       /**< An error was detected (CRC, Modbus exception, or transport error) */
    MODBUS_CLIENT_EVENT_RESTART_FROM_ERROR    /**< Restart FSM after an error */
} modbus_client_event_t;

/* -------------------------------------------------------------------------- */
/*                          API of the Modbus Master                          */
/* -------------------------------------------------------------------------- */

/**
 * @brief Creates and initializes the Modbus Master (client) context.
 *
 * This function sets up the FSM with the initial state, initializes buffers, and prepares
 * the Master to send Modbus requests and handle responses.
 *
 * @param[in,out] modbus          Pointer to the Modbus context. The context must be allocated by the user,
 *                                 and at least the `transport` and `device_info` fields must be set before calling.
 * @param[in]     platform_conf    Pointer to the transport configuration (`modbus_transport_t`).
 * @param[in]     baudrate         Pointer to the Modbus baud rate.
 * @param[in,out] client           Pointer to the client state storage supplied by the caller. Must remain valid
 *                                 for the lifetime of the Modbus context.
 * 
 * @return modbus_error_t `MODBUS_ERROR_NONE` on success, or an error code if initialization fails.
 *
 * @retval MODBUS_ERROR_NONE           Master initialized successfully.
 * @retval MODBUS_ERROR_INVALID_ARGUMENT Invalid arguments provided.
 * @retval MODBUS_ERROR_MEMORY         Memory allocation failed (if applicable).
 * @retval Others                       Various error codes as defined in `modbus_error_t`.
 *
 * @warning
 * - Ensure that `modbus`, `platform_conf`, and `baudrate` are valid pointers.
 * - The transport layer functions (`write`, `read`, `get_reference_msec`, `measure_time_msec`) must be properly implemented.
 * 
 * @example
 * ```c
 * modbus_context_t ctx;
 * modbus_client_data_t client;
 * modbus_transport_t transport = {
 *     .write = transport_write_function,        // User-defined transport write function
 *     .read = transport_read_function,          // User-defined transport read function
 *     .get_reference_msec = get_msec_function,  // User-defined function to get current time in ms
 *     .measure_time_msec = measure_time_function // User-defined function to measure elapsed time
 * };
 * 
 * uint16_t baud = 19200;
 * 
 * modbus_error_t error = modbus_client_create(&ctx, &transport, &baud, &client);
 * if (error != MODBUS_ERROR_NONE) {
 *     // Handle initialization error
 * }
 * ```
 */
modbus_error_t modbus_client_create(modbus_context_t *modbus, modbus_transport_t *platform_conf, uint16_t *baudrate, modbus_client_data_t *client);

/**
 * @brief Polls the Modbus Master state machine.
 *
 * If the Master uses an FSM, this function should be called periodically in the main loop
 * to process events, send pending requests, check for timeouts, and process responses.
 *
 * @param[in,out] ctx Pointer to the Modbus context.
 *
 * @note
 * - The polling function manages the FSM transitions based on incoming events and data.
 * - It is essential to regularly call this function to ensure timely processing of Modbus requests and responses.
 *
 * @example
 * ```c
 * while (1) {
 *     // Poll the Modbus Master FSM
 *     modbus_client_poll(&ctx);
 *     
 *     // Other application code
 * }
 * ```
 */
void modbus_client_poll(modbus_context_t *ctx);

/**
 * @brief Sends a request to read holding registers (Function Code 0x03).
 *
 * This function constructs a Modbus frame for reading holding registers and enqueues it for transmission.
 * If using an FSM, the request will be sent according to the FSM's state management.
 * The read data will be available once the response is received and processed, accessible via `modbus_client_get_read_data()`.
 *
 * @param[in,out] ctx           Pointer to the Modbus context.
 * @param[in]     slave_address Address of the slave device from which to read the registers.
 * @param[in]     start_address Starting address of the registers to read.
 * @param[in]     quantity      Number of registers to read.
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` if the request was accepted, or an error code otherwise.
 *
 * @retval MODBUS_ERROR_NONE          Request accepted successfully.
 * @retval MODBUS_ERROR_INVALID_ARGUMENT Invalid arguments provided.
 * @retval MODBUS_ERROR_NO_MEMORY     No space available to queue the request.
 * @retval Others                      Various error codes as defined in `modbus_error_t`.
 *
 * @warning
 * - Ensure that the `slave_address` is valid and corresponds to an existing slave device.
 * - The `quantity` should not exceed `MODBUS_MAX_READ_WRITE_SIZE`.
 * 
 * @example
 * ```c
 * modbus_error_t error = modbus_client_read_holding_registers(&ctx, 0x01, 0x0000, 10);
 * if (error != MODBUS_ERROR_NONE) {
 *     // Handle request error
 * }
 * ```
 */
modbus_error_t modbus_client_read_holding_registers(modbus_context_t *ctx, uint8_t slave_address, uint16_t start_address, uint16_t quantity);

/**
 * @brief Function called when a byte of the response is received.
 *
 * The user should call this function whenever a byte arrives from the UART/TCP.
 * It injects the `MODBUS_CLIENT_EVENT_RX_BYTE_RECEIVED` event into the FSM and stores
 * the received byte in the RX buffer.
 *
 * @param[in,out] fsm  Pointer to the FSM instance associated with the Modbus Master.
 * @param[in]     data Byte received from the transport layer.
 *
 * @warning
 * - Ensure that `fsm` is properly initialized and points to a valid FSM instance.
 * - This function is thread-safe and can be called from interrupt service routines (ISRs) if necessary.
 *
 * @example
 * ```c
 * // Assume `received_byte` is obtained from the transport layer
 * uint8_t received_byte = get_received_byte(); // User-defined function
 * modbus_client_receive_data_event(&ctx.client_data.fsm, received_byte);
 * ```
 */
void modbus_client_receive_data_event(fsm_t *fsm, uint8_t data);

/**
 * @brief Sets the response timeout for the Modbus Master.
 *
 * The Master will wait for a specified duration for a response from the Slave before considering it a timeout.
 *
 * @param[in,out] ctx       Pointer to the Modbus context.
 * @param[in]     timeout_ms Maximum time in milliseconds to wait for a response.
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` if configured successfully, or an error code otherwise.
 *
 * @retval MODBUS_ERROR_NONE          Timeout set successfully.
 * @retval MODBUS_ERROR_INVALID_ARGUMENT Invalid arguments provided.
 * @retval Others                      Various error codes as defined in `modbus_error_t`.
 *
 * @warning
 * - The `timeout_ms` should be sufficient to accommodate the expected response time based on the baud rate and data size.
 * 
 * @example
 * ```c
 * modbus_error_t error = modbus_client_set_timeout(&ctx, 500); // Set timeout to 500 ms
 * if (error != MODBUS_ERROR_NONE) {
 *     // Handle timeout configuration error
 * }
 * ```
 */
modbus_error_t modbus_client_set_timeout(modbus_context_t *ctx, uint16_t timeout_ms);

/**
 * @brief Retrieves the data read from the last Modbus read request.
 *
 * Depending on the implementation, the Master may store the read data internally.
 * This function allows the user to access the read data after receiving confirmation
 * that the response has been processed.
 *
 * @param[in,out] ctx       Pointer to the Modbus context.
 * @param[out]    buffer    Buffer where the read register values will be stored.
 * @param[in]     max_regs  Maximum number of registers to read into the buffer.
 *
 * @return uint16_t Number of registers read and copied into the buffer.
 *
 * @retval >0 Number of registers read successfully.
 * @retval 0  No data available or an error occurred.
 *
 * @warning
 * - Ensure that `buffer` has enough space to hold `max_regs` elements.
 * - This function should be called after confirming that the response has been received and processed.
 * 
 * @example
 * ```c
 * int16_t data_buffer[10];
 * uint16_t regs_read = modbus_client_get_read_data(&ctx, data_buffer, 10);
 * if (regs_read > 0) {
 *     // Process the read data
 * }
 * ```
 */
uint16_t modbus_client_get_read_data(modbus_context_t *ctx, int16_t *buffer, uint16_t max_regs);

#ifdef __cplusplus
}
#endif

#endif /* MODBUS_MASTER_H */

/** @} */
