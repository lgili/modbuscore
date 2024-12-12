/******************************************************************************  
* The information contained herein is confidential property of Embraco. The  
* user, copying, transfer or disclosure of such information is prohibited  
* except by express written agreement with Embraco.  
******************************************************************************/ 
  

/**  
* @file    fsm.c  
* @brief   Finite State Machine (FSM).  
* @author   
* @date    2024-10-11  
*/  
#include <string.h>
#include "fsm.h"   

/**   
* @brief Initializes the finite state machine (FSM).
*   
* This function initializes the FSM with the given initial state and user-specific 
* data. It also initializes the event queue.   
*   
* @param fsm Pointer to the FSM structure.   
* @param initial_state Pointer to the initial state of the FSM.  
* @param user_data Pointer to user-specific data to be associated with the FSM. 
*/   
void fsm_init(fsm_t *fsm, const fsm_state_t *initial_state, void *user_data) { 
    fsm->current_state = initial_state;   
    fsm->user_data = user_data;   
    // Initialize event queue 
    fsm->event_queue.head = 0; 
    fsm->event_queue.tail = 0; 
}     
  

/**   
* @brief Adds an event to the FSM's event queue. 
*   
* This function safely adds an event to the FSM's event queue. It is designed to be called from both  
* ISR and main loop contexts.   
*   
* @param fsm Pointer to the FSM structure.   
* @param event The event to add to the queue. 
*/   
void fsm_handle_event(fsm_t *fsm, uint8_t event) {   
    uint8_t next_tail = (fsm->event_queue.tail + 1) % FSM_EVENT_QUEUE_SIZE; 
    if (next_tail != fsm->event_queue.head) { 
        // Add event to the queue 
        fsm->event_queue.events[fsm->event_queue.tail] = event; 
        // Update the tail index atomically 
        fsm->event_queue.tail = next_tail; 
    } 
    // If the queue is full, the event is discarded 
}   

  
/**   
* @brief Processes pending events from the FSM's event queue. 
*   
* This function retrieves events from the event queue and processes them by evaluating  
* the current state's transitions. If a valid transition is found based on the event  
* and guard condition (if provided), the FSM will execute the associated action and  
* move to the next state.   
*   
* @param fsm Pointer to the FSM structure.  
*/   
void fsm_run(fsm_t *fsm) { 
    const fsm_state_t *state = fsm->current_state;  

    // Retrieve the next event from the queue 
    uint8_t event = fsm->event_queue.events[fsm->event_queue.head]; 
    // Update the head index 
    fsm->event_queue.head = (fsm->event_queue.head + 1) % FSM_EVENT_QUEUE_SIZE;   

    const fsm_transition_t *transitions = state->transitions; 
    uint8_t num_transitions = state->num_transitions;   

    // Iterate through all transitions to find a match for the event 
    bool event_processed = false; 
    for (uint8_t i = 0; i < num_transitions; ++i) { 
        const fsm_transition_t *transition = &transitions[i]; 
        if (transition->event == event) { 
            // Check if the guard condition (if any) allows the transition
            if (transition->guard == NULL || transition->guard(fsm)) { 
                // Execute the action associated with the transition (if any)
                if (transition->action != NULL) { 
                    transition->action(fsm); 
                } 

                // Move to the next state 
                fsm->current_state = transition->next_state;
                state = fsm->current_state; // Update current state
            } 

            // Event processed 
            event_processed = true; 
            break; 
        } 
    }   

    if (!event_processed) { 
        // Handle unknown event if needed 
    } 

    // If no events are pending, execute the default action of the current state
    if (state->default_action != NULL) { 
        state->default_action(fsm); 
    } 
} 

 

 