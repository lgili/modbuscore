/**
 * @file test_ringbuf_concurrent.cpp
 * @brief ThreadSanitizer-enabled concurrency tests for ring buffer.
 *
 * These tests validate the ring buffer's behavior when used from multiple
 * threads (e.g., ISR producer + main thread consumer).
 */

#include <algorithm>

#include <modbus/internal/ringbuf.h>
#include "gtest/gtest.h"

#include <thread>
#include <atomic>
#include <vector>
#include <chrono>

namespace {

constexpr size_t kCapacity = 128U;
constexpr size_t kIterations = 10000U;

class RingbufConcurrentTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(MB_OK, mb_ringbuf_init(&rb_, storage_, kCapacity));
    }

    mb_ringbuf_t rb_{};
    uint8_t storage_[kCapacity]{};
};

// Test: Single producer, single consumer with interleaved operations
TEST_F(RingbufConcurrentTest, SPSCInterleavedReadWrite)
{
    std::atomic<bool> stop{false};
    std::atomic<size_t> total_written{0};
    std::atomic<size_t> total_read{0};

    // Producer thread: write bytes incrementally
    std::thread producer([&]() {
        uint8_t value = 0;
        while (!stop.load(std::memory_order_acquire)) {
            if (mb_ringbuf_write(&rb_, &value, 1U) == 1U) {
                ++value;
                total_written.fetch_add(1U, std::memory_order_release);
            }
            std::this_thread::yield();
        }
    });

    // Consumer thread: read bytes and validate ordering
    std::thread consumer([&]() {
        uint8_t expected = 0;
        size_t read_count = 0;
        
        while (read_count < kIterations) {
            uint8_t value = 0;
            if (mb_ringbuf_read(&rb_, &value, 1U) == 1U) {
                EXPECT_EQ(expected, value) << "Read out-of-order at iteration " << read_count;
                ++expected;
                ++read_count;
                total_read.fetch_add(1U, std::memory_order_release);
            }
            std::this_thread::yield();
        }
    });

    consumer.join();
    stop.store(true, std::memory_order_release);
    producer.join();

    EXPECT_GE(total_written.load(), kIterations);
    EXPECT_EQ(total_read.load(), kIterations);
}

// Test: Burst writes from producer with consumer draining in chunks
TEST_F(RingbufConcurrentTest, SPSCBurstProducer)
{
    std::atomic<bool> producer_done{false};
    std::atomic<size_t> total_written{0};
    std::atomic<size_t> total_read{0};

    constexpr size_t kBurstSize = 32U;
    constexpr size_t kBursts = 100U;

    std::thread producer([&]() {
        for (size_t burst = 0; burst < kBursts; ++burst) {
            uint8_t data[kBurstSize];
            for (size_t i = 0; i < kBurstSize; ++i) {
                data[i] = static_cast<uint8_t>((burst * kBurstSize + i) & 0xFFU);
            }

            size_t written = 0;
            while (written < kBurstSize) {
                const size_t n = mb_ringbuf_write(&rb_, &data[written], kBurstSize - written);
                written += n;
                total_written.fetch_add(n, std::memory_order_release);
                if (n == 0) {
                    std::this_thread::yield();
                }
            }
        }
        producer_done.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        size_t expected_value = 0;
        
        while (!producer_done.load(std::memory_order_acquire) || mb_ringbuf_size(&rb_) > 0) {
            uint8_t chunk[16];
            const size_t n = mb_ringbuf_read(&rb_, chunk, sizeof(chunk));
            
            for (size_t i = 0; i < n; ++i) {
                const uint8_t expected = static_cast<uint8_t>(expected_value & 0xFFU);
                EXPECT_EQ(expected, chunk[i]) << "Mismatch at byte " << expected_value;
                ++expected_value;
            }
            
            total_read.fetch_add(n, std::memory_order_release);
            
            if (n == 0) {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(total_written.load(), kBursts * kBurstSize);
    EXPECT_EQ(total_read.load(), kBursts * kBurstSize);
}

// Test: Multiple producers (simulating spurious ISR events)
TEST_F(RingbufConcurrentTest, MPSCMultipleProducers)
{
    constexpr size_t kProducers = 4U;
    constexpr size_t kItemsPerProducer = 1000U;

    std::atomic<size_t> total_written{0};
    std::atomic<size_t> total_read{0};
    std::atomic<bool> all_done{false};

    std::vector<std::thread> producers;
    for (size_t p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, producer_id = p]() {
            for (size_t i = 0; i < kItemsPerProducer; ++i) {
                const uint8_t value = static_cast<uint8_t>((producer_id << 6) | (i & 0x3FU));
                
                while (mb_ringbuf_write(&rb_, &value, 1U) == 0) {
                    std::this_thread::yield();
                }
                
                total_written.fetch_add(1U, std::memory_order_release);
            }
        });
    }

    std::thread consumer([&]() {
        size_t read_count = 0;
        const size_t target = kProducers * kItemsPerProducer;
        
        while (read_count < target) {
            uint8_t buffer[32];
            const size_t n = mb_ringbuf_read(&rb_, buffer, sizeof(buffer));
            read_count += n;
            total_read.fetch_add(n, std::memory_order_release);
            
            if (n == 0) {
                std::this_thread::yield();
            }
        }
        
        all_done.store(true, std::memory_order_release);
    });

    for (auto &t : producers) {
        t.join();
    }
    consumer.join();

    EXPECT_EQ(total_written.load(), kProducers * kItemsPerProducer);
    EXPECT_EQ(total_read.load(), kProducers * kItemsPerProducer);
    EXPECT_TRUE(all_done.load());
}

// Test: Reset while concurrent operations are in progress
TEST_F(RingbufConcurrentTest, ConcurrentResetDoesNotCrash)
{
    std::atomic<bool> stop{false};
    std::atomic<size_t> reset_count{0};

    std::thread writer([&]() {
        uint8_t value = 0;
        while (!stop.load(std::memory_order_acquire)) {
            mb_ringbuf_write(&rb_, &value, 1U);
            ++value;
            std::this_thread::yield();
        }
    });

    std::thread reader([&]() {
        uint8_t buffer[8];
        while (!stop.load(std::memory_order_acquire)) {
            mb_ringbuf_read(&rb_, buffer, sizeof(buffer));
            std::this_thread::yield();
        }
    });

    std::thread resetter([&]() {
        for (size_t i = 0; i < 100; ++i) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            mb_ringbuf_reset(&rb_);
            reset_count.fetch_add(1U, std::memory_order_release);
        }
    });

    resetter.join();
    stop.store(true, std::memory_order_release);
    writer.join();
    reader.join();

    EXPECT_EQ(reset_count.load(), 100U);
    // Main assertion: no crash or TSan warnings
}

} // namespace
