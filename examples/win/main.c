#include <stdio.h>

#include "modbus.h"

int32_t read_serial(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    //Serial.setTimeout(byte_timeout_ms);
    return 0;
}


int32_t write_serial(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg) {
    //Serial.setTimeout(byte_timeout_ms);
    return 0;
}

int main(){
    printf("Hello dude, from modbus!\n");

    modbus_platform_conf platform_conf;
    modbus_platform_conf_create(&platform_conf);
    platform_conf.transport = NMBS_TRANSPORT_RTU;
    platform_conf.read = read_serial;
    platform_conf.write = write_serial;

    return 0;
}
