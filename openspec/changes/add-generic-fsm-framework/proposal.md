# Add Generic FSM Framework (from oldmodbus) - 3 Phases

## Why

The oldmodbus FSM implementation is **significantly superior** to the current engine.c approach. We will implement this in 3 incremental phases for safety and validation.

**Base FSM provides:**

**oldmodbus FSM Advantages:**
- ✅ **Generic & Reusable** - Works for any protocol, not just Modbus
- ✅ **Event Queue** - ISR-safe circular buffer for event injection
- ✅ **Data-Driven** - States defined as data structures, not switch statements
- ✅ **Guards** - Conditional transitions make logic explicit
- ✅ **Actions** - Per-transition callbacks for clean separation
- ✅ **Default Actions** - Idle processing (timeouts, polling, etc.)
- ✅ **Smaller Code** - ~240 lines vs 500+ in engine.c
- ✅ **Battle-Tested** - Proven in production on Renesas MCUs
- ✅ **Easier to Test** - Declarative design enables unit testing
- ✅ **Better Maintainability** - Add states without touching core logic

**Current engine.c Issues:**
- ❌ Hardcoded for Modbus only
- ❌ No event queue (must poll constantly)
- ❌ Switch-based FSM (imperative, hard to extend)
- ❌ No guards (conditions mixed with logic)
- ❌ No default actions (awkward timeout handling)
- ❌ Higher CPU usage (no event-driven sleep)

**See docs/FSM_COMPARISON.md for detailed analysis.**

## What Changes - 3 Phases

### Phase A: Basic Port (Foundation)
**Goal:** Get oldmodbus FSM working as-is
- Port `fsm.h` and `fsm.c` from oldmodbus unchanged
- Make critical section macros configurable
- Add to common utilities
- Basic tests to validate port
- Minimal documentation

**Effort:** ~8 hours
**Risk:** Very low (proven code)

### Phase B: Core Improvements (Safety & Usability)
**Goal:** Essential improvements for production use
- Add type safety (enums instead of uint8_t)
- Add entry/exit actions per state
- Add null safety checks (debug mode)
- Add event overflow callback
- Add better macros (cleaner API)
- Enhanced tests
- Updated documentation

**Effort:** ~8 hours
**Risk:** Low (additive changes)

### Phase C: Debug Features (Observability)
**Goal:** Make FSM easier to debug and maintain
- Add tracing callback (log transitions)
- Add state history (last 8 transitions)
- Add compile-time validation (static asserts)
- Debug utilities and helpers
- Comprehensive documentation
- Performance benchmarks

**Effort:** ~4 hours
**Risk:** Very low (optional features)

### Integration (All Phases)
- Update Modbus protocol engine to use FSM
- Refactor engine.c to be data-driven
- Port all Modbus states to new FSM
- Integration tests

**Effort:** ~12 hours
**Risk:** Medium (significant refactor)

## Impact

### Affected specs
- `common-fsm` - New generic FSM framework
- `protocol-engine` - Refactor to use FSM
- All other specs benefit from event-driven architecture

### Affected code
- `include/modbuscore/common/fsm.h` - New file (ported)
- `src/common/fsm.c` - New file (ported)
- `src/protocol/engine.c` - Refactor to use FSM
- All interrupt-driven features easier to implement
- CMakeLists.txt - Add new files

### Breaking Changes
**Minor:**
- engine.c internal implementation changes
- External API remains the same

## Success Criteria

### Phase A (Basic Port)
1. ✓ FSM compiles without errors
2. ✓ Event queue works from ISR context
3. ✓ Critical sections configurable
4. ✓ Basic tests pass
5. ✓ No regressions vs oldmodbus

### Phase B (Core Improvements)
6. ✓ Type-safe events (enums)
7. ✓ Entry/exit actions work
8. ✓ Null checks catch errors in debug
9. ✓ Overflow callback notifies on full queue
10. ✓ Macros are easier to use
11. ✓ All Phase A tests still pass

### Phase C (Debug Features)
12. ✓ Tracing logs all transitions
13. ✓ State history tracks last 8 states
14. ✓ Compile-time validation catches errors
15. ✓ Debug utilities available
16. ✓ Performance >= Phase B

### Integration
17. ✓ Modbus engine uses new FSM
18. ✓ All Modbus states data-driven
19. ✓ Performance >= old engine.c
20. ✓ All integration tests pass
21. ✓ Can be reused for other protocols
22. ✓ Enables interrupt-driven RX easily
