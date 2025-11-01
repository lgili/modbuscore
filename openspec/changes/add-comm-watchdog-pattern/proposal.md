# Add Communication Watchdog Pattern

## Why

Embedded systems using Modbus often implement "broken cable mode" or "communication timeout" features where a fallback value is used if no valid command is received within a configurable timeout period. The old `oldmodbus` implementation had this as a hardcoded feature for speed control.

This pattern is useful beyond just speed control - any critical parameter might need a safe fallback when communication is lost. Providing a reusable pattern makes the library more valuable for MCU developers.

## What Changes

- Create documented pattern for communication timeout monitoring
- Provide example implementation in runtime layer
- Document integration with Modbus callbacks
- Show how to implement "broken cable mode" or similar features
- Add example for motor speed control with emergency fallback

**Note:** This is NOT a core library feature but a documented pattern/example.

## Impact

### Affected specs
- `runtime-patterns` - New documentation capability
- `mcu-examples` - Add watchdog example

### Affected code
- `docs/advanced/comm-watchdog-pattern.md` - New guide
- `examples/broken_cable_mode/` - New example
- No changes to core library

### Breaking Changes
None - documentation and examples only.

## Success Criteria

1. ✓ Pattern is clearly documented with rationale
2. ✓ Example shows integration with Modbus callbacks
3. ✓ Example is portable (no platform-specific code in core)
4. ✓ Timing requirements are documented
5. ✓ Multiple use cases are shown (not just speed control)
