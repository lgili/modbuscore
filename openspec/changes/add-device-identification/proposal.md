# Add Device Identification Support (FC2B)

## Why

Function Code 0x2B (Read Device Identification) is an industry-standard mechanism for device discovery and identification in Modbus networks. It allows SCADA systems and network managers to:

- Automatically discover devices
- Identify vendor and product information
- Read firmware versions and serial numbers
- Verify device capabilities

The old `oldmodbus` implementation has complete FC2B support with device package management. This is a critical feature for professional industrial applications and is required by many SCADA systems.

**Real-world benefits:**
- Automated device inventory
- Remote diagnostics
- Configuration management
- Compliance with industrial standards
- Enhanced interoperability

## What Changes

- Add FC2B (Read Device Identification) to protocol module
- Implement MEI (Modbus Encapsulated Interface) parsing
- Add device information management API
- Support standard objects (Vendor Name, Product Code, etc.)
- Implement conformity levels (Basic, Regular, Extended)
- Add examples demonstrating device info configuration
- Full test coverage

## Impact

### Affected specs
- `protocol-fc2b` - New FC2B implementation
- `runtime-server` - Device info management
- `mcu-examples` - Device info configuration examples

### Affected code
- `include/modbuscore/protocol/fc2b.h` - New header
- `src/protocol/fc2b.c` - FC2B implementation
- `include/modbuscore/runtime/runtime.h` - Device info API
- `src/runtime/device_info.c` - Device info management
- `examples/*/` - Add device info to examples

### Breaking Changes
None - This is an additive feature.

## Success Criteria

1. ✓ FC2B requests are properly parsed
2. ✓ MEI type 0x0E (Read Device ID) supported
3. ✓ Basic conformity level supported (objects 0x00-0x02)
4. ✓ Regular conformity level supported (objects 0x00-0x06)
5. ✓ Extended conformity level optional
6. ✓ Multi-packet responses handled (more follows)
7. ✓ Standard objects supported (Vendor, Product, Version)
8. ✓ Custom objects can be added
9. ✓ Examples demonstrate typical configuration
10. ✓ Comprehensive tests pass
