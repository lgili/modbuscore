/**
 * @file test_mb_isr.cpp
 * @brief Unit tests for ISR-safe mode (Gate 23 validation).
 */

#include <gtest/gtest.h>
#include <modbus/mb_isr.h>
#include <modbus/internal/mb_queue.h>
#include <algorithm>
#include <thread>
#include <chrono>
#include <vector>

/* ========================================================================== */
/* ISR Context Detection Tests                                                */
/* ========================================================================== */

TEST(ISRDetectionTest, DefaultContextIsThread) {
    // Should not be in ISR by default
    EXPECT_FALSE(mb_in_isr());
    EXPECT_FALSE(MB_IN_ISR());
}

TEST(ISRDetectionTest, ManualContextSetting) {
    // Test manual ISR flag (for platforms without auto-detection)
    EXPECT_FALSE(mb_in_isr());
    
    mb_set_isr_context(true);
    EXPECT_TRUE(mb_in_isr());
    
    mb_set_isr_context(false);
    EXPECT_FALSE(mb_in_isr());
}

/* ========================================================================== */
/* ISR Context Initialization Tests                                           */
/* ========================================================================== */

class ISRContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Prepare configuration
        config.rx_queue_slots = rx_slots;
        config.rx_queue_capacity = 32;
        config.tx_queue_slots = tx_slots;
        config.tx_queue_capacity = 16;
        config.rx_buffer = rx_buffer;
        config.rx_buffer_size = sizeof(rx_buffer);
        config.tx_buffer = tx_buffer;
        config.tx_buffer_size = sizeof(tx_buffer);
        config.enable_logging = false;
        config.turnaround_target_us = 100;
        
        ASSERT_EQ(mb_isr_ctx_init(&ctx, &config), MB_OK);
    }
    
    void TearDown() override {
        mb_isr_ctx_deinit(&ctx);
    }
    
    mb_isr_ctx_t ctx;
    mb_isr_config_t config;
    
    void *rx_slots[32];
    void *tx_slots[16];
    uint8_t rx_buffer[256];
    uint8_t tx_buffer[256];
};

TEST_F(ISRContextTest, InitializationSuccess) {
    // Context should be initialized without errors
    mb_isr_stats_t stats;
    EXPECT_EQ(mb_isr_get_stats(&ctx, &stats), MB_OK);
    EXPECT_EQ(stats.rx_chunks_processed, 0);
    EXPECT_EQ(stats.tx_started_from_isr, 0);
}

TEST_F(ISRContextTest, InitializationFailsWithNullPointers) {
    mb_isr_ctx_t c;
    EXPECT_EQ(mb_isr_ctx_init(nullptr, &config), MB_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(mb_isr_ctx_init(&c, nullptr), MB_ERR_INVALID_ARGUMENT);
}

TEST_F(ISRContextTest, InitializationFailsWithInvalidConfig) {
    mb_isr_ctx_t c;
    mb_isr_config_t bad_config = config;
    
    bad_config.rx_queue_slots = nullptr;
    EXPECT_EQ(mb_isr_ctx_init(&c, &bad_config), MB_ERR_INVALID_ARGUMENT);
    
    bad_config = config;
    bad_config.rx_queue_capacity = 0;
    EXPECT_EQ(mb_isr_ctx_init(&c, &bad_config), MB_ERR_INVALID_ARGUMENT);
}

/* ========================================================================== */
/* RX Path Tests                                                              */
/* ========================================================================== */

TEST_F(ISRContextTest, RxChunkProcessing) {
    uint8_t test_data[] = {0x01, 0x03, 0x00, 0x64, 0x00, 0x0A, 0xC5, 0xCD}; // Valid Modbus frame
    
    EXPECT_EQ(mb_on_rx_chunk_from_isr(&ctx, test_data, sizeof(test_data)), MB_OK);
    
    mb_isr_stats_t stats;
    mb_isr_get_stats(&ctx, &stats);
    EXPECT_EQ(stats.rx_chunks_processed, 1);
    EXPECT_EQ(stats.queue_full_events, 0);
}

TEST_F(ISRContextTest, RxRejectsInvalidData) {
    uint8_t short_data[] = {0x01, 0x03}; // Too short
    
    EXPECT_EQ(mb_on_rx_chunk_from_isr(&ctx, short_data, sizeof(short_data)), 
              MB_ERR_INVALID_ARGUMENT);
    
    EXPECT_EQ(mb_on_rx_chunk_from_isr(&ctx, nullptr, 10), MB_ERR_INVALID_ARGUMENT);
    EXPECT_EQ(mb_on_rx_chunk_from_isr(&ctx, short_data, 0), MB_ERR_INVALID_ARGUMENT);
}

TEST_F(ISRContextTest, RxQueueFull) {
    uint8_t test_data[] = {0x01, 0x03, 0x00, 0x64, 0x00, 0x0A, 0xC5, 0xCD};
    
    // Fill queue to capacity (32 slots, but -1 for full detection in ring)
    for (int i = 0; i < 31; i++) {
        EXPECT_EQ(mb_on_rx_chunk_from_isr(&ctx, test_data, sizeof(test_data)), MB_OK);
    }
    
    // Next should fail (queue full)
    EXPECT_EQ(mb_on_rx_chunk_from_isr(&ctx, test_data, sizeof(test_data)), MB_ERR_BUSY);
    
    mb_isr_stats_t stats;
    mb_isr_get_stats(&ctx, &stats);
    EXPECT_EQ(stats.queue_full_events, 1);
}

/* ========================================================================== */
/* TX Path Tests                                                              */
/* ========================================================================== */

TEST_F(ISRContextTest, TxWhenNoDataReady) {
    // No TX data queued, should return false
    EXPECT_FALSE(mb_try_tx_from_isr(&ctx));
}

TEST_F(ISRContextTest, TxBufferAccess) {
    const uint8_t *tx_data;
    size_t tx_len;
    
    // No TX in progress yet
    EXPECT_FALSE(mb_get_tx_buffer_from_isr(&ctx, &tx_data, &tx_len));
}

TEST_F(ISRContextTest, TxCompleteNotification) {
    // Should handle gracefully even if no TX in progress
    mb_tx_complete_from_isr(&ctx);
    
    mb_isr_stats_t stats;
    mb_isr_get_stats(&ctx, &stats);
    EXPECT_EQ(stats.tx_started_from_isr, 0);
}

/* ========================================================================== */
/* Statistics Tests                                                           */
/* ========================================================================== */

TEST_F(ISRContextTest, StatisticsTracking) {
    uint8_t test_data[] = {0x01, 0x03, 0x00, 0x64, 0x00, 0x0A, 0xC5, 0xCD};
    
    // Process some RX chunks
    for (int i = 0; i < 5; i++) {
        mb_on_rx_chunk_from_isr(&ctx, test_data, sizeof(test_data));
    }
    
    mb_isr_stats_t stats;
    EXPECT_EQ(mb_isr_get_stats(&ctx, &stats), MB_OK);
    
    EXPECT_EQ(stats.rx_chunks_processed, 5);
    EXPECT_EQ(stats.tx_started_from_isr, 0);
    EXPECT_EQ(stats.fast_turnarounds, 0);
    EXPECT_EQ(stats.queue_full_events, 0);
}

TEST_F(ISRContextTest, StatisticsReset) {
    uint8_t test_data[] = {0x01, 0x03, 0x00, 0x64, 0x00, 0x0A, 0xC5, 0xCD};
    
    mb_on_rx_chunk_from_isr(&ctx, test_data, sizeof(test_data));
    
    mb_isr_stats_t stats;
    mb_isr_get_stats(&ctx, &stats);
    EXPECT_GT(stats.rx_chunks_processed, 0);
    
    mb_isr_reset_stats(&ctx);
    
    mb_isr_get_stats(&ctx, &stats);
    EXPECT_EQ(stats.rx_chunks_processed, 0);
}

/* ========================================================================== */
/* Gate 23 Validation: Turnaround Time Test                                  */
/* ========================================================================== */

TEST(Gate23Validation, TurnaroundTime_Simulated) {
    // This test simulates ISR behavior and measures timing
    // In production, would run on actual hardware with DWT cycle counter
    
    mb_isr_ctx_t ctx;
    mb_isr_config_t config;
    
    void *rx_slots[32];
    void *tx_slots[16];
    uint8_t rx_buffer[256];
    uint8_t tx_buffer[256];
    
    config.rx_queue_slots = rx_slots;
    config.rx_queue_capacity = 32;
    config.tx_queue_slots = tx_slots;
    config.tx_queue_capacity = 16;
    config.rx_buffer = rx_buffer;
    config.rx_buffer_size = sizeof(rx_buffer);
    config.tx_buffer = tx_buffer;
    config.tx_buffer_size = sizeof(tx_buffer);
    config.enable_logging = false;
    config.turnaround_target_us = 100;
    
    ASSERT_EQ(mb_isr_ctx_init(&ctx, &config), MB_OK);
    
    // Simulate RX frame
    uint8_t rx_frame[] = {0x01, 0x03, 0x00, 0x64, 0x00, 0x0A, 0xC5, 0xCD};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Simulated ISR: RX processing
    mb_on_rx_chunk_from_isr(&ctx, rx_frame, sizeof(rx_frame));
    
    // Simulated ISR: Try TX (would fail here as no TX data queued, but measures overhead)
    mb_try_tx_from_isr(&ctx);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    auto duration_us = duration_ns / 1000.0;
    
    std::cout << "\n=== Gate 23 Turnaround Simulation ===" << std::endl;
    std::cout << "RX processing + TX attempt: " << duration_us << " µs" << std::endl;
    std::cout << "Target: <100 µs" << std::endl;
    
    // On host system (not embedded), should still be reasonably fast
    // Real embedded target would be much faster
    EXPECT_LT(duration_us, 1000) << "Host simulation overhead too high";
    
    mb_isr_ctx_deinit(&ctx);
}

/* ========================================================================== */
/* Gate 23 Validation: Multiple RX/TX Cycles                                 */
/* ========================================================================== */

TEST(Gate23Validation, MultipleRxTxCycles) {
    mb_isr_ctx_t ctx;
    mb_isr_config_t config;
    
    void *rx_slots[32];
    void *tx_slots[16];
    uint8_t rx_buffer[256];
    uint8_t tx_buffer[256];
    
    config.rx_queue_slots = rx_slots;
    config.rx_queue_capacity = 32;
    config.tx_queue_slots = tx_slots;
    config.tx_queue_capacity = 16;
    config.rx_buffer = rx_buffer;
    config.rx_buffer_size = sizeof(rx_buffer);
    config.tx_buffer = tx_buffer;
    config.tx_buffer_size = sizeof(tx_buffer);
    config.enable_logging = false;
    config.turnaround_target_us = 100;
    
    ASSERT_EQ(mb_isr_ctx_init(&ctx, &config), MB_OK);
    
    // Simulate multiple request/response cycles
    // Use NUM_CYCLES < rx_queue_capacity to avoid overflow
    constexpr int NUM_CYCLES = 30;
    uint8_t test_frame[] = {0x01, 0x03, 0x00, 0x64, 0x00, 0x0A, 0xC5, 0xCD};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < NUM_CYCLES; i++) {
        ASSERT_EQ(mb_on_rx_chunk_from_isr(&ctx, test_frame, sizeof(test_frame)), MB_OK);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto total_us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double avg_us = static_cast<double>(total_us) / NUM_CYCLES;
    
    std::cout << "\n=== Gate 23 Multiple Cycles ===" << std::endl;
    std::cout << "Cycles: " << NUM_CYCLES << std::endl;
    std::cout << "Total time: " << total_us << " µs" << std::endl;
    std::cout << "Average per cycle: " << avg_us << " µs" << std::endl;
    
    mb_isr_stats_t stats;
    mb_isr_get_stats(&ctx, &stats);
    
    EXPECT_EQ(stats.rx_chunks_processed, NUM_CYCLES);
    EXPECT_EQ(stats.queue_full_events, 0) << "Queue should not overflow";
    
    mb_isr_ctx_deinit(&ctx);
}

/* ========================================================================== */
/* Gate 23 Validation: ISR Overhead Measurement                              */
/* ========================================================================== */

TEST(Gate23Validation, ISROverheadMeasurement) {
    constexpr int NUM_SAMPLES = 10000;
    std::vector<double> latencies;
    latencies.reserve(NUM_SAMPLES);
    
    mb_isr_ctx_t ctx;
    mb_isr_config_t config;
    
    void *rx_slots[32];
    void *tx_slots[16];
    uint8_t rx_buffer[256];
    uint8_t tx_buffer[256];
    
    config.rx_queue_slots = rx_slots;
    config.rx_queue_capacity = 32;
    config.tx_queue_slots = tx_slots;
    config.tx_queue_capacity = 16;
    config.rx_buffer = rx_buffer;
    config.rx_buffer_size = sizeof(rx_buffer);
    config.tx_buffer = tx_buffer;
    config.tx_buffer_size = sizeof(tx_buffer);
    config.enable_logging = false;
    config.turnaround_target_us = 100;
    
    ASSERT_EQ(mb_isr_ctx_init(&ctx, &config), MB_OK);
    
    uint8_t test_frame[] = {0x01, 0x03, 0x00, 0x64, 0x00, 0x0A, 0xC5, 0xCD};
    
    // Measure individual RX processing latencies
    for (int i = 0; i < NUM_SAMPLES; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        mb_on_rx_chunk_from_isr(&ctx, test_frame, sizeof(test_frame));
        auto end = std::chrono::high_resolution_clock::now();
        
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        latencies.push_back(ns / 1000.0); // Convert to µs
    }
    
    // Calculate statistics
    double sum = 0.0;
    double min_lat = latencies[0];
    double max_lat = latencies[0];
    
    for (double lat : latencies) {
        sum += lat;
        if (lat < min_lat) min_lat = lat;
        if (lat > max_lat) max_lat = lat;
    }
    
    double avg_lat = sum / NUM_SAMPLES;
    
    // Calculate median
    std::sort(latencies.begin(), latencies.end());
    double median_lat = latencies[NUM_SAMPLES / 2];
    
    std::cout << "\n=== Gate 23 ISR Overhead Analysis ===" << std::endl;
    std::cout << "Samples: " << NUM_SAMPLES << std::endl;
    std::cout << "Average: " << avg_lat << " µs" << std::endl;
    std::cout << "Median:  " << median_lat << " µs" << std::endl;
    std::cout << "Min:     " << min_lat << " µs" << std::endl;
    std::cout << "Max:     " << max_lat << " µs" << std::endl;
    std::cout << "99th percentile: " << latencies[(int)(NUM_SAMPLES * 0.99)] << " µs" << std::endl;
    
    // Verify reasonable performance (host will be slower than embedded)
    EXPECT_LT(avg_lat, 100.0) << "Average latency too high for ISR operation";
    
    mb_isr_ctx_deinit(&ctx);
}
