# Implementation Tasks - 3 Phases

## PHASE A: Basic Port (~8 hours)

### 1. Port FSM Framework
- [ ] 1.1 Create `include/modbuscore/common/fsm.h`
- [ ] 1.2 Create `src/common/fsm.c`
- [ ] 1.3 Copy oldmodbus FSM code
- [ ] 1.4 Remove platform-specific dependencies
- [ ] 1.5 Make critical section macros configurable
- [ ] 1.6 Add to CMakeLists.txt

### 2. Generic FSM Configuration
- [ ] 2.1 Make FSM_EVENT_QUEUE_SIZE configurable
- [ ] 2.2 Add FSM_ENTER_CRITICAL/EXIT_CRITICAL macros
- [ ] 2.3 Document critical section requirements
- [ ] 2.4 Provide default implementations
- [ ] 2.5 Add configuration header

### 3. Basic Tests (Phase A)
- [ ] 3.1 Test fsm_init()
- [ ] 3.2 Test fsm_handle_event()
- [ ] 3.3 Test fsm_run()
- [ ] 3.4 Test event queue
- [ ] 3.5 Test guards
- [ ] 3.6 Test actions
- [ ] 3.7 Test default actions
- [ ] 3.8 Verify ISR safety

### 4. Basic Documentation (Phase A)
- [ ] 4.1 Document FSM API
- [ ] 4.2 Add basic usage example
- [ ] 4.3 Document critical sections

### 5. Phase A Review & Validation
- [ ] 5.1 Code review
- [ ] 5.2 All tests pass
- [ ] 5.3 Performance baseline
- [ ] 5.4 Phase A sign-off

---

## PHASE B: Core Improvements (~8 hours)

### 6. Type Safety

## 3. Refactor Protocol Engine
- [ ] 3.1 Define Modbus server states
- [ ] 3.2 Define state transitions
- [ ] 3.3 Extract guards into functions
- [ ] 3.4 Extract actions into functions
- [ ] 3.5 Replace switch statements with FSM
- [ ] 3.6 Integrate event queue

## 4. State Definitions
- [ ] 4.1 Define STATE_IDLE
- [ ] 4.2 Define STATE_RECEIVING
- [ ] 4.3 Define STATE_PARSING_ADDRESS
- [ ] 4.4 Define STATE_PARSING_FUNCTION
- [ ] 4.5 Define STATE_PROCESSING
- [ ] 4.6 Define STATE_VALIDATING
- [ ] 4.7 Define STATE_BUILDING_RESPONSE
- [ ] 4.8 Define STATE_SENDING
- [ ] 4.9 Define STATE_ERROR

## 5. Transition Definitions
- [ ] 5.1 IDLE → RECEIVING (on RX byte)
- [ ] 5.2 RECEIVING → PARSING_ADDRESS (on frame complete)
- [ ] 5.3 PARSING_ADDRESS → PARSING_FUNCTION (on valid address)
- [ ] 5.4 PARSING_FUNCTION → PROCESSING (on valid function)
- [ ] 5.5 PROCESSING → VALIDATING (on data ready)
- [ ] 5.6 VALIDATING → BUILDING_RESPONSE (on CRC ok)
- [ ] 5.7 BUILDING_RESPONSE → SENDING (on response ready)
- [ ] 5.8 SENDING → IDLE (on TX complete)
- [ ] 5.9 Any → ERROR (on error)
- [ ] 5.10 ERROR → IDLE (on timeout/reset)

## 6. Guard Functions
- [ ] 6.1 guard_valid_address() - Check if address matches
- [ ] 6.2 guard_broadcast() - Check if broadcast
- [ ] 6.3 guard_valid_function() - Check if function supported
- [ ] 6.4 guard_crc_valid() - Check CRC
- [ ] 6.5 guard_custom_fc() - Check if custom FC registered

## 7. Action Functions
- [ ] 7.1 action_start_rx() - Initialize RX buffer
- [ ] 7.2 action_parse_address() - Extract address
- [ ] 7.3 action_parse_function() - Extract function code
- [ ] 7.4 action_process_request() - Handle request
- [ ] 7.5 action_validate_crc() - Check CRC
- [ ] 7.6 action_build_response() - Create response
- [ ] 7.7 action_send() - Transmit response
- [ ] 7.8 action_error() - Handle error

## 8. Default Actions
- [ ] 8.1 default_check_timeout() - In RECEIVING state
- [ ] 8.2 default_poll_tx() - In SENDING state
- [ ] 8.3 default_idle() - In IDLE state

## 9. Event Integration
- [ ] 9.1 Replace direct state changes with events
- [ ] 9.2 Add event injection from ISR
- [ ] 9.3 Remove polling where events can be used
- [ ] 9.4 Add event queue overflow handling

## 10. Documentation
- [ ] 10.1 Document FSM framework usage
- [ ] 10.2 Document state diagram
- [ ] 10.3 Document events
- [ ] 10.4 Document critical section configuration
- [ ] 10.5 Add FSM usage examples
- [ ] 10.6 Update API documentation

## 11. Testing
- [ ] 11.1 Unit tests for FSM framework
- [ ] 11.2 Unit tests for state transitions
- [ ] 11.3 Unit tests for guards
- [ ] 11.4 Unit tests for actions
- [ ] 11.5 Integration tests for Modbus FSM
- [ ] 11.6 Test event queue from ISR
- [ ] 11.7 Test timeout handling
- [ ] 11.8 Performance benchmarking

## 12. Review
- [ ] 12.1 Code review
- [ ] 12.2 Architecture review
- [ ] 12.3 Performance review
- [ ] 12.4 Update CHANGELOG
