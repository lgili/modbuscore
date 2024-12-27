/**
 * @file fsm.c
 * @brief Finite State Machine (FSM) framework implementation.
 *
 * This file implements the functions declared in fsm.h. It provides a simple
 * event-driven finite state machine framework where events are queued, and the
 * state machine processes these events to possibly trigger state transitions.
 * Actions and guard conditions can be defined for each transition.
 *
 * The FSM is designed to be non-blocking and can be integrated into a main loop
 * or a cooperative multitasking environment. This makes it suitable for use
 * in embedded systems, including those implementing communication protocols
 * such as Modbus.
 *
 * @author  Luiz Carlos Gili
 * @date    2024-12-20
 */

#include <modbus/fsm.h>
#include <stddef.h>

/**
 * @brief Initializes the finite state machine (FSM).
 *
 * Sets the initial state, user data, and clears the event queue.
 */
void fsm_init(fsm_t *fsm, const fsm_state_t *initial_state, void *user_data) {
    if (!fsm || !initial_state) {
        return; // Invalid arguments
    }
    fsm->current_state = initial_state;
    fsm->user_data = user_data;
    fsm->event_queue.head = 0;
    fsm->event_queue.tail = 0;
}

/**
 * @brief Adds an event to the FSM's event queue.
 *
 * If the queue is full, the event is discarded.
 */
void fsm_handle_event(fsm_t *fsm, uint8_t event) {
    if (!fsm) {
        return;
    }
    uint8_t next_tail = (uint8_t)((fsm->event_queue.tail + 1U) % FSM_EVENT_QUEUE_SIZE);
    if (next_tail != fsm->event_queue.head) {
        fsm->event_queue.events[fsm->event_queue.tail] = event;
        fsm->event_queue.tail = next_tail;
    } 
    // If the queue is full, the event is simply not queued.
}

/**
 * @brief Processes any pending events from the queue and executes state transitions if needed.
 *
 * If no events are available, it executes the current state's default action (if any).
 */
void fsm_run(fsm_t *fsm) {
    if (!fsm || !fsm->current_state) {
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
    fsm->event_queue.head = (uint8_t)((fsm->event_queue.head + 1U) % FSM_EVENT_QUEUE_SIZE);

    // Try to find a matching transition for this event
    const fsm_state_t *state = fsm->current_state;
    const fsm_transition_t *transitions = state->transitions;
    uint8_t num_transitions = state->num_transitions;
    bool event_processed = false;

    for (uint8_t i = 0; i < num_transitions; i++) {
        const fsm_transition_t *transition = &transitions[i];
        if (transition->event == event) {
            // Check guard
            if (transition->guard == NULL || transition->guard(fsm)) {
                // Execute action if any
                if (transition->action) {
                    transition->action(fsm);
                }
                // Transition to next state
                fsm->current_state = transition->next_state;
            }
            event_processed = true;
            break;
        }
    }

    // If event not processed, we can ignore it or handle "unknown event" scenario here.
    // Currently, we do nothing special for unrecognized events.

    // After handling this event, if no transitions occurred or no matching event,
    // consider calling default action. This decision depends on the desired behavior.
    // Typically, default_action is called only if no events are pending,
    // so we won't call it here unless no transitions matched and no more events are queued.
    if (!event_processed && fsm->event_queue.head == fsm->event_queue.tail && fsm->current_state->default_action) {
        fsm->current_state->default_action(fsm);
    }
}
