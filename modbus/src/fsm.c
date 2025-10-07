/**
 * @file fsm.c
 * @brief Finite State Machine (FSM) framework implementation.
 *
 * This file implements the functions declared in `fsm.h`. It provides a simple
 * event-driven finite state machine framework where events are queued, and the
 * state machine processes these events to potentially trigger state transitions.
 * Actions and guard conditions can be defined for each transition.
 *
 * The FSM is designed to be non-blocking and can be integrated into a main loop
 * or a cooperative multitasking environment. This makes it suitable for use
 * in embedded systems, including those implementing communication protocols
 * such as Modbus Client and Server.
 *
 * **Key Features:**
 * - Initialization of the FSM with an initial state and optional user data.
 * - Handling and queuing of events in a thread-safe manner.
 * - Processing of events to execute state transitions based on defined transitions.
 * - Execution of actions associated with transitions.
 * - Support for guard conditions to control transition eligibility.
 *
 * **Usage Example:**
 * @code
 * // Define action and guard callbacks
 * void on_enter_state(fsm_t *fsm) { 
 *     // Action to perform upon entering a new state
 * }
 * 
 * bool guard_condition(fsm_t *fsm) { 
 *     // Condition to allow transition
 *     return true; 
 * }
 *
 * // Define transitions for the IDLE state
 * static const fsm_transition_t state_idle_transitions[] = {
 *     FSM_TRANSITION(EVENT_START, state_running, on_enter_state, guard_condition)
 * };
 *
 * // Define the IDLE state
 * static const fsm_state_t state_idle = FSM_STATE("IDLE", 0, state_idle_transitions, NULL);
 *
 * // Define transitions for the RUNNING state
 * static const fsm_transition_t state_running_transitions[] = {
 *     FSM_TRANSITION(EVENT_STOP, state_idle, on_exit_state, NULL)
 * };
 *
 * // Define the RUNNING state
 * static const fsm_state_t state_running = FSM_STATE("RUNNING", 1, state_running_transitions, on_run_action);
 *
 * // Initialize and use the FSM
 * fsm_t my_fsm;
 * fsm_init(&my_fsm, &state_idle, NULL);
 * fsm_handle_event(&my_fsm, EVENT_START);
 * fsm_run(&my_fsm);  // Processes EVENT_START and transitions to RUNNING state.
 * @endcode
 *
 * @note
 * - Ensure that states are defined before they are referenced in transitions.
 * - The FSM framework is designed to be thread-safe; however, ensure that
 *   event handling and state transitions are managed appropriately in concurrent environments.
 * 
 * @see fsm.h
 *
 * @ingroup FSM
 * @{
 */

#include <modbus/fsm.h>
#include <stddef.h>

extern uint16_t get_current_time_ms(void);

/**
 * @brief Initializes the finite state machine (FSM).
 *
 * Sets the initial state, associates optional user data, and clears the event queue.
 * This function must be called before using the FSM to ensure it starts in a known state.
 *
 * @param[in,out] fsm           Pointer to the FSM instance to initialize.
 * @param[in]     initial_state Pointer to the initial state of the FSM.
 * @param[in]     user_data     Pointer to user-defined data (can be `NULL`).
 *
 * @warning
 * - Both `fsm` and `initial_state` must not be `NULL`. If either is `NULL`, the function returns immediately without initializing.
 *
 * @example
 * ```c
 * fsm_t my_fsm;
 * fsm_init(&my_fsm, &state_idle, &app_context);
 * ```
 */
static inline uint16_t fsm_now(const fsm_t *fsm)
{
    if (fsm == NULL) {
        return 0U;
    }

    if (fsm->time_fn != NULL) {
        return fsm->time_fn();
    }

    return get_current_time_ms();
}

static void fsm_queue_reset(fsm_t *fsm)
{
    if (fsm == NULL) {
        return;
    }

    fsm->event_queue.head = 0U;
    fsm->event_queue.tail = 0U;
}

static void fsm_bind_queue(fsm_t *fsm, const fsm_config_t *config)
{
    if (fsm == NULL) {
        return;
    }

    if (config != NULL && config->queue_storage != NULL && config->queue_capacity > 0U) {
        fsm->event_queue.events = config->queue_storage;
        fsm->event_queue.capacity = config->queue_capacity;
    }
#if FSM_CONF_INLINE_QUEUE
    else {
        fsm->event_queue.events = fsm->inline_queue;
        fsm->event_queue.capacity = FSM_EVENT_QUEUE_SIZE;
    }
#else
    else {
        fsm->event_queue.events = NULL;
        fsm->event_queue.capacity = 0U;
    }
#endif

    if (fsm->event_queue.capacity < 2U) {
        fsm->event_queue.events = NULL;
        fsm->event_queue.capacity = 0U;
    }

    fsm_queue_reset(fsm);
}

void fsm_init_with_config(fsm_t *fsm,
                          const fsm_state_t *initial_state,
                          void *user_data,
                          const fsm_config_t *config)
{
    if (fsm == NULL || initial_state == NULL) {
        return;
    }

    fsm->current_state = initial_state;
    fsm->user_data = user_data;
    fsm->time_fn = (config != NULL && config->time_fn != NULL) ? config->time_fn : get_current_time_ms;
    fsm->event_drop_cb = (config != NULL) ? config->on_event_drop : NULL;

    fsm_bind_queue(fsm, config);

    fsm->state_entry_time = fsm_now(fsm);
    fsm->has_timeout = false;
}

void fsm_init(fsm_t *fsm, const fsm_state_t *initial_state, void *user_data)
{
    fsm_init_with_config(fsm, initial_state, user_data, NULL);
}

/**
 * @brief Adds an event to the FSM's event queue.
 *
 * This function queues an event to be processed by the FSM. If the event queue is full,
 * the new event is discarded to prevent overflow. It is safe to call this function
 * from both interrupt service routines (ISRs) and the main loop.
 *
 * @param[in,out] fsm   Pointer to the FSM instance.
 * @param[in]     event Event to handle.
 *
 * @warning
 * - If the event queue is full, the new event will be discarded. Consider increasing
 *   `FSM_EVENT_QUEUE_SIZE` if event loss is unacceptable.
 *
 * @example
 * ```c
 * fsm_handle_event(&my_fsm, EVENT_START);
 * ```
 */
void fsm_handle_event(fsm_t *fsm, uint8_t event) {
    if (!fsm) {
        // Optionally, log an error or handle the invalid FSM pointer
        return;
    }
    if (fsm->event_queue.events == NULL || fsm->event_queue.capacity == 0U) {
        return;
    }

    fsm_queue_index_t next_tail = fsm->event_queue.tail + 1U;
    if (next_tail >= fsm->event_queue.capacity) {
        next_tail = 0U;
    }

    if (next_tail != fsm->event_queue.head) {
        fsm->event_queue.events[fsm->event_queue.tail] = event;
        fsm->event_queue.tail = next_tail;
    } else if (fsm->event_drop_cb != NULL) {
        fsm->event_drop_cb(fsm, event);
    }
}

/**
 * @brief Processes any pending events from the queue and executes state transitions if needed.
 *
 * This function should be called regularly (e.g., in the main loop). It retrieves events
 * from the queue and checks transitions in the current state. If a matching transition
 * is found and its guard returns `true`, it executes the action and changes state.
 * If no event matches or no event is pending, it executes the state's default action (if any).
 *
 * @param[in,out] fsm Pointer to the FSM instance.
 *
 * @warning
 * - Ensure that `fsm_init` has been called before using `fsm_run`.
 * - This function is non-blocking and should be integrated into an iterative loop.
 *
 * @example
 * ```c
 * while (1) {
 *     fsm_run(&my_fsm);
 *     // Other application code
 * }
 * ```
 */
void fsm_run(fsm_t *fsm) {
    if (!fsm || !fsm->current_state) {
        // Optionally, log an error or handle the invalid FSM state
        return;
    }

    const fsm_state_t *s = fsm->current_state;
    fsm->has_timeout = false;

    // 1) se este estado tem timeout configurado, verifica se excedeu
    if (s->timeout_ms > 0U) {
        const uint16_t now = fsm_now(fsm);
        const uint16_t elapsed = now - fsm->state_entry_time;
        if (elapsed >= s->timeout_ms) {
            // limpa fila e dispara evento de timeout
            fsm_queue_reset(fsm);
            fsm->has_timeout = true;
            fsm_handle_event(fsm, FSM_EVENT_STATE_TIMEOUT);
            // atualiza timestamp (para não disparar repetidamente)
            fsm->state_entry_time = now;
        }
    }

    if (fsm->event_queue.events == NULL || fsm->event_queue.capacity == 0U) {
        if (fsm->current_state->default_action) {
            fsm->current_state->default_action(fsm);
        }
        return;
    }

    // Check if we have an event to process
    if (fsm->event_queue.head == fsm->event_queue.tail) {
        // No events pending, execute default action if available
        if (fsm->current_state->default_action) {
            fsm->current_state->default_action(fsm);
        }
        return;
    }

    // Get next event from the queue
    uint8_t event = fsm->event_queue.events[fsm->event_queue.head];
    fsm->event_queue.head = (fsm->event_queue.head + 1U);
    if (fsm->event_queue.head >= fsm->event_queue.capacity) {
        fsm->event_queue.head = 0U;
    }

    // Try to find a matching transition for this event
    const fsm_state_t *state = fsm->current_state;
    const fsm_transition_t *transitions = state->transitions;
    uint8_t num_transitions = state->num_transitions;
    bool event_processed = false;

    for (uint8_t i = 0; i < num_transitions; i++) {
        const fsm_transition_t *transition = &transitions[i];
        if (transition->event == event) {
            bool guard_passed = (transition->guard == NULL) || transition->guard(fsm);
            if (guard_passed) {
                if (transition->action) {
                    transition->action(fsm);
                }
                fsm->current_state = transition->next_state;
                fsm->state_entry_time = fsm_now(fsm);
                event_processed = true;
            } else {
                /* Guard rejected transition, requeue event for future evaluation */
                fsm_handle_event(fsm, event);
            }
            break;
        }
    }

    // If the event was not processed (no matching transition), it is ignored.
    // Optionally, handle unprocessed events here (e.g., logging or error handling).

    // After handling this event, if no transitions occurred and no more events are queued,
    // execute the default action if defined.
    if (!event_processed && fsm->event_queue.head == fsm->event_queue.tail && fsm->current_state->default_action) {
        fsm->current_state->default_action(fsm);
    }
}

