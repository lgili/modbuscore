/**
 * @file test_mb_txpool.cpp
 * @brief Unit tests and stress tests for transaction pool (Gate 22 validation).
 */

#include <gtest/gtest.h>
#include <modbus/internal/mb_txpool.h>
#include <modbus/port/mutex.h>
#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>
#include <chrono>

/* ========================================================================== */
/* Test Transaction Structure                                                 */
/* ========================================================================== */

struct test_transaction {
    uint8_t  slave_addr;
    uint16_t reg_addr;
    uint16_t reg_count;
    uint8_t  fc;
    uint32_t seq_number;
    uint64_t timestamp_us;
};

/* ========================================================================== */
/* Basic Transaction Pool Tests                                              */
/* ========================================================================== */

class TxPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        mb_txpool_init(&pool, storage, sizeof(test_transaction), CAPACITY);
    }
    
    void TearDown() override {
        // No explicit deinit needed
    }
    
    static constexpr size_t CAPACITY = 16;
    uint8_t storage[CAPACITY * sizeof(test_transaction)];
    mb_txpool_t pool;
};

TEST_F(TxPoolTest, InitializationSuccess) {
    EXPECT_EQ(mb_txpool_capacity(&pool), CAPACITY);
    EXPECT_EQ(mb_txpool_available(&pool), CAPACITY);
    EXPECT_EQ(mb_txpool_in_use(&pool), 0);
    EXPECT_FALSE(mb_txpool_is_empty(&pool));
}

TEST_F(TxPoolTest, InitializationFailsWithInvalidArgs) {
    mb_txpool_t p;
    uint8_t buf[256];
    
    EXPECT_EQ(mb_txpool_init(nullptr, buf, 32, 8), MB_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(mb_txpool_init(&p, nullptr, 32, 8), MB_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(mb_txpool_init(&p, buf, 0, 8), MB_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(mb_txpool_init(&p, buf, 32, 0), MB_ERR_INVALID_ARGUMENT);
}

TEST_F(TxPoolTest, AcquireReleaseBasic) {
    test_transaction *tx1 = static_cast<test_transaction*>(mb_txpool_acquire(&pool));
    ASSERT_NE(tx1, nullptr);
    EXPECT_EQ(mb_txpool_in_use(&pool), 1);
    EXPECT_EQ(mb_txpool_available(&pool), CAPACITY - 1);
    
    test_transaction *tx2 = static_cast<test_transaction*>(mb_txpool_acquire(&pool));
    ASSERT_NE(tx2, nullptr);
    EXPECT_NE(tx1, tx2);
    EXPECT_EQ(mb_txpool_in_use(&pool), 2);
    
    EXPECT_EQ(mb_txpool_release(&pool, tx1), MB_OK);
    EXPECT_EQ(mb_txpool_in_use(&pool), 1);
    EXPECT_EQ(mb_txpool_available(&pool), CAPACITY - 1);
    
    EXPECT_EQ(mb_txpool_release(&pool, tx2), MB_OK);
    EXPECT_EQ(mb_txpool_in_use(&pool), 0);
    EXPECT_EQ(mb_txpool_available(&pool), CAPACITY);
}

TEST_F(TxPoolTest, AcquireAllTransactions) {
    std::vector<test_transaction*> transactions;
    
    for (size_t i = 0; i < CAPACITY; i++) {
        test_transaction *tx = static_cast<test_transaction*>(mb_txpool_acquire(&pool));
        ASSERT_NE(tx, nullptr);
        transactions.push_back(tx);
    }
    
    EXPECT_EQ(mb_txpool_in_use(&pool), CAPACITY);
    EXPECT_EQ(mb_txpool_available(&pool), 0);
    EXPECT_TRUE(mb_txpool_is_empty(&pool));
    
    // Next acquire should fail
    EXPECT_EQ(mb_txpool_acquire(&pool), nullptr);
    
    // Release all
    for (auto *tx : transactions) {
        EXPECT_EQ(mb_txpool_release(&pool, tx), MB_OK);
    }
    
    EXPECT_EQ(mb_txpool_in_use(&pool), 0);
    EXPECT_EQ(mb_txpool_available(&pool), CAPACITY);
}

TEST_F(TxPoolTest, HighWaterMarkTracking) {
    std::vector<test_transaction*> transactions;
    
    EXPECT_EQ(mb_txpool_high_water(&pool), 0);
    
    // Acquire 5
    for (int i = 0; i < 5; i++) {
        transactions.push_back(static_cast<test_transaction*>(mb_txpool_acquire(&pool)));
    }
    EXPECT_EQ(mb_txpool_high_water(&pool), 5);
    
    // Release 2
    for (int i = 0; i < 2; i++) {
        mb_txpool_release(&pool, transactions.back());
        transactions.pop_back();
    }
    EXPECT_EQ(mb_txpool_high_water(&pool), 5); // Should not decrease
    
    // Acquire 8 more (total 11)
    for (int i = 0; i < 8; i++) {
        transactions.push_back(static_cast<test_transaction*>(mb_txpool_acquire(&pool)));
    }
    EXPECT_EQ(mb_txpool_high_water(&pool), 11);
    
    // Cleanup
    for (auto *tx : transactions) {
        mb_txpool_release(&pool, tx);
    }
}

TEST_F(TxPoolTest, StatisticsAccurate) {
    mb_txpool_stats_t stats;
    
    mb_txpool_get_stats(&pool, &stats);
    EXPECT_EQ(stats.capacity, CAPACITY);
    EXPECT_EQ(stats.in_use, 0);
    EXPECT_EQ(stats.available, CAPACITY);
    EXPECT_EQ(stats.total_acquired, 0);
    EXPECT_EQ(stats.total_released, 0);
    EXPECT_EQ(stats.failed_acquires, 0);
    
    // Acquire 3
    std::vector<test_transaction*> txs;
    for (int i = 0; i < 3; i++) {
        txs.push_back(static_cast<test_transaction*>(mb_txpool_acquire(&pool)));
    }
    
    mb_txpool_get_stats(&pool, &stats);
    EXPECT_EQ(stats.in_use, 3);
    EXPECT_EQ(stats.total_acquired, 3);
    EXPECT_EQ(stats.total_released, 0);
    
    // Release 2
    for (int i = 0; i < 2; i++) {
        mb_txpool_release(&pool, txs[i]);
    }
    
    mb_txpool_get_stats(&pool, &stats);
    EXPECT_EQ(stats.in_use, 1);
    EXPECT_EQ(stats.total_acquired, 3);
    EXPECT_EQ(stats.total_released, 2);
    
    // Cleanup
    mb_txpool_release(&pool, txs[2]);
}

TEST_F(TxPoolTest, ResetClearsState) {
    std::vector<test_transaction*> txs;
    for (int i = 0; i < 5; i++) {
        txs.push_back(static_cast<test_transaction*>(mb_txpool_acquire(&pool)));
    }
    
    EXPECT_EQ(mb_txpool_in_use(&pool), 5);
    
    mb_txpool_reset(&pool);
    
    EXPECT_EQ(mb_txpool_in_use(&pool), 0);
    EXPECT_EQ(mb_txpool_available(&pool), CAPACITY);
}

TEST_F(TxPoolTest, TransactionDataIsolation) {
    test_transaction *tx1 = static_cast<test_transaction*>(mb_txpool_acquire(&pool));
    test_transaction *tx2 = static_cast<test_transaction*>(mb_txpool_acquire(&pool));
    
    ASSERT_NE(tx1, nullptr);
    ASSERT_NE(tx2, nullptr);
    
    // Write to tx1
    tx1->slave_addr = 1;
    tx1->reg_addr = 100;
    tx1->reg_count = 10;
    tx1->fc = 0x03;
    tx1->seq_number = 42;
    
    // Write to tx2
    tx2->slave_addr = 2;
    tx2->reg_addr = 200;
    tx2->reg_count = 20;
    tx2->fc = 0x10;
    tx2->seq_number = 99;
    
    // Verify isolation
    EXPECT_EQ(tx1->slave_addr, 1);
    EXPECT_EQ(tx1->seq_number, 42);
    EXPECT_EQ(tx2->slave_addr, 2);
    EXPECT_EQ(tx2->seq_number, 99);
    
    mb_txpool_release(&pool, tx1);
    mb_txpool_release(&pool, tx2);
}

/* ========================================================================== */
/* Gate 22 Stress Test: 1M Transactions without Leaks                        */
/* ========================================================================== */

TEST(TxPoolStressTest, Gate22_OneMillionTransactionsNoLeaks) {
    constexpr size_t POOL_SIZE = 64;
    constexpr size_t NUM_TRANSACTIONS = 1000000;
    
    uint8_t storage[POOL_SIZE * sizeof(test_transaction)];
    mb_txpool_t pool;
    
    ASSERT_EQ(mb_txpool_init(&pool, storage, sizeof(test_transaction), POOL_SIZE), MB_OK);
    
    // Track all allocated pointers to verify no leaks
    std::vector<void*> all_ptrs;
    
    for (size_t i = 0; i < NUM_TRANSACTIONS; i++) {
        test_transaction *tx = static_cast<test_transaction*>(mb_txpool_acquire(&pool));
        ASSERT_NE(tx, nullptr) << "Pool exhausted at iteration " << i;
        
        // Populate transaction
        tx->slave_addr = static_cast<uint8_t>(i % 256);
        tx->reg_addr = static_cast<uint16_t>(i % 65536);
        tx->reg_count = static_cast<uint16_t>((i % 100) + 1);
        tx->fc = 0x03;
        tx->seq_number = static_cast<uint32_t>(i);
        tx->timestamp_us = i * 1000;
        
        all_ptrs.push_back(tx);
        
        // Release immediately (simulating rapid churn)
        ASSERT_EQ(mb_txpool_release(&pool, tx), MB_OK);
        all_ptrs.pop_back();
    }
    
    // Verify pool state
    mb_txpool_stats_t stats;
    mb_txpool_get_stats(&pool, &stats);
    
    EXPECT_EQ(stats.in_use, 0) << "Leak detected: transactions still in use";
    EXPECT_EQ(stats.available, POOL_SIZE) << "Not all transactions returned";
    EXPECT_EQ(stats.total_acquired, NUM_TRANSACTIONS);
    EXPECT_EQ(stats.total_released, NUM_TRANSACTIONS);
    EXPECT_EQ(stats.failed_acquires, 0);
    EXPECT_FALSE(mb_txpool_has_leaks(&pool));
    
    std::cout << "\n=== Gate 22 Stress Test Results ===" << std::endl;
    std::cout << "Total transactions: " << NUM_TRANSACTIONS << std::endl;
    std::cout << "Pool capacity: " << POOL_SIZE << std::endl;
    std::cout << "High water mark: " << stats.high_water << std::endl;
    std::cout << "Total acquired: " << stats.total_acquired << std::endl;
    std::cout << "Total released: " << stats.total_released << std::endl;
    std::cout << "Failed acquires: " << stats.failed_acquires << std::endl;
    std::cout << "Final in-use: " << stats.in_use << " (should be 0)" << std::endl;
    std::cout << "Leak detected: " << (mb_txpool_has_leaks(&pool) ? "YES" : "NO") << std::endl;
}

/* ========================================================================== */
/* Gate 22 Latency Test: Fixed-Latency Operations                            */
/* ========================================================================== */

TEST(TxPoolStressTest, Gate22_FixedLatencyOperations) {
    constexpr size_t POOL_SIZE = 32;
    constexpr size_t NUM_SAMPLES = 100000;
    
    uint8_t storage[POOL_SIZE * sizeof(test_transaction)];
    mb_txpool_t pool;
    
    ASSERT_EQ(mb_txpool_init(&pool, storage, sizeof(test_transaction), POOL_SIZE), MB_OK);
    
    std::vector<uint64_t> acquire_times;
    std::vector<uint64_t> release_times;
    acquire_times.reserve(NUM_SAMPLES);
    release_times.reserve(NUM_SAMPLES);
    
    for (size_t i = 0; i < NUM_SAMPLES; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        test_transaction *tx = static_cast<test_transaction*>(mb_txpool_acquire(&pool));
        auto end = std::chrono::high_resolution_clock::now();
        
        ASSERT_NE(tx, nullptr);
        acquire_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
        
        start = std::chrono::high_resolution_clock::now();
        mb_txpool_release(&pool, tx);
        end = std::chrono::high_resolution_clock::now();
        
        release_times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    
    // Calculate statistics
    auto calc_stats = [](const std::vector<uint64_t> &times) {
        uint64_t sum = 0;
        uint64_t min = UINT64_MAX;
        uint64_t max = 0;
        
        for (uint64_t t : times) {
            sum += t;
            if (t < min) min = t;
            if (t > max) max = t;
        }
        
        uint64_t avg = sum / times.size();
        
        // Calculate median
        std::vector<uint64_t> sorted = times;
        std::sort(sorted.begin(), sorted.end());
        uint64_t median = sorted[sorted.size() / 2];
        
        return std::make_tuple(avg, median, min, max);
    };
    
    auto [acq_avg, acq_med, acq_min, acq_max] = calc_stats(acquire_times);
    auto [rel_avg, rel_med, rel_min, rel_max] = calc_stats(release_times);
    
    std::cout << "\n=== Gate 22 Latency Test Results ===" << std::endl;
    std::cout << "Samples: " << NUM_SAMPLES << std::endl;
    std::cout << "\nAcquire latency (ns):" << std::endl;
    std::cout << "  Average: " << acq_avg << std::endl;
    std::cout << "  Median:  " << acq_med << std::endl;
    std::cout << "  Min:     " << acq_min << std::endl;
    std::cout << "  Max:     " << acq_max << std::endl;
    std::cout << "\nRelease latency (ns):" << std::endl;
    std::cout << "  Average: " << rel_avg << std::endl;
    std::cout << "  Median:  " << rel_med << std::endl;
    std::cout << "  Min:     " << rel_min << std::endl;
    std::cout << "  Max:     " << rel_max << std::endl;
    
    // Verify latency is reasonable (should be O(1), typically < 1us)
    EXPECT_LT(acq_avg, 1000) << "Average acquire latency too high (> 1us)";
    EXPECT_LT(rel_avg, 1000) << "Average release latency too high (> 1us)";
}

/* ========================================================================== */
/* Gate 22 Concurrent Access Test: Thread Safety with External Mutex         */
/* ========================================================================== */

TEST(TxPoolStressTest, Gate22_ConcurrentAccessWithMutex) {
    constexpr size_t POOL_SIZE = 128;
    constexpr int NUM_THREADS = 8;
    constexpr int OPS_PER_THREAD = 10000;
    
    uint8_t storage[POOL_SIZE * sizeof(test_transaction)];
    mb_txpool_t pool;
    mb_port_mutex_t pool_mutex;
    
    ASSERT_EQ(mb_txpool_init(&pool, storage, sizeof(test_transaction), POOL_SIZE), MB_OK);
    ASSERT_EQ(mb_port_mutex_init(&pool_mutex), MB_OK);
    
    std::atomic<int> total_ops{0};
    std::atomic<int> failed_ops{0};
    
    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < OPS_PER_THREAD; i++) {
                // Acquire with mutex
                mb_port_mutex_lock(&pool_mutex);
                test_transaction *tx = static_cast<test_transaction*>(mb_txpool_acquire(&pool));
                mb_port_mutex_unlock(&pool_mutex);
                
                if (tx) {
                    // Populate (no mutex needed for owned transaction)
                    tx->slave_addr = static_cast<uint8_t>(t);
                    tx->seq_number = static_cast<uint32_t>(i);
                    
                    // Simulate work
                    std::this_thread::sleep_for(std::chrono::microseconds(1));
                    
                    // Release with mutex
                    mb_port_mutex_lock(&pool_mutex);
                    mb_txpool_release(&pool, tx);
                    mb_port_mutex_unlock(&pool_mutex);
                    
                    total_ops++;
                } else {
                    failed_ops++;
                    std::this_thread::yield();
                }
            }
        });
    }
    
    for (auto &th : threads) {
        th.join();
    }
    
    mb_port_mutex_deinit(&pool_mutex);
    
    // Verify no leaks
    mb_txpool_stats_t stats;
    mb_txpool_get_stats(&pool, &stats);
    
    std::cout << "\n=== Gate 22 Concurrent Access Test ===" << std::endl;
    std::cout << "Threads: " << NUM_THREADS << std::endl;
    std::cout << "Operations per thread: " << OPS_PER_THREAD << std::endl;
    std::cout << "Total successful ops: " << total_ops << std::endl;
    std::cout << "Failed ops (backpressure): " << failed_ops << std::endl;
    std::cout << "High water mark: " << stats.high_water << std::endl;
    std::cout << "Final in-use: " << stats.in_use << std::endl;
    
    EXPECT_EQ(stats.in_use, 0) << "Transactions leaked";
    EXPECT_GT(total_ops, 0) << "No successful operations";
}
