# Add Custom Function Code Support

## Why

Real-world Modbus implementations often require custom or vendor-specific function codes beyond the standard set. The old `oldmodbus` implementation supports custom function codes like:
- FC41: Special broadcast command for group control
- FC42: Special broadcast command for group control
- FC43: Broadcast address commissioning

Additionally, device manufacturers commonly use:
- Vendor-specific diagnostic functions (FC08 variants)
- Custom bulk transfer operations
- Proprietary extensions
- Legacy protocol compatibility

Without custom function code support, the library cannot:
- Support vendor-specific requirements
- Maintain backward compatibility with legacy systems
- Implement domain-specific protocols
- Extend Modbus for specialized applications

**Use cases:**
- HVAC systems with custom commands
- Motor controllers with proprietary tuning functions
- Energy meters with vendor diagnostics
- Industrial equipment with specialized protocols

## What Changes

- Add callback mechanism for custom function codes
- Allow registration of handler for specific function code
- Provide request/response buffer access in callback
- Support both standard and custom codes simultaneously
- Handle exceptions from custom handlers
- Add examples for common custom function patterns
- Comprehensive documentation and tests

## Impact

### Affected specs
- `protocol-engine` - Custom FC registration and handling
- `runtime-server` - Server-side custom FC API
- `mcu-examples` - Custom FC examples

### Affected code
- `include/modbuscore/protocol/engine.h` - Custom FC callback
- `src/protocol/engine.c` - Handler dispatch logic
- `include/modbuscore/runtime/runtime.h` - Registration API
- `src/runtime/custom_fc.c` - Custom FC management
- `examples/custom_fc/` - Examples

### Breaking Changes
None - This is an additive feature.

## Success Criteria

1. ✓ Custom function codes (1-127, except standard) can be registered
2. ✓ Handler receives complete request frame
3. ✓ Handler can build custom response
4. ✓ Handler can return exception
5. ✓ Standard function codes unaffected
6. ✓ Multiple custom codes supported
7. ✓ Examples show broadcast commands
8. ✓ Examples show request/response patterns
9. ✓ Thread-safe registration
10. ✓ Comprehensive tests pass
