/******************************************************************************
 * The information contained herein is confidential property of Embraco. The
 * user, copying, transfer or disclosure of such information is prohibited
 * except by express written agreement with Embraco.
 ******************************************************************************/
/**

* @file fsm.h
* @brief Finite State Machine (FSM).
*
* This header defines the structures and functions necessary for implementing
* a simple finite state machine. It provides the ability to define states,
* transitions, actions, and guards for event-based state transitions.
*
* @author  Luiz Carlos
* @date    2024-10-11
*/

#ifndef FSM_H
#define FSM_H

#include <stdint.h>
#include <stdbool.h>

/**
* Forward declaration of FSM structures.
*/
typedef struct fsm fsm_t;
typedef struct fsm_state fsm_state_t;

/**
* @typedef fsm_action_t
* @brief Function pointer type for actions.
*
* Defines a function pointer type for actions to be executed during state transitions.
* These functions receive the FSM structure as a parameter to allow custom logic
* related to the current FSM state.
*
* @param fsm Pointer to the FSM structure.
*/
typedef void (*fsm_action_t)(fsm_t *fsm);

/**
* @typedef fsm_guard_t
* @brief Function pointer type for guards.
*
* Defines a function pointer type for guards (conditions) that determine whether
* a state transition can occur. These functions return a boolean indicating
* whether the transition is allowed.
*
* @param fsm Pointer to the FSM structure.
* @return true if the transition is allowed, false otherwise.
*/
typedef bool (*fsm_guard_t)(fsm_t *fsm);

/**
* @struct fsm_transition_t
* @brief Defines a transition between states.
*
* This structure represents a single transition from one state to another in
* response to a specific event. The transition can optionally be guarded by
* a condition and can trigger an action when executed.
*/
typedef struct
{
    uint8_t event;                  /**< Event that triggers the transition. */
    const fsm_state_t *next_state;  /**< Pointer to the next state. */
    fsm_action_t action;            /**< Action to be executed during the transition. */
    fsm_guard_t guard;              /**< Optional guard condition for the transition. */
} fsm_transition_t;

/**
* @struct fsm_state_t
* @brief Defines a state in the FSM.
*
* A state can have a set of possible transitions that are triggered by events.
* The state also has an optional name for debugging purposes.
*/
struct fsm_state
{
    const char *name;                       /**< Optional name of the state, useful for debugging. */
    uint8_t id;                             /**< Id of the state, useful for use for other functions. */
    const fsm_transition_t *transitions;    /**< Array of possible transitions from this state. */
    uint8_t num_transitions;                /**< Number of transitions from this state. */
    fsm_action_t default_action;            /**< Default action executed when no event are pending */
};

/**
* @def FSM_EVENT_QUEUE_SIZE
* @brief Defines the size of the event queue.
*
* Adjust this value based on the maximum expected number of events in the queue.
*/
#define FSM_EVENT_QUEUE_SIZE 10

/**
* @struct fsm_event_queue_t
* @brief Event queue structure for managing FSM events.
*
* This structure implements a circular buffer (ring buffer) to store events safely between
* the ISR and the main loop.
*/
typedef struct
{
    volatile uint8_t events[FSM_EVENT_QUEUE_SIZE];  /**< Circular buffer to store events. */
    volatile uint8_t head;                          /**< Index of the next event to process. */
    volatile uint8_t tail;                          /**< Index where the next event will be added. */
} fsm_event_queue_t;

/**
* @struct fsm
* @brief Represents the finite state machine.
*
* This structure holds the current state of the FSM and additional data used for
* managing transitions and events.
*/
struct fsm
{
    const fsm_state_t *current_state;   /**< Pointer to the current state of the FSM. */
    void *user_data;                    /**< User-defined data, specific to the application. */
    fsm_event_queue_t event_queue;      /**< Event queue for handling events safely. */
};

/**
* @brief Initializes the finite state machine (FSM).
*
* This function sets the initial state of the FSM and allows the user to attach
* custom data (user_data) to the FSM for context-specific operations.
*
* @param fsm Pointer to the FSM structure.
* @param initial_state Pointer to the initial state to start the FSM.
* @param user_data Pointer to user-defined data to be associated with the FSM.
*/
void fsm_init(fsm_t *fsm, const fsm_state_t *initial_state, void *user_data);

/**
* @brief Handles an event and processes state transitions.
*
* This function adds an event to the event queue to be processed by the FSM.
*
* @param fsm Pointer to the FSM structure.
* @param event The event to handle.
*/
void fsm_handle_event(fsm_t *fsm, uint8_t event);

/**
* @brief Runs the FSM to process any pending events.
*
* This function should be called regularly to allow the FSM to process any pending
* events from the event queue. If events are pending, they will be handled by the FSM,
* potentially triggering state transitions.
*
* @param fsm Pointer to the FSM structure.
*/
void fsm_run(fsm_t *fsm);

/**
* @brief Macro to define a state transition in the FSM.
*
* This macro is used to define a state transition within the finite state machine (FSM).
* It takes the event that triggers the transition, the next state to transition to, the action
* to be executed during the transition, and an optional guard condition.
*
* @param _event The event that triggers the transition.
* @param _next_state The next state that the FSM will transition to if the guard condition (if any) is satisfied.
* @param _action The action function to execute when the transition occurs (can be NULL).
* @param _guard The guard function to check if the transition should occur (can be NULL).
*/
#define FSM_TRANSITION(_event, _next_state, _action, _guard) \
    { _event, &_next_state, _action, _guard }

/**
* @brief Macro to define an FSM state.
*
* This macro is used to define a state in the FSM. It takes a name for the state (useful for debugging)
* and a list of transitions. The size of the transitions array is automatically calculated.
*
* @param _name The name of the state, provided as a string for debugging purposes.
* @param _state_id The id of the state, provided as a int for debugging purposes or external use.
* @param _transitions The array of transitions that define how the state reacts to different events.
* @param _default_action The default action function to execute when no events are pending.
*/
#define FSM_STATE(_name, _state_id, _transitions, _default_action) \
    { #_name, _state_id, _transitions, sizeof(_transitions) / sizeof(fsm_transition_t),  _default_action}

#endif // FSM_H
