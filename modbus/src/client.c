/**
 * @file modbus_master.c
 * @brief Modbus Master (Client) state and logic implementation.
 *
 * This implementation exemplifies a Modbus Master that sends read requests (Function Code 0x03)
 * and waits for responses from a Slave device.
 *
 * **Features:**
 * - Uses FSM to manage states: Idle, Sending Request, Waiting Response, etc.
 * - When the user calls `modbus_client_read_holding_registers()`, the FSM transitions to the
 *   `SENDING_REQUEST` state and sends the frame.
 * - Upon receiving response bytes, calls `modbus_client_receive_data_event()` to feed the FSM.
 * - If the response is complete and valid, the FSM transitions to `PROCESSING_RESPONSE` and then back to Idle,
 *   storing the read data in the context.
 * - In case of timeout or error, transitions to the Error state and then returns to Idle.
 * - No dynamic memory allocation.
 *
 * **Prerequisites:**
 * - `modbus_context_t` has internal space to store client data (`user_data`).
 * - The transport is already configured in `ctx->transport`.
 *
 * Adjust as needed for real application requirements.
 *
 * **Author:**
 * Luiz Carlos Gili
 * 
 * **Date:**
 * 2024-12-20
 */

#include <modbus/client.h>
#include <modbus/core.h>
#include <modbus/utils.h>
#include <modbus/fsm.h>
#include <string.h>

#include <modbus/mb_log.h>

/* -------------------------------------------------------------------------- */
/*                           Global Variables                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Global client data structure.
 *
 * This global instance holds all client-specific data, including FSM state, context, device information,
 * current request details, read data buffer, and timeout references.
 */
modbus_client_data_t g_client;

/* -------------------------------------------------------------------------- */
/*                           Internal Prototypes                                */
/* -------------------------------------------------------------------------- */

/* FSM Action and Guard function prototypes */
static void action_idle(fsm_t *fsm);
static void action_send_request(fsm_t *fsm);
static void action_wait_response(fsm_t *fsm);
static void action_process_response(fsm_t *fsm);
static void action_handle_error(fsm_t *fsm);

static bool guard_tx_complete(fsm_t *fsm);
static bool guard_response_complete(fsm_t *fsm);
static bool guard_timeout(fsm_t *fsm);

/* -------------------------------------------------------------------------- */
/*                           FSM State Definitions                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Transition table for the Idle state.
 *
 * When the `MODBUS_CLIENT_EVENT_SEND_REQUEST` event is received, transition to
 * the `SENDING_REQUEST` state and execute `action_send_request`.
 */
static const fsm_transition_t state_idle_transitions[] = {
    FSM_TRANSITION(MODBUS_CLIENT_EVENT_SEND_REQUEST, modbus_client_state_sending_request, action_send_request, NULL)
};

/**
 * @brief Modbus Master FSM state: Idle.
 *
 * This state represents the idle condition of the Modbus FSM, where it is waiting
 * for a new request to be sent.
 */
const fsm_state_t modbus_client_state_idle = FSM_STATE("IDLE", MODBUS_CLIENT_STATE_IDLE, state_idle_transitions, action_idle, 0);

/**
 * @brief Transition table for the Sending Request state.
 *
 * - On `MODBUS_CLIENT_EVENT_TX_COMPLETE` with `guard_tx_complete` passing, transition to `WAITING_RESPONSE`
 *   and execute `action_wait_response`.
 * - On `MODBUS_CLIENT_EVENT_ERROR_DETECTED`, transition to `ERROR` and execute `action_handle_error`.
 */
static const fsm_transition_t state_sending_request_transitions[] = {
    FSM_TRANSITION(MODBUS_CLIENT_EVENT_TX_COMPLETE, modbus_client_state_waiting_response, action_wait_response, guard_tx_complete),
    FSM_TRANSITION(MODBUS_CLIENT_EVENT_ERROR_DETECTED, modbus_client_state_error, action_handle_error, NULL)
};
/**
 * @brief Modbus Master FSM state: Sending Request.
 *
 * This state handles the transmission of a Modbus request to the slave device.
 */
const fsm_state_t modbus_client_state_sending_request = FSM_STATE("SENDING_REQUEST", MODBUS_CLIENT_STATE_SENDING_REQUEST, state_sending_request_transitions, NULL, 0);


/**
 * @brief Transition table for the Waiting Response state.
 *
 * - On `MODBUS_CLIENT_EVENT_RESPONSE_COMPLETE` with `guard_response_complete` passing, transition to `PROCESSING_RESPONSE`.
 * - On `MODBUS_CLIENT_EVENT_TIMEOUT` with `guard_timeout` passing, transition to `ERROR`.
 * - On `MODBUS_CLIENT_EVENT_ERROR_DETECTED`, transition to `ERROR` and execute `action_handle_error`.
 */
static const fsm_transition_t state_waiting_response_transitions[] = {
    FSM_TRANSITION(MODBUS_CLIENT_EVENT_RESPONSE_COMPLETE, modbus_client_state_processing_response, action_process_response, guard_response_complete),
    FSM_TRANSITION(MODBUS_CLIENT_EVENT_TIMEOUT, modbus_client_state_error, action_handle_error, guard_timeout),
    FSM_TRANSITION(MODBUS_CLIENT_EVENT_ERROR_DETECTED, modbus_client_state_error, action_handle_error, NULL)
};

/**
 * @brief Modbus Master FSM state: Waiting Response.
 *
 * This state represents the period where the Master is waiting for a response
 * from the slave device after sending a request.
 */
const fsm_state_t modbus_client_state_waiting_response = FSM_STATE("WAITING_RESPONSE", MODBUS_CLIENT_STATE_WAITING_RESPONSE, state_waiting_response_transitions, NULL, 0);

/**
 * @brief Transition table for the Processing Response state.
 *
 * - On `MODBUS_CLIENT_EVENT_RESTART_FROM_ERROR`, transition back to `IDLE`.
 */
static const fsm_transition_t state_processing_response_transitions[] = {
    FSM_TRANSITION(MODBUS_CLIENT_EVENT_RESTART_FROM_ERROR, modbus_client_state_idle, NULL, NULL)
};
/**
 * @brief Modbus Master FSM state: Processing Response.
 *
 * This state handles the processing of the received response from the slave device,
 * including parsing and validating the data.
 */
const fsm_state_t modbus_client_state_processing_response = FSM_STATE("PROCESSING_RESPONSE", MODBUS_CLIENT_STATE_PROCESSING_RESPONSE, state_processing_response_transitions, action_process_response, 0);

/**
 * @brief Transition table for the Error state.
 *
 * - On `MODBUS_CLIENT_EVENT_RESTART_FROM_ERROR`, transition back to `IDLE`.
 */
static const fsm_transition_t state_error_transitions[] = {
    FSM_TRANSITION(MODBUS_CLIENT_EVENT_RESTART_FROM_ERROR, modbus_client_state_idle, NULL, NULL)
};
/**
 * @brief Modbus Master FSM state: Error.
 *
 * This state represents an error condition in the Modbus FSM. The FSM can recover
 * and transition back to the idle state when a new event occurs.
 */
const fsm_state_t modbus_client_state_error = FSM_STATE("ERROR", MODBUS_CLIENT_STATE_ERROR, state_error_transitions, NULL, 0);

/* -------------------------------------------------------------------------- */
/*                           FSM State Initialization                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initializes the FSM states and their transitions.
 *
 * This function sets up the FSM by defining states and their associated transitions.
 */
// static void initialize_fsm_states(void) {
//     /* Define transitions for each state */
//     fsm_state_set_transitions(&modbus_client_state_idle, state_idle_transitions, sizeof(state_idle_transitions) / sizeof(fsm_transition_t));
//     fsm_state_set_transitions(&modbus_client_state_sending_request, state_sending_request_transitions, sizeof(state_sending_request_transitions) / sizeof(fsm_transition_t));
//     fsm_state_set_transitions(&modbus_client_state_waiting_response, state_waiting_response_transitions, sizeof(state_waiting_response_transitions) / sizeof(fsm_transition_t));
//     fsm_state_set_transitions(&modbus_client_state_processing_response, state_processing_response_transitions, sizeof(state_processing_response_transitions) / sizeof(fsm_transition_t));
//     fsm_state_set_transitions(&modbus_client_state_error, state_error_transitions, sizeof(state_error_transitions) / sizeof(fsm_transition_t));
// }

/* -------------------------------------------------------------------------- */
/*                           Public API Functions                               */
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
 * - The transport layer must expose `write`, `read`, and a monotonic `get_reference_msec` implementation (or provide a custom
 *   `mb_transport_if_t`).
 * 
 * @example
 * ```c
 * modbus_context_t ctx;
 * modbus_transport_t transport = {
 *     .write = transport_write_function,        // User-defined transport write function
 *     .read = transport_read_function,          // User-defined transport read function
 *     .get_reference_msec = get_msec_function  // User-defined function to get current time in ms
 * };
 * 
 * uint16_t baud = 19200;
 * 
 * modbus_error_t error = modbus_client_create(&ctx, &transport, &baud);
 * if (error != MODBUS_ERROR_NONE) {
 *     // Handle initialization error
 * }
 * ```
 */
modbus_error_t modbus_client_create(modbus_context_t *modbus, modbus_transport_t *platform_conf, uint16_t *baudrate) {
    if (modbus == NULL || baudrate == NULL || platform_conf == NULL)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!platform_conf->read || !platform_conf->write || !platform_conf->get_reference_msec)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    /* Assign global client data to the context */
    modbus->user_data = &g_client;
    g_client.ctx = modbus;

    /* Initialize device information */
    g_client.device_info.baudrate = baudrate;
    modbus->transport = *platform_conf;

    modbus_error_t bind_status = modbus_transport_bind_legacy(&modbus->transport_iface, &modbus->transport);
    if (bind_status != MODBUS_ERROR_NONE) {
        return bind_status;
    }

    /* Initialize reference times */
    const mb_time_ms_t now = mb_transport_now(&modbus->transport_iface);
    modbus->rx_reference_time = now;
    modbus->tx_reference_time = now;

    /* Initialize default timeout */
    g_client.timeout_ms = MASTER_DEFAULT_TIMEOUT_MS;

    /* Initialize FSM */
    fsm_init(&g_client.fsm, &modbus_client_state_idle, &g_client);
    modbus->role = MODBUS_ROLE_CLIENT;

    return MODBUS_ERROR_NONE;
}

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
void modbus_client_poll(modbus_context_t *ctx) {
    if (!ctx) return;
    modbus_client_data_t *client = (modbus_client_data_t *)ctx->user_data;
    fsm_run(&client->fsm);

    /* Check for timeout in WAITING_RESPONSE state */
    if (client->fsm.current_state->id == MODBUS_CLIENT_STATE_WAITING_RESPONSE) {
        const mb_time_ms_t elapsed = mb_transport_elapsed_since(&ctx->transport_iface,
                                                                client->request_time_ref);
        if (elapsed > (mb_time_ms_t)client->timeout_ms) {
            fsm_handle_event(&client->fsm, MODBUS_CLIENT_EVENT_TIMEOUT);
        }
    }
}

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
modbus_error_t modbus_client_read_holding_registers(modbus_context_t *ctx, uint8_t slave_address, uint16_t start_address, uint16_t quantity) {
    if (!ctx) return MODBUS_ERROR_INVALID_ARGUMENT;
    if (quantity < 1 || quantity > MODBUS_MAX_READ_WRITE_SIZE) return MODBUS_ERROR_INVALID_ARGUMENT;

    modbus_client_data_t *client = (modbus_client_data_t *)ctx->user_data;

    /* Configure current request */
    client->current_slave_address = slave_address;
    client->current_function = MODBUS_FUNC_READ_HOLDING_REGISTERS;
    client->current_start_address = start_address;
    client->current_quantity = quantity;
    client->read_data_count = 0;

    /* Trigger event to send request */
    fsm_handle_event(&client->fsm, MODBUS_CLIENT_EVENT_SEND_REQUEST);
    return MODBUS_ERROR_NONE;
}

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
void modbus_client_receive_data_event(fsm_t *fsm, uint8_t data) {
    modbus_client_data_t *client = (modbus_client_data_t *)fsm->user_data;
    modbus_context_t *ctx = client->ctx;

    /* Update reference time for RX */
    ctx->rx_reference_time = mb_transport_now(&ctx->transport_iface);
    MB_LOG_TRACE("RECEIVED Byte %u on %llu ms", data,
                 (unsigned long long)ctx->rx_reference_time);
    /* Store received byte in RX buffer */
    if (ctx->rx_count < sizeof(ctx->rx_buffer)) {
        ctx->rx_buffer[ctx->rx_count++] = data;
    } else {
        /* Buffer overflow detected */
        fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_ERROR_DETECTED);
        return;
    }

    fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_RESPONSE_COMPLETE);

    /* 
     * Here, you can implement logic to determine if a complete frame has been received.
     * This can involve checking the expected length based on function code and data.
     * For simplicity, this example assumes that frame completeness is handled elsewhere,
     * possibly in the polling function.
     */
}

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
modbus_error_t modbus_client_set_timeout(modbus_context_t *ctx, uint16_t timeout_ms) {
    if (!ctx) return MODBUS_ERROR_INVALID_ARGUMENT;
    modbus_client_data_t *client = (modbus_client_data_t *)ctx->user_data;
    client->timeout_ms = timeout_ms;
    return MODBUS_ERROR_NONE;
}

/**
 * @brief Retrieves the data read from the last Modbus read request.
 *
 * Depending on the implementation, the Master may store the read data internally.
 * This function allows the user to access the read data after receiving confirmation
 * that the response has been received and processed.
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
uint16_t modbus_client_get_read_data(modbus_context_t *ctx, int16_t *buffer, uint16_t max_regs) {
    if (!ctx || !buffer) return 0;
    modbus_client_data_t *client = (modbus_client_data_t *)ctx->user_data;
    uint16_t count = (client->read_data_count < max_regs) ? client->read_data_count : max_regs;
    memcpy(buffer, client->read_data, count * sizeof(int16_t));
    return count;
}

/* -------------------------------------------------------------------------- */
/*                           FSM Action Implementations                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Idle action for the FSM.
 *
 * This action is performed when the FSM enters the idle state. Currently, no specific action is needed.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_idle(fsm_t *fsm) {
    /* No action needed in Idle state */
    (void)fsm;
}

/**
 * @brief Action to send a Modbus request.
 *
 * This action constructs the Modbus request frame based on the current request parameters
 * and sends it through the transport layer. It handles any errors that occur during transmission.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_send_request(fsm_t *fsm) {
    modbus_client_data_t *client = (modbus_client_data_t *)fsm->user_data;
    modbus_context_t *ctx = client->ctx;

    /* Build the request frame */
    ctx->tx_index = 0;
    ctx->rx_count = 0; // Clear RX buffer
    ctx->rx_index = 0;

    if (client->current_function == MODBUS_FUNC_READ_HOLDING_REGISTERS) {
        uint8_t data[4];
        data[0] = (uint8_t)((client->current_start_address >> 8) & 0xFF);
        data[1] = (uint8_t)(client->current_start_address & 0xFF);
        data[2] = (uint8_t)((client->current_quantity >> 8) & 0xFF);
        data[3] = (uint8_t)(client->current_quantity & 0xFF);

        uint16_t len = modbus_build_rtu_frame(client->current_slave_address, client->current_function,
                                              data, 4, ctx->tx_buffer, sizeof(ctx->tx_buffer));
        if (len == 0) {
            /* Failed to build frame */
            fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_ERROR_DETECTED);
            return;
        }
        ctx->tx_index = len;
    } else {
        /* Unsupported function */
        fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_ERROR_DETECTED);
        return;
    }

    /* Send the frame */
    modbus_error_t err = modbus_send_frame(ctx, ctx->tx_buffer, ctx->tx_index);
    if (err != MODBUS_ERROR_NONE) {
        /* Transmission error */
        fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_ERROR_DETECTED);
        return;
    }

    /* Trigger TX_COMPLETE event */
    fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_TX_COMPLETE);
}

/**
 * @brief Action to wait for a response.
 *
 * This action marks the reference time for the current request to handle timeouts.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_wait_response(fsm_t *fsm) {
    modbus_client_data_t *client = (modbus_client_data_t *)fsm->user_data;
    modbus_context_t *ctx = client->ctx;
    MB_LOG_TRACE("action_wait_response");

    /* Mark reference time for timeout */
    client->request_time_ref = mb_transport_now(&ctx->transport_iface);
    /* Now waiting for bytes from the response */
}

/**
 * @brief Action to process the received response.
 *
 * This action parses the received Modbus response frame, validates it, and extracts the data.
 * It handles any errors that occur during processing.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_process_response(fsm_t *fsm) {
    modbus_client_data_t *client = (modbus_client_data_t *)fsm->user_data;
    modbus_context_t *ctx = client->ctx;
    MB_LOG_TRACE("action_process_response");

    /* Parse the received frame */
    uint8_t address, function;
    const uint8_t *payload;
    uint16_t payload_len;
    modbus_error_t err = modbus_parse_rtu_frame(ctx->rx_buffer, ctx->rx_count,
                                                &address, &function, &payload, &payload_len);
    if (err != MODBUS_ERROR_NONE) {
        /* Parsing error */
        fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_ERROR_DETECTED);
        return;
    }

    if (modbus_is_error_response(function)) {
        /* Error response from slave */
        /* Read exception code */
        if (payload_len < 1) {
            fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_ERROR_DETECTED);
            return;
        }
        /* Log or handle specific exception codes as needed */
        fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_ERROR_DETECTED);
        return;
    }

    /* Handle Function Code 0x03: Read Holding Registers */
    if (function == MODBUS_FUNC_READ_HOLDING_REGISTERS) {
        /* payload[0] = byte_count */
        if (payload_len < 1) {
            fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_ERROR_DETECTED);
            return;
        }
        uint8_t byte_count = payload[0];
        if (byte_count % 2 != 0 || byte_count > payload_len - 1) {
            fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_ERROR_DETECTED);
            return;
        }
        uint16_t reg_count = (uint16_t)(byte_count / 2);
        if (reg_count > MODBUS_MAX_READ_WRITE_SIZE) {
            fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_ERROR_DETECTED);
            return;
        }

        client->read_data_count = reg_count;
        for (uint16_t i = 0; i < reg_count; i++) {
            uint16_t high = payload[1 + i*2];
            uint16_t low = payload[1 + i*2 + 1];
            int16_t val = (int16_t)((high << 8) | low);
            client->read_data[i] = val;
        }
    } else {
        /* Unsupported function */
        fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_ERROR_DETECTED);
        return;
    }

    /* Successfully processed response: transition back to Idle */
    fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_RESTART_FROM_ERROR);
}

/**
 * @brief Action to handle errors.
 *
 * This action handles errors by restarting the UART (if available) and transitioning back to Idle.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 */
static void action_handle_error(fsm_t *fsm) {
    modbus_client_data_t *client = (modbus_client_data_t *)fsm->user_data;
    modbus_context_t *ctx = client->ctx;

    /* Restart UART if available */
    if (ctx->transport.restart_uart) {
        ctx->transport.restart_uart();
    }

    /* Transition back to Idle */
    fsm_handle_event(fsm, MODBUS_CLIENT_EVENT_RESTART_FROM_ERROR);
}

/* -------------------------------------------------------------------------- */
/*                           FSM Guard Implementations                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief Guard function to determine if transmission is complete.
 *
 * This guard checks if the transmission of the request has been completed.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 *
 * @return true if transmission is complete, false otherwise.
 */
static bool guard_tx_complete(fsm_t *fsm) {
    /* Assuming transmission is synchronous and completes immediately */
    /* If transmission is asynchronous, implement appropriate checks */
    (void)fsm;
    MB_LOG_TRACE("guard_tx_complete");
    return true;
}

/**
 * @brief Guard function to determine if the response is complete.
 *
 * This guard checks if a complete and valid Modbus response frame has been received.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 *
 * @return true if the response is complete and valid, false otherwise.
 */
static bool guard_response_complete(fsm_t *fsm) {
    modbus_client_data_t *client = (modbus_client_data_t *)fsm->user_data;
    modbus_context_t *ctx = client->ctx;
    MB_LOG_TRACE("guard_response_complete");

    /* Verify CRC and minimum size */
    if (ctx->rx_count < 4) {
        return false;
    }
    if (modbus_crc_validate(ctx->rx_buffer, (uint16_t)ctx->rx_count)) {
        return true; /* Frame is complete and valid */
    }
    return false;
}

/**
 * @brief Guard function to determine if a timeout has occurred.
 *
 * This guard is triggered when a timeout event is received.
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 *
 * @return true if a timeout has occurred, false otherwise.
 */
static bool guard_timeout(fsm_t *fsm) {
    /* This guard is called when the EVENT_TIMEOUT occurs, indicating that a timeout has already happened */
    (void)fsm;
    return true;
}
