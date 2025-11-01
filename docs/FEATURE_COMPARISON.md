# Feature Comparison: ModbusCore vs OldModbus

## Function Code Support

### Current ModbusCore (NEW)
Supports only **3 function codes**:
- ✅ FC03: Read Holding Registers
- ✅ FC06: Write Single Register
- ✅ FC16: Write Multiple Registers

### OldModbus (REFERENCE)
Supports **13 function codes**:

#### Standard Function Codes
- ✅ FC01: Read Coils
- ✅ FC02: Read Discrete Inputs
- ✅ FC03: Read Holding Registers
- ✅ FC04: Read Input Registers
- ✅ FC05: Write Single Coil
- ✅ FC06: Write Single Register
- ✅ FC0F: Write Multiple Coils
- ✅ FC10: Write Multiple Registers
- ✅ FC17: Read/Write Multiple Registers
- ✅ FC2B: Read Device Information (Device Identification)

#### Custom Function Codes
- ✅ FC41: Special Broadcast Command (Group Control)
- ✅ FC42: Special Broadcast Command (Group Control)
- ✅ FC43: Broadcast Address Commissioning

## Register Type Support

### Current ModbusCore
- ✅ Holding Registers (FC03, FC06, FC16)
- ❌ Input Registers (not implemented)
- ❌ Coils (not implemented)
- ❌ Discrete Inputs (not implemented)

### OldModbus
- ✅ Holding Registers
- ✅ Input Registers (read-only)
- ✅ Coils (optional)
- ✅ Discrete Inputs (optional)

## Device Identification Support

### Current ModbusCore
- ❌ FC2B not implemented
- ❌ No device info structure

### OldModbus
- ✅ FC2B: Read Device Information
- ✅ Device package structure
- ✅ MEI type support
- ✅ Conformity level
- ✅ Multiple device info objects (vendor name, product code, etc.)

**Location:** `modbus_server_nidec.h:109-130`
```c
typedef struct {
    uint8_t id;
    uint8_t length;
    uint8_t value_in_ascii[MAX_DEVICE_PACKAGE_VALUES];
} device_package_info_t;

typedef struct {
    uint16_t *address;
    uint16_t *baudrate;
    uint8_t mei_type;
    uint8_t conformity_level;
    uint8_t more_follow;
    uint8_t next_obj_id;
    device_package_info_t data[MAX_DEVICE_PACKAGES];
    uint8_t info_saved;
} device_identification_t;
```

## MCU-Specific Features

### Current ModbusCore
- ✅ Transport abstraction (flexible)
- ✅ Platform-agnostic
- ❌ No MCU examples yet
- ❌ No RS485 DE/RE control example
- ❌ No bootloader integration

### OldModbus
- ✅ Renesas RL78 complete implementation
- ✅ RS485 DE/RE pin control (two variations)
- ✅ Bootloader integration
- ✅ Broken cable mode (watchdog)
- ✅ Auto-commissioning via GPIO
- ✅ UUID management
- ✅ Signal inversion support
- ✅ UART watchdog recovery
- ✅ Dual baudrate support

## API Differences

### Server API

| Feature | ModbusCore | OldModbus |
|---------|------------|-----------|
| Set holding register | ❌ | ✅ `modbus_set_holding_register()` |
| Set holding register array | ❌ | ✅ `modbus_set_array_holding_register()` |
| Set input register | ❌ | ✅ `modbus_set_input_register()` |
| Add device info | ❌ | ✅ `modbus_server_add_device_info()` |
| Update baudrate | ❌ | ✅ `update_baudrate()` |
| Receive byte event | ❌ | ✅ `modbus_server_receive_data_from_uart_event()` |
| Receive buffer event | ❌ | ✅ `modbus_server_receive_buffer_from_uart_event()` |

### Callbacks

| Callback Type | ModbusCore | OldModbus |
|---------------|------------|-----------|
| Read callback | ❌ | ✅ `modbus_read_callback_t` |
| Write callback | ❌ | ✅ `modbus_write_callback_t` |
| Read-only flag | ❌ | ✅ Per-register configuration |

## FSM/State Machine

### Current ModbusCore
```c
typedef enum {
    MBC_ENGINE_STATE_IDLE,
    MBC_ENGINE_STATE_RECEIVING,
    MBC_ENGINE_STATE_SENDING,
    MBC_ENGINE_STATE_WAIT_RESPONSE
} mbc_engine_state_t;
```

**States:** 4 states (simplified)

### OldModbus
```c
typedef enum {
    MODBUS_SERVER_STATE_IDLE,
    MODBUS_SERVER_STATE_RECEIVING,
    MODBUS_SERVER_STATE_PARSING_ADDRESS,
    MODBUS_SERVER_STATE_PARSING_FUNCTION,
    MODBUS_SERVER_STATE_PROCESSING,
    MODBUS_SERVER_STATE_VALIDATING_FRAME,
    MODBUS_SERVER_STATE_BUILDING_RESPONSE,
    MODBUS_SERVER_STATE_PUTTING_DATA_ON_BUF,
    MODBUS_SERVER_STATE_CALCULATING_CRC,
    MODBUS_SERVER_STATE_SENDING,
    MODBUS_SERVER_STATE_ERROR
} modbus_server_state_t;
```

**States:** 11 states (granular control)

### Events

**OldModbus Events:**
```c
MODBUS_EVENT_RX_BYTE_RECEIVED
MODBUS_EVENT_PARSE_ADDRESS
MODBUS_EVENT_PARSE_FUNCTION
MODBUS_EVENT_PROCESS_FRAME
MODBUS_EVENT_VALIDATE_FRAME
MODBUS_EVENT_BUILD_RESPONSE
MODBUS_EVENT_BROADCAST_DONT_ANSWER
MODBUS_EVENT_PUT_DATA_ON_BUFFER
MODBUS_EVENT_CALCULATE_CRC
MODBUS_EVENT_SEND_RESPONSE
MODBUS_EVENT_TX_COMPLETE
MODBUS_EVENT_ERROR_DETECTED
MODBUS_EVENT_ERROR_WRONG_BAUDRATE
MODBUS_EVENT_TIMEOUT
MODBUS_EVENT_BOOTLOADER  <--- ALREADY EXISTS!
MODBUS_EVENT_RESTART_FROM_ERROR
```

**Note:** The bootloader event already exists in oldmodbus!

## Missing Features Analysis

### Critical for MCU RTU Slave
1. ❌ **FC04: Read Input Registers** - Very common in industrial applications
2. ❌ **FC2B: Device Identification** - Important for device discovery
3. ❌ **Input Register Support** - Different from holding registers
4. ❌ **Register Callbacks** - For validation, persistence, side effects
5. ❌ **Interrupt-driven RX** - `receive_data_from_uart_event()`

### Nice to Have
6. ⏳ FC01/FC02: Coils and Discrete Inputs (less common in MCU slaves)
7. ⏳ FC17: Read/Write Multiple (advanced use case)
8. ⏳ Custom broadcast commands (application-specific)

### Already Planned
9. ✅ Bootloader integration (OpenSpec created)
10. ✅ MCU examples (OpenSpec created)
11. ✅ Communication watchdog (OpenSpec created)

## Recommendations

### Immediate Priority (MUST HAVE for MCU RTU Slave)

1. **Add FC04: Read Input Registers**
   - Common in industrial applications
   - Read-only data (sensors, status)
   - Easy to implement (similar to FC03)

2. **Add Input Register Management**
   - Similar API to holding registers
   - Read callback support
   - Separate address space from holding registers

3. **Add FC2B: Device Identification**
   - Industry standard for device discovery
   - Required by many SCADA systems
   - Enhances interoperability

4. **Add Register Callbacks**
   - Read callbacks for dynamic values
   - Write callbacks for validation/persistence
   - Read-only flag per register

5. **Add Interrupt-Driven RX Support**
   - Critical for MCU performance
   - `receive_byte_event()` pattern
   - Buffer-based variant for DMA

### Medium Priority (SHOULD HAVE)

6. **Add FC01/FC02: Coils Support**
   - Common for digital I/O control
   - Relatively simple to implement
   - Increases protocol compatibility

7. **Enhanced FSM Events**
   - More granular state control
   - Better error recovery
   - Timeout handling

### Low Priority (NICE TO HAVE)

8. **FC17: Read/Write Multiple**
   - Advanced use case
   - Less common in simple slaves
   - Defer until requested

9. **Custom Function Codes**
   - Application-specific
   - Can be added via extension mechanism
   - Not part of standard

## Migration Impact

### Breaking Changes
None if we add features incrementally. All additions are backward compatible.

### API Extensions Needed
- `mbc_server_set_input_register()`
- `mbc_server_set_holding_register_with_callbacks()`
- `mbc_server_add_device_info()`
- `mbc_engine_receive_byte()` (interrupt-driven)
- `mbc_pdu_build_read_input_request()`
- `mbc_pdu_parse_read_input_response()`

### Estimated Work
- **FC04 + Input Registers:** ~8 hours (spec + implementation + tests)
- **FC2B + Device Info:** ~12 hours (more complex structure)
- **Register Callbacks:** ~6 hours (refactor existing code)
- **Interrupt RX:** ~4 hours (simple byte injection)

**Total:** ~30 hours additional work

## Conclusion

The current ModbusCore library is **NOT** feature-complete for replacing oldmodbus in MCU RTU slave applications without adding:

1. ✅ Input Register support (FC04)
2. ✅ Device Identification (FC2B)
3. ✅ Register callbacks
4. ✅ Interrupt-driven byte reception

The **3 OpenSpec specifications already created** cover:
- ✅ Bootloader integration
- ✅ Communication watchdog pattern
- ✅ MCU integration guide

But we need **4 additional OpenSpec specifications** for:
- ⏳ FC04: Input Registers
- ⏳ FC2B: Device Identification
- ⏳ Register Callbacks
- ⏳ Interrupt-Driven Reception

**Recommendation:** Create 4 more OpenSpec documents before starting implementation.

---

**Document Version:** 1.0
**Date:** 2025-01-28
**Status:** Analysis Complete
