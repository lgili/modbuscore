#include <gtest/gtest.h>

extern "C" {
#include <modbus/mb_log.h>
}

#include <string>

namespace {
struct SinkState {
    bool called = false;
    mb_log_level_t level = MB_LOG_INFO_LEVEL;
    std::string message;
};
} // namespace

static SinkState *g_sink_state = nullptr;

static void mb_test_sink(mb_log_level_t level, char *msg)
{
    if (g_sink_state == nullptr) {
        return;
    }

    g_sink_state->called = true;
    g_sink_state->level = level;
    g_sink_state->message = (msg != nullptr) ? msg : "";
}

class MbLogTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        MB_LOG_INIT();
    }

    void TearDown() override
    {
        if (g_sink_state != nullptr) {
            (void)MB_LOG_UNSUBSCRIBE(mb_test_sink);
        }
        g_sink_state = nullptr;
    }
};

TEST_F(MbLogTest, DispatchReachesSubscribedSink)
{
    SinkState state{};
    g_sink_state = &state;

    ASSERT_EQ(MB_LOG_ERR_NONE, MB_LOG_SUBSCRIBE(mb_test_sink, MB_LOG_DEBUG_LEVEL));

    MB_LOG_INFO("hello %d", 42);

    EXPECT_TRUE(state.called);
    EXPECT_EQ(MB_LOG_INFO_LEVEL, state.level);
    EXPECT_EQ(std::string("hello 42"), state.message);
}

TEST_F(MbLogTest, ThresholdFiltersMessages)
{
    SinkState state{};
    g_sink_state = &state;

    ASSERT_EQ(MB_LOG_ERR_NONE, MB_LOG_SUBSCRIBE(mb_test_sink, MB_LOG_ERROR_LEVEL));

    MB_LOG_WARNING("ignored");
    EXPECT_FALSE(state.called);

    MB_LOG_ERROR("boom");
    EXPECT_TRUE(state.called);
    EXPECT_EQ(MB_LOG_ERROR_LEVEL, state.level);
    EXPECT_EQ(std::string("boom"), state.message);
}

TEST_F(MbLogTest, BootstrapIsIdempotent)
{
    // Should be safe to invoke multiple times without crashing or double
    // registering sinks.
    mb_log_bootstrap_defaults();
    mb_log_bootstrap_defaults();
    SUCCEED();
}
