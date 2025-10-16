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
#include <modbus/internal/fsm.h>
#include "gtest/gtest.h"

extern "C" {
extern void mock_advance_time(uint16_t ms);
}

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
static bool timeout_action_called = false;
static bool drop_callback_called = false;
static uint8_t drop_callback_last_event = 0U;

static uint16_t fake_now_value = 0U;

static void reset_test_flags() {
    action_start_called = false;
    action_next_called = false;
    action_error_called = false;
    guard_deny_called = false;
    guard_result = true;
    default_action_called = false;
    timeout_action_called = false;
    drop_callback_called = false;
    drop_callback_last_event = 0U;
    fake_now_value = 0U;
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

static void timeout_default_action(fsm_t *fsm) {
    (void)fsm;
    timeout_action_called = true;
}

static uint16_t fake_now(void) {
    return fake_now_value;
}

static void on_event_drop(fsm_t *fsm, uint8_t event) {
    (void)fsm;
    drop_callback_called = true;
    drop_callback_last_event = event;
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

TEST(FsmMisc, HandleEventWithNullFsm) {
    fsm_handle_event(nullptr, 0x42U);
    SUCCEED();
}

TEST(FsmMisc, RunWithNullFsm) {
    fsm_run(nullptr);
    SUCCEED();
}

TEST_F(FsmTest, QueueFullDropsEvents) {
    const auto initial_tail = static_cast<uint8_t>(fsm.event_queue.tail);
    for (uint8_t i = 0; i < (FSM_EVENT_QUEUE_SIZE - 1U); ++i) {
        fsm_handle_event(&fsm, static_cast<uint8_t>(i));
    }

    const auto tail_after_fill = static_cast<uint8_t>(fsm.event_queue.tail);
    EXPECT_NE(initial_tail, tail_after_fill);

    fsm_handle_event(&fsm, 0xAAU);
    EXPECT_EQ(tail_after_fill, fsm.event_queue.tail);
}

TEST(FsmTimeout, TimeoutTriggersEvent) {
    reset_test_flags();

    static const fsm_state_t timeout_state = {
        "TIMEOUT",
        0xA0,
        nullptr,
        0,
        timeout_default_action,
        5U
    };

    fsm_t timeout_fsm{};
    fsm_init(&timeout_fsm, &timeout_state, nullptr);

    mock_advance_time(6U);
    fsm_run(&timeout_fsm);

    EXPECT_TRUE(timeout_fsm.has_timeout);
    EXPECT_TRUE(timeout_action_called);
}

TEST(FsmConfig, ExternalQueueAndCallbacks) {
    reset_test_flags();

    fsm_t cfg_fsm{};
    uint8_t external_queue[2]{};

    fsm_config_t config{};
    config.queue_storage = external_queue;
    config.queue_capacity = sizeof external_queue;
    config.time_fn = fake_now;
    config.on_event_drop = on_event_drop;

    fake_now_value = 42U;

    fsm_init_with_config(&cfg_fsm, &state_idle, NULL, &config);

    EXPECT_EQ(fake_now_value, cfg_fsm.state_entry_time);
    ASSERT_EQ(static_cast<fsm_queue_index_t>(sizeof external_queue), cfg_fsm.event_queue.capacity);
    ASSERT_EQ(external_queue, cfg_fsm.event_queue.events);

    // First event fits in the queue
    fsm_handle_event(&cfg_fsm, TEST_EVENT_START);
    EXPECT_FALSE(drop_callback_called);

    // Second event overflows the tiny queue and should trigger the drop callback
    fsm_handle_event(&cfg_fsm, TEST_EVENT_NEXT);
    EXPECT_TRUE(drop_callback_called);
    EXPECT_EQ(TEST_EVENT_NEXT, drop_callback_last_event);

    // Process the queued event and ensure custom time source is used for transitions
    fake_now_value = 100U;
    fsm_run(&cfg_fsm);
    EXPECT_TRUE(action_start_called);
    EXPECT_EQ(cfg_fsm.state_entry_time, fake_now_value);
}

