# Bootloader Integration Design

## Context

Embedded systems often require field-upgradeable firmware. The Modbus interface provides an ideal channel for firmware updates since it's already present for device communication. The old implementation had a successful bootloader protocol that needs to be ported to the new architecture.

### Constraints
- Must not break existing Modbus protocol behavior
- Zero overhead when bootloader callback is NULL
- Must work in interrupt-driven environments
- Cannot use dynamic memory allocation
- Must preserve frame integrity (CRC validation)

### Stakeholders
- MCU developers needing firmware update capabilities
- System integrators requiring field updates
- Library maintainers ensuring backward compatibility

## Goals / Non-Goals

### Goals
- Enable custom frame handling before Modbus processing
- Support legacy bootloader protocols
- Provide clear integration patterns
- Maintain library simplicity and performance
- Document thoroughly with examples

### Non-Goals
- Implementing a specific bootloader (application-specific)
- Defining a standard bootloader protocol (too restrictive)
- Adding complex state management (keep simple)
- Supporting multiple simultaneous protocols (single callback sufficient)

## Decisions

### Decision 1: Callback-Based Interception
**What:** Add optional callback to transport layer that executes before Modbus frame parsing.

**Why:**
- Minimal library changes
- Zero overhead when NULL
- Maximum flexibility for users
- Clear separation of concerns
- Easy to test

**Alternatives Considered:**
1. **Plugin system with registration** - Too complex, adds overhead
2. **Subclass/inheritance** - Not idiomatic C, harder to use
3. **Preprocessor conditionals** - Less flexible, compile-time only
4. **State machine extension** - Couples bootloader to FSM, too rigid

### Decision 2: Return Code Strategy
**What:** Callback returns status indicating how to proceed:
```c
typedef enum {
    MODBUS_BOOTLOADER_NOT_HANDLED = 0,  // Continue normal Modbus processing
    MODBUS_BOOTLOADER_HANDLED     = 1,  // Bypass Modbus, response already sent
    MODBUS_BOOTLOADER_ERROR       = 2   // Error occurred, reset state
} modbus_bootloader_result_t;
```

**Why:**
- Explicit control flow
- Clear intent in code
- Allows partial handling (forward to Modbus if not recognized)
- Error path for recovery

### Decision 3: Early Interception Point
**What:** Invoke callback in `action_process_frame()` AFTER address/function parse but BEFORE full frame validation.

**Why:**
- Address filter already applied (correct device)
- Minimal processing done (efficient)
- Can access raw buffer for custom validation
- Can inject response before CRC calculation

**Alternatives:**
- **Before address parse** - Would process broadcasts meant for other devices
- **After CRC validation** - Bootloader frames might have different CRC

### Decision 4: FSM Event for Bypass
**What:** Add `MODBUS_EVENT_BOOTLOADER` that transitions directly to SENDING state.

**Why:**
- Clean state machine flow
- Skips normal processing path
- Allows custom response injection
- Maintains FSM integrity

### Decision 5: Buffer Ownership
**What:** Callback owns TX buffer during execution and must prepare complete response.

**Why:**
- Simple responsibility model
- No buffer copying overhead
- Clear lifecycle
- Matches existing write pattern

## Implementation Details

### Callback Signature
```c
typedef uint8_t (*modbus_bootloader_callback_t)(
    const uint8_t *rx_buffer,  // Received frame (raw)
    uint16_t rx_length,        // Frame length
    uint8_t *tx_buffer,        // Response buffer
    uint16_t *tx_length        // Response length (out)
);
```

### Integration Flow
```
RX Byte → FSM RECEIVING → PARSE_ADDRESS → PARSE_FUNCTION
                                           ↓
                                    PROCESS_FRAME
                                           ↓
                              [Bootloader Callback?]
                              /                    \
                          YES (HANDLED)          NO (NOT_HANDLED)
                             /                        \
                    BOOTLOADER_EVENT              Continue normal
                           ↓                       Modbus processing
                    Skip to SENDING                      ↓
                           ↓                     VALIDATE → BUILD → SEND
                    Send custom response
```

### Example Usage
```c
uint8_t parse_bootloader(const uint8_t *rx, uint16_t rx_len,
                         uint8_t *tx, uint16_t *tx_len) {
    // Check magic bytes
    if (rx[0] != BOOTLOADER_ADDR || rx[1] != BOOTLOADER_FUNCTION) {
        return MODBUS_BOOTLOADER_NOT_HANDLED;
    }

    // Process bootloader command
    switch (rx[2]) {
        case BOOT_CMD_ENTER:
            *tx_len = prepare_enter_response(tx);
            return MODBUS_BOOTLOADER_HANDLED;

        case BOOT_CMD_FLASH:
            *tx_len = prepare_flash_response(tx, &rx[3]);
            return MODBUS_BOOTLOADER_HANDLED;
    }

    return MODBUS_BOOTLOADER_ERROR;
}

// In setup:
transport.parse_bootloader_request = parse_bootloader;
```

## Risks / Trade-offs

### Risk 1: Callback Execution Time
**Risk:** Long-running bootloader callback blocks Modbus processing.

**Mitigation:**
- Document maximum execution time guidelines (<10ms)
- Show async pattern in examples
- Add timeout mechanism in FSM

### Risk 2: Buffer Corruption
**Risk:** Callback writes beyond buffer bounds.

**Mitigation:**
- Document buffer size guarantees
- Add buffer size parameter to callback
- Runtime bounds checking in debug builds
- Example code shows proper bounds checking

### Risk 3: Re-entrancy Issues
**Risk:** Interrupt calls callback during callback execution.

**Mitigation:**
- Document re-entrancy requirements
- Example shows proper locking
- User responsibility (like all callbacks)

### Trade-off 1: Flexibility vs Complexity
**Chosen:** Maximum flexibility (raw buffer access).

**Cost:** Users must handle framing, CRC, etc.

**Justification:** Bootloader protocols are diverse; library can't predict all patterns. Power users need full control.

### Trade-off 2: Performance vs Safety
**Chosen:** Minimal overhead (single NULL check).

**Cost:** Less runtime validation of callback behavior.

**Justification:** Embedded systems prioritize performance; validation in debug builds only.

## Migration Plan

### Phase 1: Library Changes (Week 1)
1. Add callback to transport structure
2. Implement FSM event handling
3. Add invocation point in processing
4. Write unit tests

### Phase 2: Documentation (Week 1)
1. Write integration guide
2. Document best practices
3. Create sequence diagrams

### Phase 3: Examples (Week 2)
1. Port Renesas RL78 bootloader
2. Create STM32 example
3. Add test cases

### Phase 4: Review & Release (Week 2)
1. Code review
2. Performance testing
3. Documentation review
4. Beta release

### Rollback Plan
If issues arise:
1. Revert callback invocation (comment out)
2. Library works as before (NULL callback)
3. No breaking changes to unwind

## Open Questions

1. **Q:** Should we support multiple callback registration?
   **A:** No, keep it simple. Users can multiplex in their callback.

2. **Q:** Should we provide a default bootloader implementation?
   **A:** No, too application-specific. Provide examples instead.

3. **Q:** How to handle bootloader timeout?
   **A:** User responsibility. Can use FSM timeout mechanism if needed.

4. **Q:** CRC calculation for custom frames?
   **A:** User responsibility. Utility function available if needed.

---

**Design Version:** 1.0
**Date:** 2025-01-28
**Status:** Proposed
**Reviewers:** TBD
