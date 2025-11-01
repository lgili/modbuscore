# Add MCU Integration Guide and Examples

## Why

The library is designed to be portable across platforms, but MCU integration can be challenging without concrete examples. The old `oldmodbus` implementation shows proven patterns for Renesas RL78, but these aren't documented as reusable patterns.

Developers need:
- Step-by-step MCU integration guide
- Multiple reference implementations for different MCU families
- Best practices for interrupt handling, timing, and UART configuration
- Copy-paste friendly code that's production-ready

This will make the library significantly more accessible to embedded developers and reduce integration time from days to hours.

## What Changes

- Create comprehensive MCU integration guide
- Port Renesas RL78 example (from oldmodbus)
- Add at least one additional MCU example (STM32 or AVR)
- Document interrupt-driven vs polling patterns
- Provide UART configuration templates
- Show RS485 control variations
- Add troubleshooting guide

## Impact

### Affected specs
- `mcu-examples` - New documentation capability
- `getting-started` - Add MCU quick-start section

### Affected code
- `docs/getting-started/mcu-integration.md` - New guide
- `examples/renesas_rl78/` - Port from oldmodbus
- `examples/stm32f1/` or `examples/avr_atmega/` - New example
- `examples/templates/` - Reusable templates

### Breaking Changes
None - documentation and examples only.

## Success Criteria

1. ✓ Guide explains transport layer implementation
2. ✓ At least 2 complete MCU examples exist
3. ✓ Examples demonstrate both RS485 variations
4. ✓ Interrupt-driven pattern is documented
5. ✓ Examples compile on real toolchains
6. ✓ Troubleshooting section exists
7. ✓ Integration time for new MCU < 4 hours
