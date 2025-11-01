# Add Bootloader Integration Support

## Why

MCU applications often require firmware update capabilities through the Modbus interface. The old `oldmodbus` implementation had a custom bootloader protocol that allowed in-field firmware updates using special non-Modbus frames. This capability is critical for embedded systems where physical access is limited.

Current implementation lacks a mechanism to inject custom protocol handlers for bootloader or other special commands, forcing users to fork the library or work around it.

## What Changes

- Add `parse_bootloader_request` callback to transport layer
- Add `MODBUS_EVENT_BOOTLOADER` FSM event for custom frame handling
- Document bootloader protocol pattern and integration
- Provide example implementation for Renesas RL78 bootloader protocol
- Add ability to bypass normal Modbus processing for special frames

## Impact

### Affected specs
- `modbus-transport` - New optional callback
- `modbus-server-fsm` - New event and state transition
- `mcu-examples` - New bootloader example (to be created)

### Affected code
- `include/modbuscore/transport/iface.h` - Add callback signature
- `include/modbuscore/runtime/runtime.h` - Add FSM event
- `src/runtime/` - FSM event handling
- `examples/renesas_rl78_bootloader/` - Example implementation

### Breaking Changes
None - This is an additive change. Existing code continues to work without modification.

### Migration Path
Not applicable - new feature, opt-in only.

## Success Criteria

1. ✓ Transport callback can intercept frames before normal processing
2. ✓ Custom frames can bypass Modbus FSM entirely
3. ✓ Example bootloader implementation works on real MCU
4. ✓ Documentation explains integration pattern clearly
5. ✓ No performance impact when callback is NULL
6. ✓ Thread-safe for interrupt-driven implementations
