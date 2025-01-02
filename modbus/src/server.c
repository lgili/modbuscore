/**
 * @file modbus_server.c
 * @brief Modbus Server (Slave) state machine and logic (Final Version).
 *
 * This file implements the logic of a Modbus server (slave) using a Finite State Machine (FSM),
 * integrating with the core protocol (`modbus_core`) and utility functions.
 *
 * **Functionalities:**
 * - Receives bytes and assembles RTU frames (FSM controls states).
 * - Parses requests (e.g., function 0x03 to read holding registers, 0x06 to write a single register, 0x2B for device information).
 * - Calls read/write callbacks for variables registered via `modbus_set_holding_register()`.
 * - Constructs and sends responses or exceptions (if not a broadcast).
 * - Does not use dynamic memory allocation.
 *
 * **Requirements:**
 * - The `modbus_context_t` must contain a field `modbus_server_data_t server_data;` to store server data.
 * - The user must:
 *   - Call `modbus_server_create()`.
 *   - Register registers using `modbus_set_holding_register()` / `modbus_set_array_holding_register()`.
 *   - Call `modbus_server_poll()` periodically.
 *   - When a byte arrives, call `modbus_server_receive_data_from_uart_event()`.
 *
 * @author
 * Luiz Carlos Gili
 * 
 * @date
 * 2024-12-20
 */

#include <modbus/transport.h>
#include <modbus/server.h>
#include <modbus/core.h>
#include <modbus/utils.h>
#include <modbus/fsm.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                             Global Variables                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Global server data structure.
 *
 * This global instance holds all server-specific data, including FSM state, context, device information,
 * current message processing state, and holding registers.
 */
modbus_server_data_t g_server;

/**
 * @brief Counter for build errors.
 *
 * This static variable counts the number of consecutive build errors to prevent infinite error loops.
 */
static int build_error_count = 0;

/**
 * @brief Flag indicating the need to update the baud rate.
 *
 * When set to `true`, the server will update the baud rate in the next idle action.
 */
static bool need_update_baudrate = false;

/* -------------------------------------------------------------------------- */
/*                             Internal Prototypes                             */
/* -------------------------------------------------------------------------- */

/* Internal function prototypes for message handling and FSM actions */
static void reset_message(modbus_server_data_t *server);
static modbus_error_t parse_request(modbus_server_data_t *server);
static void handle_function(modbus_server_data_t *server);

/* Specific parsing functions for different Modbus function codes */
static modbus_error_t parse_read_holding_registers(modbus_server_data_t *server, const uint8_t *buf, uint16_t *idx, uint16_t size);
static modbus_error_t parse_write_single_register(modbus_server_data_t *server, const uint8_t *buf, uint16_t *idx, uint16_t size);
static modbus_error_t parse_device_info_request(modbus_server_data_t *server, const uint8_t *buf, uint16_t *idx, uint16_t size);

/* FSM Action functions */
static void action_idle(fsm_t *fsm);
static void action_start_receiving(fsm_t *fsm);
static void action_parse_address(fsm_t *fsm);
static void action_parse_function(fsm_t *fsm);
static void action_process_frame(fsm_t *fsm);
static void action_validate_frame(fsm_t *fsm);
static void action_build_response(fsm_t *fsm);
static void action_put_data_on_buffer(fsm_t *fsm);
static void action_calculate_crc_response(fsm_t *fsm);
static void action_send_response(fsm_t *fsm);
static void action_handle_error(fsm_t *fsm);
static void action_handle_wrong_baudrate(fsm_t *fsm);

/* FSM Guard functions */
static bool guard_receive_finished(fsm_t *fsm);
static bool guard_send_finished(fsm_t *fsm);

/* Register access functions */
static int find_register(modbus_server_data_t *server, uint16_t address);

/* -------------------------------------------------------------------------- */
/*                             FSM State Definitions                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Modbus FSM state: Idle.
 *
 * This state represents the idle condition of the Modbus FSM, where it is waiting for an event to occur.
 * In this state, the FSM will transition to the `MODBUS_SERVER_STATE_RECEIVING` state when a byte is received.
 */
static const fsm_transition_t state_idle_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_RX_BYTE_RECEIVED, modbus_server_state_receiving, action_start_receiving, NULL)
};

const fsm_state_t modbus_server_state_idle = FSM_STATE("IDLE", MODBUS_SERVER_STATE_IDLE, state_idle_transitions, action_idle);

/**
 * @brief Modbus FSM state: Receiving.
 *
 * This state represents the process of receiving Modbus data. The FSM remains in this state
 * while bytes are being received from the transport layer.
 */
static const fsm_transition_t state_receiving_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_RX_BYTE_RECEIVED, modbus_server_state_receiving, action_start_receiving, NULL),
    FSM_TRANSITION(MODBUS_EVENT_PARSE_ADDRESS, modbus_server_state_parsing_address, action_parse_address, guard_receive_finished),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_WRONG_BAUDRATE, modbus_server_state_error, action_handle_wrong_baudrate, NULL)
};
const fsm_state_t modbus_server_state_receiving = FSM_STATE("RECEIVING", MODBUS_SERVER_STATE_RECEIVING, state_receiving_transitions, action_start_receiving);

/**
 * @brief Modbus FSM state: Parsing Address.
 *
 * This state parses the incoming data to extract the slave address from the received frame.
 */
static const fsm_transition_t state_parsing_address_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_PARSE_FUNCTION, modbus_server_state_parsing_function, action_parse_function, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_parsing_address = FSM_STATE("PARSING_ADDRESS", MODBUS_SERVER_STATE_PARSING_ADDRESS, state_parsing_address_transitions, NULL);

/**
 * @brief Modbus FSM state: Parsing Function.
 *
 * This state parses the incoming data to extract the function code from the received frame.
 */
static const fsm_transition_t state_parsing_function_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_PROCESS_FRAME, modbus_server_state_processing, action_process_frame, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_parsing_function = FSM_STATE("PARSING_FUNCTION", MODBUS_SERVER_STATE_PARSING_FUNCTION, state_parsing_function_transitions, NULL);

/**
 * @brief Modbus FSM state: Processing.
 *
 * This state handles the processing of the received Modbus frame, executing the requested function
 * (e.g., reading or writing registers).
 */
static const fsm_transition_t state_processing_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_VALIDATE_FRAME, modbus_server_state_validating_frame, action_validate_frame, NULL),
    FSM_TRANSITION(MODBUS_EVENT_BOOTLOADER, modbus_server_state_sending, action_send_response, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_processing = FSM_STATE("PROCESSING", MODBUS_SERVER_STATE_PROCESSING, state_processing_transitions, NULL);

/**
 * @brief Modbus FSM state: Validating Frame.
 *
 * This state runs the CRC check on the received frame to validate its integrity.
 */
static const fsm_transition_t state_validating_frame_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_BUILD_RESPONSE, modbus_server_state_building_response, action_build_response, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_validating_frame = FSM_STATE("VALIDATING_FRAME", MODBUS_SERVER_STATE_VALIDATING_FRAME, state_validating_frame_transitions, NULL);

/**
 * @brief Modbus FSM state: Building Response.
 *
 * This state builds a response frame based on the processed request data.
 */
static const fsm_transition_t state_building_response_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_PUT_DATA_ON_BUFFER, modbus_server_state_putting_data_on_buffer, action_put_data_on_buffer, NULL),
    FSM_TRANSITION(MODBUS_EVENT_BROADCAST_DONT_ANSWER, modbus_server_state_idle, NULL, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_building_response = FSM_STATE("BUILDING_RESPONSE", MODBUS_SERVER_STATE_BUILDING_RESPONSE, state_building_response_transitions, NULL);

/**
 * @brief Modbus FSM state: Putting Data on Buffer.
 *
 * This state prepares the response data to be sent by placing it into the transmit buffer.
 */
static const fsm_transition_t state_putting_data_on_buffer_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_CALCULATE_CRC, modbus_server_state_calculating_crc, action_calculate_crc_response, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_putting_data_on_buffer = FSM_STATE("PUTTING_DATA_ON_BUF", MODBUS_SERVER_STATE_PUTTING_DATA_ON_BUF, state_putting_data_on_buffer_transitions, NULL);

/**
 * @brief Modbus FSM state: Calculating CRC.
 *
 * This state calculates the CRC for the response data to ensure data integrity during transmission.
 */
static const fsm_transition_t state_calculating_crc_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_SEND_RESPONSE, modbus_server_state_sending, action_send_response, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_calculating_crc = FSM_STATE("CALCULATING_CRC", MODBUS_SERVER_STATE_CALCULATING_CRC, state_calculating_crc_transitions, NULL);

/**
 * @brief Modbus FSM state: Sending.
 *
 * This state handles the sending of the response frame after it has been built and CRC-checked.
 */
static const fsm_transition_t state_sending_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_TX_COMPLETE, modbus_server_state_idle, NULL, guard_send_finished),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_sending = FSM_STATE("SENDING", MODBUS_SERVER_STATE_SENDING, state_sending_transitions, action_send_response);

/**
 * @brief Modbus FSM state: Error.
 *
 * This state represents an error condition in the Modbus FSM. The FSM can recover and transition
 * back to the idle state when a new byte is received.
 */
static const fsm_transition_t state_error_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_RX_BYTE_RECEIVED, modbus_server_state_idle, NULL, NULL),
    FSM_TRANSITION(MODBUS_EVENT_RESTART_FROM_ERROR, modbus_server_state_idle, NULL, NULL)
};
const fsm_state_t modbus_server_state_error = FSM_STATE("ERROR", MODBUS_SERVER_STATE_ERROR, state_error_transitions, NULL);


/* -------------------------------------------------------------------------- */
/*                           Public API Functions                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Creates and initializes the Modbus server (slave) context.
 *
 * This function sets up the FSM with the initial state, initializes buffers, and prepares
 * the server to receive and respond to Modbus requests.
 *
 * @param[in,out] modbus          Pointer to the Modbus context. The context must be allocated by the user,
 *                                 and at least the `transport` and `device_info` fields must be set before calling.
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
 * - Ensure that `modbus`, `platform_conf`, `device_address`, and `baudrate` are valid pointers.
 * - The transport layer functions (`write`, `read`, `get_reference_msec`, `measure_time_msec`) must be properly implemented.
 * 
 * @example
 * ```c
 * modbus_context_t ctx;
 * modbus_transport_t transport = {
 *     .write = transport_write_function,       // User-defined transport write function
 *     .read = transport_read_function,         // User-defined transport read function
 *     .get_reference_msec = get_msec_function, // User-defined function to get current time in ms
 *     .measure_time_msec = measure_time_function // User-defined function to measure elapsed time
 * };
 * 
 * uint16_t device_addr = 10;
 * uint16_t baud = 19200;
 * 
 * modbus_error_t error = modbus_server_create(&ctx, &transport, &device_addr, &baud);
 * if (error != MODBUS_ERROR_NONE) {
 *     // Handle initialization error
 * }
 * ```
 */
modbus_error_t modbus_server_create(modbus_context_t *modbus, modbus_transport_t *platform_conf, uint16_t *device_address, uint16_t *baudrate)
{
    if (modbus == NULL || device_address == NULL || baudrate == NULL || platform_conf == NULL)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (platform_conf->transport == MODBUS_TRANSPORT_RTU && *device_address == 0)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!platform_conf->read || !platform_conf->write || !platform_conf->get_reference_msec || !platform_conf->measure_time_msec)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    /* Assign global server data to the context */
    modbus->user_data = &g_server;
    g_server.ctx = modbus;
    modbus_reset_buffers(modbus);

    /* Initialize device information */
    g_server.device_info.address = device_address;
    g_server.device_info.baudrate = baudrate;
    modbus->transport = *platform_conf;

    /* Initialize reference times */
    modbus->rx_reference_time = modbus->transport.get_reference_msec();
    modbus->tx_reference_time = modbus->transport.get_reference_msec();
    modbus->error_timer = modbus->transport.get_reference_msec();

    /* Initialize FSM */
    fsm_init(&g_server.fsm, &modbus_server_state_idle, &g_server);
    modbus->role = MODBUS_ROLE_SERVER;

    /* Set conformity level (fixed value for now) */
    g_server.device_info.conformity_level = 0x81;

    return MODBUS_ERROR_NONE;
}

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
void modbus_server_poll(modbus_context_t *ctx) {
    if (!ctx) return;
    modbus_server_data_t *server = (modbus_server_data_t *)ctx->user_data;
    fsm_run(&server->fsm);
}

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
void modbus_server_receive_data_from_uart_event(fsm_t *fsm, uint8_t data) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    /* Update reference time for RX */
    ctx->rx_reference_time = ctx->transport.get_reference_msec();

    /* Store received byte in RX buffer */
    if (ctx->rx_count < sizeof(ctx->rx_buffer)) {
        ctx->rx_buffer[ctx->rx_count++] = data;
    } else {
        /* Buffer overflow detected */
        server->msg.error = MODBUS_ERROR_INVALID_REQUEST;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        return;
    }

    /* Trigger receiving state if not already in receiving */
    if (fsm->current_state->id != MODBUS_SERVER_STATE_RECEIVING) {
        fsm_handle_event(fsm, MODBUS_EVENT_RX_BYTE_RECEIVED);
    }
}

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
                                           modbus_read_callback_t read_cb, modbus_write_callback_t write_cb) {

    modbus_server_data_t *server = (modbus_server_data_t *)ctx->user_data;

    if (server->holding_register_count >= MAX_SIZE_HOLDING_REGISTERS || !variable) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    server->holding_registers[server->holding_register_count].address = address;
    server->holding_registers[server->holding_register_count].variable_ptr = variable;
    server->holding_registers[server->holding_register_count].read_only = read_only;
    server->holding_registers[server->holding_register_count].read_callback = read_cb;
    server->holding_registers[server->holding_register_count].write_callback = write_cb;
    server->holding_register_count++;

    /* Sort holding registers by address for efficient searching */
    modbus_selection_sort(server->holding_registers, server->holding_register_count);

    return MODBUS_ERROR_NONE;
}

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
                                                 modbus_read_callback_t read_cb, modbus_write_callback_t write_cb) {
    modbus_server_data_t *server = (modbus_server_data_t *)ctx->user_data;

    if (!variable || length == 0 || (server->holding_register_count + length) > MAX_SIZE_HOLDING_REGISTERS) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    for (uint16_t i = 0; i < length; i++) {
        server->holding_registers[server->holding_register_count].address = start_address + i;
        server->holding_registers[server->holding_register_count].variable_ptr = &variable[i];
        server->holding_registers[server->holding_register_count].read_only = read_only;
        server->holding_registers[server->holding_register_count].read_callback = read_cb;
        server->holding_registers[server->holding_register_count].write_callback = write_cb;
        server->holding_register_count++;
    }

    /* Sort holding registers by address for efficient searching */
    modbus_selection_sort(server->holding_registers, server->holding_register_count);

    return MODBUS_ERROR_NONE;
}

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
modbus_error_t modbus_server_add_device_info(modbus_context_t *ctx, const char *value, uint8_t length) {

    if (!ctx || !value || length > MAX_DEVICE_PACKAGE_VALUES) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    modbus_server_data_t *server = (modbus_server_data_t *)ctx->user_data;
    if (server->device_info.info_saved >= MAX_DEVICE_PACKAGES) {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    uint8_t id = server->device_info.info_saved;
    server->device_info.data[id].id = id;
    server->device_info.data[id].length = length;
    memcpy(server->device_info.data[id].value_in_ascii, value, length);
    server->device_info.info_saved++;
    return MODBUS_ERROR_NONE;
}

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
int16_t update_baudrate(uint16_t baud) {
    need_update_baudrate = true;
    return baud;
}

/* -------------------------------------------------------------------------- */
/*                           FSM Action Implementations                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Idle action for the FSM.
 *
 * This action is performed when the FSM enters the idle state. It checks if a baud rate update is needed
 * and performs the update if necessary.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_idle(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    if (need_update_baudrate == true)
    {
        need_update_baudrate = false;
        *server->device_info.baudrate = ctx->transport.change_baudrate(*server->device_info.baudrate);
        if (ctx->transport.restart_uart) {
            ctx->transport.restart_uart();
        }
    }
    /* Additional idle actions can be added here */
}

/**
 * @brief Start receiving action for the FSM.
 *
 * This action transitions the FSM to the parsing address state after receiving data.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_start_receiving(fsm_t *fsm) {
    fsm_handle_event(fsm, MODBUS_EVENT_PARSE_ADDRESS);
}

/**
 * @brief Parse address action for the FSM.
 *
 * This action parses the slave address from the received frame and determines if the message is intended
 * for this server or is a broadcast.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_parse_address(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;
    reset_message(server);

    uint8_t slave_address;
    uint16_t idx = ctx->rx_index;
    if (!modbus_read_uint8(ctx->rx_buffer, &idx, ctx->rx_count, &slave_address)) {
        server->msg.error = MODBUS_ERROR_INVALID_ARGUMENT;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        return;
    }
    server->msg.slave_address = slave_address;
    ctx->rx_index = idx;

    /* Check if the message is for this server or is a broadcast */
    if ((server->msg.slave_address == *server->device_info.address) ||
        (server->msg.slave_address == MODBUS_BROADCAST_ADDRESS) ||
        (server->msg.slave_address == MODBUS_BOOTLOADER_ADDRESS)) {
        if (server->msg.slave_address == MODBUS_BROADCAST_ADDRESS) {
            server->msg.broadcast = true;
        }
        fsm_handle_event(fsm, MODBUS_EVENT_PARSE_FUNCTION);
    } else {
        server->msg.error = MODBUS_ERROR_INVALID_ARGUMENT;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
    }
}

/**
 * @brief Parse function action for the FSM.
 *
 * This action parses the function code from the received frame.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_parse_function(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    uint8_t function_code;
    if (!modbus_read_uint8(ctx->rx_buffer, &ctx->rx_index, ctx->rx_count, &function_code)) {
        server->msg.error = MODBUS_ERROR_INVALID_ARGUMENT;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        return;
    }
    server->msg.function_code = function_code;
    fsm_handle_event(fsm, MODBUS_EVENT_PROCESS_FRAME);
}

/**
 * @brief Process frame action for the FSM.
 *
 * This action parses the request and handles the function based on the function code.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_process_frame(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_error_t err = parse_request(server);
    if (err != MODBUS_ERROR_NONE) {
        server->msg.error = err;
        if (err == MODBUS_ERROR_OTHER) {
            fsm_handle_event(fsm, MODBUS_EVENT_BOOTLOADER);
        } else {
            fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        }
        return;
    }
    fsm_handle_event(fsm, MODBUS_EVENT_VALIDATE_FRAME);
}

/**
 * @brief Validate frame action for the FSM.
 *
 * This action validates the CRC of the received frame to ensure data integrity.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_validate_frame(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    /* Verify CRC */
    if ((ctx->rx_index + 2U) > ctx->rx_count) {
        server->msg.error = MODBUS_ERROR_CRC;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        return;
    }

    uint16_t calc_crc = modbus_crc_with_table(ctx->rx_buffer, ctx->rx_index);
    uint16_t recv_crc = (uint16_t)((ctx->rx_buffer[ctx->rx_index + 1U] << 8U) | ctx->rx_buffer[ctx->rx_index]);
    if (calc_crc != recv_crc) {
        server->msg.error = MODBUS_ERROR_CRC;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        return;
    }

    fsm_handle_event(fsm, MODBUS_EVENT_BUILD_RESPONSE);
}

/**
 * @brief Build response action for the FSM.
 *
 * This action builds the response based on the processed request and determines the next steps
 * in the FSM based on the request type and broadcast status.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_build_response(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;
    handle_function(server);

    if(server->msg.error != MODBUS_ERROR_NONE) {
        return;
    }

    /* Determine if the response should be sent or not based on broadcast */
    if((server->msg.current_read_index >=  server->msg.read_quantity)
       || (server->msg.write_quantity >= 1)
       || (server->msg.mei_type != 0))
    {
        if(server->msg.broadcast == true) {
            fsm_handle_event(fsm, MODBUS_EVENT_BROADCAST_DONT_ANSWER);
            ctx->tx_raw_index = 0;
            ctx->rx_count = 0;
        } else {
            fsm_handle_event(fsm, MODBUS_EVENT_PUT_DATA_ON_BUFFER);
        }
        build_error_count = 0;
    } else {        
        if(server->msg.current_read_index == 0 && server->msg.write_quantity == 0){
            server->msg.error = MODBUS_ERROR_TRANSPORT;
            fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        } else {
            build_error_count++;
            if(build_error_count >=  128) {
                server->msg.error = MODBUS_ERROR_TRANSPORT;
                fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
                build_error_count = 0;
            }
        }
    }
}

/**
 * @brief Put data on buffer action for the FSM.
 *
 * This action copies the response data into the transmission buffer and prepares it for CRC calculation.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_put_data_on_buffer(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    uint16_t quantity_to_send = 0;
    if(server->msg.function_code == MODBUS_FUNC_READ_COILS) {
        quantity_to_send = server->msg.read_quantity + 1;
    }
    else if(server->msg.function_code <= MODBUS_FUNC_READ_INPUT_REGISTERS) {
        quantity_to_send = server->msg.read_quantity * 2 + 1;
    }
    else { 
        quantity_to_send = ctx->tx_raw_index;
    }
    ctx->tx_raw_index = 0;
    ctx->tx_index = 0;

    /* Create Modbus frame (RTU) */
    ctx->tx_buffer[ctx->tx_index++] = server->msg.slave_address;
    ctx->tx_buffer[ctx->tx_index++] = server->msg.function_code;

    /* Copy data to the Modbus transmission buffer */
    memcpy(&ctx->tx_buffer[ctx->tx_index], ctx->tx_raw_buffer, quantity_to_send);
    ctx->tx_index += quantity_to_send;

    fsm_handle_event(fsm, MODBUS_EVENT_CALCULATE_CRC);
}

/**
 * @brief Calculate CRC for response action for the FSM.
 *
 * This action calculates the CRC for the response data to ensure data integrity before transmission.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_calculate_crc_response(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    uint16_t crc = modbus_crc_with_table(ctx->tx_buffer, ctx->tx_index);
    ctx->tx_buffer[ctx->tx_index++] = GET_LOW_BYTE(crc);
    ctx->tx_buffer[ctx->tx_index++] = GET_HIGH_BYTE(crc);

    fsm_handle_event(fsm, MODBUS_EVENT_SEND_RESPONSE);
}

/**
 * @brief Send response action for the FSM.
 *
 * This action sends the constructed response frame through the transport layer.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_send_response(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    modbus_error_t err = modbus_send_frame(ctx, ctx->tx_buffer, ctx->tx_index);
    if (err != MODBUS_ERROR_NONE) {
        server->msg.error = err;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        return;
    }
    fsm_handle_event(fsm, MODBUS_EVENT_TX_COMPLETE);
}

/**
 * @brief Handle error action for the FSM.
 *
 * This action handles errors by sending exception responses if applicable and performing recovery actions.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_handle_error(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    if (modbus_error_is_exception(server->msg.error) && !server->msg.broadcast) {
        /* Send exception response */
        uint8_t exception_function = server->msg.function_code | 0x80;
        uint8_t exception_code = server->msg.error;
        uint8_t frame[5];
        uint16_t len = modbus_build_rtu_frame(server->msg.slave_address, exception_function,
                                              &exception_code, 1, frame, sizeof(frame));
        if (len > 0) {
            modbus_send_frame(ctx, frame, len);
        }
    } else {
        /* Internal error handling, restart UART if available */
        if (ctx->transport.restart_uart) {
            ctx->transport.restart_uart();
        }
    }
    fsm_handle_event(fsm, MODBUS_EVENT_RESTART_FROM_ERROR);
}

/**
 * @brief Handle wrong baud rate action for the FSM.
 *
 * This action handles scenarios where a wrong baud rate is detected by attempting to update and restart UART.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_handle_wrong_baudrate(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    if (ctx->transport.change_baudrate && ctx->transport.restart_uart) {
        *server->device_info.baudrate = ctx->transport.change_baudrate(19200);
        ctx->transport.restart_uart();
    }
    fsm_handle_event(fsm, MODBUS_EVENT_RESTART_FROM_ERROR);
}

/* -------------------------------------------------------------------------- */
/*                           FSM Guard Implementations                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief Guard function to determine if receiving has finished.
 *
 * This guard checks if the inter-character timeout has been exceeded, indicating that the frame reception is complete.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 *
 * @return true if the receiving is finished, false otherwise.
 */
static bool guard_receive_finished(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    uint16_t rx_position_length_time;
    rx_position_length_time = ctx->transport.measure_time_msec(ctx->rx_reference_time);
    if (rx_position_length_time >= MODBUS_CONVERT_CHAR_INTERVAL_TO_MS(3.5, *server->device_info.baudrate)) {
        if (ctx->rx_count >= 1U && ctx->rx_count <= 3U) {
            fsm_handle_event(fsm, MODBUS_EVENT_ERROR_WRONG_BAUDRATE);
        }
        else {
            return true;
        }
    }
    return false;
}

/**
 * @brief Guard function to determine if sending has finished.
 *
 * This guard checks if the inter-character timeout has been exceeded, indicating that the frame sending is complete.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 *
 * @return true if the sending is finished, false otherwise.
 */
static bool guard_send_finished(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    uint16_t tx_position_length_time;
    tx_position_length_time = ctx->transport.measure_time_msec(ctx->tx_reference_time);
    if (tx_position_length_time >= MODBUS_CONVERT_CHAR_INTERVAL_TO_MS(3.5, *server->device_info.baudrate)) {
        return true;
    }
    return false;
}

/* -------------------------------------------------------------------------- */
/*                           Auxiliary Functions                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Resets the current message state.
 *
 * This function clears the current message data, resets the receive index, and prepares for a new message.
 *
 * @param[in,out] server Pointer to the server data structure.
 */
static void reset_message(modbus_server_data_t *server) {
    memset(&server->msg, 0, sizeof(server->msg));
    server->ctx->rx_index = 0;
}

/**
 * @brief Parses the received Modbus request.
 *
 * This function determines which Modbus function code is being requested and calls the appropriate parsing function.
 *
 * @param[in,out] server Pointer to the server data structure.
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` if parsing is successful, or an error code otherwise.
 */
static modbus_error_t parse_request(modbus_server_data_t *server) {
    modbus_context_t *ctx = server->ctx;
    uint16_t *idx = &ctx->rx_index;
    const uint8_t *buffer = ctx->rx_buffer;
    uint16_t size = ctx->rx_count;

    switch (server->msg.function_code) {
        case MODBUS_FUNC_READ_HOLDING_REGISTERS:
            return parse_read_holding_registers(server, buffer, idx, size);
        case MODBUS_FUNC_WRITE_SINGLE_REGISTER:
            return parse_write_single_register(server, buffer, idx, size);
        case MODBUS_FUNC_READ_DEVICE_INFORMATION:
            return parse_device_info_request(server, buffer, idx, size);
        default:
            /* Unsupported function */
            server->msg.error = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
            return MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
    }
}

/**
 * @brief Parses the Read Holding Registers request.
 *
 * This function extracts the starting address and quantity of registers to read from the received buffer.
 *
 * @param[in,out] server Pointer to the server data structure.
 * @param[in]     buf    Pointer to the received buffer.
 * @param[in,out] idx    Pointer to the current index in the buffer.
 * @param[in]     size   Size of the received buffer.
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` if parsing is successful, or an error code otherwise.
 */
static modbus_error_t parse_read_holding_registers(modbus_server_data_t *server, const uint8_t *buf, uint16_t *idx, uint16_t size) {
    uint16_t start_addr, quantity;
    if (!modbus_read_uint16(buf, idx, size, &start_addr)) return MODBUS_ERROR_INVALID_ARGUMENT;
    if (!modbus_read_uint16(buf, idx, size, &quantity)) return MODBUS_ERROR_INVALID_ARGUMENT;

    if (quantity < 1 || quantity > MODBUS_MAX_READ_WRITE_SIZE) return MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    if ((start_addr + quantity) > MAX_ADDRESS_HOLDING_REGISTERS) return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;

    server->msg.read_address = start_addr;
    server->msg.read_quantity = quantity;
    return MODBUS_ERROR_NONE;
}

/**
 * @brief Parses the Write Single Register request.
 *
 * This function extracts the register address and value to write from the received buffer.
 *
 * @param[in,out] server Pointer to the server data structure.
 * @param[in]     buf    Pointer to the received buffer.
 * @param[in,out] idx    Pointer to the current index in the buffer.
 * @param[in]     size   Size of the received buffer.
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` if parsing is successful, or an error code otherwise.
 */
static modbus_error_t parse_write_single_register(modbus_server_data_t *server, const uint8_t *buf, uint16_t *idx, uint16_t size) {
    uint16_t addr, value;
    if (!modbus_read_uint16(buf, idx, size, &addr)) return MODBUS_ERROR_INVALID_ARGUMENT;
    if (!modbus_read_uint16(buf, idx, size, &value)) return MODBUS_ERROR_INVALID_ARGUMENT;

    if (addr > MAX_ADDRESS_HOLDING_REGISTERS) return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    server->msg.write_address = addr;
    server->msg.write_value = (int16_t)value;

    /* Writing is handled in handle_function() */
    return MODBUS_ERROR_NONE;
}

/**
 * @brief Parses the Read Device Information request.
 *
 * This function extracts the MEI type, device ID code, and object ID from the received buffer.
 *
 * @param[in,out] server Pointer to the server data structure.
 * @param[in]     buf    Pointer to the received buffer.
 * @param[in,out] idx    Pointer to the current index in the buffer.
 * @param[in]     size   Size of the received buffer.
 *
 * @return modbus_error_t `MODBUS_ERROR_NONE` if parsing is successful, or an error code otherwise.
 */
static modbus_error_t parse_device_info_request(modbus_server_data_t *server, const uint8_t *buf, uint16_t *idx, uint16_t size) {
    uint8_t mei_type, dev_id_code, obj_id;
    if (!modbus_read_uint8(buf, idx, size, &mei_type)) return MODBUS_ERROR_INVALID_ARGUMENT;
    if (!modbus_read_uint8(buf, idx, size, &dev_id_code)) return MODBUS_ERROR_INVALID_ARGUMENT;
    if (!modbus_read_uint8(buf, idx, size, &obj_id)) return MODBUS_ERROR_INVALID_ARGUMENT;

    server->msg.mei_type = mei_type;
    server->msg.device_id_code = dev_id_code;
    server->msg.device_obj_id = obj_id;
    return MODBUS_ERROR_NONE;
}

/* -------------------------------------------------------------------------- */
/*                           Register Access Implementations                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Finds the index of a holding register by its address using binary search.
 *
 * This function assumes that the holding registers are sorted by address.
 *
 * @param[in]  server  Pointer to the server data structure.
 * @param[in]  address The address of the holding register to find.
 *
 * @return int The index of the holding register if found, or -1 if not found.
 */
static int find_register(modbus_server_data_t *server, uint16_t address) {
    return modbus_binary_search(server->holding_registers, 0, (uint16_t)(server->holding_register_count - 1), address);
}

/**
 * @brief Reads multiple registers and stores the data in the transmission buffer.
 *
 * This function reads the specified number of holding registers starting from the given address and
 * stores their values in the transmission buffer.
 *
 * @param[in,out] server        Pointer to the server data structure.
 * @param[in]     start_address Starting address to read from.
 * @param[in]     quantity      Number of registers to read.
 * @param[out]    tx_buffer     Transmission buffer to store the read data.
 * @param[in,out] tx_index      Pointer to the current index in the transmission buffer.
 *
 * @return bool `true` if successful, `false` if an error occurred.
 *
 * @retval true  Registers read successfully.
 * @retval false An error occurred during reading.
 *
 * @warning
 * - Ensure that `tx_buffer` has enough space to store the read data.
 * 
 * @example
 * ```c
 * uint8_t tx_buffer[256];
 * uint16_t tx_index = 0;
 * bool success = read_registers(server, 0x0000, 10, tx_buffer, &tx_index);
 * if (success) {
 *     // Data is ready in tx_buffer
 * }
 * ```
 */
static inline bool read_registers(modbus_server_data_t *server, uint16_t start_address, uint16_t quantity, uint8_t *tx_buffer, uint16_t *tx_index)
{
    if ((start_address + quantity) > MAX_ADDRESS_HOLDING_REGISTERS)
    {
        server->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        return false;
    }

    if(server->msg.current_read_index == 0) {
        *tx_index = 0;
        tx_buffer[(*tx_index)++] = (uint8_t)(quantity * 2); // Byte count
    }

    uint16_t value;
    uint16_t register_address = start_address + server->msg.current_read_index;
    /* Find the register */
    int idx = find_register(server, register_address); 
    if(idx < 0) {
        server->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        return false;
    }

    variable_modbus_t *var = &server->holding_registers[idx];
    if (var->read_callback) {
        value = var->read_callback();
    } else {
        value = *var->variable_ptr;
    }   

    tx_buffer[(*tx_index)++] = GET_HIGH_BYTE(value);
    tx_buffer[(*tx_index)++] = GET_LOW_BYTE(value);
    server->msg.current_read_index++;

    return true;
}

/**
 * @brief Writes a single holding register.
 *
 * This function writes a value to a single holding register at the specified address.
 *
 * @param[in,out] server Pointer to the server data structure.
 * @param[in]     address The address of the holding register to write to.
 * @param[in]     value   The value to write to the holding register.
 *
 * @return bool `true` if the write was successful, `false` otherwise.
 *
 * @retval true  Register written successfully.
 * @retval false An error occurred during writing.
 *
 * @warning
 * - Ensure that the address is valid and within the range of registered holding registers.
 * - Read-only registers cannot be written to.
 * 
 * @example
 * ```c
 * bool success = write_single_register(server, 0x0001, 200);
 * if (success) {
 *     // Register written successfully
 * }
 * ```
 */
static bool write_single_register(modbus_server_data_t *server, uint16_t address, uint16_t value)
{
    if (address >= MAX_ADDRESS_HOLDING_REGISTERS)
    {
        server->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        return false;
    }

    /* Find the register */
    int idx = find_register(server, address); 
    if(idx < 0) {
        server->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        return false;
    }
    variable_modbus_t *var = &server->holding_registers[idx];

    if (!var->read_only)
    {
        if (var->write_callback)
        {
            *var->variable_ptr = var->write_callback(value);
        }
        else
        {
            *var->variable_ptr = value;
        }
        return true;
    }
    else
    {
        server->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        return false;
    }
}

/**
 * @brief Writes multiple holding registers.
 *
 * This function writes values to multiple holding registers starting from the specified address.
 *
 * @param[in,out] server Pointer to the server data structure.
 * @param[in]     start_address Starting address to write to.
 * @param[in]     quantity      Number of registers to write.
 *
 * @return bool `true` if the write was successful, `false` otherwise.
 *
 * @retval true  Registers written successfully.
 * @retval false An error occurred during writing.
 *
 * @warning
 * - Ensure that the addresses are valid and within the range of registered holding registers.
 * - Read-only registers cannot be written to.
 * 
 * @example
 * ```c
 * bool success = write_registers(server, 0x0010, 5);
 * if (success) {
 *     // Registers written successfully
 * }
 * ```
 */
static inline bool write_registers(modbus_server_data_t *server, uint16_t start_address, uint16_t quantity)
{
    if ((start_address + quantity) > MAX_ADDRESS_HOLDING_REGISTERS)
    {
        server->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        uint16_t index = start_address + i;
        /* Find the register */
        int idx = find_register(server, index); 
        if(idx < 0) {
            server->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
            return false;
        }
        variable_modbus_t *var = &server->holding_registers[idx];
        if (!var->read_only)
        {
            uint16_t data = server->msg.buffer[i];
            if (var->write_callback)
            {
                *var->variable_ptr = var->write_callback(data);
            }
            else
            {
                *var->variable_ptr = data;
            }
        }
    }

    return true;
}

/**
 * @brief Sends a Modbus response with address and quantity.
 *
 * This function constructs a response segment containing the starting address and the quantity
 * of registers or coils involved in the operation.
 *
 * @param[in,out] tx_buffer Pointer to the transmission buffer where the response will be stored.
 * @param[in,out] tx_index  Pointer to the current index in the transmission buffer.
 * @param[in]     address   Starting address of the registers or coils.
 * @param[in]     quantity  Quantity of registers or coils.
 *
 * @return void
 *
 * @warning
 * - Ensure that `tx_buffer` has enough space to store the address and quantity.
 * 
 * @example
 * ```c
 * uint8_t tx_buffer[256];
 * uint16_t tx_index = 0;
 * send_address_quantity_response(tx_buffer, &tx_index, 0x0001, 10);
 * // tx_buffer now contains the address and quantity
 * ```
 */
static void send_address_quantity_response(uint8_t *tx_buffer, uint16_t *tx_index, uint16_t address, uint16_t quantity)
{
    tx_buffer[(*tx_index)++] = GET_HIGH_BYTE(address);
    tx_buffer[(*tx_index)++] = GET_LOW_BYTE(address);
    tx_buffer[(*tx_index)++] = GET_HIGH_BYTE(quantity);
    tx_buffer[(*tx_index)++] = GET_LOW_BYTE(quantity);
}

/**
 * @brief Handles Modbus read functions (Function Codes: 0x01, 0x02, 0x03, 0x04).
 *
 * This function reads the requested registers or coils and prepares the response data.
 *
 * @param[in,out] server    Pointer to the server data structure.
 * @param[in]     read_func Function pointer to read data (coils or registers).
 *
 * @return void
 *
 * @warning
 * - Ensure that the `read_func` correctly reads the requested data and populates the transmission buffer.
 * 
 * @example
 * ```c
 * handle_read_function(server, read_registers);
 * ```
 */
static inline void handle_read_function(modbus_server_data_t *server, bool (*read_func)(modbus_server_data_t *, uint16_t, uint16_t, uint8_t *, uint16_t *))
{
    modbus_context_t *ctx = server->ctx;
    /* Check the effective range of the number of reads */
    if ((1 <=  server->msg.read_quantity) && ( server->msg.read_quantity <= MODBUS_MAX_READ_WRITE_SIZE))
    {
        /* Attempt to read data */
        if (read_func(server, server->msg.read_address,  server->msg.read_quantity, ctx->tx_raw_buffer, &ctx->tx_raw_index))
        {
            /* Successfully read data */
            server->msg.error = MODBUS_ERROR_NONE;
        }
    }
    else
    {
        server->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }
}

/**
 * @brief Handles Modbus write single functions (Function Codes: 0x05, 0x06).
 *
 * This function writes a single register or coil and prepares the response data.
 *
 * @param[in,out] server     Pointer to the server data structure.
 * @param[in]     write_func Function pointer to write data (coil or register).
 *
 * @return void
 *
 * @warning
 * - Ensure that the `write_func` correctly writes the requested data and prepares the response.
 * 
 * @example
 * ```c
 * handle_write_single_function(server, write_single_register);
 * ```
 */
static void handle_write_single_function(modbus_server_data_t *server, bool (*write_func)(modbus_server_data_t *, uint16_t, uint16_t))
{
    modbus_context_t *ctx = server->ctx;
    uint16_t address = server->msg.write_address;
    uint16_t value = server->msg.write_value;
    ctx->tx_raw_index = 0;

    /* Attempt to write data */
    if (write_func(server, address, value))
    {
        /* Create and send response frame if not broadcast */
        if (server->msg.slave_address != MODBUS_BROADCAST_ADDRESS)
        {
            send_address_quantity_response(ctx->tx_raw_buffer, &ctx->tx_raw_index, address, value);
            server->msg.error = MODBUS_ERROR_NONE;
        }
    }
}

/**
 * @brief Handles Modbus write multiple functions (Function Codes: 0x0F, 0x10).
 *
 * This function writes multiple registers or coils and prepares the response data.
 *
 * @param[in,out] server     Pointer to the server data structure.
 * @param[in]     write_func Function pointer to write multiple data (coils or registers).
 *
 * @return void
 *
 * @warning
 * - Ensure that the `write_func` correctly writes the requested data and prepares the response.
 * 
 * @example
 * ```c
 * handle_write_multiple_function(server, write_registers);
 * ```
 */
static void handle_write_multiple_function(modbus_server_data_t *server, bool (*write_func)(modbus_server_data_t *, uint16_t, uint16_t))
{
    modbus_context_t *ctx = server->ctx;
    uint16_t start_address = server->msg.write_address;
    uint16_t quantity = server->msg.write_quantity;
    ctx->tx_raw_index = 0;

    /* Check the effective range of the number of writes */
    if ((1 <= quantity) && (quantity <= MODBUS_MAX_READ_WRITE_SIZE))
    {
        /* Attempt to write data */
        if (write_func(server, start_address, quantity))
        {
            /* Create and send response frame if not broadcast */
            if (server->msg.slave_address != MODBUS_BROADCAST_ADDRESS)
            {
                send_address_quantity_response(ctx->tx_raw_buffer ,&ctx->tx_raw_index, start_address, quantity);
            }
        }
    }
    else
    {
        server->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }
}

/**
 * @brief Puts data related to device information into the TX buffer.
 *
 * This function prepares the response data for device information requests (Function Code 0x2B).
 *
 * @param[in,out] server Pointer to the server data structure.
 *
 * @return void
 *
 * @warning
 * - Ensure that the TX buffer has enough space to store all device information packages.
 * 
 * @example
 * ```c
 * handle_read_device_information(server);
 * ```
 */
static void handle_read_device_information(modbus_server_data_t *server) {
    modbus_context_t *ctx = server->ctx;
    ctx->tx_raw_index = 0;
    ctx->tx_raw_buffer[ctx->tx_raw_index++] = server->msg.mei_type;
    ctx->tx_raw_buffer[ctx->tx_raw_index++] = server->msg.device_id_code;
    ctx->tx_raw_buffer[ctx->tx_raw_index++] = server->device_info.conformity_level;
    ctx->tx_raw_buffer[ctx->tx_raw_index++] = 0; // Number of additional packages, set to 0 for now
    ctx->tx_raw_buffer[ctx->tx_raw_index++] = 0; // Next object ID
    ctx->tx_raw_buffer[ctx->tx_raw_index++] = server->device_info.info_saved;
    /* Copy all device information packages to TX buffer */
    for (size_t i = 0; i < server->device_info.info_saved; i++)
    {
        ctx->tx_raw_buffer[ctx->tx_raw_index++] = server->device_info.data[i].id;
        ctx->tx_raw_buffer[ctx->tx_raw_index++] = server->device_info.data[i].length;
        memcpy(&ctx->tx_raw_buffer[ctx->tx_raw_index], server->device_info.data[i].value_in_ascii, server->device_info.data[i].length);
        ctx->tx_raw_index += server->device_info.data[i].length;        
    }
}


/**
 * @brief Handles the Modbus function based on the function code.
 *
 * This function executes the requested Modbus function (e.g., read/write registers) and prepares the response.
 *
 * @param[in,out] server Pointer to the server data structure.
 */
static void handle_function(modbus_server_data_t *server)
{
    /* Reset error */
    server->msg.error = MODBUS_ERROR_NONE;

    switch (server->msg.function_code)
    {    
    case MODBUS_FUNC_READ_HOLDING_REGISTERS:
    case MODBUS_FUNC_READ_INPUT_REGISTERS:
        handle_read_function(server, read_registers);
        break;

    case MODBUS_FUNC_WRITE_SINGLE_REGISTER:
        handle_write_single_function(server, write_single_register);
        break;

    case MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS:
        handle_write_multiple_function(server, write_registers);
        break;

    case MODBUS_FUNC_READ_WRITE_MULTIPLE_REGISTERS:
        /* Handle read/write multiple registers */
        {
            modbus_context_t *ctx = server->ctx;
            /* First, write the registers */
            if (write_registers(server, server->msg.write_address, server->msg.write_quantity))
            {
                /* Then, read the registers */
                if (read_registers(server, server->msg.read_address, server->msg.read_quantity, ctx->tx_raw_buffer, &ctx->tx_raw_index))
                {
                    /* Successfully handled read/write */
                }
            }
        }
        break;
    case MODBUS_FUNC_READ_DEVICE_INFORMATION:
        handle_read_device_information(server);
        break;

    default:
        /* Function not implemented */
        server->msg.error = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
        break;
    }

    /* Handle error response if any */
    if (server->msg.error != MODBUS_ERROR_NONE)
    {
        /* Send Modbus exception response */
        fsm_handle_event(&server->fsm, MODBUS_EVENT_ERROR_DETECTED);
    }
}
