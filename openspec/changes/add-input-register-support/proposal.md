# Add Input Register Support (FC04)

## Why

Input Registers are a fundamental part of the Modbus protocol, commonly used for read-only data such as sensor values, status information, and measurements. They occupy a separate address space from Holding Registers (FC03) and are accessed via Function Code 04.

The old `oldmodbus` implementation fully supports Input Registers with callbacks and proper address space separation. The current library only supports Holding Registers, which limits its applicability for typical industrial MCU applications where sensors and read-only data are common.

**Real-world use cases:**
- Temperature sensors
- Pressure readings
- Status flags
- Error counters
- Diagnostic values
- Calculated values

## What Changes

- Add FC04 (Read Input Registers) to PDU module
- Add input register management to server/runtime
- Implement separate address space for input registers
- Support read callbacks for dynamic values
- Add PDU encoding/decoding functions for FC04
- Update examples to demonstrate input register usage
- Add comprehensive tests

## Impact

### Affected specs
- `protocol-pdu` - New FC04 functions
- `runtime-server` - Input register management API
- `mcu-examples` - Example usage

### Affected code
- `include/modbuscore/protocol/pdu.h` - Add FC04 functions
- `src/protocol/pdu.c` - Implement FC04 encoding/decoding
- `include/modbuscore/runtime/runtime.h` - Server API for input registers
- `src/runtime/server.c` - Input register handling
- `examples/*/` - Update examples

### Breaking Changes
None - This is an additive change. Existing code continues to work.

## Success Criteria

1. ✓ FC04 requests are properly parsed
2. ✓ Input registers have separate address space (0x0000-0xFFFF)
3. ✓ Read callbacks work for dynamic values
4. ✓ Multiple input registers can be registered
5. ✓ Array registration supported
6. ✓ Examples demonstrate typical sensor use case
7. ✓ Performance equivalent to holding registers
8. ✓ Comprehensive tests pass
