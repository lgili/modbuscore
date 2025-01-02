/**
 * @file modbus_server.c
 * @brief Modbus Server (Slave) state machine and logic (versão final).
 *
 * Este arquivo implementa a lógica de um servidor (slave) Modbus utilizando a FSM,
 * integrando com o núcleo do protocolo (modbus_core) e funções utilitárias.  
 *
 * Funcionalidades:
 * - Recebe bytes e monta frames RTU (FSM controla estados).
 * - Faz parsing da requisição (ex: função 0x03 para ler holding registers, 0x06 para escrever um único registro, 0x2B para informações do dispositivo).
 * - Chama callbacks de leitura/escrita de variáveis cadastradas via `modbus_set_holding_register()`.
 * - Constrói e envia respostas ou exceções (se não for broadcast).
 * - Não usa alocação dinâmica.
 *
 * Requer que o `modbus_context_t` possua um campo `modbus_server_data_t server_data;` para armazenar dados do servidor.
 * O usuário deve:
 *  - Chamar `modbus_server_create()`.
 *  - Registrar registradores com `modbus_set_holding_register()`/`modbus_set_array_holding_register()`.
 *  - Chamar `modbus_server_poll()` periodicamente.
 *  - Quando um byte chega, chamar `modbus_server_receive_data_from_uart_event()`.
 *
 * Autor:  
 * Data: 2024-12-20
 */

#include <modbus/transport.h>
#include <modbus/server.h>
#include <modbus/core.h>
#include <modbus/utils.h>
#include <modbus/fsm.h>
#include <string.h>


/**************************************************************************** */
modbus_server_data_t g_server;
static int build_error_count = 0;
static bool need_update_baudrate = false;

/* Prototipos internos */
static void reset_message(modbus_server_data_t *server);
static modbus_error_t parse_request(modbus_server_data_t *server);
static void handle_function(modbus_server_data_t *server);


/* Funções parse específicas */
static modbus_error_t parse_read_holding_registers(modbus_server_data_t *server, const uint8_t *buf, uint16_t *idx, uint16_t size);
static modbus_error_t parse_write_single_register(modbus_server_data_t *server, const uint8_t *buf, uint16_t *idx, uint16_t size);
static modbus_error_t parse_device_info_request(modbus_server_data_t *server, const uint8_t *buf, uint16_t *idx, uint16_t size);

/* Funções de ação da FSM */
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

/* Guards */
static bool guard_receive_finished(fsm_t *fsm);
static bool guard_send_finished(fsm_t *fsm);

/* Funções de acesso a registradores */
static int find_register(modbus_server_data_t *server, uint16_t address);

/* Estados da FSM */
static const fsm_transition_t state_idle_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_RX_BYTE_RECEIVED, modbus_server_state_receiving, action_start_receiving, NULL)
};
const fsm_state_t modbus_server_state_idle = FSM_STATE("IDLE", MODBUS_SERVER_STATE_IDLE, state_idle_transitions, action_idle);

static const fsm_transition_t state_receiving_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_RX_BYTE_RECEIVED, modbus_server_state_receiving, action_start_receiving, NULL),
    FSM_TRANSITION(MODBUS_EVENT_PARSE_ADDRESS, modbus_server_state_parsing_address, action_parse_address, guard_receive_finished),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_WRONG_BAUDRATE, modbus_server_state_error, action_handle_wrong_baudrate, NULL)
};
const fsm_state_t modbus_server_state_receiving = FSM_STATE("RECEIVING", MODBUS_SERVER_STATE_RECEIVING, state_receiving_transitions, action_start_receiving);

static const fsm_transition_t state_parsing_address_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_PARSE_FUNCTION, modbus_server_state_parsing_function, action_parse_function, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_parsing_address = FSM_STATE("PARSING_ADDRESS", MODBUS_SERVER_STATE_PARSING_ADDRESS, state_parsing_address_transitions, NULL);

static const fsm_transition_t state_parsing_function_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_PROCESS_FRAME, modbus_server_state_processing, action_process_frame, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_parsing_function = FSM_STATE("PARSING_FUNCTION", MODBUS_SERVER_STATE_PARSING_FUNCTION, state_parsing_function_transitions, NULL);

static const fsm_transition_t state_processing_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_VALIDATE_FRAME, modbus_server_state_validating_frame, action_validate_frame, NULL),
    FSM_TRANSITION(MODBUS_EVENT_BOOTLOADER, modbus_server_state_sending, action_send_response, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_processing = FSM_STATE("PROCESSING", MODBUS_SERVER_STATE_PROCESSING, state_processing_transitions, NULL);

static const fsm_transition_t state_validating_frame_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_BUILD_RESPONSE, modbus_server_state_building_response, action_build_response, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_validating_frame = FSM_STATE("VALIDATING_FRAME", MODBUS_SERVER_STATE_VALIDATING_FRAME, state_validating_frame_transitions, NULL);

static const fsm_transition_t state_building_response_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_PUT_DATA_ON_BUFFER, modbus_server_state_putting_data_on_buffer, action_put_data_on_buffer, NULL),
    FSM_TRANSITION(MODBUS_EVENT_BROADCAST_DONT_ANSWER, modbus_server_state_idle, NULL, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_building_response = FSM_STATE("BUILDING_RESPONSE", MODBUS_SERVER_STATE_BUILDING_RESPONSE, state_building_response_transitions, NULL);

static const fsm_transition_t state_putting_data_on_buffer_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_CALCULATE_CRC, modbus_server_state_calculating_crc, action_calculate_crc_response, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_putting_data_on_buffer = FSM_STATE("PUTTING_DATA_ON_BUF", MODBUS_SERVER_STATE_PUTTING_DATA_ON_BUF, state_putting_data_on_buffer_transitions, NULL);

static const fsm_transition_t state_calculating_crc_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_SEND_RESPONSE, modbus_server_state_sending, action_send_response, NULL),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_calculating_crc = FSM_STATE("CALCULATING_CRC", MODBUS_SERVER_STATE_CALCULATING_CRC, state_calculating_crc_transitions, NULL);

static const fsm_transition_t state_sending_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_TX_COMPLETE, modbus_server_state_idle, NULL, guard_send_finished),
    FSM_TRANSITION(MODBUS_EVENT_ERROR_DETECTED, modbus_server_state_error, action_handle_error, NULL)
};
const fsm_state_t modbus_server_state_sending = FSM_STATE("SENDING", MODBUS_SERVER_STATE_SENDING, state_sending_transitions, action_send_response);

static const fsm_transition_t state_error_transitions[] = {
    FSM_TRANSITION(MODBUS_EVENT_RX_BYTE_RECEIVED, modbus_server_state_idle, NULL, NULL),
    FSM_TRANSITION(MODBUS_EVENT_RESTART_FROM_ERROR, modbus_server_state_idle, NULL, NULL)
};
const fsm_state_t modbus_server_state_error = FSM_STATE("ERROR", MODBUS_SERVER_STATE_ERROR, state_error_transitions, NULL);


int16_t update_baudrate(uint16_t baud) {
    need_update_baudrate = true;
    return baud;
}

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

    // Obter ponteiro para dados do servidor (estático dentro do contexto)
    
    // modbus_server_data_t *server = (modbus_server_data_t *)&modbus->user_data;
    modbus->user_data = &g_server;
    g_server.ctx = modbus;
    modbus_reset_buffers(modbus);

    // put register address array in order
    // modbus_selection_sort(server->holding_registers, server->holding_register_count);

    g_server.device_info.address = device_address;
    g_server.device_info.baudrate = baudrate;
    modbus->transport = *platform_conf;

    modbus->rx_reference_time = modbus->transport.get_reference_msec();
    modbus->tx_reference_time = modbus->transport.get_reference_msec();
    modbus->error_timer = modbus->transport.get_reference_msec();
    // g_time_to_start_modbus = modbus->transport.get_reference_msec();

    fsm_init(&g_server.fsm, &modbus_server_state_idle, &g_server);
    modbus->role = MODBUS_ROLE_SLAVE;

    // fixed value for now!!!
    g_server.device_info.conformity_level = 0x81;

    return MODBUS_ERROR_NONE;
}


void modbus_server_poll(modbus_context_t *ctx) {
    if (!ctx) return;
    modbus_server_data_t *server = (modbus_server_data_t *)ctx->user_data;
    fsm_run(&server->fsm);
}

void modbus_server_receive_data_from_uart_event(fsm_t *fsm, uint8_t data) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    ctx->rx_reference_time = ctx->transport.get_reference_msec();

    if (ctx->rx_count < sizeof(ctx->rx_buffer)) {
        ctx->rx_buffer[ctx->rx_count++] = data;
    } else {
        // Overflow
        server->msg.error = MODBUS_ERROR_INVALID_REQUEST;
        fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        return;
    }

    if (fsm->current_state->id != MODBUS_SERVER_STATE_RECEIVING) {
        fsm_handle_event(fsm, MODBUS_EVENT_RX_BYTE_RECEIVED);
    }
}

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

    // Reordenar pelo endereço para busca binária
    modbus_selection_sort(server->holding_registers, server->holding_register_count);

    return MODBUS_ERROR_NONE;
}

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

    modbus_selection_sort(server->holding_registers, server->holding_register_count);

    return MODBUS_ERROR_NONE;
}

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

/* -------------------- Ações da FSM -------------------- */

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
    // set_mode_as_reciver(modbus);
}

static void action_start_receiving(fsm_t *fsm) {
    fsm_handle_event(fsm, MODBUS_EVENT_PARSE_ADDRESS);
}

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

static void action_process_frame(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_error_t err = parse_request(server);
    if (err != MODBUS_ERROR_NONE) {
        server->msg.error = err;
        if (err == MODBUS_OTHERS_REQUESTS) {
            fsm_handle_event(fsm, MODBUS_EVENT_BOOTLOADER);
        } else {
            fsm_handle_event(fsm, MODBUS_EVENT_ERROR_DETECTED);
        }
        return;
    }
    fsm_handle_event(fsm, MODBUS_EVENT_VALIDATE_FRAME);
}

static void action_validate_frame(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    // Verificar CRC
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

static void action_build_response(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;
    handle_function(server);

    if(server->msg.error != MODBUS_ERROR_NONE) {
        return;
    }

    if((server->msg.current_read_index >=  server->msg.read_quantity)
       || (server->msg.write_quantity >= 1)
       || (server->msg.mei_type != 0))
    {
        if(server->msg.broadcast == true) {
            fsm_handle_event(fsm, MODBUS_EVENT_BROADCAST_DONT_ANSWER);
            // server->raw_data.tx_index = 0;
            // server->raw_data.rx_count = 0U;
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

static void action_put_data_on_buffer(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    uint16_t quantity_to_send = 0;
    if(server->msg.function_code == MODBUS_FUNC_READ_COILS) {
        quantity_to_send = server->msg.read_quantity + 1;
    }
    else if(server->msg.function_code <= MODBUS_FUNC_READ_INPUT_REGISTERS) {
        quantity_to_send = server->msg.read_quantity*2 + 1;
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

static void action_calculate_crc_response(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    uint16_t crc = modbus_crc_with_table(ctx->tx_buffer, ctx->tx_index);
    ctx->tx_buffer[ctx->tx_index++] = GET_LOW_BYTE(crc);
    ctx->tx_buffer[ctx->tx_index++] = GET_HIGH_BYTE(crc);

    fsm_handle_event(fsm, MODBUS_EVENT_SEND_RESPONSE);
}

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

static void action_handle_error(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    if (modbus_error_is_exception(server->msg.error) && !server->msg.broadcast) {
        // Enviar frame de exceção
        uint8_t exception_function = server->msg.function_code | 0x80;
        uint8_t exception_code = server->msg.error;
        uint8_t frame[5];
        uint16_t len = modbus_build_rtu_frame(server->msg.slave_address, exception_function,
                                              &exception_code, 1, frame, sizeof(frame));
        if (len > 0) {
            modbus_send_frame(ctx, frame, len);
        }
    } else {
        // Erro interno, reiniciar UART se existir
        if (ctx->transport.restart_uart) {
            ctx->transport.restart_uart();
        }
    }
    fsm_handle_event(fsm, MODBUS_EVENT_RESTART_FROM_ERROR);
}

static void action_handle_wrong_baudrate(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    if (ctx->transport.change_baudrate && ctx->transport.restart_uart) {
        *server->device_info.baudrate = ctx->transport.change_baudrate(19200);
        ctx->transport.restart_uart();
    }
    fsm_handle_event(fsm, MODBUS_EVENT_RESTART_FROM_ERROR);
}

/* ------------------- Guards ------------------- */
static bool guard_receive_finished(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;

    uint16_t rx_position_length_time;
    rx_position_length_time = ctx->transport.measure_time_msec(ctx->rx_reference_time);
    if (rx_position_length_time >= (MODBUS_CONVERT_CHAR_INTERVAL_TO_MS(3.5, *server->device_info.baudrate)))
    {
        if (ctx->rx_count >= 1U && ctx->rx_count <= 3U)
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

static bool guard_send_finished(fsm_t *fsm) {
    modbus_server_data_t *server = (modbus_server_data_t *)fsm->user_data;
    modbus_context_t *ctx = server->ctx;


    uint16_t tx_position_length_time;
    tx_position_length_time = ctx->transport.measure_time_msec(ctx->tx_reference_time);
    if (tx_position_length_time >= (MODBUS_CONVERT_CHAR_INTERVAL_TO_MS(3.5, *server->device_info.baudrate)))
    {
        return true;
    }
    return false;
}

/* ------------------- Funções auxiliares ------------------- */

static void reset_message(modbus_server_data_t *server) {
    memset(&server->msg, 0, sizeof(server->msg));
    server->ctx->rx_index = 0;
    // modbus_reset_buffers(server->ctx);
}

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
            // Função não suportada
            server->msg.error = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
            return MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
    }
}

/* Funções de parsing específicas */
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

static modbus_error_t parse_write_single_register(modbus_server_data_t *server, const uint8_t *buf, uint16_t *idx, uint16_t size) {
    uint16_t addr, value;
    if (!modbus_read_uint16(buf, idx, size, &addr)) return MODBUS_ERROR_INVALID_ARGUMENT;
    if (!modbus_read_uint16(buf, idx, size, &value)) return MODBUS_ERROR_INVALID_ARGUMENT;

    if (addr > MAX_ADDRESS_HOLDING_REGISTERS) return MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    server->msg.write_address = addr;
    server->msg.write_value = (int16_t)value;

    // Tentar escrever já no parse pode ser opcional; aqui fazemos no handle_function().
    return MODBUS_ERROR_NONE;
}

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



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Acesso a registradores */

static int find_register(modbus_server_data_t *server, uint16_t address) {
    return modbus_binary_search(server->holding_registers, 0, (uint16_t)(server->holding_register_count - 1), address);
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
    // TODO check if is needed to retunr error on address that dont exist
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
    server->msg.current_read_index ++;

    return true;
}

/**
 * @brief Writes a single holding register.
 * @param modbus        Modbus context.
 * @param address       Register address to write to.
 * @param value         Value to write.
 * @return              True if successful, false if error occurred.
 */
static bool write_single_register(modbus_server_data_t *server, uint16_t address, uint16_t value)
{
    if (address >= MAX_ADDRESS_HOLDING_REGISTERS)
    {
        server->msg.error = MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
        return false;
    }

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
 * @brief Writes multiple registers with the data from the Modbus message buffer.
 * @param modbus        Modbus context.
 * @param start_address Starting address to write to.
 * @param quantity      Number of registers to write.
 * @return              True if successful, false if error occurred.
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
        // TODO check if this is the better way to write more than one register
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
 * @param tx_buffer     Transmission buffer to store the response.
 * @param tx_index      Pointer to the current index in the transmission buffer.
 * @param address       Starting address.
 * @param quantity      Quantity of registers/coils.
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
 * @param modbus        Modbus context.
 * @param read_func     Function pointer to read data (coils or registers).
 * @return              None.
 */
static inline void handle_read_function(modbus_server_data_t *server, bool (*read_func)(modbus_server_data_t *, uint16_t, uint16_t, uint8_t *, uint16_t *))
{
    modbus_context_t *ctx = server->ctx;
    // Check the effective range of the number of reads
    if ((1 <=  server->msg.read_quantity) && ( server->msg.read_quantity <= MODBUS_MAX_READ_WRITE_SIZE))
    {
        // Attempt to read data
        if (read_func(server, server->msg.read_address,  server->msg.read_quantity, ctx->tx_raw_buffer, &ctx->tx_raw_index))
        {
            // Send Modbus frame
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
 * @param modbus        Modbus context.
 * @param write_func    Function pointer to write data (coil or register).
 * @return              None.
 */
static void handle_write_single_function(modbus_server_data_t *server, bool (*write_func)(modbus_server_data_t *, uint16_t, uint16_t))
{
    modbus_context_t *ctx = server->ctx;
    uint16_t address = server->msg.write_address;
    uint16_t value = server->msg.write_value;
    ctx->tx_raw_index = 0;

    // Attempt to write data
    if (write_func(server, address, value))
    {
        // Create and send response frame if not broadcast
        if (server->msg.slave_address != MODBUS_BROADCAST_ADDRESS)
        {
            send_address_quantity_response(ctx->tx_raw_buffer, &ctx->tx_raw_index, address, value);
            server->msg.error = MODBUS_ERROR_NONE;
        }
    }
}


/**
 * @brief Handles Modbus write multiple functions (Function Codes: 0x0F, 0x10).
 * @param modbus        Modbus context.
 * @param write_func    Function pointer to write multiple data (coils or registers).
 * @return              None.
 */
static void handle_write_multiple_function(modbus_server_data_t *server, bool (*write_func)(modbus_server_data_t *, uint16_t, uint16_t))
{
    modbus_context_t *ctx = server->ctx;
    uint16_t start_address = server->msg.write_address;
    uint16_t quantity = server->msg.write_quantity;
    ctx->tx_raw_index = 0;

    // Check the effective range of the number of writes
    if ((1 <= quantity) && (quantity <= MODBUS_MAX_READ_WRITE_SIZE))
    {
        // Attempt to write data
        if (write_func(server, start_address, quantity))
        {
            // Create and send response frame if not broadcast
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
 * @brief Put data related to device information into TX buffer.
 * @param modbus        Modbus instance.
 */
static void handle_read_device_information(modbus_server_data_t *server) {
    modbus_context_t *ctx = server->ctx;
    ctx->tx_raw_index = 0;
    ctx->tx_raw_buffer[ctx->tx_raw_index++] = server->msg.mei_type;
    ctx->tx_raw_buffer[ctx->tx_raw_index++] = server->msg.device_id_code;
    ctx->tx_raw_buffer[ctx->tx_raw_index++] = server->device_info.conformity_level;
    ctx->tx_raw_buffer[ctx->tx_raw_index++] = 0; // More package that will follow, just this will be sent
    ctx->tx_raw_buffer[ctx->tx_raw_index++] = 0; // Next object id
    ctx->tx_raw_buffer[ctx->tx_raw_index++] = server->device_info.info_saved;
    // copy all infos to tx buffer
    for (size_t i = 0; i < server->device_info.info_saved; i++)
    {
        ctx->tx_raw_buffer[ctx->tx_raw_index++] = server->device_info.data[i].id;
        ctx->tx_raw_buffer[ctx->tx_raw_index++] = server->device_info.data[i].length;
        memcpy(&ctx->tx_raw_buffer[ctx->tx_raw_index], server->device_info.data[i].value_in_ascii, server->device_info.data[i].length);
        ctx->tx_raw_index += server->device_info.data[i].length;        
    }
}

/**
 * @brief Handles parsed Modbus requests based on the function code.
 * @param modbus        Modbus instance.
 */
static void handle_function(modbus_server_data_t *server)
{
    // Reset error
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
        // Handle read/write multiple registers separately
        // First, write the registers
        modbus_context_t *ctx = server->ctx;
        if (write_registers(server, server->msg.write_address, server->msg.write_quantity))
        {
            // Then, read the registers
            //uint16_t tx_buffer_index = 0;

            if (read_registers(server, server->msg.read_address, server->msg.read_quantity, ctx->tx_raw_buffer, &ctx->tx_raw_index))
            {
                // Send Modbus frame
            }
        }
        break;
    case MODBUS_FUNC_READ_DEVICE_INFORMATION:
        handle_read_device_information(server);
        break;

    default:
        // Function not implemented
        server->msg.error = MODBUS_EXCEPTION_ILLEGAL_FUNCTION;
        break;
    }

    // Handle error response if any
    if (server->msg.error != MODBUS_ERROR_NONE)
    {
        // Send Modbus exception response
        fsm_handle_event(&server->fsm, MODBUS_EVENT_ERROR_DETECTED);
    }
}

