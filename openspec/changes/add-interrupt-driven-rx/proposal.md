# Add Interrupt-Driven Reception Support

## Why

MCU applications typically use interrupt-driven UART for efficiency and responsiveness. The old `oldmodbus` implementation has functions like `modbus_server_receive_data_from_uart_event()` that allow bytes to be injected directly from ISRs.

Current library only supports polling via `mbc_engine_step()`, which requires:
- Frequent polling loops
- Higher CPU usage
- Potential for missed bytes
- Less responsive to incoming data

**Benefits of interrupt-driven reception:**
- **Lower CPU usage** - Sleep until interrupt
- **Faster response** - Process immediately when byte arrives
- **No missed data** - Hardware buffer + interrupt priority
- **Better power efficiency** - Deep sleep between interrupts
- **Standard MCU pattern** - Most UART drivers are interrupt-based

## What Changes

- Add `mbc_engine_inject_byte()` function for ISR usage
- Add `mbc_engine_inject_buffer()` for DMA completion
- Ensure functions are ISR-safe (no allocations, no blocking)
- Update examples to show interrupt-driven pattern
- Document ISR safety requirements
- Add timing guidelines for ISR execution
- Comprehensive tests

## Impact

### Affected specs
- `protocol-engine` - Byte injection API
- `mcu-examples` - Interrupt examples
- `transport-uart` - ISR integration patterns

### Affected code
- `include/modbuscore/protocol/engine.h` - New functions
- `src/protocol/engine.c` - Byte injection logic
- `examples/*/uart_isr.c` - ISR examples
- Documentation updates

### Breaking Changes
None - This is an additive feature.

## Success Criteria

1. ✓ Bytes can be injected from ISR context
2. ✓ Functions are ISR-safe (no malloc, no blocking)
3. ✓ Buffers can be injected from DMA completion
4. ✓ Frame timeout handled correctly
5. ✓ Examples show typical MCU ISR pattern
6. ✓ Timing requirements documented
7. ✓ No race conditions or corruption
8. ✓ Performance better than polling
9. ✓ Comprehensive tests pass
