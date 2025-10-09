/**
 * @file test_mb_qos.cpp
 * @brief Unit tests for QoS and backpressure management (Gate 24 validation).
 */

#include <gtest/gtest.h>
#include <modbus/mb_qos.h>
#include <vector>
#include <algorithm>
#include <chrono>

/* ========================================================================== */
/* Test Transaction Structure                                                 */
/* ========================================================================== */

struct TestTransaction {
    uint8_t  slave_address;
    uint8_t  function_code;
    uint32_t deadline_ms;
    uint32_t enqueue_timestamp;
    mb_qos_priority_t priority;
    int      id;  // For tracking
};

/* ========================================================================== */
/* Mock Timestamp Function                                                    */
/* ========================================================================== */

static uint32_t g_mock_time_ms = 0;

static uint32_t mock_now_ms(void) {
    return g_mock_time_ms;
}

static void advance_time(uint32_t ms) {
    g_mock_time_ms += ms;
}

/* ========================================================================== */
/* Priority Detection Tests                                                   */
/* ========================================================================== */

TEST(QoSPriorityTest, FCBasedPriority) {
    // High priority FCs: 05, 06, 08
    EXPECT_TRUE(mb_qos_is_high_priority_fc(0x05));  // Write Single Coil
    EXPECT_TRUE(mb_qos_is_high_priority_fc(0x06));  // Write Single Register
    EXPECT_TRUE(mb_qos_is_high_priority_fc(0x08));  // Diagnostics
    
    // Normal priority FCs
    EXPECT_FALSE(mb_qos_is_high_priority_fc(0x01)); // Read Coils
    EXPECT_FALSE(mb_qos_is_high_priority_fc(0x03)); // Read Holding Registers
    EXPECT_FALSE(mb_qos_is_high_priority_fc(0x04)); // Read Input Registers
    EXPECT_FALSE(mb_qos_is_high_priority_fc(0x10)); // Write Multiple Registers
}

/* ========================================================================== */
/* Context Initialization Tests                                               */
/* ========================================================================== */

class QoSContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_mock_time_ms = 0;
        
        config.high_queue_slots = high_slots;
        config.high_capacity = 8;          // Power of 2
        config.normal_queue_slots = normal_slots;
        config.normal_capacity = 32;       // Power of 2 (was 24)
        config.policy = MB_QOS_POLICY_FC_BASED;
        config.deadline_threshold_ms = 100;
        config.enable_monitoring = true;
        config.now_ms = mock_now_ms;
        
        ASSERT_EQ(mb_qos_ctx_init(&ctx, &config), MB_OK);
    }
    
    void TearDown() override {
        mb_qos_ctx_deinit(&ctx);
    }
    
    mb_qos_ctx_t ctx;
    mb_qos_config_t config;
    
    void *high_slots[8];
    void *normal_slots[32];  // Power of 2 (was 24)
};

TEST_F(QoSContextTest, InitializationSuccess) {
    mb_qos_stats_t stats;
    EXPECT_EQ(mb_qos_get_stats(&ctx, &stats), MB_OK);
    
    EXPECT_EQ(stats.high.enqueued, 0);
    EXPECT_EQ(stats.normal.enqueued, 0);
    EXPECT_EQ(stats.current_high_depth, 0);
    EXPECT_EQ(stats.current_normal_depth, 0);
}

TEST(QoSInitTest, InitializationFailsWithNullPointers) {
    mb_qos_ctx_t c;
    mb_qos_config_t config = {};
    
    EXPECT_EQ(mb_qos_ctx_init(nullptr, &config), MB_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(mb_qos_ctx_init(&c, nullptr), MB_ERR_INVALID_ARGUMENT);
}

TEST(QoSInitTest, InitializationFailsWithInvalidConfig) {
    mb_qos_ctx_t c;
    mb_qos_config_t config = {};
    
    void *slots[8];
    config.high_queue_slots = nullptr;  // Invalid
    config.high_capacity = 8;
    config.normal_queue_slots = slots;
    config.normal_capacity = 8;         // Power of 2
    config.policy = MB_QOS_POLICY_FC_BASED;
    
    EXPECT_EQ(mb_qos_ctx_init(&c, &config), MB_ERR_INVALID_ARGUMENT);
}

/* ========================================================================== */
/* Enqueue/Dequeue Tests                                                      */
/* ========================================================================== */

TEST_F(QoSContextTest, EnqueueHighPriorityTransaction) {
    TestTransaction tx = {};
    tx.function_code = 0x06;  // High priority
    tx.id = 1;
    
    EXPECT_EQ(mb_qos_enqueue(&ctx, &tx), MB_OK);
    
    mb_qos_stats_t stats;
    mb_qos_get_stats(&ctx, &stats);
    
    EXPECT_EQ(stats.high.enqueued, 1);
    EXPECT_EQ(stats.normal.enqueued, 0);
    EXPECT_EQ(stats.current_high_depth, 1);
}

TEST_F(QoSContextTest, EnqueueNormalPriorityTransaction) {
    TestTransaction tx = {};
    tx.function_code = 0x03;  // Normal priority
    tx.id = 1;
    
    EXPECT_EQ(mb_qos_enqueue(&ctx, &tx), MB_OK);
    
    mb_qos_stats_t stats;
    mb_qos_get_stats(&ctx, &stats);
    
    EXPECT_EQ(stats.high.enqueued, 0);
    EXPECT_EQ(stats.normal.enqueued, 1);
    EXPECT_EQ(stats.current_normal_depth, 1);
}

TEST_F(QoSContextTest, DequeueRespectsHighPriority) {
    TestTransaction tx_high = {};
    tx_high.function_code = 0x06;  // High
    tx_high.id = 1;
    
    TestTransaction tx_normal = {};
    tx_normal.function_code = 0x03;  // Normal
    tx_normal.id = 2;
    
    // Enqueue normal first, then high
    ASSERT_EQ(mb_qos_enqueue(&ctx, &tx_normal), MB_OK);
    ASSERT_EQ(mb_qos_enqueue(&ctx, &tx_high), MB_OK);
    
    // Should dequeue high priority first
    TestTransaction *result = (TestTransaction *)mb_qos_dequeue(&ctx);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->id, 1);  // High priority transaction
    
    // Next should be normal
    result = (TestTransaction *)mb_qos_dequeue(&ctx);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->id, 2);  // Normal priority transaction
}

/* ========================================================================== */
/* Backpressure Tests                                                         */
/* ========================================================================== */

TEST_F(QoSContextTest, NormalQueueFullAppliesBackpressure) {
    std::vector<TestTransaction> transactions(35);  // More than capacity (32)
    
    // Fill normal priority queue
    size_t enqueued = 0;
    for (size_t i = 0; i < transactions.size(); i++) {
        transactions[i].function_code = 0x03;  // Normal priority
        transactions[i].id = static_cast<int>(i);
        
        mb_err_t err = mb_qos_enqueue(&ctx, &transactions[i]);
        if (err == MB_OK) {
            enqueued++;
        } else {
            EXPECT_EQ(err, MB_ERR_BUSY);  // Backpressure applied
            break;
        }
    }
    
    // Should have enqueued up to capacity (32 - 1 for ring buffer)
    EXPECT_LE(enqueued, static_cast<size_t>(32));
    
    mb_qos_stats_t stats;
    mb_qos_get_stats(&ctx, &stats);
    
    EXPECT_GT(stats.normal.rejected, 0);
    EXPECT_GT(stats.queue_full_events, 0);
}

TEST_F(QoSContextTest, HighPriorityBypassesBackpressure) {
    // Fill normal queue to capacity (32 - 1 for SPSC ring buffer)
    std::vector<TestTransaction> normal_txs(32);
    for (size_t i = 0; i < normal_txs.size(); i++) {
        normal_txs[i].function_code = 0x03;
        normal_txs[i].id = static_cast<int>(i);
        mb_qos_enqueue(&ctx, &normal_txs[i]);
    }
    
    // Normal queue should be full now
    TestTransaction normal_tx = {};
    normal_tx.function_code = 0x03;
    EXPECT_EQ(mb_qos_enqueue(&ctx, &normal_tx), MB_ERR_BUSY);
    
    // But high priority should still work
    TestTransaction high_tx = {};
    high_tx.function_code = 0x06;  // High priority
    EXPECT_EQ(mb_qos_enqueue(&ctx, &high_tx), MB_OK);
}

/* ========================================================================== */
/* Latency Tracking Tests                                                     */
/* ========================================================================== */

TEST_F(QoSContextTest, LatencyTrackingWorks) {
    TestTransaction tx = {};
    tx.function_code = 0x03;
    tx.id = 1;
    
    g_mock_time_ms = 100;
    ASSERT_EQ(mb_qos_enqueue(&ctx, &tx), MB_OK);
    
    TestTransaction *dequeued = (TestTransaction *)mb_qos_dequeue(&ctx);
    ASSERT_NE(dequeued, nullptr);
    
    g_mock_time_ms = 150;  // 50 ms elapsed
    mb_qos_complete(&ctx, dequeued);
    
    mb_qos_stats_t stats;
    mb_qos_get_stats(&ctx, &stats);
    
    EXPECT_EQ(stats.normal.completed, 1);
    EXPECT_EQ(stats.normal.min_latency_ms, 50);
    EXPECT_EQ(stats.normal.max_latency_ms, 50);
    EXPECT_EQ(stats.normal.avg_latency_ms, 50);
}

TEST_F(QoSContextTest, DeadlineMissDetection) {
    TestTransaction tx = {};
    tx.function_code = 0x03;
    tx.deadline_ms = 200;
    
    g_mock_time_ms = 100;
    ASSERT_EQ(mb_qos_enqueue(&ctx, &tx), MB_OK);
    
    TestTransaction *dequeued = (TestTransaction *)mb_qos_dequeue(&ctx);
    ASSERT_NE(dequeued, nullptr);
    
    g_mock_time_ms = 250;  // Past deadline
    mb_qos_complete(&ctx, dequeued);
    
    mb_qos_stats_t stats;
    mb_qos_get_stats(&ctx, &stats);
    
    EXPECT_EQ(stats.normal.deadline_misses, 1);
}

/* ========================================================================== */
/* Gate 24 Validation: Chaos Test Under Load                                 */
/* ========================================================================== */

TEST(Gate24Validation, CriticalTransactionsMeetLatencyTargets) {
    mb_qos_ctx_t ctx;
    mb_qos_config_t config;
    
    void *high_slots[8];
    void *normal_slots[32];  // Power of 2 (was 24)
    
    g_mock_time_ms = 0;
    
    config.high_queue_slots = high_slots;
    config.high_capacity = 8;
    config.normal_queue_slots = normal_slots;
    config.normal_capacity = 32;  // Power of 2 (was 24)
    config.policy = MB_QOS_POLICY_FC_BASED;
    config.deadline_threshold_ms = 100;
    config.enable_monitoring = true;
    config.now_ms = mock_now_ms;
    
    ASSERT_EQ(mb_qos_ctx_init(&ctx, &config), MB_OK);
    
    // Simulate heavy load: 80% normal, 20% high priority
    constexpr int NUM_TRANSACTIONS = 100;
    std::vector<TestTransaction> transactions(NUM_TRANSACTIONS);
    
    for (int i = 0; i < NUM_TRANSACTIONS; i++) {
        transactions[i].id = i;
        transactions[i].function_code = (i % 5 == 0) ? 0x06 : 0x03;  // 20% high, 80% normal
        transactions[i].deadline_ms = g_mock_time_ms + 500;
        
        mb_err_t err = mb_qos_enqueue(&ctx, &transactions[i]);
        
        // High priority should never be rejected
        if (transactions[i].function_code == 0x06) {
            EXPECT_NE(err, MB_ERR_BUSY) << "High priority transaction rejected!";
        }
        
        // Process some transactions to avoid queue overflow
        if (i % 5 == 0) {
            TestTransaction *tx = (TestTransaction *)mb_qos_dequeue(&ctx);
            if (tx) {
                advance_time(10);  // Simulate 10ms processing
                mb_qos_complete(&ctx, tx);
            }
        }
    }
    
    // Process remaining transactions
    while (true) {
        TestTransaction *tx = (TestTransaction *)mb_qos_dequeue(&ctx);
        if (!tx) break;
        
        advance_time(10);
        mb_qos_complete(&ctx, tx);
    }
    
    // Validate statistics
    mb_qos_stats_t stats;
    mb_qos_get_stats(&ctx, &stats);
    
    std::cout << "\n=== Gate 24 Chaos Test Results ===" << std::endl;
    std::cout << "High Priority:" << std::endl;
    std::cout << "  Enqueued: " << stats.high.enqueued << std::endl;
    std::cout << "  Completed: " << stats.high.completed << std::endl;
    std::cout << "  Rejected: " << stats.high.rejected << std::endl;
    std::cout << "  Avg latency: " << stats.high.avg_latency_ms << " ms" << std::endl;
    std::cout << "  Max latency: " << stats.high.max_latency_ms << " ms" << std::endl;
    std::cout << "  Deadline misses: " << stats.high.deadline_misses << std::endl;
    
    std::cout << "\nNormal Priority:" << std::endl;
    std::cout << "  Enqueued: " << stats.normal.enqueued << std::endl;
    std::cout << "  Completed: " << stats.normal.completed << std::endl;
    std::cout << "  Rejected: " << stats.normal.rejected << std::endl;
    std::cout << "  Avg latency: " << stats.normal.avg_latency_ms << " ms" << std::endl;
    std::cout << "  Max latency: " << stats.normal.max_latency_ms << " ms" << std::endl;
    
    std::cout << "\nOverall:" << std::endl;
    std::cout << "  Queue full events: " << stats.queue_full_events << std::endl;
    std::cout << "  Priority inversions: " << stats.priority_inversions << std::endl;
    
    // Critical assertions
    EXPECT_EQ(stats.high.rejected, 0) << "High priority transactions should never be rejected";
    EXPECT_EQ(stats.priority_inversions, 0) << "Priority inversions detected";
    EXPECT_LT(stats.high.max_latency_ms, 200) << "High priority exceeded latency target";
    
    mb_qos_ctx_deinit(&ctx);
}

/* ========================================================================== */
/* Policy Tests                                                               */
/* ========================================================================== */

TEST(QoSPolicyTest, DeadlineBasedPolicy) {
    mb_qos_ctx_t ctx;
    mb_qos_config_t config;
    
    void *high_slots[8];
    void *normal_slots[32];  // Power of 2 (was 24)
    
    g_mock_time_ms = 1000;
    
    config.high_queue_slots = high_slots;
    config.high_capacity = 8;
    config.normal_queue_slots = normal_slots;
    config.normal_capacity = 32;  // Power of 2 (was 24)
    config.policy = MB_QOS_POLICY_DEADLINE_BASED;
    config.deadline_threshold_ms = 50;  // Urgent if < 50ms to deadline
    config.enable_monitoring = false;
    config.now_ms = mock_now_ms;
    
    ASSERT_EQ(mb_qos_ctx_init(&ctx, &config), MB_OK);
    
    // Transaction with urgent deadline (40ms away)
    TestTransaction tx_urgent = {};
    tx_urgent.function_code = 0x03;  // Would be normal priority in FC-based
    tx_urgent.deadline_ms = 1040;  // 40ms away
    tx_urgent.id = 1;
    
    // Transaction with relaxed deadline (200ms away)
    TestTransaction tx_relaxed = {};
    tx_relaxed.function_code = 0x03;
    tx_relaxed.deadline_ms = 1200;  // 200ms away
    tx_relaxed.id = 2;
    
    ASSERT_EQ(mb_qos_enqueue(&ctx, &tx_relaxed), MB_OK);
    ASSERT_EQ(mb_qos_enqueue(&ctx, &tx_urgent), MB_OK);
    
    // Should dequeue urgent first (high priority due to deadline)
    TestTransaction *result = (TestTransaction *)mb_qos_dequeue(&ctx);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->id, 1);  // Urgent transaction
    
    mb_qos_ctx_deinit(&ctx);
}

/* ========================================================================== */
/* Statistics Tests                                                           */
/* ========================================================================== */

TEST_F(QoSContextTest, StatisticsReset) {
    TestTransaction tx = {};
    tx.function_code = 0x06;
    
    mb_qos_enqueue(&ctx, &tx);
    
    mb_qos_stats_t stats;
    mb_qos_get_stats(&ctx, &stats);
    EXPECT_GT(stats.high.enqueued, 0);
    
    mb_qos_reset_stats(&ctx);
    
    mb_qos_get_stats(&ctx, &stats);
    EXPECT_EQ(stats.high.enqueued, 0);
    EXPECT_EQ(stats.normal.enqueued, 0);
}
