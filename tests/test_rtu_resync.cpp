/**
 * @file test_rtu_resync.cpp
 * @brief Tests for RTU resynchronization mechanism
 *
 * Copyright (c) 2025 ModbusCore
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>
#include "modbus/rtu_resync.h"
#include "modbus/utils.h"

class RtuResyncTest : public ::testing::Test {
protected:
    mb_rtu_resync_t rs;
    
    void SetUp() override {
        mb_rtu_resync_init(&rs);
    }
    
    /**
     * Build a valid RTU frame
     */
    size_t build_frame(uint8_t *buf, uint8_t slave, uint8_t fc, const uint8_t *data, size_t data_len) {
        buf[0] = slave;
        buf[1] = fc;
        if (data && data_len > 0) {
            memcpy(&buf[2], data, data_len);
        }
        uint16_t crc = modbus_crc_with_table(buf, (uint16_t)(2 + data_len));
        buf[2 + data_len] = crc & 0xFF;
        buf[2 + data_len + 1] = (crc >> 8) & 0xFF;
        return 2 + data_len + 2;
    }
};

TEST_F(RtuResyncTest, InitializationSuccess) {
    EXPECT_EQ(mb_rtu_resync_available(&rs), 0u);
    
    uint32_t attempts, discarded, recovered;
    mb_rtu_resync_get_stats(&rs, &attempts, &discarded, &recovered);
    EXPECT_EQ(attempts, 0u);
    EXPECT_EQ(discarded, 0u);
    EXPECT_EQ(recovered, 0u);
}

TEST_F(RtuResyncTest, AddDataSuccess) {
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00};
    size_t added = mb_rtu_resync_add_data(&rs, data, sizeof(data));
    
    EXPECT_EQ(added, sizeof(data));
    EXPECT_EQ(mb_rtu_resync_available(&rs), sizeof(data));
}

TEST_F(RtuResyncTest, AddDataWraparound) {
    /* Fill buffer almost to capacity */
    uint8_t data[MB_RESYNC_BUFFER_SIZE + 10];
    memset(data, 0xAA, sizeof(data));
    
    mb_rtu_resync_add_data(&rs, data, sizeof(data));
    
    /* Should have wrapped and discarded oldest bytes */
    uint32_t attempts, discarded, recovered;
    mb_rtu_resync_get_stats(&rs, &attempts, &discarded, &recovered);
    EXPECT_GT(discarded, 0u);
}

TEST_F(RtuResyncTest, FindFrameStartValidAddress) {
    /* Add garbage followed by valid frame */
    uint8_t garbage[] = {0xFF, 0xFF, 0x00, 0x00};  /* Invalid addresses */
    mb_rtu_resync_add_data(&rs, garbage, sizeof(garbage));
    
    uint8_t frame[10];
    size_t frame_len = build_frame(frame, 0x01, 0x03, nullptr, 0);
    mb_rtu_resync_add_data(&rs, frame, frame_len);
    
    /* Should find frame start at offset 4 (after garbage) */
    int offset = mb_rtu_find_frame_start(&rs);
    EXPECT_EQ(offset, 4);
}

TEST_F(RtuResyncTest, FindFrameStartNoValidAddress) {
    /* Add only invalid addresses */
    uint8_t garbage[] = {0x00, 0xFF, 0xFE, 0xF8};  /* All invalid */
    mb_rtu_resync_add_data(&rs, garbage, sizeof(garbage));
    
    int offset = mb_rtu_find_frame_start(&rs);
    EXPECT_EQ(offset, -1);
}

TEST_F(RtuResyncTest, QuickCrcCheckValid) {
    uint8_t frame[10];
    size_t len = build_frame(frame, 0x01, 0x03, nullptr, 0);
    
    EXPECT_TRUE(mb_rtu_quick_crc_check(frame, len));
}

TEST_F(RtuResyncTest, QuickCrcCheckInvalid) {
    uint8_t frame[10];
    size_t len = build_frame(frame, 0x01, 0x03, nullptr, 0);
    
    /* Corrupt CRC */
    frame[len - 1] ^= 0xFF;
    
    EXPECT_FALSE(mb_rtu_quick_crc_check(frame, len));
}

TEST_F(RtuResyncTest, QuickCrcCheckTooShort) {
    uint8_t frame[] = {0x01, 0x03};  /* Too short for valid frame */
    
    EXPECT_FALSE(mb_rtu_quick_crc_check(frame, sizeof(frame)));
}

TEST_F(RtuResyncTest, IsValidSlaveAddr) {
    EXPECT_TRUE(mb_rtu_is_valid_slave_addr(1));
    EXPECT_TRUE(mb_rtu_is_valid_slave_addr(100));
    EXPECT_TRUE(mb_rtu_is_valid_slave_addr(247));
    
    EXPECT_FALSE(mb_rtu_is_valid_slave_addr(0));
    EXPECT_FALSE(mb_rtu_is_valid_slave_addr(248));
    EXPECT_FALSE(mb_rtu_is_valid_slave_addr(255));
}

TEST_F(RtuResyncTest, DiscardBytes) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    mb_rtu_resync_add_data(&rs, data, sizeof(data));
    
    mb_rtu_resync_discard(&rs, 2);
    
    EXPECT_EQ(mb_rtu_resync_available(&rs), 3u);
    
    uint32_t attempts, discarded, recovered;
    mb_rtu_resync_get_stats(&rs, &attempts, &discarded, &recovered);
    EXPECT_EQ(discarded, 2u);
}

TEST_F(RtuResyncTest, CopyBytes) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    mb_rtu_resync_add_data(&rs, data, sizeof(data));
    
    uint8_t dest[10];
    size_t copied = mb_rtu_resync_copy(&rs, dest, 3);
    
    EXPECT_EQ(copied, 3u);
    EXPECT_EQ(dest[0], 0x01);
    EXPECT_EQ(dest[1], 0x02);
    EXPECT_EQ(dest[2], 0x03);
    
    /* Buffer should still have all data (copy doesn't consume) */
    EXPECT_EQ(mb_rtu_resync_available(&rs), 5u);
}

TEST_F(RtuResyncTest, ResetStats) {
    /* Generate some activity */
    uint8_t data[100];
    memset(data, 0xFF, sizeof(data));
    mb_rtu_resync_add_data(&rs, data, sizeof(data));
    mb_rtu_resync_discard(&rs, 10);
    
    uint32_t attempts, discarded, recovered;
    mb_rtu_resync_get_stats(&rs, &attempts, &discarded, &recovered);
    EXPECT_GT(discarded, 0u);
    
    /* Reset stats */
    mb_rtu_resync_reset_stats(&rs);
    
    mb_rtu_resync_get_stats(&rs, &attempts, &discarded, &recovered);
    EXPECT_EQ(attempts, 0u);
    EXPECT_EQ(discarded, 0u);
    EXPECT_EQ(recovered, 0u);
}

TEST_F(RtuResyncTest, ResyncScenario_CorruptedFrameRecovery) {
    /* Simulate: garbage + partial corrupt frame + valid frame */
    
    /* 1. Add some garbage */
    uint8_t garbage[] = {0xFF, 0xFE, 0xFD, 0xFC};
    mb_rtu_resync_add_data(&rs, garbage, sizeof(garbage));
    
    /* 2. Add corrupted frame (wrong CRC) */
    uint8_t corrupt_frame[10];
    size_t corrupt_len = build_frame(corrupt_frame, 0x01, 0x03, nullptr, 0);
    corrupt_frame[corrupt_len - 1] ^= 0xFF;  /* Corrupt CRC */
    mb_rtu_resync_add_data(&rs, corrupt_frame, corrupt_len);
    
    /* 3. Add valid frame */
    uint8_t valid_frame[10];
    uint8_t data[] = {0x00, 0x10, 0x00, 0x01};
    size_t valid_len = build_frame(valid_frame, 0x02, 0x03, data, sizeof(data));
    mb_rtu_resync_add_data(&rs, valid_frame, valid_len);
    
    /* 4. Find first potential frame start (will find corrupt frame) */
    int offset1 = mb_rtu_find_frame_start(&rs);
    ASSERT_GE(offset1, 0);
    
    /* 5. Copy and check CRC (should fail) */
    uint8_t test_buf[20];
    mb_rtu_resync_copy(&rs, test_buf, corrupt_len);
    EXPECT_FALSE(mb_rtu_quick_crc_check(test_buf, corrupt_len));
    
    /* 6. Discard garbage + corrupt frame */
    mb_rtu_resync_discard(&rs, sizeof(garbage) + corrupt_len);
    
    /* 7. Find valid frame */
    int offset2 = mb_rtu_find_frame_start(&rs);
    ASSERT_GE(offset2, 0);
    
    /* 8. Verify valid frame */
    mb_rtu_resync_copy(&rs, test_buf, valid_len);
    EXPECT_TRUE(mb_rtu_quick_crc_check(test_buf, valid_len));
    EXPECT_EQ(test_buf[0], 0x02);  /* Slave address */
    EXPECT_EQ(test_buf[1], 0x03);  /* Function code */
}

TEST_F(RtuResyncTest, ResyncScenario_MultipleValidFrames) {
    /* Add multiple valid frames separated by garbage */
    
    uint8_t frame1[10];
    size_t len1 = build_frame(frame1, 0x01, 0x03, nullptr, 0);
    mb_rtu_resync_add_data(&rs, frame1, len1);
    
    uint8_t garbage[] = {0xFF, 0x00};
    mb_rtu_resync_add_data(&rs, garbage, sizeof(garbage));
    
    uint8_t frame2[10];
    size_t len2 = build_frame(frame2, 0x02, 0x06, nullptr, 0);
    mb_rtu_resync_add_data(&rs, frame2, len2);
    
    /* Find and validate frame 1 */
    int offset1 = mb_rtu_find_frame_start(&rs);
    EXPECT_EQ(offset1, 0);
    
    uint8_t buf[20];
    mb_rtu_resync_copy(&rs, buf, len1);
    EXPECT_TRUE(mb_rtu_quick_crc_check(buf, len1));
    
    /* Consume frame 1 */
    mb_rtu_resync_discard(&rs, len1);
    
    /* Skip garbage */
    mb_rtu_resync_discard(&rs, sizeof(garbage));
    
    /* Find and validate frame 2 */
    int offset2 = mb_rtu_find_frame_start(&rs);
    EXPECT_EQ(offset2, 0);
    
    mb_rtu_resync_copy(&rs, buf, len2);
    EXPECT_TRUE(mb_rtu_quick_crc_check(buf, len2));
    EXPECT_EQ(buf[0], 0x02);
}

TEST_F(RtuResyncTest, NullPointerHandling) {
    EXPECT_EQ(mb_rtu_resync_add_data(nullptr, nullptr, 10), 0u);
    EXPECT_EQ(mb_rtu_find_frame_start(nullptr), -1);
    EXPECT_FALSE(mb_rtu_quick_crc_check(nullptr, 10));
    
    mb_rtu_resync_discard(nullptr, 10);  /* Should not crash */
    EXPECT_EQ(mb_rtu_resync_available(nullptr), 0u);
    EXPECT_EQ(mb_rtu_resync_copy(nullptr, nullptr, 10), 0u);
}
