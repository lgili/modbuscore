# Modbus RTU MCU Migration Analysis

## Executive Summary

This document analyzes the migration from the old `oldmodbus` implementation (Renesas RTU Slave) to the new modern Modbus library, focusing on MCU-specific features and flexibility for embedded systems.

## Current State Analysis

### Old Implementation (oldmodbus)

The old implementation has two main variants:

1. **modbus_serial.c (Haier Protocol)**
   - Basic RTU slave for Renesas RL78
   - RS485 control with DE/RE pins
   - Parameter reading with callbacks
   - Bootloader integration support

2. **modbus_serial_nidec.c (Nidec Protocol)**
   - More complete and modern implementation
   - Flash parameter persistence
   - Broken Cable Mode (Emergency mode)
   - GPIO-based auto-commissioning
   - UUID management (11 slots)
   - UART watchdog recovery
   - Dual baudrate support (Modbus vs USB/Normal)
   - Signal inversion support (SOL config)

### New Implementation (modbuscore)

The new implementation is cleaner and more portable:
- Platform-agnostic core
- FSM-based protocol engine
- Multiple transport support (TCP, RTU, etc.)
- Better separation of concerns
- Modern C structure

## Key Features to Migrate

### 1. RS485 Hardware Control Variations

**Old Implementation:**
```c
// Two callback functions in transport:
uint8_t (*write_rs485_de_as_receiver)(void);
uint8_t (*write_rs485_de_as_transmitter)(void);

// Two hardware configurations:
// A) RS485 with DE/RE control pin
//    - Requires GPIO toggle before TX/RX
//    - modbus_serial.c:428-442
// B) Pure TX/RX without DE/RE control
//    - No pin control needed
```

**Migration Strategy:**
- Keep optional RS485 callbacks in transport layer
- Make them truly optional (NULL check)
- Document both usage patterns

### 2. Bootloader Integration

**Old Implementation:**
```c
// Custom bootloader protocol:
#define RECEPTION_IDENTIFICATION_1ST            0x5A
#define ANSWER_REQUEST_BOOTLOADER_CO_EXT_2ND    0x5B
#define SERIAL_BOOT_FIRST_REQ_CO_EXT_3ND        0xB0
#define SERIAL_BOOT_SECOND_REQ_CO_EXT_3ND       0xB1
#define SERIAL_BOOT_FIRST_REQ_CO_EXT_4ND        0xBE
#define SERIAL_BOOT_SECOND_REQ_CO_EXT_4ND       0xBF

// Two-stage bootloader activation:
// Stage 1: Request bootloader mode (0xB0/0xBE)
// Stage 2: Calculate CRC and perform boot swap (0xB1/0xBF)

// Transport callback:
uint8_t (*parse_bootloader_request)(const uint8_t *buffer, uint16_t *length);
```

**Key Functions:**
- `parse_bootloader()` - modbus_serial_nidec.c:744-801
- Intercepts special non-Modbus frames
- Returns `MODBUS_OTHERS_REQUESTS` to bypass normal processing
- Sends custom response frames

**Migration Strategy:**
- Keep `parse_bootloader_request` callback in transport
- Add special event `MODBUS_EVENT_BOOTLOADER` to FSM
- Allow custom frame injection

### 3. Broken Cable Mode (Emergency Speed)

**Old Implementation:**
```c
// Configurable parameters:
static int16_t g_emergency_speed = MIN_MOTOR_SPEED;
static int16_t g_emergency_timeout = MODBUS_BCM_TIMEOUT_DEFAULT;

// Runtime tracking:
static uint16_t g_time_since_last_speed_cmd_sec_ref = 0;
static uint8_t g_broken_cable_mode_active = FALSE;

// Logic in emergency_broken_cable_task():
// 1. Reset timer on every speed command received
// 2. If timeout expires without commands, activate emergency speed
// 3. Set SERIAL_FAIL communication status flag
```

**Location:** modbus_serial_nidec.c:375-399

**Migration Strategy:**
- Create generic "watchdog" module in runtime layer
- Configurable timeout and fallback value
- Optional callback for timeout action
- Can be used for any register, not just speed

### 4. Auto-Commissioning

**Old Implementation:**
```c
// GPIO-based address override:
// State machine with 4 states:
typedef enum {
    AC_STATE_INIT,
    AC_STATE_DELAY,        // 10 second delay
    AC_STATE_CHECK_SIGNAL, // Read GPIO pin
    AC_STATE_DONE
} auto_commissioning_state_t;

// Logic:
// 1. Start at default address (0x10)
// 2. Wait 10 seconds
// 3. Check GPIO pin
// 4. If HIGH, force address to 0x11
```

**Location:** modbus_serial_nidec.c:324-365

**Migration Strategy:**
- Create example/template in demos
- Not part of core library (application-specific)
- Document pattern for users

### 5. UUID Management

**Old Implementation:**
```c
// 11 UUID slots for device identification:
static int16_t g_uuid_slots[UUID_SLOT_COUNT];
static int16_t g_uuid_slots_SDH[UUID_SLOT_COUNT]; // Shadow copy
static int16_t g_uuid_locker; // Write protection

// Write callbacks with locker check:
// - If locked, restore from shadow copy
// - If unlocked, write to flash and update shadow
```

**Location:** modbus_serial_nidec.c:131-244

**Migration Strategy:**
- Document as example pattern
- Not core feature (application-specific)
- Show how to implement write protection with callbacks

### 6. UART Configuration Flexibility

**Old Implementation:**

**Multiple Configuration Options:**
```c
// A) Normal vs Modbus mode
//    - Different parity settings
//    - Different baudrates
//    - modbus_serial.c:461-478

// B) Signal inversion support
//    - SOL register configuration
//    - _0001_SAU_CHANNEL0_INVERTED vs _0000_SAU_CHANNEL0_NORMAL
//    - modbus_serial_nidec.c:903, 952-953

// C) Watchdog recovery
//    - Detects stuck UART (5-10 second timeout)
//    - Auto-restart on timeout
//    - modbus_serial_nidec.c:983-996
```

**Migration Strategy:**
- Keep `restart_uart()` callback in transport
- Document UART configuration patterns
- Add watchdog pattern to examples

### 7. Parameter Persistence

**Old Implementation:**
```c
// Flash-backed parameter system:
typedef struct {
    MIN, MAX, DEFAULT, MULTIPLIER,
    S16_VALIDATION_CB, U16_VALIDATION_CB,
    IS_UNSIGNED
} param_config_t;

// Modbus parameters (module-specific):
[MODBUS_PARAM_SLAVE_ADDR]
[MODBUS_PARAM_DEVICE_GROUP]
[MODBUS_PARAM_SET_SPEED_BCM]
[MODBUS_PARAM_TIMEOUT_BCM]
[MODBUS_PARAM_UUID_SLOT1..11]

// Callbacks validate and persist:
parameters_set_value(MODULE_MODBUS, param, value);
```

**Location:** modbus_serial_nidec.c:175-192, 411-428

**Migration Strategy:**
- Document as example integration pattern
- Not part of core library (project-specific)
- Show how to use Modbus write callbacks for persistence

## Hardware Abstraction Differences

### Old Implementation
- **Highly Renesas-specific:**
  - Direct register access (SMR, SCR, SDR, etc.)
  - Interrupt service routines with `#pragma vector`
  - Platform-specific macros (UART_SHARED_REGISTERS_BASE_ADDRESS, etc.)

### New Implementation
- **Platform-agnostic:**
  - Transport layer abstraction
  - Callback-based I/O
  - Portable C code

## Migration Priority

### Phase 1: Core RS485 Support ✓
Already implemented in new library:
- RS485 DE/RE callbacks in transport
- Optional GPIO control

### Phase 2: Bootloader Integration (HIGH PRIORITY)
- Add bootloader callback to transport
- Add FSM event for custom frames
- Document protocol pattern

### Phase 3: Application Features (MEDIUM PRIORITY)
- Broken Cable Mode example
- Parameter persistence pattern
- UUID management pattern
- Auto-commissioning example

### Phase 4: UART Patterns (LOW PRIORITY)
- Watchdog recovery pattern
- Configuration examples
- Signal inversion documentation

## Recommended OpenSpec Improvements

1. **MCU-RTU-001: Enhanced Bootloader Support**
   - Bootloader callback integration
   - Custom frame injection
   - FSM event handling

2. **MCU-RTU-002: Communication Watchdog Pattern**
   - Generic timeout monitoring
   - Configurable fallback actions
   - Example implementation

3. **MCU-RTU-003: Parameter Persistence Integration**
   - Callback patterns
   - Flash write examples
   - Validation strategies

4. **MCU-RTU-004: UART Configuration Guide**
   - Multi-mode configuration
   - Recovery strategies
   - Signal inversion

5. **MCU-RTU-005: MCU Integration Examples**
   - Renesas RL78 port
   - STM32 port
   - AVR port
   - ESP32 port

## Success Criteria

The migration is successful when:

1. ✓ Core Modbus protocol works on MCU
2. ✓ RS485 hardware control is flexible
3. ⏳ Bootloader integration is documented and working
4. ⏳ Parameter persistence pattern is clear
5. ⏳ Example MCU ports exist (at least 2 platforms)
6. ⏳ Documentation is complete
7. ⏳ Migration guide exists

## Next Steps

1. Create OpenSpec specifications for each improvement
2. Implement bootloader support (highest priority)
3. Create comprehensive MCU examples
4. Document all patterns
5. Test on multiple MCU platforms
6. Update migration guide

---

**Document Version:** 1.0
**Date:** 2025-01-28
**Author:** Migration Analysis Team
