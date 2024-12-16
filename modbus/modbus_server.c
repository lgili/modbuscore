#include <modbus/modbus_server.h>

/******************************************************************************
 * Private variables Declarations
 ******************************************************************************/
uint16_t g_holding_register_quantity = 0;
variable_modbus_t g_holding_register[MAX_SIZE_HOLDING_REGISTERS];

static bool need_update_baudrate = false;
static int build_error_count = 0;
bool g_modbus_started;
uint16_t g_time_to_start_modbus;
/******************************************************************************
 * Public variables Declarations
 ******************************************************************************/
extern uint8_t g_modbus_tx_buffer[MODBUS_SEND_BUFFER_SIZE];

/**
 * @brief Modbus FSM action functions
 *
 * These functions are responsible for handling actions that occur during specific state transitions
 * in the Modbus finite state machine. Each action is associated with a particular event and state transition.
 */
static void modbus_action_idle(fsm_t *fsm);
static void modbus_action_start_receiving(fsm_t *fsm);
static void modbus_action_parse_address(fsm_t *fsm);
static void modbus_action_parse_function(fsm_t *fsm);
static void modbus_action_process_frame(fsm_t *fsm);
static void modbus_action_validate_frame(fsm_t *fsm);
static void modbus_action_build_response(fsm_t *fsm);
static void modbus_action_put_data_on_buffer(fsm_t *fsm);
static void modbus_action_calculate_crc_response(fsm_t *fsm);
static void modbus_action_send_response(fsm_t *fsm);
static void modbus_action_handle_error(fsm_t *fsm);
static void modbus_action_handle_wrong_baudrate(fsm_t *fsm);

/**
 * @brief Modbus FSM guard functions
 *
 * These functions are used as guards for conditional state transitions.
 * A guard function determines whether a transition should occur based on specific conditions.
 */
static bool modbus_guard_receive_finished(fsm_t *fsm);
static bool modbus_guard_send_finished(fsm_t *fsm);

/* Helper Functions */
static void modbus_handle_function(modbus_context_t *modbus);

/* Parsing Functions */
static modbus_error_t parse_read_request(modbus_context_t *modbus, const uint8_t *buffer, uint16_t *index, uint16_t buffer_size);
static modbus_error_t parse_write_single_request(modbus_context_t *modbus, const uint8_t *buffer, uint16_t *index, uint16_t buffer_size);
static modbus_error_t parse_write_multiple_coils_request(modbus_context_t *modbus, const uint8_t *buffer, uint16_t *index, uint16_t buffer_size);
static modbus_error_t parse_write_multiple_registers_request(modbus_context_t *modbus, const uint8_t *buffer, uint16_t *index, uint16_t buffer_size);
static modbus_error_t parse_read_write_multiple_registers_request(modbus_context_t *modbus, const uint8_t *buffer, uint16_t *index, uint16_t buffer_size);
static modbus_error_t parse_bootloader_request(modbus_context_t *modbus, uint8_t *buffer, uint16_t *index, uint16_t buffer_size);
static modbus_error_t parse_device_info_request(modbus_context_t *modbus, const uint8_t *buffer, uint16_t *index, uint16_t buffer_size);

/******************************************************************************
 * FSM State Definitions
 ******************************************************************************/
/* Idle State Transitions */
static const fsm_transition_t modbus_state_idle_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_RX_BYTE_RECEIVED, modbus_state_receiving, modbus_action_start_receiving, NULL)};
const fsm_state_t modbus_state_idle = FSM_STATE("MODBUS_STATE_IDLE", MODBUS_STATE_IDLE, modbus_state_idle_transitions, modbus_action_idle);

/* Receiving State Transitions */
static const fsm_transition_t modbus_state_receiving_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_RX_BYTE_RECEIVED, modbus_state_receiving, modbus_action_start_receiving, NULL),
    FSM_TRANSITION(MODBUS_EVENT_PARSE_ADDRESS, modbus_state_parsing_address, modbus_action_parse_address, modbus_guard_receive_finished),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_state_error, modbus_action_handle_error, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_WRONG_BAUDRATE, modbus_state_error, modbus_action_handle_wrong_baudrate, NULL)};
const fsm_state_t modbus_state_receiving = FSM_STATE("MODBUS_STATE_RECEIVING", MODBUS_STATE_RECEIVING, modbus_state_receiving_transitions, modbus_action_start_receiving);

/* Parsing Address State Transitions */
static const fsm_transition_t modbus_state_parsing_address_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_PARSE_FUNCTION, modbus_state_parsing_function, modbus_action_parse_function, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_state_error, modbus_action_handle_error, NULL)};
const fsm_state_t modbus_state_parsing_address = FSM_STATE("MODBUS_STATE_PARSING_ADDRESS", MODBUS_STATE_PARSING_ADDRESS, modbus_state_parsing_address_transitions, NULL);

/* Parsing Function State Transitions */
static const fsm_transition_t modbus_state_parsing_function_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_PROCESS_FRAME, modbus_state_processing, modbus_action_process_frame, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_state_error, modbus_action_handle_error, NULL)};
const fsm_state_t modbus_state_parsing_function = FSM_STATE("MODBUS_STATE_PARSING_FUNCTION", MODBUS_STATE_PARSING_FUNCTION, modbus_state_parsing_function_transitions, NULL);

/* Processing State Transitions */
static const fsm_transition_t modbus_state_processing_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_VALIDATE_FRAME, modbus_state_validating_frame, modbus_action_validate_frame, NULL),
    FSM_TRANSITION(MODBUS_EVENT_BOOTLOADER, modbus_state_sending, modbus_action_send_response, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_state_error, modbus_action_handle_error, NULL)};
const fsm_state_t modbus_state_processing = FSM_STATE("MODBUS_STATE_PROCESSING", MODBUS_STATE_PROCESSING, modbus_state_processing_transitions, NULL);

/* Validating frame State Transitions */
static const fsm_transition_t modbus_state_validating_frame_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_BUILD_RESPONSE, modbus_state_building_response, modbus_action_build_response, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_state_error, modbus_action_handle_error, NULL)};
const fsm_state_t modbus_state_validating_frame = FSM_STATE("MODBUS_STATE_VALIDATING_FRAME", MODBUS_STATE_VALIDATING_FRAME, modbus_state_validating_frame_transitions, NULL);

/* Building Response State Transitions */
static const fsm_transition_t modbus_state_building_response_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_PUT_DATA_ON_BUFFER, modbus_state_putting_data_on_buffer, modbus_action_put_data_on_buffer, NULL),
    FSM_TRANSITION(MODBUS_EVENT_BROADCAST_DONT_ANSWER, modbus_state_idle, NULL, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_state_error, modbus_action_handle_error, NULL)};
const fsm_state_t modbus_state_building_response = FSM_STATE("MODBUS_STATE_BUILDING_RESPONSE", MODBUS_STATE_BUILDING_RESPONSE, modbus_state_building_response_transitions, NULL);

/* Put data on buffer to send State Transitions */
static const fsm_transition_t modbus_state_putting_data_on_buffer_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_CALCULATE_CRC, modbus_state_calculating_crc, modbus_action_calculate_crc_response, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_state_error, modbus_action_handle_error, NULL)};
const fsm_state_t modbus_state_putting_data_on_buffer = FSM_STATE("MODBUS_STATE_PUTTING_DATA_ON_BUFFER", MODBUS_STATE_PUTTING_DATA_ON_BUFFER, modbus_state_putting_data_on_buffer_transitions, NULL);

/* Calculating CRC Response State Transitions */
static const fsm_transition_t modbus_state_calculating_crc_response_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_SEND_RESPONSE, modbus_state_sending, modbus_action_send_response, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_state_error, modbus_action_handle_error, NULL)};
const fsm_state_t modbus_state_calculating_crc = FSM_STATE("MODBUS_STATE_CALCULATING_CRC", MODBUS_STATE_CALCULATING_CRC, modbus_state_calculating_crc_response_transitions, NULL);

/* Sending State Transitions */
static const fsm_transition_t modbus_state_sending_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_TX_COMPLETE, modbus_state_idle, NULL, modbus_guard_send_finished),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_state_error, modbus_action_handle_error, NULL)};
const fsm_state_t modbus_state_sending = FSM_STATE("MODBUS_STATE_SENDING", MODBUS_STATE_SENDING, modbus_state_sending_transitions, modbus_action_send_response);

/* Error State Transitions */
static const fsm_transition_t modbus_state_error_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_RX_BYTE_RECEIVED, modbus_state_idle, NULL, NULL),
    FSM_TRANSITION(MODBUS_EVENT_RESTART_FROM_ERROR, modbus_state_idle, NULL, NULL)};
const fsm_state_t modbus_state_error = FSM_STATE("MODBUS_STATE_ERROR", MODBUS_STATE_ERROR, modbus_state_error_transitions, NULL);

/******************************************************************************
 * Public Functions Implementation
 ******************************************************************************/
modbus_error_t restart_modbus_from_critical(modbus_context_t * modbus) {

    modbus_server_create(modbus, &modbus->platform, modbus->device_info.address, modbus->device_info.baudrate);
    modbus->platform.restart_uart();
    return MODBUS_ERROR_NONE;
}

/**
 * @brief Polls the Modbus server FSM to process events.
 *
 * @param modbus Pointer to the Modbus context.
 */
void modbus_server_poll(modbus_context_t *modbus)
{
    if (modbus != NULL)
    {
        // special function to init modbus with diferente baudrate. When the timeout ocurrs will change to default baud
        uint16_t getout_from_serial_port_timer = modbus->platform.measure_time_msec(g_time_to_start_modbus);
        if (getout_from_serial_port_timer >= TIME_TO_START_MODBUS_MS && g_modbus_started == false)
        {
            *modbus->device_info.baudrate = modbus->platform.change_baudrate(MODBUS_BAUDRATE);
            modbus->platform.restart_uart();
            need_update_baudrate = true;
            g_modbus_started = true;
        }

        // uint16_t error_timer = modbus->platform.measure_time_msec(modbus->error_timer);
        // if (error_timer >= 1000)
        // {
        //     restart_modbus_from_critical(modbus);
        //     modbus_reset_message(modbus);
        // }

        fsm_run(&modbus->fsm);
    }
}

/**
 * @brief Initializes the Modbus server context.
 * Before create the server add all holding register to memory
 *
 * @param modbus         Pointer to the Modbus context.
 * @param device_address Pointer to the device address.
 * @param baudrate       Pointer to the baud rate.
 * @return modbus_error_t Error code indicating success or failure.
 */
modbus_error_t modbus_server_create(modbus_context_t *modbus, const modbus_platform_conf *platform_conf, uint16_t *device_address, uint16_t *baudrate)
{
    if (modbus == NULL || device_address == NULL || baudrate == NULL || platform_conf == NULL)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (platform_conf->transport == NMBS_TRANSPORT_RTU && *device_address == 0)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    if (!platform_conf->read || !platform_conf->write || !platform_conf->get_reference_msec || !platform_conf->measure_time_msec)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    // put register address array in order
    selection_sort(g_holding_register, g_holding_register_quantity);

    memset(modbus, 0, sizeof(modbus_context_t));

    modbus->device_info.address = device_address;
    modbus->device_info.baudrate = baudrate;
    modbus->platform = *platform_conf;

    modbus->rx_reference_time = modbus->platform.get_reference_msec();
    modbus->tx_reference_time = modbus->platform.get_reference_msec();
    modbus->error_timer = modbus->platform.get_reference_msec();
    g_time_to_start_modbus = modbus->platform.get_reference_msec();

    fsm_init(&modbus->fsm, &modbus_state_idle, modbus);

    // fixed value for now!!!
    modbus->device_info.conformity_level = 0x81;

    return MODBUS_ERROR_NONE;
}

/**
 * @brief Associates a variable with a Modbus holding register.
 *
 * @param address        Address in the Modbus register map.
 * @param variable       Pointer to the variable.
 * @param read_only_flag True if the variable is read-only.
 * @param read_function  Read callback function.
 * @param write_function Write callback function.
 * @return modbus_error_t Error code indicating success or failure.
 */
modbus_error_t modbus_set_holding_register(uint16_t address, int16_t *variable, bool read_only, modbus_read_callback_t read_cb, modbus_write_callback_t write_cb)
{
    if (g_holding_register_quantity >= MAX_SIZE_HOLDING_REGISTERS || g_holding_register_quantity < 0 || variable == NULL)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    g_holding_register[g_holding_register_quantity].address = address;
    g_holding_register[g_holding_register_quantity].variable_ptr = variable;
    g_holding_register[g_holding_register_quantity].read_only = read_only;
    g_holding_register[g_holding_register_quantity].read_callback = read_cb;
    g_holding_register[g_holding_register_quantity].write_callback = write_cb;

    g_holding_register_quantity++;

    return MODBUS_ERROR_NONE;
}

modbus_error_t modbus_set_array_holding_register(uint16_t address, uint16_t lenght, int16_t *variable, bool read_only, modbus_read_callback_t read_cb, modbus_write_callback_t write_cb)
{

    for (size_t i = 0; i < lenght; i++)
    {
        modbus_error_t error = modbus_set_holding_register(address + i, variable++, read_only, read_cb, write_cb);
        if (error != MODBUS_ERROR_NONE)
        {
            return error;
        }
    }
}

/**
 * @brief Add device information as ASCII value.
 *
 * @param modbus      Pointer to the Modbus context.
 * @param value       Char value to convert to ASCII.
 * @param lenght      Lenght of char.
 * @return modbus_error_t Error code indicating success or failure.
 */
modbus_error_t add_info_to_device(modbus_context_t *modbus, const char *value, uint8_t lenght)
{

    if (modbus == NULL || value == NULL || modbus->device_info.info_saved >= MAX_DEVICE_PACKAGES || lenght > MAX_DEVICE_PACKAGE_VALUES)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }

    uint8_t id = modbus->device_info.info_saved;
    modbus->device_info.data[id].lenght = lenght;
    modbus->device_info.data[id].id = id;
    for (size_t i = 0; i < lenght; i++)
    {
        modbus->device_info.data[id].value_in_ascii[i] = (uint8_t)value[i];
    }
    modbus->device_info.info_saved++;
    return MODBUS_ERROR_NONE;
}

/**
 * @brief Handles data received from UART for Modbus communication.
 *
 * @param fsm  Pointer to the FSM instance.
 * @param data Received byte.
 */
void modbus_server_receive_data_from_uart_event(fsm_t *fsm, uint8_t data)
{
    modbus_context_t *modbus = (modbus_context_t *)fsm->user_data;

    modbus->rx_reference_time = modbus->platform.get_reference_msec();

    if (modbus->raw_data.rx_count < MODBUS_RECEIVE_BUFFER_SIZE)
    {
        modbus->raw_data.rx_buffer[modbus->raw_data.rx_count++] = data;
    }
    else
    {
        /* Buffer overflow, handle error */
        modbus->msg.error = MODBUS_ERROR_INVALID_REQUEST;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        return;
    }
    if (fsm->current_state->id != MODBUS_STATE_RECEIVING)
    {
        fsm_handle_event(fsm, MODBUS_EVENT_RX_BYTE_RECEIVED);
    }
}

/******************************************************************************
 * FSM Action Functions
 ******************************************************************************/

/**
 * @brief FSM action for the idle state.
 *
 * @param fsm Pointer to the FSM instance.
 */
static void modbus_action_idle(fsm_t *fsm)
{
    modbus_context_t *modbus = (modbus_context_t *)fsm->user_data;

    if (need_update_baudrate == true)
    {
        need_update_baudrate = false;
        if (modbus->platform.change_baudrate)
        {
            *modbus->device_info.baudrate = modbus->platform.change_baudrate(*modbus->device_info.baudrate);
        }
        if (modbus->platform.restart_uart)
        {
            modbus->platform.restart_uart();
        }
    }
    set_mode_as_receiver(modbus);
    // modbus->error_timer =  get_reference_in_msec();
}

/**
 * @brief FSM action to start receiving data.
 *
 * @param fsm Pointer to the FSM instance.
 */
static void modbus_action_start_receiving(fsm_t *fsm)
{
    /* Attempt to parse the address if the frame might be complete */
    fsm_handle_event(fsm, MODBUS_EVENT_PARSE_ADDRESS);
}

/**
 * @brief FSM action to parse the slave address.
 *
 * @param fsm Pointer to the FSM instance.
 */
static void modbus_action_parse_address(fsm_t *fsm)
{
    modbus_context_t *modbus = (modbus_context_t *)fsm->user_data;

    if ((modbus == NULL) || (modbus->raw_data.rx_buffer == NULL))
    {
        modbus->msg.error = MODBUS_ERROR_INVALID_ARGUMENT;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        return;
    }

    modbus_reset_message(modbus);

    /* Read slave address */
    uint8_t slave_address;
    if (!read_uint8(modbus->raw_data.rx_buffer, &modbus->raw_data.rx_index, modbus->raw_data.rx_count, &slave_address))
    {
        modbus->msg.error = MODBUS_ERROR_INVALID_ARGUMENT;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        return;
    }
    modbus->msg.slave_address = slave_address;

    if ((modbus->msg.slave_address == *modbus->device_info.address) ||
        (modbus->msg.slave_address == MODBUS_BROADCAST_ADDRESS) ||
        (modbus->msg.slave_address == MODBUS_BOOTLOADER_ADDRESS))
    {
        if (modbus->msg.slave_address == MODBUS_BROADCAST_ADDRESS)
        {
            modbus->msg.broadcast = true;
        }
        fsm_handle_event(fsm, MODBUS_EVENT_PARSE_FUNCTION);
    }
    else
    {
        modbus->msg.error = MODBUS_ERROR_WRONG_DEVICE_ID;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
    }
}

/**
 * @brief FSM action to parse the function code.
 *
 * @param fsm Pointer to the FSM instance.
 */
static void modbus_action_parse_function(fsm_t *fsm)
{
    modbus_context_t *modbus = (modbus_context_t *)fsm->user_data;

    if (modbus)
    {
        /* Read function code */
        uint8_t function_code;
        if (!read_uint8(modbus->raw_data.rx_buffer, &modbus->raw_data.rx_index, modbus->raw_data.rx_count, &function_code))
        {
            modbus->msg.error = MODBUS_ERROR_INVALID_ARGUMENT;
            fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
            return;
        }

        modbus->msg.function_code = function_code;
        fsm_handle_event(fsm, MODBUS_EVENT_PROCESS_FRAME);
    }
    else
    {
        modbus->msg.error = MODBUS_ERROR_TRANSPORT;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
    }
}

/**
 * @brief FSM action to process the Modbus frame.
 *
 * @param fsm Pointer to the FSM instance.
 */
static void modbus_action_process_frame(fsm_t *fsm)
{
    modbus_context_t *modbus = (modbus_context_t *)fsm->user_data;
    modbus_error_t parse_error = MODBUS_ERROR_NONE;

    switch (modbus->msg.function_code)
    {
    case MODBUS_FUNC_READ_COILS:
    case MODBUS_FUNC_READ_DISCRETE_INPUTS:
    case MODBUS_FUNC_READ_HOLDING_REGISTERS:
    case MODBUS_FUNC_READ_INPUT_REGISTERS:
        parse_error = parse_read_request(modbus, modbus->raw_data.rx_buffer, &modbus->raw_data.rx_index, modbus->raw_data.rx_count);
        break;
    case MODBUS_FUNC_WRITE_SINGLE_COIL:
    case MODBUS_FUNC_WRITE_SINGLE_REGISTER:
        parse_error = parse_write_single_request(modbus, modbus->raw_data.rx_buffer, &modbus->raw_data.rx_index, modbus->raw_data.rx_count);
        break;
    case MODBUS_FUNC_WRITE_MULTIPLE_COILS:
        parse_error = parse_write_multiple_coils_request(modbus, modbus->raw_data.rx_buffer, &modbus->raw_data.rx_index, modbus->raw_data.rx_count);
        break;
    case MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS:
        parse_error = parse_write_multiple_registers_request(modbus, modbus->raw_data.rx_buffer, &modbus->raw_data.rx_index, modbus->raw_data.rx_count);
        break;
    case MODBUS_FUNC_READ_WRITE_MULTIPLE_REGISTERS:
        parse_error = parse_read_write_multiple_registers_request(modbus, modbus->raw_data.rx_buffer, &modbus->raw_data.rx_index, modbus->raw_data.rx_count);
        break;
    case MODBUS_FUNC_READ_DEVICE_INFORMATION:
        parse_error = parse_device_info_request(modbus, modbus->raw_data.rx_buffer, &modbus->raw_data.rx_index, modbus->raw_data.rx_count);
        break;
#if (ENABLE_BOOTLOADER == TRUE)
        uint8_t error = 1;
        if (modbus->platform.parse_bootloader_request)
        {
            error = modbus->platform.parse_bootloader_request(modbus->raw_data.rx_buffer, &modbus->raw_data.rx_count);
        }
        if (error == 1)
        {
            parse_error = MODBUS_EVENT_ERROR_DETECTED;
        }
        else
        {
            uart_send(modbus, (uint8_t *)modbus->raw_data.rx_buffer, modbus->raw_data.rx_count);
            parse_error = MODBUS_OTHERS_REQUESTS;
        }
        break;
#endif
    default:
        modbus->msg.error = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        return;
    }

    if (parse_error != MODBUS_ERROR_NONE)
    {
        if (parse_error == MODBUS_OTHERS_REQUESTS)
        {
            fsm_handle_event(fsm, MODBUS_EVENT_BOOTLOADER);
            return;
        }
        modbus->msg.error = parse_error;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        return;
    }

    /* Ensure we have enough bytes for CRC */
    if ((modbus->raw_data.rx_index + 2U) > modbus->raw_data.rx_count)
    {
        modbus->msg.error = MODBUS_ERROR_INVALID_ARGUMENT;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        return;
    }

    fsm_handle_event(fsm, MODBUS_EVENT_VALIDATE_FRAME);
}

static void modbus_action_validate_frame(fsm_t *fsm)
{
    modbus_context_t *modbus = (modbus_context_t *)fsm->user_data;

    /* Calculate checksum */
    if (modbus->platform.crc_calc)
    {
        uint16_t local_check_sum = modbus->platform.crc_calc(modbus->raw_data.rx_buffer, modbus->raw_data.rx_index);
        uint16_t received_check_sum = (modbus->raw_data.rx_buffer[modbus->raw_data.rx_index + 1U] << 8U) | modbus->raw_data.rx_buffer[modbus->raw_data.rx_index];

        if (local_check_sum != received_check_sum)
        {
            modbus->msg.error = MODBUS_ERROR_CRC;
            fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
            return;
        }
    }
    else
    {
        modbus->msg.error = MODBUS_ERROR_CRC;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        return;
    }

    fsm_handle_event(fsm, MODBUS_EVENT_BUILD_RESPONSE);
}

/**
 * @brief FSM action to build the Modbus response.
 *
 * @param fsm Pointer to the FSM instance.
 */
static void modbus_action_build_response(fsm_t *fsm)
{
    modbus_context_t *context = (modbus_context_t *)fsm->user_data;
    modbus_handle_function(context);

    if ((context->msg.current_read_index >= context->msg.read_quantity) || (context->msg.write_quantity >= 1) || (context->msg.mei_type != 0))
    {
        if (context->msg.broadcast == true)
        {
            fsm_handle_event(fsm, MODBUS_EVENT_BROADCAST_DONT_ANSWER);
            context->raw_data.tx_index = 0;
            context->raw_data.rx_count = 0U;
        }
        else
        {
            fsm_handle_event(fsm, MODBUS_EVENT_PUT_DATA_ON_BUFFER);
        }
        build_error_count = 0;
    }
    else
    {
        if (context->msg.current_read_index == 0 && context->msg.write_quantity == 0)
        {
            context->msg.error = MODBUS_ERROR_TRANSPORT;
            fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        }
        else
        {
            build_error_count++;
            if (build_error_count >= 128)
            {
                context->msg.error = MODBUS_ERROR_TRANSPORT;
                fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
                build_error_count = 0;
            }
        }
    }
}

/**
 * @brief FSM action to put data on buffer to send on uart.
 *
 * @param fsm Pointer to the FSM instance.
 */
static void modbus_action_put_data_on_buffer(fsm_t *fsm)
{
    modbus_context_t *context = (modbus_context_t *)fsm->user_data;
    set_mode_as_transmiter(context);

    uint16_t quantity_to_send = 0;
    if (context->msg.function_code == MODBUS_FUNC_READ_COILS)
    {
        quantity_to_send = context->msg.read_quantity + 1;
    }
    else if (context->msg.function_code <= MODBUS_FUNC_READ_INPUT_REGISTERS)
    {
        quantity_to_send = context->msg.read_quantity * 2 + 1;
    }
    else
    {
        quantity_to_send = context->raw_data.tx_index;
    }
    context->raw_data.tx_index = 0;

    /* Create Modbus frame (RTU) */
    g_modbus_tx_buffer[context->raw_data.tx_index++] = context->msg.slave_address;
    g_modbus_tx_buffer[context->raw_data.tx_index++] = context->msg.function_code;

    /* Copy data to the Modbus transmission buffer */
    memcpy(&g_modbus_tx_buffer[context->raw_data.tx_index], context->raw_data.tx_buffer, quantity_to_send);
    context->raw_data.tx_index += quantity_to_send;

    fsm_handle_event(fsm, MODBUS_EVENT_CALCULATE_CRC);
}

/**
 * @brief FSM action to calculate CRC of the response.
 *
 * @param fsm Pointer to the FSM instance.
 */
static void modbus_action_calculate_crc_response(fsm_t *fsm)
{
    modbus_context_t *modbus = (modbus_context_t *)fsm->user_data;

    uint16_t tx_buffer_index = modbus->raw_data.tx_index;

    /* Calculate and append the CRC */
    if (modbus->platform.crc_calc)
    {
        uint16_t local_check_sum = modbus->platform.crc_calc(g_modbus_tx_buffer, tx_buffer_index);
        g_modbus_tx_buffer[tx_buffer_index++] = GET_LOW_BYTE(local_check_sum);
        g_modbus_tx_buffer[tx_buffer_index++] = GET_HIGH_BYTE(local_check_sum);

        fsm_handle_event(fsm, MODBUS_EVENT_SEND_RESPONSE);
    }
    else
    {
        modbus->msg.error = MODBUS_ERROR_CRC;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
    }
}

/**
 * @brief FSM action to send the Modbus response.
 *
 * @param fsm Pointer to the FSM instance.
 */
static void modbus_action_send_response(fsm_t *fsm)
{
    fsm_handle_event(fsm, MODBUS_EVENT_TX_COMPLETE);
}

/**
 * @brief FSM action to handle errors.
 *
 * @param fsm Pointer to the FSM instance.
 */
static void modbus_action_handle_error(fsm_t *fsm)
{
    modbus_context_t *modbus = (modbus_context_t *)fsm->user_data;

    /* Restart UART on non-exception errors */
    if (!MODBUS_ERROR_IS_EXCEPTION(modbus->msg.error))
    {
        if (modbus->msg.error == MODBUS_ERROR_WRONG_DEVICE_ID)
        {
            modbus->raw_data.tx_index = 0;
            modbus->raw_data.rx_count = 0U;
        }
        else
        {
            if (modbus->platform.restart_uart)
            {
                modbus->platform.restart_uart();
            }
        }
    }
    else
    {
        // if is a modbus error should change the function code value to 0x83
        modbus->msg.function_code = MODBUS_FUNC_ERROR_CODE; // TODO check if is this correct
        modbus_send_error_response(modbus);
    }

    fsm_handle_event(fsm, MODBUS_EVENT_RESTART_FROM_ERROR);
}

/**
 * @brief FSM action to handle wrong baud rate error.
 *
 * @param fsm Pointer to the FSM instance.
 */
static void modbus_action_handle_wrong_baudrate(fsm_t *fsm)
{
    modbus_context_t *modbus = (modbus_context_t *)fsm->user_data;

    if (modbus->platform.change_baudrate && modbus->platform.restart_uart)
    {
        *modbus->device_info.baudrate = modbus->platform.change_baudrate(19200);
        modbus->platform.restart_uart();

        modbus_send_error_response(modbus);
    }
    fsm_handle_event(fsm, MODBUS_EVENT_RESTART_FROM_ERROR);
}

/******************************************************************************
 * FSM Guard Functions
 ******************************************************************************/

/**
 * @brief FSM guard to check if receiving is complete.
 *
 * @param fsm Pointer to the FSM instance.
 * @return True if receiving is complete, False otherwise.
 */
static inline bool modbus_guard_receive_finished(fsm_t *fsm)
{
    modbus_context_t *modbus = (modbus_context_t *)fsm->user_data;

    uint16_t rx_position_length_time;
    if (modbus->platform.measure_time_msec)
    {
        rx_position_length_time = modbus->platform.measure_time_msec(modbus->rx_reference_time);
    }
    //if (rx_position_length_time >= (MODBUS_CONVERT_CHAR_INTERVAL_TO_MS(3.5, *modbus->device_info.baudrate)))
    if(modbus->raw_data.rx_count >= 8)
    {
        if (modbus->raw_data.rx_count >= 1U && modbus->raw_data.rx_count <= 3U)
        {
            fsm_handle_event(fsm, MODBUS_EVENT_ERROR_WRONG_BAUDRATE);
        }
        else
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief FSM guard to check if transmission is complete.
 *
 * @param fsm Pointer to the FSM instance.
 * @return True if transmission is complete, False otherwise.
 */
static inline bool modbus_guard_send_finished(fsm_t *fsm)
{
    modbus_context_t *modbus = (modbus_context_t *)fsm->user_data;

    uint16_t tx_position_length_time;
    if (modbus->platform.measure_time_msec)
    {
        tx_position_length_time = modbus->platform.measure_time_msec(modbus->tx_reference_time);
    }

    if (tx_position_length_time >= (MODBUS_CONVERT_CHAR_INTERVAL_TO_MS(3.5, *modbus->device_info.baudrate)))
    {
        return true;
    }
    return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Parses read requests (Function Codes: 0x01, 0x02, 0x03, 0x04).
 * @param modbus        Modbus context.
 * @param buffer        Pointer to the buffer.
 * @param index         Pointer to the current index in the buffer.
 * @param buffer_size   Size of the buffer.
 * @return              Modbus error code.
 */
static inline modbus_error_t parse_read_request(modbus_context_t *modbus, const uint8_t *buffer, uint16_t *index, uint16_t buffer_size)
{
    uint16_t read_address, read_quantity;
    // Get read start address
    if (!read_uint16(buffer, index, buffer_size, &read_address))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    if (read_address >= MAX_ADDRESS_HOLDING_REGISTERS)
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    modbus->msg.read_address = read_address;

    // Get the number of reads
    if (!read_uint16(buffer, index, buffer_size, &read_quantity))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    if (read_quantity <= 0)
        return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    modbus->msg.read_quantity = read_quantity;

    return MODBUS_ERROR_NONE;
}

/**
 * @brief Parses write single requests (Function Codes: 0x05, 0x06).
 * @param modbus        Modbus context.
 * @param buffer        Pointer to the buffer.
 * @param index         Pointer to the current index in the buffer.
 * @param buffer_size   Size of the buffer.
 * @return              Modbus error code.
 */
static inline modbus_error_t parse_write_single_request(modbus_context_t *modbus, const uint8_t *buffer, uint16_t *index, uint16_t buffer_size)
{
    uint16_t write_address, write_value;
    // Get write start address
    if (!read_uint16(buffer, index, buffer_size, &write_address))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.write_address = write_address;

    // Get write value
    if (!read_uint16(buffer, index, buffer_size, &write_value))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.write_value = write_value;
    modbus->msg.write_quantity = 1;

    return MODBUS_ERROR_NONE;
}

/**
 * @brief Parses write multiple coils request (Function Code: 0x0F).
 * @param modbus        Modbus context.
 * @param buffer        Pointer to the buffer.
 * @param index         Pointer to the current index in the buffer.
 * @param buffer_size   Size of the buffer.
 * @return              Modbus error code.
 */
static inline modbus_error_t parse_write_multiple_coils_request(modbus_context_t *modbus, const uint8_t *buffer, uint16_t *index, uint16_t buffer_size)
{
    uint16_t write_address, write_quantity;
    uint8_t byte_count;

    // Get write start address
    if (!read_uint16(buffer, index, buffer_size, &write_address))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.write_address = write_address;

    // Get the number of writes
    if (!read_uint16(buffer, index, buffer_size, &write_quantity))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.write_quantity = write_quantity;

    // Get byte count
    if (!read_uint8(buffer, index, buffer_size, &byte_count))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.byte_count = byte_count;

    // Get write data
    if ((*index + byte_count) > buffer_size)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    memcpy(modbus->msg.buffer, &buffer[*index], byte_count);
    *index += byte_count;

    return MODBUS_ERROR_NONE;
}

/**
 * @brief Parses write multiple registers request (Function Code: 0x10).
 * @param modbus        Modbus context.
 * @param buffer        Pointer to the buffer.
 * @param index         Pointer to the current index in the buffer.
 * @param buffer_size   Size of the buffer.
 * @return              Modbus error code.
 */
static inline modbus_error_t parse_write_multiple_registers_request(modbus_context_t *modbus, const uint8_t *buffer, uint16_t *index, uint16_t buffer_size)
{
    uint16_t write_address, write_quantity;
    uint8_t byte_count;

    // Get write start address
    if (!read_uint16(buffer, index, buffer_size, &write_address))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.write_address = write_address;

    // Get the number of writes
    if (!read_uint16(buffer, index, buffer_size, &write_quantity))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.write_quantity = write_quantity;

    // Get byte count
    if (!read_uint8(buffer, index, buffer_size, &byte_count))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.byte_count = byte_count;

    // Get write data
    uint16_t expected_bytes = write_quantity * 2;
    if (byte_count != expected_bytes || (*index + byte_count) > buffer_size)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    for (uint16_t i = 0; i < write_quantity; i++)
    {
        if (!read_uint16(buffer, index, buffer_size, (uint16_t *)modbus->msg.buffer[i]))
        {
            return MODBUS_ERROR_INVALID_ARGUMENT;
        }
    }

    return MODBUS_ERROR_NONE;
}

/**
 * @brief Parses read/write multiple registers request (Function Code: 0x17).
 * @param modbus        Modbus context.
 * @param buffer        Pointer to the buffer.
 * @param index         Pointer to the current index in the buffer.
 * @param buffer_size   Size of the buffer.
 * @return              Modbus error code.
 */
static inline modbus_error_t parse_read_write_multiple_registers_request(modbus_context_t *modbus, const uint8_t *buffer, uint16_t *index, uint16_t buffer_size)
{
    uint16_t read_address, read_quantity, write_address, write_quantity;
    uint8_t byte_count;

    // Get read start address
    if (!read_uint16(buffer, index, buffer_size, &read_address))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.read_address = read_address;

    // Get the number of reads
    if (!read_uint16(buffer, index, buffer_size, &read_quantity))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.read_quantity = read_quantity;

    // Get write start address
    if (!read_uint16(buffer, index, buffer_size, &write_address))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.write_address = write_address;

    // Get the number of writes
    if (!read_uint16(buffer, index, buffer_size, &write_quantity))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.write_quantity = write_quantity;

    // Get byte count
    if (!read_uint8(buffer, index, buffer_size, &byte_count))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.byte_count = byte_count;

    // Get write data
    uint16_t expected_bytes = write_quantity * 2;
    if (byte_count != expected_bytes || (*index + byte_count) > buffer_size)
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    for (uint16_t i = 0; i < write_quantity; i++)
    {
        if (!read_uint16(buffer, index, buffer_size, (uint16_t *)modbus->msg.buffer[i]))
        {
            return MODBUS_ERROR_INVALID_ARGUMENT;
        }
    }

    return MODBUS_ERROR_NONE;
}

/**
 * @brief Parses read device information.
 * @param modbus        Modbus context.
 * @param buffer        Pointer to the buffer.
 * @param index         Pointer to the current index in the buffer.
 * @param buffer_size   Size of the buffer.
 * @return              Modbus error code.
 */
static inline modbus_error_t parse_device_info_request(modbus_context_t *modbus, const uint8_t *buffer, uint16_t *index, uint16_t buffer_size)
{
    uint8_t mei_type, device_id_code, device_obj_id;

    if (!read_uint8(buffer, index, buffer_size, &mei_type))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.mei_type = mei_type;

    if (!read_uint8(buffer, index, buffer_size, &device_id_code))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.device_id_code = device_id_code;

    if (!read_uint8(buffer, index, buffer_size, &device_obj_id))
    {
        return MODBUS_ERROR_INVALID_ARGUMENT;
    }
    modbus->msg.device_obj_id = device_obj_id;

    return MODBUS_ERROR_NONE;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Sends a Modbus response with address and quantity.
 * @param modbus        Modbus context.
 * @param tx_buffer     Transmission buffer to store the response.
 * @param tx_index      Pointer to the current index in the transmission buffer.
 * @param address       Starting address.
 * @param quantity      Quantity of registers/coils.
 */
static void send_address_quantity_response(modbus_context_t *modbus, uint8_t *tx_buffer, uint16_t *tx_index, uint16_t address, uint16_t quantity)
{
    tx_buffer[(*tx_index)++] = GET_HIGH_BYTE(address);
    tx_buffer[(*tx_index)++] = GET_LOW_BYTE(address);
    tx_buffer[(*tx_index)++] = GET_HIGH_BYTE(quantity);
    tx_buffer[(*tx_index)++] = GET_LOW_BYTE(quantity);
}

/**
 * @brief Handles Modbus read functions (Function Codes: 0x01, 0x02, 0x03, 0x04).
 * @param modbus        Modbus context.
 * @param read_func     Function pointer to read data (coils or registers).
 * @return              None.
 */
static inline void handle_read_function(modbus_context_t *modbus, bool (*read_func)(modbus_context_t *, uint16_t, uint16_t, uint8_t *, uint16_t *))
{

    // Check the effective range of the number of reads
    if ((1 <= modbus->msg.read_quantity) && (modbus->msg.read_quantity <= MODBUS_MAX_READ_WRITE_SIZE))
    {
        // Attempt to read data
        if (read_func(modbus, modbus->msg.read_address, modbus->msg.read_quantity, modbus->raw_data.tx_buffer, &modbus->raw_data.tx_index))
        {
            // Send Modbus frame
            modbus->msg.error = MODBUS_ERROR_NONE;
        }
    }
    else
    {
        modbus->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }
}

/**
 * @brief Handles Modbus write single functions (Function Codes: 0x05, 0x06).
 * @param modbus        Modbus context.
 * @param write_func    Function pointer to write data (coil or register).
 * @return              None.
 */
static void handle_write_single_function(modbus_context_t *modbus, bool (*write_func)(modbus_context_t *, uint16_t, uint16_t))
{
    uint16_t address = modbus->msg.write_address;
    uint16_t value = modbus->msg.write_value;
    modbus->raw_data.tx_index = 0;

    // Attempt to write data
    if (write_func(modbus, address, value))
    {
        // Create and send response frame if not broadcast
        if (modbus->msg.slave_address != MODBUS_BROADCAST_ADDRESS)
        {
            send_address_quantity_response(modbus, modbus->raw_data.tx_buffer, &modbus->raw_data.tx_index, address, value);
            modbus->msg.error = MODBUS_ERROR_NONE;
        }
    }
}

/**
 * @brief Handles Modbus write multiple functions (Function Codes: 0x0F, 0x10).
 * @param modbus        Modbus context.
 * @param write_func    Function pointer to write multiple data (coils or registers).
 * @return              None.
 */
static void handle_write_multiple_function(modbus_context_t *modbus, bool (*write_func)(modbus_context_t *, uint16_t, uint16_t))
{
    uint16_t start_address = modbus->msg.write_address;
    uint16_t quantity = modbus->msg.write_quantity;
    modbus->raw_data.tx_index = 0;

    // Check the effective range of the number of writes
    if ((1 <= quantity) && (quantity <= MODBUS_MAX_READ_WRITE_SIZE))
    {
        // Attempt to write data
        if (write_func(modbus, start_address, quantity))
        {
            // Create and send response frame if not broadcast
            if (modbus->msg.slave_address != MODBUS_BROADCAST_ADDRESS)
            {
                send_address_quantity_response(modbus, modbus->raw_data.tx_buffer, &modbus->raw_data.tx_index, start_address, quantity);
            }
        }
    }
    else
    {
        modbus->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE;
    }
}

/**
 * @brief Reads multiple registers and stores the data in the transmission buffer.
 * @param modbus        Modbus context.
 * @param start_address Starting address to read from.
 * @param quantity      Number of registers to read.
 * @param tx_buffer     Transmission buffer to store the read data.
 * @param tx_index      Pointer to the current index in the transmission buffer.
 * @return              True if successful, false if error occurred.
 */
static inline bool read_registers(modbus_context_t *modbus, uint16_t start_address, uint16_t quantity, uint8_t *tx_buffer, uint16_t *tx_index)
{
    if ((start_address + quantity) > MAX_ADDRESS_HOLDING_REGISTERS)
    {
        modbus->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        return false;
    }

    if (modbus->msg.current_read_index == 0)
    {
        *tx_index = 0;
        tx_buffer[(*tx_index)++] = (uint8_t)(quantity * 2); // Byte count
    }

    uint16_t value;
    uint16_t register_address = start_address + modbus->msg.current_read_index;
    int address_id = binary_search(g_holding_register, 0, g_holding_register_quantity, register_address);
    if(address_id == -1) {
        modbus->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        return false;
    }

    if (g_holding_register[address_id].read_callback != NULL)
    {
        value = g_holding_register[address_id].read_callback();
    }
    else
    {
        value = *g_holding_register[address_id].variable_ptr;
    }

    tx_buffer[(*tx_index)++] = GET_HIGH_BYTE(value);
    tx_buffer[(*tx_index)++] = GET_LOW_BYTE(value);
    modbus->msg.current_read_index++;

    return true;
}

/**
 * @brief Writes a single holding register.
 * @param modbus        Modbus context.
 * @param address       Register address to write to.
 * @param value         Value to write.
 * @return              True if successful, false if error occurred.
 */
static bool write_single_register(modbus_context_t *modbus, uint16_t address, uint16_t value)
{
    if (address >= MAX_ADDRESS_HOLDING_REGISTERS)
    {
        modbus->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        return false;
    }

    int address_id = binary_search(g_holding_register, 0, g_holding_register_quantity - 1, address);
    if (address_id == -1)
    {
        modbus->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        return false;
    }

    if (!g_holding_register[address_id].read_only)
    {
        if (g_holding_register[address_id].write_callback != NULL)
        {
            *g_holding_register[address_id].variable_ptr = g_holding_register[address_id].write_callback(value);
        }
        else
        {
            *g_holding_register[address_id].variable_ptr = value;
        }
        return true;
    }
    else
    {
        modbus->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        return false;
    }
}

/**
 * @brief Writes multiple registers with the data from the Modbus message buffer.
 * @param modbus        Modbus context.
 * @param start_address Starting address to write to.
 * @param quantity      Number of registers to write.
 * @return              True if successful, false if error occurred.
 */
static inline bool write_registers(modbus_context_t *modbus, uint16_t start_address, uint16_t quantity)
{
    if ((start_address + quantity) > MAX_ADDRESS_HOLDING_REGISTERS)
    {
        modbus->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        uint16_t index = start_address + i;
        // TODO check if this is the better way to write more than one register
        int address_id = binary_search(g_holding_register, 0, g_holding_register_quantity - 1, index);
        if (address_id == -1)
        {
            modbus->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
            return false;
        }

        if (!g_holding_register[address_id].read_only)
        {
            uint16_t data = modbus->msg.buffer[i];
            if (g_holding_register[address_id].write_callback != NULL)
            {
                *g_holding_register[address_id].variable_ptr = g_holding_register[address_id].write_callback(data);
            }
            else
            {
                *g_holding_register[address_id].variable_ptr = data;
            }
        }
    }

    return true;
}

/**
 * @brief Put data related to device information into TX buffer.
 * @param modbus        Modbus instance.
 */
static void handle_read_device_information(modbus_context_t *modbus)
{
    modbus->raw_data.tx_index = 0;
    modbus->raw_data.tx_buffer[modbus->raw_data.tx_index++] = modbus->msg.mei_type;
    modbus->raw_data.tx_buffer[modbus->raw_data.tx_index++] = modbus->msg.device_id_code;
    modbus->raw_data.tx_buffer[modbus->raw_data.tx_index++] = modbus->device_info.conformity_level;
    modbus->raw_data.tx_buffer[modbus->raw_data.tx_index++] = 0; // More package that will follow, just this will be sent
    modbus->raw_data.tx_buffer[modbus->raw_data.tx_index++] = 0; // Next object id
    modbus->raw_data.tx_buffer[modbus->raw_data.tx_index++] = modbus->device_info.info_saved;
    // copy all infos to tx buffer
    for (size_t i = 0; i < modbus->device_info.info_saved; i++)
    {
        modbus->raw_data.tx_buffer[modbus->raw_data.tx_index++] = modbus->device_info.data[i].id;
        modbus->raw_data.tx_buffer[modbus->raw_data.tx_index++] = modbus->device_info.data[i].lenght;
        memcpy(&modbus->raw_data.tx_buffer[modbus->raw_data.tx_index], modbus->device_info.data[i].value_in_ascii, modbus->device_info.data[i].lenght);
        modbus->raw_data.tx_index += modbus->device_info.data[i].lenght;
    }
}

/**
 * @brief Handles parsed Modbus requests based on the function code.
 * @param modbus        Modbus instance.
 */
void modbus_handle_function(modbus_context_t *modbus)
{
    // Reset error
    modbus->msg.error = MODBUS_ERROR_NONE;

    switch (modbus->msg.function_code)
    {
    case MODBUS_FUNC_READ_HOLDING_REGISTERS:
    case MODBUS_FUNC_READ_INPUT_REGISTERS:
        handle_read_function(modbus, read_registers);
        break;

    case MODBUS_FUNC_WRITE_SINGLE_REGISTER:
        handle_write_single_function(modbus, write_single_register);
        break;

    case MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS:
        handle_write_multiple_function(modbus, write_registers);
        break;

    case MODBUS_FUNC_READ_WRITE_MULTIPLE_REGISTERS:
        // Handle read/write multiple registers separately
        // First, write the registers
        if (write_registers(modbus, modbus->msg.write_address, modbus->msg.write_quantity))
        {
            // Then, read the registers
            // uint16_t tx_buffer_index = 0;

            if (read_registers(modbus, modbus->msg.read_address, modbus->msg.read_quantity, modbus->raw_data.tx_buffer, &modbus->raw_data.tx_index))
            {
                // Send Modbus frame
                // modbus_send(modbus, modbus_tx_buffer, tx_buffer_index);
            }
        }
        break;
    case MODBUS_FUNC_READ_DEVICE_INFORMATION:
        handle_read_device_information(modbus);
        break;

    default:
        // Function not implemented
        modbus->msg.error = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
        break;
    }

    // Handle error response if any
    if (modbus->msg.error != MODBUS_ERROR_NONE)
    {
        // Send Modbus exception response
        // modbus_send_exception(modbus, modbus->msg.function_code, modbus->msg.error);
        fsm_handle_event(&modbus->fsm, MODBUS_EVENT_ERROR_DETECTED);
    }
}