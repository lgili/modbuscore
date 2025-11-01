# Implementation Tasks - Complete List (3 Phases)

## PHASE A: Basic Port (~8 hours)

### 1. Port FSM Framework
- [ ] 1.1 Create `include/modbuscore/common/fsm.h`
- [ ] 1.2 Create `src/common/fsm.c`
- [ ] 1.3 Copy oldmodbus FSM code verbatim
- [ ] 1.4 Remove platform-specific dependencies
- [ ] 1.5 Make critical section macros configurable
- [ ] 1.6 Add to CMakeLists.txt

### 2. Generic FSM Configuration
- [ ] 2.1 Make FSM_EVENT_QUEUE_SIZE configurable
- [ ] 2.2 Add FSM_ENTER_CRITICAL/EXIT_CRITICAL macros
- [ ] 2.3 Document critical section requirements
- [ ] 2.4 Provide default weak implementations
- [ ] 2.5 Add fsm_config.h template

### 3. Basic Tests (Phase A)
- [ ] 3.1 Test fsm_init()
- [ ] 3.2 Test fsm_handle_event() basic
- [ ] 3.3 Test fsm_run() basic
- [ ] 3.4 Test event queue wraparound
- [ ] 3.5 Test guards (allow/deny)
- [ ] 3.6 Test actions execute
- [ ] 3.7 Test default actions
- [ ] 3.8 Verify ISR safety with mock ISR

### 4. Basic Documentation (Phase A)
- [ ] 4.1 Document FSM API reference
- [ ] 4.2 Add basic usage example
- [ ] 4.3 Document critical section requirements
- [ ] 4.4 Document event queue behavior

### 5. Phase A Review & Validation
- [ ] 5.1 Code review
- [ ] 5.2 All tests pass
- [ ] 5.3 Performance baseline measurement
- [ ] 5.4 **Phase A sign-off**

---

## PHASE B: Core Improvements (~8 hours)

### 6. Type Safety
- [ ] 6.1 Add fsm_event_t typedef (user-defined enum)
- [ ] 6.2 Update fsm_handle_event() signature
- [ ] 6.3 Update transition definitions
- [ ] 6.4 Add compile-time type checks where possible
- [ ] 6.5 Update all examples to use enums

### 7. Entry/Exit Actions
- [ ] 7.1 Add entry_action field to fsm_state_t
- [ ] 7.2 Add exit_action field to fsm_state_t
- [ ] 7.3 Invoke entry_action when entering state
- [ ] 7.4 Invoke exit_action when leaving state
- [ ] 7.5 Update FSM_STATE macro to support entry/exit
- [ ] 7.6 Test entry/exit action ordering

### 8. Null Safety (Debug Mode)
- [ ] 8.1 Add FSM_ASSERT macro
- [ ] 8.2 Add null checks in fsm_init()
- [ ] 8.3 Add null checks in fsm_handle_event()
- [ ] 8.4 Add null checks in fsm_run()
- [ ] 8.5 Add transition array validation
- [ ] 8.6 Make assertions compile-out in release

### 9. Event Overflow Callback
- [ ] 9.1 Add overflow_callback field to fsm_t
- [ ] 9.2 Add overflow_count field to fsm_t
- [ ] 9.3 Invoke callback when queue full
- [ ] 9.4 Add fsm_set_overflow_callback() function
- [ ] 9.5 Test overflow handling
- [ ] 9.6 Document overflow behavior

### 10. Better Macros
- [ ] 10.1 Add FSM_STATE_FULL macro (all options)
- [ ] 10.2 Add FSM_STATE_SIMPLE macro (minimal)
- [ ] 10.3 Add FSM_TRANSITION_FULL macro
- [ ] 10.4 Add FSM_TRANSITION_SIMPLE macro
- [ ] 10.5 Add FSM_TRANSITION_GUARDED macro
- [ ] 10.6 Update examples to use new macros

### 11. Enhanced Tests (Phase B)
- [ ] 11.1 Test entry actions execute once
- [ ] 11.2 Test exit actions execute once
- [ ] 11.3 Test entry/exit order (exit old, action, entry new)
- [ ] 11.4 Test overflow callback invocation
- [ ] 11.5 Test overflow count increment
- [ ] 11.6 Test null checks in debug mode
- [ ] 11.7 Verify type safety

### 12. Updated Documentation (Phase B)
- [ ] 12.1 Document entry/exit actions
- [ ] 12.2 Document overflow callback
- [ ] 12.3 Document new macros
- [ ] 12.4 Add advanced examples
- [ ] 12.5 Update state diagram notation

### 13. Phase B Review & Validation
- [ ] 13.1 Code review
- [ ] 13.2 All Phase A + B tests pass
- [ ] 13.3 Performance >= Phase A
- [ ] 13.4 **Phase B sign-off**

---

## PHASE C: Debug Features (~4 hours)

### 14. Tracing Callback
- [ ] 14.1 Add trace_callback field to fsm_t
- [ ] 14.2 Add fsm_set_trace_callback() function
- [ ] 14.3 Invoke callback on every transition
- [ ] 14.4 Pass from/to states, event, timestamp
- [ ] 14.5 Test tracing with mock callback
- [ ] 14.6 Add tracing example

### 15. State History
- [ ] 15.1 Add FSM_HISTORY_SIZE config
- [ ] 15.2 Add history array to fsm_t
- [ ] 15.3 Add history_idx to fsm_t
- [ ] 15.4 Record transitions in circular buffer
- [ ] 15.5 Add fsm_get_history() function
- [ ] 15.6 Test history wraparound

### 16. Compile-Time Validation
- [ ] 16.1 Add FSM_STATIC_ASSERT macro
- [ ] 16.2 Validate transition array not empty
- [ ] 16.3 Validate state ID in range
- [ ] 16.4 Validate event queue size > 0
- [ ] 16.5 Test compile-time failures (manually)

### 17. Debug Utilities
- [ ] 17.1 Add fsm_get_state_name() function
- [ ] 17.2 Add fsm_get_event_name() function (user hook)
- [ ] 17.3 Add fsm_print_state() debug helper
- [ ] 17.4 Add fsm_dump_history() debug helper
- [ ] 17.5 Add example debug output

### 18. Comprehensive Documentation (Phase C)
- [ ] 18.1 Document tracing callback
- [ ] 18.2 Document state history
- [ ] 18.3 Document debug utilities
- [ ] 18.4 Add troubleshooting guide
- [ ] 18.5 Add performance tuning guide

### 19. Performance Benchmarks
- [ ] 19.1 Measure transition latency
- [ ] 19.2 Measure event queue overhead
- [ ] 19.3 Measure memory footprint
- [ ] 19.4 Compare vs Phase A baseline
- [ ] 19.5 Document results

### 20. Phase C Review & Validation
- [ ] 20.1 Code review
- [ ] 20.2 All Phase A + B + C tests pass
- [ ] 20.3 Performance >= Phase B
- [ ] 20.4 **Phase C sign-off**

---

## INTEGRATION: Modbus Engine Refactor (~12 hours)

### 21. Define Modbus States
- [ ] 21.1 Define STATE_IDLE
- [ ] 21.2 Define STATE_RECEIVING
- [ ] 21.3 Define STATE_PARSING_ADDRESS
- [ ] 21.4 Define STATE_PARSING_FUNCTION
- [ ] 21.5 Define STATE_PROCESSING
- [ ] 21.6 Define STATE_VALIDATING
- [ ] 21.7 Define STATE_BUILDING_RESPONSE
- [ ] 21.8 Define STATE_SENDING
- [ ] 21.9 Define STATE_ERROR

### 22. Define Modbus Events
- [ ] 22.1 Define event enum for Modbus
- [ ] 22.2 Map old engine events to FSM events
- [ ] 22.3 Document event flow

### 23. Define Transition Table
- [ ] 23.1 IDLE → RECEIVING
- [ ] 23.2 RECEIVING → PARSING_ADDRESS
- [ ] 23.3 PARSING_ADDRESS → PARSING_FUNCTION
- [ ] 23.4 PARSING_FUNCTION → PROCESSING
- [ ] 23.5 PROCESSING → VALIDATING
- [ ] 23.6 VALIDATING → BUILDING_RESPONSE
- [ ] 23.7 BUILDING_RESPONSE → SENDING
- [ ] 23.8 SENDING → IDLE
- [ ] 23.9 Error transitions
- [ ] 23.10 Timeout transitions

### 24. Extract Guard Functions
- [ ] 24.1 guard_valid_address()
- [ ] 24.2 guard_broadcast()
- [ ] 24.3 guard_valid_function()
- [ ] 24.4 guard_crc_valid()
- [ ] 24.5 guard_custom_fc()

### 25. Extract Action Functions
- [ ] 25.1 action_start_rx()
- [ ] 25.2 action_parse_address()
- [ ] 25.3 action_parse_function()
- [ ] 25.4 action_process_request()
- [ ] 25.5 action_validate_crc()
- [ ] 25.6 action_build_response()
- [ ] 25.7 action_send()
- [ ] 25.8 action_error()

### 26. Extract Entry/Exit Actions
- [ ] 26.1 entry_receiving() - start timeout
- [ ] 26.2 exit_receiving() - stop timeout
- [ ] 26.3 entry_sending() - start TX
- [ ] 26.4 entry_error() - log error

### 27. Default Actions
- [ ] 27.1 default_check_rx_timeout() - RECEIVING
- [ ] 27.2 default_poll_tx() - SENDING
- [ ] 27.3 default_idle() - IDLE

### 28. Event Injection Integration
- [ ] 28.1 Replace state changes with fsm_handle_event()
- [ ] 28.2 Add ISR event injection points
- [ ] 28.3 Remove polling where events used
- [ ] 28.4 Test event queue from ISR

### 29. Remove Old FSM Code
- [ ] 29.1 Remove switch-based state machine
- [ ] 29.2 Remove manual state tracking
- [ ] 29.3 Clean up dead code
- [ ] 29.4 Update API if needed

### 30. Integration Tests
- [ ] 30.1 Test all Modbus function codes
- [ ] 30.2 Test error handling
- [ ] 30.3 Test timeout scenarios
- [ ] 30.4 Test broadcast handling
- [ ] 30.5 Test ISR event injection
- [ ] 30.6 Performance regression tests

### 31. Integration Documentation
- [ ] 31.1 Document Modbus FSM states
- [ ] 31.2 Document Modbus FSM events
- [ ] 31.3 Add state diagram
- [ ] 31.4 Document integration points

### 32. Integration Review & Sign-off
- [ ] 32.1 Code review
- [ ] 32.2 Architecture review
- [ ] 32.3 All tests pass
- [ ] 32.4 Performance >= old engine
- [ ] 32.5 **Integration sign-off**

---

## Summary

**Phase A:** 31 tasks (~8h) - Basic port, get it working
**Phase B:** 45 tasks (~8h) - Core improvements, production-ready
**Phase C:** 35 tasks (~4h) - Debug features, observability
**Integration:** 61 tasks (~12h) - Modbus engine refactor

**Total:** 172 tasks, ~32 hours

**Success Gates:**
- ✅ Phase A sign-off before starting Phase B
- ✅ Phase B sign-off before starting Phase C
- ✅ Phase C sign-off before starting Integration
- ✅ Integration sign-off = Feature complete
