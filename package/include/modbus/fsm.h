/**
 * @file fsm.h
 * @brief Finite State Machine (FSM) framework for event-driven execution.
 *
 * This header defines a generic Finite State Machine (FSM) framework, allowing
 * the creation of states, events, transitions, and actions. It is completely
 * independent of any hardware or protocol, making it suitable for use in a wide
 * range of embedded applications, including Modbus Client and Server implementations.
 *
 * **Features:**
 * - Each state can have multiple transitions triggered by specific events.
 * - Transitions can have optional guard functions (conditions) to determine if
 *   the transition is allowed.
 * - Actions can be associated with transitions to perform operations during
 *   state changes.
 * - Provides an event queue to safely handle events from both ISR and main loop.
 * - Non-blocking state handling: `fsm_run()` processes events incrementally.
 *
 * **Example Usage:**
 * @code
 * // Define action and guard callbacks
 * void on_enter_state(fsm_t *fsm) { 
 *     // Action to perform upon entering the state
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
 * const fsm_state_t state_idle = FSM_STATE("IDLE", 0, state_idle_transitions, NULL, 0);
 *
 * // Define transitions for the RUNNING state
 * static const fsm_transition_t state_running_transitions[] = {
 *     FSM_TRANSITION(EVENT_STOP, state_idle, on_exit_state, NULL)
 * };
 *
 * // Define the RUNNING state
 * const fsm_state_t state_running = FSM_STATE("RUNNING", 1, state_running_transitions, on_run_action, 0);
 *
 * // Initialize and use the FSM
 * fsm_t my_fsm;
 * fsm_init(&my_fsm, &state_idle, NULL);
 * fsm_handle_event(&my_fsm, EVENT_START);
 * fsm_run(&my_fsm);  // Processes EVENT_START and transitions to RUNNING state.
 * @endcode
 *
 * **Notes:**
 * - Ensure that states are defined before they are referenced in transitions.
 * - The FSM framework is designed to be thread-safe; however, ensure that
 *   event handling and state transitions are managed appropriately in concurrent environments.
 * 
 * @note
 * - Adjust `FSM_EVENT_QUEUE_SIZE` as necessary based on the expected event load.
 * - For larger applications, consider implementing more efficient event queue mechanisms.
 *
 * @author
 * Luiz Carlos Gili
 * 
 * @date
 * 2024-12-20
 *
 * @addtogroup FSM
 * @{
 */

#ifndef FSM_H
#define FSM_H

#include <stdint.h>
#include <stdbool.h>

#ifndef FSM_EVENT_QUEUE_SIZE
#define FSM_EVENT_QUEUE_SIZE 20
#endif

#ifndef FSM_CONF_INLINE_QUEUE
#define FSM_CONF_INLINE_QUEUE 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FSM_EVENT_STATE_TIMEOUT
#define FSM_EVENT_STATE_TIMEOUT  0xFF
#endif

typedef uint16_t fsm_queue_index_t;

/**
 * @brief Opaque structure representing the FSM.
 *
 * The FSM structure encapsulates the current state, user-defined data, and the event queue.
 */
typedef struct fsm fsm_t;

/**
 * @brief Opaque structure representing a state in the FSM.
 *
 * Each state is uniquely identified and contains transitions that define possible state changes.
 */
typedef struct fsm_state fsm_state_t;

/**
 * @brief Action function pointer type.
 *
 * Actions are functions executed when a transition occurs.
 *
 * @param fsm Pointer to the FSM instance.
 *
 * @example
 * ```c
 * void on_enter_state(fsm_t *fsm) {
 *     // Perform actions upon entering a new state
 * }
 * ```
 */
typedef void (*fsm_action_t)(fsm_t *fsm);

/**
 * @brief Guard function pointer type.
 *
 * Guards are boolean conditions that determine whether a transition can occur.
 *
 * @param fsm Pointer to the FSM instance.
 * @return `true` if the transition is allowed, `false` otherwise.
 *
 * @example
 * ```c
 * bool guard_condition(fsm_t *fsm) {
 *     // Check if a certain condition is met
 *     return (some_condition);
 * }
 * ```
 */
typedef bool (*fsm_guard_t)(fsm_t *fsm);

typedef uint16_t (*fsm_time_fn_t)(void);

typedef void (*fsm_event_drop_cb_t)(fsm_t *fsm, uint8_t event);

/**
 * @brief Structure representing a single transition in the FSM.
 *
 * Each transition is triggered by a specific event and can optionally have a guard
 * and an action. If the guard evaluates to true, the FSM transitions to the next
 * state and executes the action (if not NULL).
 */
typedef struct {
    uint8_t event;                  /**< Event that triggers the transition. */
    const fsm_state_t *next_state;  /**< Pointer to the next state after transition. */
    fsm_action_t action;            /**< Action executed during the transition (optional). */
    fsm_guard_t guard;              /**< Guard condition for the transition (optional). */
} fsm_transition_t;

/**
 * @brief Structure representing a state in the FSM.
 *
 * Each state has:
 * - An optional name (for debugging)
 * - A numeric ID
 * - A set of transitions triggered by events
 * - A default action executed if no events are pending (optional)
 */
struct fsm_state {
    const char *name;                       /**< Human-readable name of the state (optional). */
    uint8_t id;                             /**< Numeric ID of the state. */
    const fsm_transition_t *transitions;    /**< Array of transitions for this state. */
    uint8_t num_transitions;                /**< Number of transitions in the array. */
    fsm_action_t default_action;            /**< Default action executed when no events are pending (optional). */
    uint16_t timeout_ms; 	                /**< Timeout *deste* estado (0=desabilitado) */

};

/**
 * @brief Structure for the FSM's event queue.
 *
 * Implements a circular buffer storing events until processed by `fsm_run()`.
 */
typedef struct {
    uint8_t *events;                                /**< Circular buffer of events. */
    volatile fsm_queue_index_t head;                /**< Index of the next event to process. */
    volatile fsm_queue_index_t tail;                /**< Index where the next event will be added. */
    fsm_queue_index_t capacity;                     /**< Total number of entries in the queue. */
} fsm_event_queue_t;

typedef struct {
    uint8_t *queue_storage;             /**< Optional external queue storage (must hold at least 2 entries). */
    fsm_queue_index_t queue_capacity;   /**< Capacity of the external queue. */
    fsm_time_fn_t time_fn;              /**< Optional time function override. */
    fsm_event_drop_cb_t on_event_drop;  /**< Optional callback for dropped events. */
} fsm_config_t;

/**
 * @brief Structure representing the finite state machine.
 *
 * The FSM holds:
 * - The current state
 * - User-defined data pointer for application-specific context
 * - An event queue for storing incoming events
 */
struct fsm {
    const fsm_state_t *current_state;   /**< Pointer to the current state. */
    void *user_data;                    /**< User data associated with the FSM instance. */
    fsm_event_queue_t event_queue;      /**< Event queue for handling asynchronous events. */
    fsm_time_fn_t time_fn;              /**< Time retrieval callback. */
    fsm_event_drop_cb_t event_drop_cb;  /**< Callback invoked when an event is dropped. */
    uint16_t state_entry_time;
    bool has_timeout;
#if FSM_CONF_INLINE_QUEUE
    uint8_t inline_queue[FSM_EVENT_QUEUE_SIZE]; /**< Default inline queue storage. */
#endif
};

/**
 * @brief Initializes the FSM.
 *
 * Sets the initial state and associates optional user data. Resets the event queue.
 *
 * @param fsm            Pointer to the FSM instance.
 * @param initial_state  Pointer to the initial state.
 * @param user_data      Pointer to user-defined data (can be `NULL`).
 *
 * @example
 * ```c
 * fsm_t my_fsm;
 * fsm_init(&my_fsm, &state_idle, &app_context);
 * ```
 */
void fsm_init(fsm_t *fsm, const fsm_state_t *initial_state, void *user_data);

/**
 * @brief Initializes the FSM with explicit configuration.
 *
 * Allows callers to override the time source, provide external queue storage,
 * and install a callback for dropped events. If @p config is NULL, the FSM
 * falls back to the default inline queue (when @ref FSM_CONF_INLINE_QUEUE is 1).
 * When supplying external storage, ensure @p queue_capacity is at least 2 so
 * that the circular buffer can distinguish between empty and full states.
 *
 * @param fsm            Pointer to the FSM instance.
 * @param initial_state  Pointer to the initial state.
 * @param user_data      Pointer to user-defined data (can be `NULL`).
 * @param config         Optional configuration structure (can be `NULL`).
 */
void fsm_init_with_config(fsm_t *fsm,
                          const fsm_state_t *initial_state,
                          void *user_data,
                          const fsm_config_t *config);

/**
 * @brief Queues an event for the FSM.
 *
 * This function adds an event to the FSM's event queue. If the queue is full, the event is discarded.
 *
 * @param fsm   Pointer to the FSM instance.
 * @param event Event to handle.
 *
 * @example
 * ```c
 * fsm_handle_event(&my_fsm, EVENT_START);
 * ```
 */
void fsm_handle_event(fsm_t *fsm, uint8_t event);

/**
 * @brief Runs the FSM to process pending events.
 *
 * This function should be called regularly (e.g., in the main loop). It retrieves events
 * from the queue and checks transitions in the current state. If a matching transition
 * is found and its guard returns `true`, it executes the action and changes state.
 * If no event matches or no event is pending, it executes the state's default action (if any).
 *
 * @param fsm Pointer to the FSM instance.
 *
 * @example
 * ```c
 * while (1) {
 *     fsm_run(&my_fsm);
 *     // Other application code
 * }
 * ```
 */
void fsm_run(fsm_t *fsm);

/**
 * @brief Helper macro to define a transition.
 *
 * **Example:**
 * @code
 * FSM_TRANSITION(EVENT_X, next_state, my_action, my_guard);
 * @endcode
 *
 * @param _event      Triggering event.
 * @param _next_state Next state to transition to if guard is `true`.
 * @param _action     Action to execute during the transition (or `NULL`).
 * @param _guard      Guard function to evaluate (or `NULL`).
 *
 * @return Initialized `fsm_transition_t` structure.
 */
#define FSM_TRANSITION(_event, _next_state, _action, _guard) \
    { _event, &_next_state, _action, _guard }

/**
 * @brief Helper macro to define a state.
 *
 * **Example:**
 * @code
 * static const fsm_transition_t idle_transitions[] = {
 *     FSM_TRANSITION(EVENT_START, state_running, start_action, NULL)
 * };
 * const fsm_state_t state_idle = FSM_STATE("IDLE", 0, idle_transitions, NULL, 0);
 * @endcode
 *
 * @param _name            Name of the state (string).
 * @param _state_id        Numeric ID of the state.
 * @param _transitions     Array of transitions.
 * @param _default_action  Default action function to execute when no events are pending (or `NULL`).
 * @param _timeout         Timeout in milliseconds before @ref FSM_EVENT_STATE_TIMEOUT is emitted (0 to disable).
 *
 * @return Initialized `fsm_state_t` structure.
 */
#define FSM_STATE(_name, _state_id, _transitions, _default_action, _timeout) \
	{ #_name, _state_id, _transitions, sizeof(_transitions)/sizeof(fsm_transition_t), _default_action, _timeout }

#ifdef __cplusplus
}
#endif

#endif /* FSM_H */

/** @} */
