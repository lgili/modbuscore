#include <stdio.h>
#include <time.h>

#include <modbus/modbus_server.h>
#include "modbus/modbus.h"

int32_t read_serial(uint8_t *buf, uint16_t count)
{
    // Serial.setTimeout(byte_timeout_ms);
    return 0;
}

int32_t write_serial(const uint8_t *buf, uint16_t count)
{
    // Serial.setTimeout(byte_timeout_ms);
    return 0;

    /* Send the first byte via UART */
    // SMR00 |= _0001_SAU_BUFFER_EMPTY;
    // STMK0 = 1U; /* Disable INTST0 interrupt */
    // TXD0 = *modbus->raw_data.tx_ptr++;
    // modbus->raw_data.tx_count--;
    // STMK0 = 0U; /* Enable INTST0 interrupt */
}

uint16_t get_time()
{
    return (uint16_t)clock();
}

uint16_t measure_time(uint16_t time)
{
    uint16_t now = (uint16_t)clock();
    return (uint16_t)(now - time);
}

uint16_t change_baud(uint16_t baud)
{
    return 19200;
}

void restart_uart()
{
    //
}

uint8_t write_gpio(uint8_t gpio, uint8_t value)
{
    //

    return 0;
}

uint8_t parse_bootloader_request(uint8_t *buffer, uint16_t *buffer_size)
{
    return 0;
}

int16_t enable_motor;

modbus_context_t g_modbus;
static int16_t g_baudrate = 19200;
int16_t g_modbus_address = 0x1;

uint8_t data_rec[8] = {0x1, 0x3, 0x0, 0x8, 0x0, 0x1, 0x5, 0xC8};
uint8_t count_rec = 0;

int main()
{
    printf("Hello dude, from modbus!\n");

    modbus_platform_conf platform_conf;
    modbus_platform_conf_create(&platform_conf);
    platform_conf.transport = NMBS_TRANSPORT_RTU;
    platform_conf.read = read_serial;
    platform_conf.write = write_serial;
    platform_conf.get_reference_msec = get_time;
    platform_conf.measure_time_msec = measure_time;
    platform_conf.change_baudrate = change_baud;
    platform_conf.restart_uart = restart_uart;
    platform_conf.write_gpio = write_gpio;

    modbus_error_t err = modbus_set_holding_register(8, &enable_motor, false, NULL, NULL); // RW
    if (err != MODBUS_ERROR_NONE)
        return 0;

    // Create the modbus server
    err = modbus_server_create(&g_modbus, &platform_conf, &g_modbus_address, &g_baudrate);
    if (err != MODBUS_ERROR_NONE)
    {
        printf("error \n");
    }
    else
    {
        // if modbus was initialized correctelly, initialize serial interface
        err = add_info_to_device(&g_modbus, "SECOP", 5); // Vendor
        if (err != MODBUS_ERROR_NONE)
            return 0;
        err = add_info_to_device(&g_modbus, "105N4700", 8); // product code
        if (err != MODBUS_ERROR_NONE)
            return 0;
        err = add_info_to_device(&g_modbus, "00.62", 5); // SW version
        if (err != MODBUS_ERROR_NONE)
            return 0;

        // modbus_serial_init_uart();
    }

    while (1)
    {
        if ((g_modbus.fsm.current_state->id == MODBUS_STATE_IDLE || g_modbus.fsm.current_state->id == MODBUS_STATE_RECEIVING) && count_rec <= 8)
        {
            modbus_server_receive_data_from_uart_event(&g_modbus.fsm, data_rec[count_rec++]);
        }
        modbus_server_poll(&g_modbus);
    }

    return 0;
}
