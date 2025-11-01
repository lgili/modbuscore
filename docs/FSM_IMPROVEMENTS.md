# FSM Framework Improvements

## Current oldmodbus FSM Analysis

### ✅ Strengths
- Generic & reusable
- ISR-safe event queue
- Data-driven states
- Guards & actions
- Small & clean (~240 lines)

### ⚠️ Weaknesses & Improvement Opportunities

## Proposed Improvements

### 1. **Type Safety** 🛡️

**Problem:** Current design uses `uint8_t` for events/states - easy to mix up.

**Current:**
```c
fsm_handle_event(&fsm, 5);  // What is 5? Magic number!
```

**Improved:**
```c
// Type-safe events
typedef enum {
    MODBUS_EVENT_RX_BYTE = 0,
    MODBUS_EVENT_TIMEOUT,
    MODBUS_EVENT_TX_COMPLETE
} modbus_event_t;

fsm_handle_event(&fsm, MODBUS_EVENT_RX_BYTE);  // Clear!
```

**Implementation:** Use enums instead of uint8_t, add compile-time checks.

---

### 2. **Entry/Exit Actions** 🚪

**Problem:** Actions only on transitions. Sometimes need actions on EVERY entry/exit.

**Current:**
```c
// Must repeat action on every transition TO this state
FSM_TRANSITION(EVENT_A, state_active, action_activate, NULL)
FSM_TRANSITION(EVENT_B, state_active, action_activate, NULL)  // Duplicate!
```

**Improved:**
```c
// Entry action runs ONCE when entering state (from any transition)
const fsm_state_t state_active = {
    .name = "ACTIVE",
    .id = STATE_ACTIVE,
    .entry_action = action_on_enter,    // NEW!
    .exit_action = action_on_exit,      // NEW!
    .transitions = active_transitions,
    .num_transitions = ARRAY_SIZE(active_transitions),
    .default_action = NULL
};
```

**Use Cases:**
- Entry: Initialize state variables, start timers, turn on LED
- Exit: Clean up resources, stop timers, turn off LED

---

### 3. **Null Safety & Validation** ✅

**Problem:** Limited null checking, potential crashes.

**Improved:**
```c
// Add validation macro
#define FSM_ASSERT(cond, msg) \
    do { if (!(cond)) { fsm_error_handler(__FILE__, __LINE__, msg); } } while(0)

void fsm_run(fsm_t *fsm) {
    FSM_ASSERT(fsm != NULL, "FSM is NULL");
    FSM_ASSERT(fsm->current_state != NULL, "Current state is NULL");
    FSM_ASSERT(fsm->current_state->transitions != NULL || fsm->current_state->num_transitions == 0,
               "Invalid transitions array");
    // ... rest of logic
}
```

**Benefits:**
- Catch bugs early in development
- Optional (compile out in release)
- Clear error messages

---

### 4. **Event Queue Overflow Handling** 🚨

**Problem:** Events silently discarded when queue full.

**Current:**
```c
if (next_tail != fsm->event_queue.head) {
    fsm->event_queue.events[fsm->event_queue.tail] = event;
    fsm->event_queue.tail = next_tail;
}
// Event lost! No indication.
```

**Improved:**
```c
typedef void (*fsm_overflow_callback_t)(fsm_t *fsm, uint8_t lost_event);

typedef struct {
    // ... existing fields
    fsm_overflow_callback_t overflow_cb;  // NEW!
    uint32_t overflow_count;              // NEW! Track stats
} fsm_t;

void fsm_handle_event(fsm_t *fsm, uint8_t event) {
    FSM_ENTER_CRITICAL();
    uint8_t next_tail = (fsm->event_queue.tail + 1U) % FSM_EVENT_QUEUE_SIZE;
    if (next_tail != fsm->event_queue.head) {
        fsm->event_queue.events[fsm->event_queue.tail] = event;
        fsm->event_queue.tail = next_tail;
    } else {
        // NEW: Handle overflow
        fsm->overflow_count++;
        if (fsm->overflow_cb) {
            fsm->overflow_cb(fsm, event);  // Notify user
        }
    }
    FSM_EXIT_CRITICAL();
}
```

**Use Cases:**
- Log critical event loss
- Trigger error handling
- Increase queue size at runtime (if dynamic)
- Debug queue sizing issues

---

### 5. **Transition Logging/Tracing** 📊

**Problem:** Hard to debug state machine behavior.

**Improved:**
```c
typedef void (*fsm_trace_callback_t)(
    fsm_t *fsm,
    const fsm_state_t *from,
    const fsm_state_t *to,
    uint8_t event,
    uint32_t timestamp_ms
);

typedef struct {
    // ... existing fields
    fsm_trace_callback_t trace_cb;  // NEW!
} fsm_t;

void fsm_transition(fsm_t *fsm, const fsm_state_t *next_state, uint8_t event) {
    if (fsm->trace_cb) {
        fsm->trace_cb(fsm, fsm->current_state, next_state, event, get_time_ms());
    }
    fsm->current_state = next_state;
}
```

**Benefits:**
- Real-time state diagram
- Event sequence analysis
- Performance profiling
- Production debugging

---

### 6. **Hierarchical States (Optional)** 🌳

**Problem:** Flat FSM can get complex. Sometimes need substates.

**Example Use Case:**
```
ACTIVE
├── PROCESSING
│   ├── VALIDATING
│   └── EXECUTING
└── IDLE
```

**Improved:**
```c
typedef struct fsm_state {
    // ... existing fields
    const fsm_state_t *parent;        // NEW! Parent state
    const fsm_state_t *initial_child; // NEW! Default substate
} fsm_state_t;

// Events not handled in child bubble up to parent
```

**Benefits:**
- Common transitions in parent
- Cleaner state organization
- Reduced duplication

**Note:** This adds complexity - make it OPTIONAL via #define.

---

### 7. **Event Priority** ⚡

**Problem:** All events treated equally. Sometimes need high-priority events.

**Improved:**
```c
typedef struct {
    uint8_t event;
    uint8_t priority;  // NEW! 0 = lowest, 255 = highest
} fsm_event_entry_t;

typedef struct {
    fsm_event_entry_t events[FSM_EVENT_QUEUE_SIZE];
    // ... rest
} fsm_event_queue_t;

// Process highest priority first
void fsm_run(fsm_t *fsm) {
    // Find highest priority event in queue
    uint8_t max_priority = 0;
    uint8_t best_idx = fsm->event_queue.head;

    // ... extract and process highest priority event
}
```

**Use Cases:**
- Emergency stop events (priority 255)
- Timeout events (priority 10)
- Normal events (priority 0)

**Caveat:** Adds overhead. Make OPTIONAL.

---

### 8. **Compile-Time Transition Validation** 🔍

**Problem:** Transition errors only found at runtime.

**Improved:**
```c
// Static assert macro
#define FSM_STATIC_ASSERT(cond, msg) \
    typedef char static_assertion_##msg[(cond)?1:-1]

// Validate at compile time
#define FSM_STATE_DEFINE(_name, _id, _transitions) \
    FSM_STATIC_ASSERT(ARRAY_SIZE(_transitions) > 0, transitions_not_empty); \
    FSM_STATIC_ASSERT(_id < 256, state_id_in_range); \
    { #_name, _id, _transitions, ARRAY_SIZE(_transitions), NULL }
```

**Benefits:**
- Catch errors before running
- No runtime overhead
- Safer state definitions

---

### 9. **State History** 📜

**Problem:** Can't track previous states for debugging.

**Improved:**
```c
#ifndef FSM_HISTORY_SIZE
#define FSM_HISTORY_SIZE 8
#endif

typedef struct {
    const fsm_state_t *state;
    uint8_t event;
    uint32_t timestamp_ms;
} fsm_history_entry_t;

typedef struct {
    // ... existing fields
    fsm_history_entry_t history[FSM_HISTORY_SIZE];  // NEW!
    uint8_t history_idx;                            // NEW!
} fsm_t;

void fsm_record_transition(fsm_t *fsm, const fsm_state_t *new_state, uint8_t event) {
    fsm->history[fsm->history_idx] = (fsm_history_entry_t){
        .state = new_state,
        .event = event,
        .timestamp_ms = get_time_ms()
    };
    fsm->history_idx = (fsm->history_idx + 1) % FSM_HISTORY_SIZE;
}
```

**Use Cases:**
- Post-mortem debugging
- Crash analysis
- Behavior replay
- Test validation

---

### 10. **Return Codes for Actions** ⚠️

**Problem:** Actions can't indicate errors.

**Current:**
```c
typedef void (*fsm_action_t)(fsm_t *fsm);
```

**Improved:**
```c
typedef enum {
    FSM_ACTION_OK = 0,
    FSM_ACTION_ERROR,
    FSM_ACTION_RETRY,
    FSM_ACTION_ABORT
} fsm_action_result_t;

typedef fsm_action_result_t (*fsm_action_t)(fsm_t *fsm);

// In fsm_run():
if (transition->action) {
    fsm_action_result_t result = transition->action(fsm);
    if (result == FSM_ACTION_ERROR) {
        fsm_handle_event(fsm, EVENT_ERROR);  // Trigger error handling
        return;
    }
}
```

**Use Cases:**
- Action validation
- Error propagation
- Conditional transitions

---

### 11. **Timeout Management Built-In** ⏱️

**Problem:** Timeout checking is manual in default_action.

**Improved:**
```c
typedef struct {
    // ... existing fields
    uint32_t timeout_ms;        // NEW! State timeout
    uint8_t timeout_event;      // NEW! Event to fire on timeout
    uint32_t entered_at_ms;     // NEW! When entered this state
} fsm_state_runtime_t;

// In fsm_run():
void fsm_check_timeout(fsm_t *fsm) {
    fsm_state_runtime_t *runtime = &fsm->state_runtime[fsm->current_state->id];
    if (runtime->timeout_ms > 0) {
        uint32_t elapsed = get_time_ms() - runtime->entered_at_ms;
        if (elapsed >= runtime->timeout_ms) {
            fsm_handle_event(fsm, runtime->timeout_event);
        }
    }
}
```

**Benefits:**
- Declarative timeouts
- No manual checking
- Consistent behavior

---

### 12. **Better Macros** 🎯

**Current Macros:**
```c
#define FSM_STATE(_name, _id, _transitions, _default_action)
#define FSM_TRANSITION(_event, _next, _action, _guard)
```

**Improved Macros:**
```c
// More flexible state definition
#define FSM_STATE_FULL(_name, _id, _entry, _exit, _default, _trans) \
    { .name = #_name, .id = _id, \
      .entry_action = _entry, .exit_action = _exit, .default_action = _default, \
      .transitions = _trans, .num_transitions = ARRAY_SIZE(_trans) }

// Simple state (no entry/exit)
#define FSM_STATE_SIMPLE(_name, _id, _trans) \
    FSM_STATE_FULL(_name, _id, NULL, NULL, NULL, _trans)

// Transition with all options
#define FSM_TRANSITION_FULL(_event, _next, _action, _guard) \
    { .event = _event, .next_state = &_next, .action = _action, .guard = _guard }

// Simple transition
#define FSM_TRANSITION_SIMPLE(_event, _next) \
    FSM_TRANSITION_FULL(_event, _next, NULL, NULL)

// Guarded transition
#define FSM_TRANSITION_GUARDED(_event, _next, _guard) \
    FSM_TRANSITION_FULL(_event, _next, NULL, _guard)
```

---

## Summary of Improvements

| Improvement | Complexity | Benefit | Make Optional? |
|-------------|-----------|---------|----------------|
| Type Safety | Low | High | No |
| Entry/Exit Actions | Low | High | No |
| Null Safety | Low | High | Yes (debug only) |
| Overflow Callback | Low | Medium | No |
| Tracing | Low | High | Yes |
| Hierarchical States | High | Medium | **YES** |
| Event Priority | Medium | Medium | **YES** |
| Compile Validation | Low | Medium | No |
| State History | Low | High | Yes |
| Action Return Codes | Low | Medium | No |
| Timeout Built-in | Medium | High | Yes |
| Better Macros | Low | High | No |

## Recommended Implementation

### Phase 1: Core Improvements (Must Have)
1. ✅ Type safety (enums)
2. ✅ Entry/Exit actions
3. ✅ Null safety (debug mode)
4. ✅ Overflow callback
5. ✅ Better macros

**Effort:** ~8 hours
**Risk:** Low

### Phase 2: Debug Features (Should Have)
6. ✅ Tracing callback
7. ✅ State history
8. ✅ Compile validation

**Effort:** ~4 hours
**Risk:** Low

### Phase 3: Advanced Features (Optional)
9. ⏳ Action return codes
10. ⏳ Timeout built-in
11. ⏳ Event priority
12. ⏳ Hierarchical states

**Effort:** ~12 hours
**Risk:** Medium

## Configuration Approach

Make features configurable via defines:

```c
// fsm_config.h
#define FSM_ENABLE_ENTRY_EXIT_ACTIONS  1
#define FSM_ENABLE_TRACING             1
#define FSM_ENABLE_HISTORY             1
#define FSM_ENABLE_OVERFLOW_CALLBACK   1
#define FSM_ENABLE_HIERARCHICAL        0  // Advanced, optional
#define FSM_ENABLE_PRIORITY_QUEUE      0  // Advanced, optional
#define FSM_ENABLE_TIMEOUT_BUILTIN     1  // Very useful!

#define FSM_EVENT_QUEUE_SIZE           16
#define FSM_HISTORY_SIZE               8
```

## Example: Improved FSM Usage

```c
// Type-safe events
typedef enum {
    MODBUS_EVENT_RX_BYTE = 0,
    MODBUS_EVENT_TIMEOUT,
    MODBUS_EVENT_TX_COMPLETE,
    MODBUS_EVENT_ERROR
} modbus_event_t;

// Entry action
fsm_action_result_t enter_receiving(fsm_t *fsm) {
    modbus_ctx_t *ctx = fsm->user_data;
    ctx->rx_length = 0;
    start_timeout_timer();
    return FSM_ACTION_OK;
}

// Exit action
fsm_action_result_t exit_receiving(fsm_t *fsm) {
    stop_timeout_timer();
    return FSM_ACTION_OK;
}

// Improved state definition
static const fsm_transition_t receiving_transitions[] = {
    FSM_TRANSITION_SIMPLE(MODBUS_EVENT_RX_BYTE, state_parsing),
    FSM_TRANSITION_SIMPLE(MODBUS_EVENT_TIMEOUT, state_error)
};

const fsm_state_t state_receiving = FSM_STATE_FULL(
    RECEIVING,                      // Name
    STATE_RECEIVING,                // ID
    enter_receiving,                // Entry action
    exit_receiving,                 // Exit action
    check_rx_complete,              // Default action
    receiving_transitions           // Transitions
);

// Overflow callback
void handle_overflow(fsm_t *fsm, uint8_t lost_event) {
    log_error("FSM queue full! Lost event: %d", lost_event);
    metrics.queue_overflows++;
}

// Trace callback
void trace_transition(fsm_t *fsm, const fsm_state_t *from,
                      const fsm_state_t *to, uint8_t event, uint32_t ts) {
    log_debug("FSM: %s -> %s (event=%d) @ %lu ms",
              from->name, to->name, event, ts);
}

// Initialize with callbacks
fsm_init(&my_fsm, &state_idle, &modbus_ctx);
fsm_set_overflow_callback(&my_fsm, handle_overflow);
fsm_set_trace_callback(&my_fsm, trace_transition);
```

## Conclusion

**YES! FSM can be significantly improved** while maintaining simplicity.

**Recommended approach:**
1. Port oldmodbus FSM as-is first (get it working)
2. Add Phase 1 improvements (core, low-risk)
3. Add Phase 2 improvements (debug features)
4. Evaluate Phase 3 (only if needed)

**Benefits:**
- ✅ Safer (null checks, overflow handling)
- ✅ Easier to debug (tracing, history)
- ✅ More flexible (entry/exit actions)
- ✅ Better errors (overflow callback)
- ✅ Cleaner code (better macros)
- ✅ Still simple (~400 lines with improvements)

**Action:** Add these improvements to the FSM OpenSpec!

---

**Document Version:** 1.0
**Date:** 2025-01-28
**Verdict:** FSM can be much better - improve it! 🚀
