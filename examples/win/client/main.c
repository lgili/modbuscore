#include <stdio.h>

#include <modbus/modbus.h>

int32_t read_serial(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    //Serial.setTimeout(byte_timeout_ms);
    return 0;
}


int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    //Serial.setTimeout(byte_timeout_ms);
    return 0;
}

uint16_t get_time() {
    return 0;
}

uint16_t measure_time(uint16_t time) {
    return 0;
}

int16_t enable_motor;

modbus_context_t g_modbus;
static int16_t g_baudrate = 19200;

int main(){
    printf("Hello XxX, from modbus!\n");


    modbus_platform_conf platform_conf;
    modbus_platform_conf_create(&platform_conf);
    platform_conf.transport = NMBS_TRANSPORT_RTU;
    platform_conf.read = read_serial;
    platform_conf.write = write_serial;
    platform_conf.get_reference_msec = get_time;
    platform_conf.measure_time_msec = measure_time;


    // Create the modbus server
    modbus_error_t err = modbus_client_create(&g_modbus, platform_conf, &g_baudrate);
    if (err != MODBUS_ERROR_NONE) {
        printf("error \n");
    } else {
		       
		//modbus_serial_init_uart();
	}
}


