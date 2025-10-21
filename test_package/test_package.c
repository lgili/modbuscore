#include <modbuscore/protocol/pdu.h>
#include <stdio.h>

int main(void) {
    printf("ModbusCore Conan package test\n");
    printf("Testing basic include and link...\n");
    
    // Basic PDU test
    mbc_pdu_t pdu = {0};
    mbc_status_t status = mbc_pdu_build_read_holding_request(&pdu, 1, 0, 10);
    
    if (status == MBC_STATUS_OK && pdu.function == 0x03) {
        printf("✓ ModbusCore package is correctly installed!\n");
        return 0;
    }
    
    printf("✗ PDU builder failed!\n");
    return 1;
}
