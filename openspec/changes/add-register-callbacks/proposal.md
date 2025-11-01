# Add Register Callback System

## Why

Register callbacks are essential for real-world MCU applications where register access requires side effects, validation, or dynamic value generation. The old `oldmodbus` implementation has robust callback support that enables:

**Read callbacks:**
- Dynamic value calculation (e.g., current temperature from ADC)
- Lazy evaluation of expensive operations
- Hardware register reading
- Status aggregation
- Diagnostic data collection

**Write callbacks:**
- Value validation before accepting
- Range clamping
- Flash persistence
- Hardware control (motor speed, relay state)
- Side effects (trigger actions, update UI)
- Write protection/locking mechanisms

Without callbacks, the library is limited to simple memory-mapped registers and cannot support the complex behaviors needed in industrial control systems.

## What Changes

- Define read callback signature for holding registers
- Define read callback signature for input registers
- Define write callback signature for holding registers
- Add callback storage to register structures
- Implement callback invocation on register access
- Support NULL callbacks (direct memory access fallback)
- Add callback context parameter (optional user data)
- Add examples demonstrating common patterns
- Comprehensive tests

## Impact

### Affected specs
- `runtime-server` - Register management with callbacks
- `mcu-examples` - Callback usage examples

### Affected code
- `include/modbuscore/runtime/runtime.h` - Callback signatures
- `src/runtime/server.c` - Callback invocation logic
- `examples/callbacks/` - New examples directory
- All register APIs updated with callback parameters

### Breaking Changes
**Minor API changes:**
- Register set functions will add optional callback parameters
- Existing code can pass NULL for callbacks (backward compatible)

## Success Criteria

1. ✓ Read callbacks invoked on FC03/FC04 requests
2. ✓ Write callbacks invoked on FC06/FC16 requests
3. ✓ NULL callbacks use direct memory access
4. ✓ Callbacks can validate and reject writes
5. ✓ Context parameter passed to callbacks
6. ✓ Read-only registers enforced
7. ✓ Examples show typical patterns
8. ✓ Performance overhead < 5% when callbacks unused
9. ✓ Thread-safe callback invocation
10. ✓ Comprehensive tests pass
