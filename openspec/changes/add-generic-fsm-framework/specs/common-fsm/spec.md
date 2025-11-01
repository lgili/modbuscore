# Generic FSM Framework - Delta Spec

## ADDED Requirements

### Requirement: Generic FSM Framework
The library SHALL provide a generic, reusable finite state machine framework.

#### Scenario: FSM is protocol-agnostic
- **GIVEN** FSM framework implementation
- **WHEN** reviewing code
- **THEN** no Modbus-specific code SHALL exist
- **AND** FSM SHALL be usable for any protocol
- **AND** FSM SHALL be usable for non-protocol applications

#### Scenario: FSM header is self-contained
- **GIVEN** `include/modbuscore/common/fsm.h`
- **WHEN** including in user code
- **THEN** header SHALL not require Modbus headers
- **AND** header SHALL compile standalone
- **AND** header SHALL be C89 compatible

### Requirement: Event Queue
The FSM SHALL provide ISR-safe event queuing.

#### Scenario: Queue events from ISR
- **GIVEN** FSM initialized
- **WHEN** `fsm_handle_event()` called from ISR
- **THEN** event SHALL be queued
- **AND** function SHALL be ISR-safe
- **AND** function SHALL complete in bounded time
- **AND** critical section SHALL protect queue

#### Scenario: Event queue is circular buffer
- **GIVEN** event queue with size N
- **WHEN** N+1 events are queued
- **THEN** oldest event SHALL remain if not processed
- **AND** newest event SHALL be discarded
- **AND** queue SHALL not overflow

#### Scenario: Queue size is configurable
- **GIVEN** FSM configuration
- **WHEN** defining FSM_EVENT_QUEUE_SIZE
- **THEN** queue size SHALL be customizable at compile time
- **AND** default SHALL be 10 events

### Requirement: Data-Driven States
States SHALL be defined as data structures, not code.

#### Scenario: State definition
- **GIVEN** state structure
- **WHEN** defining state
- **THEN** state SHALL have name (optional, for debugging)
- **AND** state SHALL have numeric ID
- **AND** state SHALL have array of transitions
- **AND** state SHALL have optional default action

**Example:**
```c
static const fsm_transition_t idle_transitions[] = {
    FSM_TRANSITION(EVENT_START, state_running, action_start, NULL)
};

const fsm_state_t state_idle = FSM_STATE("IDLE", 0, idle_transitions, NULL);
```

#### Scenario: Add state without changing core
- **GIVEN** existing FSM with N states
- **WHEN** adding new state
- **THEN** no changes to fsm.c SHALL be required
- **AND** only state definition data SHALL be added

### Requirement: Transitions with Guards
Transitions SHALL support conditional guards.

#### Scenario: Guard controls transition
- **GIVEN** transition with guard function
- **WHEN** event triggers transition
- **THEN** guard SHALL be evaluated
- **AND** if guard returns true, transition SHALL occur
- **AND** if guard returns false, transition SHALL NOT occur
- **AND** state SHALL remain unchanged

#### Scenario: NULL guard always allows transition
- **GIVEN** transition with NULL guard
- **WHEN** event triggers transition
- **THEN** transition SHALL always occur

**Example:**
```c
bool guard_valid_address(fsm_t *fsm) {
    return check_address_valid(fsm->user_data);
}

FSM_TRANSITION(EVENT_ADDR, state_process, action_parse, guard_valid_address)
```

### Requirement: Transition Actions
Transitions SHALL execute actions during state changes.

#### Scenario: Action executes on transition
- **GIVEN** transition with action function
- **WHEN** transition occurs
- **THEN** action SHALL be executed
- **AND** action SHALL receive FSM instance
- **AND** action SHALL have access to user_data

#### Scenario: NULL action is allowed
- **GIVEN** transition with NULL action
- **WHEN** transition occurs
- **THEN** only state SHALL change
- **AND** no action SHALL execute

**Example:**
```c
void action_start_processing(fsm_t *fsm) {
    app_data_t *data = (app_data_t*)fsm->user_data;
    data->processing_started = true;
}

FSM_TRANSITION(EVENT_START, state_process, action_start_processing, NULL)
```

### Requirement: Default Actions
States SHALL support default actions executed when idle.

#### Scenario: Default action in idle state
- **GIVEN** state with default_action
- **WHEN** `fsm_run()` called with no events
- **THEN** default_action SHALL be executed
- **AND** default_action receives FSM instance

#### Scenario: Timeout checking via default action
- **GIVEN** RECEIVING state with timeout check
- **WHEN** no events pending
- **THEN** default_action SHALL check timeout
- **AND** if timeout, SHALL inject timeout event

**Example:**
```c
void default_check_timeout(fsm_t *fsm) {
    app_data_t *data = (app_data_t*)fsm->user_data;
    if (timeout_expired(data)) {
        fsm_handle_event(fsm, EVENT_TIMEOUT);
    }
}

const fsm_state_t state_receiving = FSM_STATE("RX", 1, transitions, default_check_timeout);
```

### Requirement: Configurable Critical Sections
Critical section implementation SHALL be configurable.

#### Scenario: User-defined critical sections
- **GIVEN** FSM framework
- **WHEN** user defines FSM_ENTER_CRITICAL and FSM_EXIT_CRITICAL
- **THEN** FSM SHALL use user definitions
- **AND** user SHALL provide interrupt disable/enable

#### Scenario: Default critical sections
- **GIVEN** FSM framework
- **WHEN** FSM_ENTER_CRITICAL not defined
- **THEN** default weak implementation SHALL be used
- **AND** documentation SHALL warn about default

#### Scenario: No critical sections (single-threaded)
- **GIVEN** single-threaded application
- **WHEN** FSM_ENTER_CRITICAL defined as empty
- **THEN** FSM SHALL work without overhead

**Example configurations:**
```c
// ARM Cortex-M
#define FSM_ENTER_CRITICAL() uint32_t primask = __get_PRIMASK(); __disable_irq()
#define FSM_EXIT_CRITICAL()  __set_PRIMASK(primask)

// RL78
#define FSM_ENTER_CRITICAL() __disable_interrupt()
#define FSM_EXIT_CRITICAL()  __enable_interrupt()

// Single-threaded (no protection needed)
#define FSM_ENTER_CRITICAL() ((void)0)
#define FSM_EXIT_CRITICAL()  ((void)0)
```

### Requirement: User Data Context
FSM SHALL support user-defined context data.

#### Scenario: Associate user data with FSM
- **GIVEN** FSM instance
- **WHEN** `fsm_init(&fsm, &initial_state, &user_data)` called
- **THEN** user_data SHALL be stored in FSM
- **AND** user_data SHALL be accessible in actions/guards
- **AND** FSM SHALL NOT modify user_data

#### Scenario: Access user data in action
- **GIVEN** action function
- **WHEN** action executes
- **THEN** action SHALL cast fsm->user_data to app type
- **AND** action SHALL access application state

**Example:**
```c
typedef struct {
    uint8_t rx_buffer[256];
    size_t rx_length;
    uint8_t address;
} modbus_context_t;

void action_parse_address(fsm_t *fsm) {
    modbus_context_t *ctx = (modbus_context_t*)fsm->user_data;
    ctx->address = ctx->rx_buffer[0];
}
```

### Requirement: Helper Macros
The framework SHALL provide convenient macros for defining states and transitions.

#### Scenario: FSM_STATE macro
- **GIVEN** FSM_STATE macro
- **WHEN** defining state
- **THEN** macro SHALL initialize fsm_state_t correctly
- **AND** macro SHALL calculate num_transitions automatically
- **AND** macro SHALL be type-safe

#### Scenario: FSM_TRANSITION macro
- **GIVEN** FSM_TRANSITION macro
- **WHEN** defining transition
- **THEN** macro SHALL initialize fsm_transition_t correctly
- **AND** macro SHALL take event, next_state, action, guard
- **AND** macro SHALL be readable

### Requirement: Non-Blocking Operation
FSM SHALL operate in non-blocking manner.

#### Scenario: fsm_run() is non-blocking
- **GIVEN** FSM with events queued
- **WHEN** `fsm_run()` called
- **THEN** function SHALL process one event
- **AND** function SHALL return quickly
- **AND** function SHALL NOT block waiting for events

#### Scenario: Integration with main loop
- **GIVEN** application main loop
- **WHEN** calling fsm_run() repeatedly
- **THEN** events SHALL be processed incrementally
- **AND** application SHALL remain responsive

**Example:**
```c
while (1) {
    fsm_run(&my_fsm);
    // Other application tasks
    do_other_work();
}
```

### Requirement: Memory Efficiency
FSM SHALL have minimal memory footprint for embedded systems.

#### Scenario: FSM structure size
- **GIVEN** fsm_t structure
- **WHEN** calculating size
- **THEN** overhead SHALL be <= 32 bytes + queue size
- **AND** size SHALL be predictable at compile time

#### Scenario: State definition size
- **GIVEN** fsm_state_t structure
- **WHEN** defining state
- **THEN** state SHALL be const (in flash/ROM)
- **AND** state SHALL NOT consume RAM

### Requirement: Modbus Protocol Engine Integration
The Modbus protocol engine SHALL be refactored to use FSM.

#### Scenario: Modbus states defined declaratively
- **GIVEN** Modbus server implementation
- **WHEN** reviewing state definitions
- **THEN** all states SHALL be defined as fsm_state_t
- **AND** all transitions SHALL be defined as fsm_transition_t
- **AND** no switch statements SHALL exist for state machine

#### Scenario: Event-driven Modbus processing
- **GIVEN** Modbus engine using FSM
- **WHEN** byte received from ISR
- **THEN** ISR SHALL call fsm_handle_event(EVENT_RX_BYTE)
- **AND** main loop SHALL call fsm_run()
- **AND** state transitions SHALL occur automatically

### Requirement: Documentation
FSM framework SHALL be thoroughly documented.

#### Scenario: Framework documentation
- **GIVEN** docs/advanced/fsm-framework.md
- **WHEN** developer wants to use FSM
- **THEN** guide SHALL explain concepts
- **AND** guide SHALL show usage examples
- **AND** guide SHALL document critical section config
- **AND** guide SHALL explain state diagram notation

#### Scenario: API reference
- **GIVEN** FSM header file
- **WHEN** reviewing documentation
- **THEN** all functions SHALL be documented
- **AND** all structures SHALL be documented
- **AND** all macros SHALL be documented
- **AND** examples SHALL be provided

## MODIFIED Requirements

### Requirement: Protocol Engine State Machine (MODIFIED)
The protocol engine SHALL use generic FSM framework instead of switch-based FSM.

**Previous implementation:**
```c
switch(engine->state) {
    case MBC_ENGINE_STATE_IDLE:
        // ...
        break;
    case MBC_ENGINE_STATE_RECEIVING:
        // ...
        break;
}
```

**New implementation:**
```c
static const fsm_transition_t idle_transitions[] = {
    FSM_TRANSITION(EVENT_RX_BYTE, state_receiving, action_start_rx, NULL)
};

const fsm_state_t state_idle = FSM_STATE("IDLE", 0, idle_transitions, NULL);
```

#### Scenario: Refactored engine uses FSM
- **GIVEN** protocol engine
- **WHEN** reviewing implementation
- **THEN** engine SHALL use fsm_t instance
- **AND** engine SHALL call fsm_run() to process events
- **AND** engine SHALL inject events via fsm_handle_event()

## REMOVED Requirements

None - This is an additive change that refactors existing code.

## RENAMED Requirements

None.
