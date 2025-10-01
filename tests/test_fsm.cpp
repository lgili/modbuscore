/**
 * @file test_fsm.cpp
 * @brief Unit tests for the generic FSM implementation using Google Test.
 *
 * This file tests the functionalities defined in fsm.h and fsm.c:
 * - State initialization
 * - Event queuing and processing
 * - Transitions, actions, and guards
 * - Default actions when no events are pending
 *
 * The test defines a simple FSM with a few states and transitions triggered by events.
 * We ensure that:
 * - Events are queued and processed in order.
 * - Actions are executed when transitions occur.
 * - Guards control whether a transition is allowed.
 * - The default action of the current state is called when no events are pending.
 *
 * Author:
 * Date: 2024-12-20
 */

#include <modbus/modbus.h>
#include "gtest/gtest.h"

// Define some custom events
enum {
    TEST_EVENT_START = 1,
    TEST_EVENT_NEXT,
    TEST_EVENT_DENY,
    TEST_EVENT_ERROR
};

// We'll have three states: IDLE, RUNNING, ERROR
enum test_states_t {
    TEST_STATE_IDLE = 0,
    TEST_STATE_RUNNING,
    TEST_STATE_ERROR_STATE
};

extern const fsm_state_t state_idle; 
extern const fsm_state_t state_running;
extern const fsm_state_t state_error;

// Global variables to track actions executed and guard checks
static bool action_start_called = false;
static bool action_next_called = false;
static bool action_error_called = false;
static bool guard_deny_called = false;
static bool guard_result = true;
static bool default_action_called = false;

static void reset_test_flags() {
    action_start_called = false;
    action_next_called = false;
    action_error_called = false;
    guard_deny_called = false;
    guard_result = true;
    default_action_called = false;
}

// Actions
static void action_start(fsm_t *fsm) {
    (void)fsm;
    action_start_called = true;
}

static void action_next(fsm_t *fsm) {
    (void)fsm;
    action_next_called = true;
}

static void action_error_state(fsm_t *fsm) {
    (void)fsm;
    action_error_called = true;
}

static void default_action(fsm_t *fsm) {
    (void)fsm;
    default_action_called = true;
}

// Guards
static bool guard_deny(fsm_t *fsm) {
    (void)fsm;
    guard_deny_called = true;
    return guard_result;
}

// Define transitions
static const fsm_transition_t idle_transitions[] = {
    FSM_TRANSITION(TEST_EVENT_START, state_running, action_start, NULL),
    FSM_TRANSITION(TEST_EVENT_ERROR, state_error, action_error_state, NULL)
};
const fsm_state_t state_idle = FSM_STATE("IDLE", TEST_STATE_IDLE, idle_transitions, default_action, 0);

static const fsm_transition_t running_transitions[] = {
    FSM_TRANSITION(TEST_EVENT_NEXT, state_running, action_next, NULL),
    FSM_TRANSITION(TEST_EVENT_DENY, state_running, action_next, guard_deny),
    FSM_TRANSITION(TEST_EVENT_ERROR, state_error, action_error_state, NULL)
};
const fsm_state_t state_running = FSM_STATE("RUNNING", TEST_STATE_RUNNING, running_transitions, default_action, 0);

const fsm_state_t state_error = {
    "ERROR",
    TEST_STATE_ERROR_STATE,
    nullptr,
    0,
    default_action,
    0
};

// Test Fixture
class FsmTest : public ::testing::Test {
protected:
    fsm_t fsm;
    void SetUp() override {
        reset_test_flags();
        fsm_init(&fsm, &state_idle, NULL);
    }
};

TEST_F(FsmTest, InitialState) {
    // Initially in IDLE state
    EXPECT_EQ(fsm.current_state->id, TEST_STATE_IDLE);
    // No events, calling fsm_run() should call default action
    fsm_run(&fsm);
    EXPECT_TRUE(default_action_called);
}

TEST_F(FsmTest, ProcessSingleEvent) {
    // Send TEST_EVENT_START which should transition to RUNNING state and call action_start
    fsm_handle_event(&fsm, TEST_EVENT_START);
    fsm_run(&fsm);
    EXPECT_TRUE(action_start_called);
    EXPECT_EQ(fsm.current_state->id, TEST_STATE_RUNNING);
}

TEST_F(FsmTest, MultipleEventsQueue) {
    // Send two events and run multiple times
    fsm_handle_event(&fsm, TEST_EVENT_START);
    fsm_run(&fsm);
    EXPECT_EQ(fsm.current_state->id, TEST_STATE_RUNNING);

    // Now in RUNNING, send TEST_EVENT_NEXT
    reset_test_flags();
    fsm_handle_event(&fsm, TEST_EVENT_NEXT);
    fsm_run(&fsm);
    EXPECT_TRUE(action_next_called);
    EXPECT_EQ(fsm.current_state->id, TEST_STATE_RUNNING);
}

TEST_F(FsmTest, GuardCheck) {
    // Move to RUNNING first
    fsm_handle_event(&fsm, TEST_EVENT_START);
    fsm_run(&fsm);

    // In RUNNING, send TEST_EVENT_DENY which has a guard
    reset_test_flags();
    guard_result = false; // guard returns false, so transition should not occur
    fsm_handle_event(&fsm, TEST_EVENT_DENY);
    fsm_run(&fsm);

    // action_next should NOT be called since guard returns false
    EXPECT_FALSE(action_next_called);
    EXPECT_TRUE(guard_deny_called);
    EXPECT_EQ(fsm.current_state->id, TEST_STATE_RUNNING);
}

TEST_F(FsmTest, ErrorTransition) {
    // From IDLE to ERROR
    fsm_handle_event(&fsm, TEST_EVENT_ERROR);
    fsm_run(&fsm);
    EXPECT_TRUE(action_error_called);
    EXPECT_EQ(fsm.current_state->id, TEST_STATE_ERROR_STATE);

    // In ERROR_STATE, no transitions; running fsm_run() calls default_action
    reset_test_flags();
    fsm_run(&fsm);
    EXPECT_TRUE(default_action_called);
}

