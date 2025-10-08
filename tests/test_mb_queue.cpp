/**
 * @file test_mb_queue.cpp
 * @brief Unit tests for lock-free SPSC and MPSC queues.
 */

#include <gtest/gtest.h>
#include <modbus/mb_queue.h>
#include <thread>
#include <vector>
#include <atomic>

/* ========================================================================== */
/* SPSC Queue Tests                                                           */
/* ========================================================================== */

class SPSCQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        mb_queue_spsc_init(&queue, slots, CAPACITY);
    }
    
    void TearDown() override {
        mb_queue_spsc_deinit(&queue);
    }
    
    static constexpr size_t CAPACITY = 16;
    void *slots[CAPACITY];
    mb_queue_spsc_t queue;
};

TEST_F(SPSCQueueTest, InitializationSuccess) {
    EXPECT_EQ(mb_queue_spsc_capacity(&queue), CAPACITY);
    EXPECT_EQ(mb_queue_spsc_size(&queue), 0);
    EXPECT_TRUE(mb_queue_spsc_is_empty(&queue));
    EXPECT_FALSE(mb_queue_spsc_is_full(&queue));
}

TEST_F(SPSCQueueTest, InitializationFailsWithInvalidCapacity) {
    mb_queue_spsc_t q;
    void *s[7]; // Not power of 2
    
    EXPECT_EQ(mb_queue_spsc_init(&q, s, 7), MB_ERR_INVALID_ARGUMENT);
}

TEST_F(SPSCQueueTest, InitializationFailsWithNullPointers) {
    mb_queue_spsc_t q;
    void *s[8];
    
    EXPECT_EQ(mb_queue_spsc_init(nullptr, s, 8), MB_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(mb_queue_spsc_init(&q, nullptr, 8), MB_ERR_INVALID_ARGUMENT);
}

TEST_F(SPSCQueueTest, EnqueueDequeueBasic) {
    int data1 = 42;
    int data2 = 100;
    
    EXPECT_TRUE(mb_queue_spsc_enqueue(&queue, &data1));
    EXPECT_EQ(mb_queue_spsc_size(&queue), 1);
    
    EXPECT_TRUE(mb_queue_spsc_enqueue(&queue, &data2));
    EXPECT_EQ(mb_queue_spsc_size(&queue), 2);
    
    void *result;
    EXPECT_TRUE(mb_queue_spsc_dequeue(&queue, &result));
    EXPECT_EQ(result, &data1);
    EXPECT_EQ(mb_queue_spsc_size(&queue), 1);
    
    EXPECT_TRUE(mb_queue_spsc_dequeue(&queue, &result));
    EXPECT_EQ(result, &data2);
    EXPECT_TRUE(mb_queue_spsc_is_empty(&queue));
}

TEST_F(SPSCQueueTest, DequeueFromEmptyQueueFails) {
    void *result;
    EXPECT_FALSE(mb_queue_spsc_dequeue(&queue, &result));
}

TEST_F(SPSCQueueTest, EnqueueToFullQueueFails) {
    std::vector<int> data(CAPACITY);
    
    // Fill queue to capacity - 1 (one slot reserved for full detection)
    for (size_t i = 0; i < CAPACITY - 1; i++) {
        data[i] = static_cast<int>(i);
        EXPECT_TRUE(mb_queue_spsc_enqueue(&queue, &data[i]));
    }
    
    EXPECT_TRUE(mb_queue_spsc_is_full(&queue));
    
    // Next enqueue should fail
    int extra = 999;
    EXPECT_FALSE(mb_queue_spsc_enqueue(&queue, &extra));
}

TEST_F(SPSCQueueTest, HighWaterMarkTracking) {
    std::vector<int> data(8);
    
    EXPECT_EQ(mb_queue_spsc_high_water(&queue), 0);
    
    // Enqueue 3 items
    for (int i = 0; i < 3; i++) {
        mb_queue_spsc_enqueue(&queue, &data[i]);
    }
    EXPECT_EQ(mb_queue_spsc_high_water(&queue), 3);
    
    // Dequeue 1
    void *result;
    mb_queue_spsc_dequeue(&queue, &result);
    EXPECT_EQ(mb_queue_spsc_high_water(&queue), 3); // Should not decrease
    
    // Enqueue 5 more (total 7)
    for (int i = 3; i < 8; i++) {
        mb_queue_spsc_enqueue(&queue, &data[i]);
    }
    EXPECT_EQ(mb_queue_spsc_high_water(&queue), 7);
}

TEST_F(SPSCQueueTest, WrapAround) {
    std::vector<int> data(CAPACITY * 2);
    
    // Fill and drain multiple times
    for (int cycle = 0; cycle < 3; cycle++) {
        // Fill to near-capacity
        for (size_t i = 0; i < CAPACITY - 1; i++) {
            size_t idx = cycle * CAPACITY + i;
            EXPECT_TRUE(mb_queue_spsc_enqueue(&queue, &data[idx]));
        }
        
        // Drain completely
        void *result;
        for (size_t i = 0; i < CAPACITY - 1; i++) {
            EXPECT_TRUE(mb_queue_spsc_dequeue(&queue, &result));
            size_t idx = cycle * CAPACITY + i;
            EXPECT_EQ(result, &data[idx]);
        }
        
        EXPECT_TRUE(mb_queue_spsc_is_empty(&queue));
    }
}

TEST_F(SPSCQueueTest, ConcurrentProducerConsumer) {
    constexpr int NUM_ITEMS = 10000;
    std::atomic<int> consumed_count{0};
    std::atomic<bool> producer_done{false};
    
    std::vector<int> data(NUM_ITEMS);
    for (int i = 0; i < NUM_ITEMS; i++) {
        data[i] = i;
    }
    
    // Producer thread
    std::thread producer([&]() {
        for (int i = 0; i < NUM_ITEMS; i++) {
            while (!mb_queue_spsc_enqueue(&queue, &data[i])) {
                std::this_thread::yield();
            }
        }
        producer_done = true;
    });
    
    // Consumer thread
    std::thread consumer([&]() {
        int last_value = -1;
        while (consumed_count < NUM_ITEMS) {
            void *result;
            if (mb_queue_spsc_dequeue(&queue, &result)) {
                int value = *static_cast<int*>(result);
                EXPECT_EQ(value, last_value + 1);
                last_value = value;
                consumed_count++;
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(consumed_count, NUM_ITEMS);
    EXPECT_TRUE(mb_queue_spsc_is_empty(&queue));
}

/* ========================================================================== */
/* MPSC Queue Tests                                                           */
/* ========================================================================== */

class MPSCQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        mb_queue_mpsc_init(&queue, slots, CAPACITY);
    }
    
    void TearDown() override {
        mb_queue_mpsc_deinit(&queue);
    }
    
    static constexpr size_t CAPACITY = 16;
    void *slots[CAPACITY];
    mb_queue_mpsc_t queue;
};

TEST_F(MPSCQueueTest, InitializationSuccess) {
    EXPECT_EQ(mb_queue_mpsc_capacity(&queue), CAPACITY);
    EXPECT_EQ(mb_queue_mpsc_size(&queue), 0);
    EXPECT_TRUE(mb_queue_mpsc_is_empty(&queue));
    EXPECT_FALSE(mb_queue_mpsc_is_full(&queue));
}

TEST_F(MPSCQueueTest, EnqueueDequeueBasic) {
    int data1 = 42;
    int data2 = 100;
    
    EXPECT_TRUE(mb_queue_mpsc_enqueue(&queue, &data1));
    EXPECT_EQ(mb_queue_mpsc_size(&queue), 1);
    
    EXPECT_TRUE(mb_queue_mpsc_enqueue(&queue, &data2));
    EXPECT_EQ(mb_queue_mpsc_size(&queue), 2);
    
    void *result;
    EXPECT_TRUE(mb_queue_mpsc_dequeue(&queue, &result));
    EXPECT_EQ(result, &data1);
    
    EXPECT_TRUE(mb_queue_mpsc_dequeue(&queue, &result));
    EXPECT_EQ(result, &data2);
    EXPECT_TRUE(mb_queue_mpsc_is_empty(&queue));
}

TEST_F(MPSCQueueTest, MultipleProducersSingleConsumer) {
    constexpr int NUM_PRODUCERS = 4;
    constexpr int ITEMS_PER_PRODUCER = 2500;
    constexpr int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS_PER_PRODUCER;
    
    std::vector<std::vector<int>> producer_data(NUM_PRODUCERS);
    for (int p = 0; p < NUM_PRODUCERS; p++) {
        producer_data[p].resize(ITEMS_PER_PRODUCER);
        for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
            producer_data[p][i] = p * 10000 + i;
        }
    }
    
    std::atomic<int> consumed_count{0};
    std::atomic<int> producers_done{0};
    
    // Start producers
    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; p++) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
                while (!mb_queue_mpsc_enqueue(&queue, &producer_data[p][i])) {
                    std::this_thread::yield();
                }
            }
            producers_done++;
        });
    }
    
    // Consumer thread
    std::thread consumer([&]() {
        while (consumed_count < TOTAL_ITEMS) {
            void *result;
            if (mb_queue_mpsc_dequeue(&queue, &result)) {
                consumed_count++;
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    for (auto &t : producers) {
        t.join();
    }
    consumer.join();
    
    EXPECT_EQ(consumed_count, TOTAL_ITEMS);
    EXPECT_EQ(producers_done, NUM_PRODUCERS);
    EXPECT_TRUE(mb_queue_mpsc_is_empty(&queue));
}

TEST_F(MPSCQueueTest, HighWaterMarkUnderLoad) {
    constexpr int NUM_PRODUCERS = 8;
    constexpr int ITEMS_PER_PRODUCER = 100;
    
    std::vector<std::vector<int>> producer_data(NUM_PRODUCERS);
    for (int p = 0; p < NUM_PRODUCERS; p++) {
        producer_data[p].resize(ITEMS_PER_PRODUCER);
        for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
            producer_data[p][i] = p * 1000 + i;
        }
    }
    
    std::atomic<bool> start{false};
    
    // Start all producers simultaneously
    std::vector<std::thread> producers;
    for (int p = 0; p < NUM_PRODUCERS; p++) {
        producers.emplace_back([&, p]() {
            while (!start) {
                std::this_thread::yield();
            }
            for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
                while (!mb_queue_mpsc_enqueue(&queue, &producer_data[p][i])) {
                    std::this_thread::yield();
                }
            }
        });
    }
    
    // Start!
    start = true;
    
    // Slow consumer to build queue pressure
    std::thread consumer([&]() {
        int consumed = 0;
        while (consumed < NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
            void *result;
            if (mb_queue_mpsc_dequeue(&queue, &result)) {
                consumed++;
                // Simulate slow processing
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
    });
    
    for (auto &t : producers) {
        t.join();
    }
    consumer.join();
    
    // Should have seen significant queue buildup
    EXPECT_GT(mb_queue_mpsc_high_water(&queue), 1);
}
