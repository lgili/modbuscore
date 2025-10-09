/**
 * @file test_dup_filter.cpp
 * @brief Tests for duplicate frame filtering
 *
 * Copyright (c) 2025 ModbusCore
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>
#include "modbus/dup_filter.h"
#include <thread>
#include <chrono>

class DupFilterTest : public ::testing::Test {
protected:
    mb_dup_filter_t df;
    
    void SetUp() override {
        mb_dup_filter_init(&df, 500);  // 500ms window
    }
    
    uint32_t mock_time_ms = 0;
    
    uint32_t get_time() {
        return mock_time_ms;
    }
    
    void advance_time(uint32_t ms) {
        mock_time_ms += ms;
    }
};

TEST_F(DupFilterTest, InitializationSuccess) {
    EXPECT_EQ(df.window_ms, 500u);
    EXPECT_EQ(df.count, 0u);
    EXPECT_EQ(df.head, 0u);
    
    uint32_t checked, duplicates, false_pos;
    mb_dup_filter_get_stats(&df, &checked, &duplicates, &false_pos);
    EXPECT_EQ(checked, 0u);
    EXPECT_EQ(duplicates, 0u);
    EXPECT_EQ(false_pos, 0u);
}

TEST_F(DupFilterTest, InitializationWithDefaultWindow) {
    mb_dup_filter_t df2;
    mb_dup_filter_init(&df2, 0);  // 0 = use default
    
    EXPECT_EQ(df2.window_ms, MB_DUP_WINDOW_MS);
}

TEST_F(DupFilterTest, HashConsistency) {
    uint8_t data[] = {0x00, 0x10, 0x00, 0x01};
    
    uint32_t hash1 = mb_adu_hash(0x01, 0x03, data, sizeof(data));
    uint32_t hash2 = mb_adu_hash(0x01, 0x03, data, sizeof(data));
    
    EXPECT_EQ(hash1, hash2);
}

TEST_F(DupFilterTest, HashDifferentForDifferentFrames) {
    uint8_t data1[] = {0x00, 0x10, 0x00, 0x01};
    uint8_t data2[] = {0x00, 0x20, 0x00, 0x01};
    
    uint32_t hash1 = mb_adu_hash(0x01, 0x03, data1, sizeof(data1));
    uint32_t hash2 = mb_adu_hash(0x01, 0x03, data2, sizeof(data2));
    
    EXPECT_NE(hash1, hash2);
}

TEST_F(DupFilterTest, HashDifferentSlaveAddress) {
    uint8_t data[] = {0x00, 0x10, 0x00, 0x01};
    
    uint32_t hash1 = mb_adu_hash(0x01, 0x03, data, sizeof(data));
    uint32_t hash2 = mb_adu_hash(0x02, 0x03, data, sizeof(data));
    
    EXPECT_NE(hash1, hash2);
}

TEST_F(DupFilterTest, HashDifferentFunctionCode) {
    uint8_t data[] = {0x00, 0x10, 0x00, 0x01};
    
    uint32_t hash1 = mb_adu_hash(0x01, 0x03, data, sizeof(data));
    uint32_t hash2 = mb_adu_hash(0x01, 0x06, data, sizeof(data));
    
    EXPECT_NE(hash1, hash2);
}

TEST_F(DupFilterTest, CheckNotDuplicateInitially) {
    uint8_t data[] = {0x00, 0x10, 0x00, 0x01};
    uint32_t hash = mb_adu_hash(0x01, 0x03, data, sizeof(data));
    
    EXPECT_FALSE(mb_dup_filter_check(&df, hash, get_time()));
}

TEST_F(DupFilterTest, DetectDuplicateWithinWindow) {
    uint8_t data[] = {0x00, 0x10, 0x00, 0x01};
    uint32_t hash = mb_adu_hash(0x01, 0x03, data, sizeof(data));
    
    // Add frame
    mb_dup_filter_add(&df, hash, get_time());
    
    // Advance time within window
    advance_time(100);
    
    // Should detect duplicate
    EXPECT_TRUE(mb_dup_filter_check(&df, hash, get_time()));
    
    uint32_t checked, duplicates, false_pos;
    mb_dup_filter_get_stats(&df, &checked, &duplicates, &false_pos);
    EXPECT_EQ(duplicates, 1u);
}

TEST_F(DupFilterTest, NotDuplicateOutsideWindow) {
    uint8_t data[] = {0x00, 0x10, 0x00, 0x01};
    uint32_t hash = mb_adu_hash(0x01, 0x03, data, sizeof(data));
    
    // Add frame
    mb_dup_filter_add(&df, hash, get_time());
    
    // Advance time beyond window
    advance_time(600);  // 600ms > 500ms window
    
    // Should NOT detect duplicate (aged out)
    EXPECT_FALSE(mb_dup_filter_check(&df, hash, get_time()));
}

TEST_F(DupFilterTest, MultipleDifferentFrames) {
    uint8_t data1[] = {0x00, 0x10, 0x00, 0x01};
    uint8_t data2[] = {0x00, 0x20, 0x00, 0x01};
    
    uint32_t hash1 = mb_adu_hash(0x01, 0x03, data1, sizeof(data1));
    uint32_t hash2 = mb_adu_hash(0x01, 0x03, data2, sizeof(data2));
    
    // Add both frames
    mb_dup_filter_add(&df, hash1, get_time());
    advance_time(50);
    mb_dup_filter_add(&df, hash2, get_time());
    
    // Both should be detected as duplicates
    advance_time(50);
    EXPECT_TRUE(mb_dup_filter_check(&df, hash1, get_time()));
    EXPECT_TRUE(mb_dup_filter_check(&df, hash2, get_time()));
}

TEST_F(DupFilterTest, WindowFillAndEviction) {
    // Fill window to capacity
    for (size_t i = 0; i < MB_DUP_WINDOW_SIZE; i++) {
        uint8_t data[] = {(uint8_t)i, 0x00, 0x00, 0x00};
        uint32_t hash = mb_adu_hash(0x01, 0x03, data, sizeof(data));
        mb_dup_filter_add(&df, hash, get_time());
        advance_time(10);
    }
    
    EXPECT_EQ(df.count, MB_DUP_WINDOW_SIZE);
    
    // Add one more (should evict oldest)
    uint8_t data[] = {0xFF, 0x00, 0x00, 0x00};
    uint32_t hash = mb_adu_hash(0x01, 0x03, data, sizeof(data));
    mb_dup_filter_add(&df, hash, get_time());
    
    EXPECT_EQ(df.count, MB_DUP_WINDOW_SIZE);
}

TEST_F(DupFilterTest, AgeOutOldEntries) {
    // Add several frames
    for (size_t i = 0; i < 4; i++) {
        uint8_t data[] = {(uint8_t)i, 0x00, 0x00, 0x00};
        uint32_t hash = mb_adu_hash(0x01, 0x03, data, sizeof(data));
        mb_dup_filter_add(&df, hash, get_time());
        advance_time(100);
    }
    
    EXPECT_EQ(df.count, 4u);
    
    // Advance time to age out first entries
    advance_time(300);  // Total 600ms from first entry
    
    // Trigger age-out by checking
    uint8_t data[] = {0xFF, 0x00, 0x00, 0x00};
    uint32_t hash = mb_adu_hash(0x01, 0x03, data, sizeof(data));
    mb_dup_filter_check(&df, hash, get_time());
    
    // Should have aged out old entries
    EXPECT_LT(df.count, 4u);
}

TEST_F(DupFilterTest, ClearFilter) {
    // Add some frames
    for (size_t i = 0; i < 4; i++) {
        uint8_t data[] = {(uint8_t)i, 0x00, 0x00, 0x00};
        uint32_t hash = mb_adu_hash(0x01, 0x03, data, sizeof(data));
        mb_dup_filter_add(&df, hash, get_time());
    }
    
    EXPECT_EQ(df.count, 4u);
    
    // Clear filter
    mb_dup_filter_clear(&df);
    
    EXPECT_EQ(df.count, 0u);
    EXPECT_EQ(df.head, 0u);
}

TEST_F(DupFilterTest, ResetStats) {
    uint8_t data[] = {0x00, 0x10, 0x00, 0x01};
    uint32_t hash = mb_adu_hash(0x01, 0x03, data, sizeof(data));
    
    // Generate some activity
    mb_dup_filter_add(&df, hash, get_time());
    mb_dup_filter_check(&df, hash, get_time());
    mb_dup_filter_check(&df, hash, get_time());
    
    uint32_t checked, duplicates, false_pos;
    mb_dup_filter_get_stats(&df, &checked, &duplicates, &false_pos);
    EXPECT_GT(checked, 0u);
    EXPECT_GT(duplicates, 0u);
    
    // Reset stats
    mb_dup_filter_reset_stats(&df);
    
    mb_dup_filter_get_stats(&df, &checked, &duplicates, &false_pos);
    EXPECT_EQ(checked, 0u);
    EXPECT_EQ(duplicates, 0u);
    EXPECT_EQ(false_pos, 0u);
}

TEST_F(DupFilterTest, RealWorldScenario_Retransmission) {
    // Simulate client sends request, gets no response, retries
    
    uint8_t request[] = {0x00, 0x10, 0x00, 0x01};
    uint32_t req_hash = mb_adu_hash(0x01, 0x03, request, sizeof(request));
    
    // First transmission
    mb_dup_filter_add(&df, req_hash, get_time());
    EXPECT_FALSE(mb_dup_filter_check(&df, req_hash, get_time()));
    
    // Client times out and retransmits after 200ms
    advance_time(200);
    mb_dup_filter_add(&df, req_hash, get_time());
    EXPECT_TRUE(mb_dup_filter_check(&df, req_hash, get_time()));  // Duplicate!
    
    // Server response
    uint8_t response[] = {0x02, 0x12, 0x34, 0x56};
    uint32_t resp_hash = mb_adu_hash(0x01, 0x03, response, sizeof(response));
    mb_dup_filter_add(&df, resp_hash, get_time());
    EXPECT_FALSE(mb_dup_filter_check(&df, resp_hash, get_time()));
}

TEST_F(DupFilterTest, RealWorldScenario_LineReflection) {
    // Simulate RS-485 line reflection causing duplicate
    
    uint8_t data[] = {0x00, 0x10, 0x00, 0x01};
    uint32_t hash = mb_adu_hash(0x01, 0x03, data, sizeof(data));
    
    // Original frame
    mb_dup_filter_add(&df, hash, get_time());
    
    // Reflection arrives 5ms later
    advance_time(5);
    EXPECT_TRUE(mb_dup_filter_check(&df, hash, get_time()));
    
    uint32_t checked, duplicates, false_pos;
    mb_dup_filter_get_stats(&df, &checked, &duplicates, &false_pos);
    EXPECT_EQ(duplicates, 1u);
}

TEST_F(DupFilterTest, NullPointerHandling) {
    uint8_t data[] = {0x00, 0x10};
    uint32_t hash = mb_adu_hash(0x01, 0x03, data, sizeof(data));
    
    mb_dup_filter_init(nullptr, 100);  // Should not crash
    EXPECT_FALSE(mb_dup_filter_check(nullptr, hash, 0));
    mb_dup_filter_add(nullptr, hash, 0);  // Should not crash
    mb_dup_filter_clear(nullptr);  // Should not crash
    mb_dup_filter_reset_stats(nullptr);  // Should not crash
}

TEST_F(DupFilterTest, EmptyDataHash) {
    // Hash with no data bytes
    uint32_t hash1 = mb_adu_hash(0x01, 0x03, nullptr, 0);
    uint32_t hash2 = mb_adu_hash(0x01, 0x03, nullptr, 0);
    
    EXPECT_EQ(hash1, hash2);
    EXPECT_NE(hash1, 0u);  // Should still produce valid hash
}

TEST_F(DupFilterTest, ShortDataHash) {
    // Hash with less than 4 bytes
    uint8_t data[] = {0x01, 0x02};
    uint32_t hash = mb_adu_hash(0x01, 0x03, data, sizeof(data));
    
    EXPECT_NE(hash, 0u);
}

TEST_F(DupFilterTest, LongDataHashOnlyUsesFirst4Bytes) {
    uint8_t data1[] = {0x01, 0x02, 0x03, 0x04, 0xFF, 0xFF};
    uint8_t data2[] = {0x01, 0x02, 0x03, 0x04, 0xAA, 0xBB};
    
    uint32_t hash1 = mb_adu_hash(0x01, 0x03, data1, sizeof(data1));
    uint32_t hash2 = mb_adu_hash(0x01, 0x03, data2, sizeof(data2));
    
    // Should be same (only first 4 bytes matter)
    EXPECT_EQ(hash1, hash2);
}
