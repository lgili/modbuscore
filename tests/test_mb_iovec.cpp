/**
 * @file test_mb_iovec.cpp
 * @brief Unit tests for zero-copy scatter-gather IO primitives.
 */

#include <gtest/gtest.h>
#include <modbus/mb_iovec.h>
#include <modbus/mb_err.h>
#include <cstring>

// Test fixture for iovec tests
class MbIovecTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset any global state if needed
#if MB_CONF_ENABLE_IOVEC_STATS
        memset(&g_mb_iovec_stats, 0, sizeof(g_mb_iovec_stats));
#endif
    }
};

// ============================================================================
// Basic iovec operations
// ============================================================================

TEST_F(MbIovecTest, IovecInit) {
    mb_iovec_t iov;
    uint8_t data[] = {1, 2, 3, 4, 5};
    
    mb_iovec_init(&iov, data, sizeof(data));
    
    EXPECT_EQ(iov.base, data);
    EXPECT_EQ(iov.len, sizeof(data));
}

TEST_F(MbIovecTest, IovecInitNull) {
    mb_iovec_init(NULL, NULL, 0);
    // Should not crash
}

TEST_F(MbIovecTest, IovecListInit) {
    mb_iovec_t vectors[4];
    mb_iovec_list_t list;
    
    mb_iovec_list_init(&list, vectors, 4);
    
    EXPECT_EQ(list.vectors, vectors);
    EXPECT_EQ(list.count, 0);
    EXPECT_EQ(list.total_len, 0);
}

TEST_F(MbIovecTest, IovecListAdd) {
    mb_iovec_t vectors[4];
    mb_iovec_list_t list;
    uint8_t data1[] = {1, 2, 3};
    uint8_t data2[] = {4, 5};
    
    mb_iovec_list_init(&list, vectors, 4);
    
    EXPECT_EQ(mb_iovec_list_add(&list, data1, sizeof(data1)), MB_OK);
    EXPECT_EQ(list.count, 1);
    EXPECT_EQ(list.total_len, 3);
    
    EXPECT_EQ(mb_iovec_list_add(&list, data2, sizeof(data2)), MB_OK);
    EXPECT_EQ(list.count, 2);
    EXPECT_EQ(list.total_len, 5);
}

TEST_F(MbIovecTest, IovecListAddZeroLength) {
    mb_iovec_t vectors[4];
    mb_iovec_list_t list;
    uint8_t data[] = {1, 2, 3};
    
    mb_iovec_list_init(&list, vectors, 4);
    
    // Add zero-length region (should be skipped)
    EXPECT_EQ(mb_iovec_list_add(&list, data, 0), MB_OK);
    EXPECT_EQ(list.count, 0);
    EXPECT_EQ(list.total_len, 0);
    
    // Add normal region
    EXPECT_EQ(mb_iovec_list_add(&list, data, sizeof(data)), MB_OK);
    EXPECT_EQ(list.count, 1);
    EXPECT_EQ(list.total_len, 3);
}

TEST_F(MbIovecTest, IovecListTotal) {
    mb_iovec_t vectors[4];
    mb_iovec_list_t list;
    uint8_t data1[] = {1, 2, 3};
    uint8_t data2[] = {4, 5, 6, 7};
    
    mb_iovec_list_init(&list, vectors, 4);
    mb_iovec_list_add(&list, data1, sizeof(data1));
    mb_iovec_list_add(&list, data2, sizeof(data2));
    
    EXPECT_EQ(mb_iovec_list_total(&list), 7);
}

// ============================================================================
// Copy operations
// ============================================================================

TEST_F(MbIovecTest, IovecListCopyout) {
    mb_iovec_t vectors[3];
    mb_iovec_list_t list;
    uint8_t data1[] = {1, 2, 3};
    uint8_t data2[] = {4, 5};
    uint8_t data3[] = {6, 7, 8, 9};
    uint8_t dst[20];
    
    mb_iovec_list_init(&list, vectors, 3);
    mb_iovec_list_add(&list, data1, sizeof(data1));
    mb_iovec_list_add(&list, data2, sizeof(data2));
    mb_iovec_list_add(&list, data3, sizeof(data3));
    
    memset(dst, 0xFF, sizeof(dst));
    size_t copied = mb_iovec_list_copyout(&list, dst, sizeof(dst));
    
    EXPECT_EQ(copied, 9);
    EXPECT_EQ(dst[0], 1);
    EXPECT_EQ(dst[1], 2);
    EXPECT_EQ(dst[2], 3);
    EXPECT_EQ(dst[3], 4);
    EXPECT_EQ(dst[4], 5);
    EXPECT_EQ(dst[5], 6);
    EXPECT_EQ(dst[6], 7);
    EXPECT_EQ(dst[7], 8);
    EXPECT_EQ(dst[8], 9);
    EXPECT_EQ(dst[9], 0xFF); // Unchanged
}

TEST_F(MbIovecTest, IovecListCopyoutPartial) {
    mb_iovec_t vectors[2];
    mb_iovec_list_t list;
    uint8_t data1[] = {1, 2, 3, 4, 5};
    uint8_t data2[] = {6, 7, 8};
    uint8_t dst[6];
    
    mb_iovec_list_init(&list, vectors, 2);
    mb_iovec_list_add(&list, data1, sizeof(data1));
    mb_iovec_list_add(&list, data2, sizeof(data2));
    
    // Only copy 6 bytes (should stop mid-vector)
    size_t copied = mb_iovec_list_copyout(&list, dst, sizeof(dst));
    
    EXPECT_EQ(copied, 6);
    EXPECT_EQ(dst[0], 1);
    EXPECT_EQ(dst[5], 6);
}

TEST_F(MbIovecTest, IovecListCopyin) {
    mb_iovec_t vectors[2];
    mb_iovec_list_t list;
    uint8_t data1[5] = {0};
    uint8_t data2[3] = {0};
    uint8_t src[] = {1, 2, 3, 4, 5, 6, 7, 8};
    
    mb_iovec_list_init(&list, vectors, 2);
    mb_iovec_list_add(&list, data1, sizeof(data1));
    mb_iovec_list_add(&list, data2, sizeof(data2));
    
    size_t copied = mb_iovec_list_copyin(&list, src, sizeof(src));
    
    EXPECT_EQ(copied, 8);
    EXPECT_EQ(data1[0], 1);
    EXPECT_EQ(data1[4], 5);
    EXPECT_EQ(data2[0], 6);
    EXPECT_EQ(data2[2], 8);
}

// ============================================================================
// Ring buffer operations
// ============================================================================

TEST_F(MbIovecTest, IovecFromRingNoWrap) {
    mb_iovec_t vectors[2];
    mb_iovec_list_t list;
    uint8_t ring[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    
    mb_iovec_list_init(&list, vectors, 2);
    
    // Read 5 bytes starting at offset 2 (no wrap-around)
    mb_err_t err = mb_iovec_from_ring(&list, ring, 10, 2, 5);
    
    EXPECT_EQ(err, MB_OK);
    EXPECT_EQ(list.count, 1); // Single contiguous region
    EXPECT_EQ(list.total_len, 5);
    EXPECT_EQ(list.vectors[0].base, ring + 2);
    EXPECT_EQ(list.vectors[0].len, 5);
}

TEST_F(MbIovecTest, IovecFromRingWithWrap) {
    mb_iovec_t vectors[2];
    mb_iovec_list_t list;
    uint8_t ring[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    
    mb_iovec_list_init(&list, vectors, 2);
    
    // Read 5 bytes starting at offset 7 (wraps around at 10)
    mb_err_t err = mb_iovec_from_ring(&list, ring, 10, 7, 5);
    
    EXPECT_EQ(err, MB_OK);
    EXPECT_EQ(list.count, 2); // Two regions (wrap-around)
    EXPECT_EQ(list.total_len, 5);
    
    // First region: 3 bytes from offset 7
    EXPECT_EQ(list.vectors[0].base, ring + 7);
    EXPECT_EQ(list.vectors[0].len, 3);
    
    // Second region: 2 bytes from offset 0
    EXPECT_EQ(list.vectors[1].base, ring);
    EXPECT_EQ(list.vectors[1].len, 2);
}

TEST_F(MbIovecTest, IovecFromRingExactFit) {
    mb_iovec_t vectors[2];
    mb_iovec_list_t list;
    uint8_t ring[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    
    mb_iovec_list_init(&list, vectors, 2);
    
    // Read 3 bytes starting at offset 7 (exactly fits, no wrap)
    mb_err_t err = mb_iovec_from_ring(&list, ring, 10, 7, 3);
    
    EXPECT_EQ(err, MB_OK);
    EXPECT_EQ(list.count, 1);
    EXPECT_EQ(list.total_len, 3);
    EXPECT_EQ(list.vectors[0].len, 3);
}

TEST_F(MbIovecTest, IovecFromRingInvalidArgs) {
    mb_iovec_t vectors[2];
    mb_iovec_list_t list;
    uint8_t ring[10] = {0};
    
    mb_iovec_list_init(&list, vectors, 2);
    
    // NULL list
    EXPECT_EQ(mb_iovec_from_ring(NULL, ring, 10, 0, 5), MB_ERR_INVALID_ARGUMENT);
    
    // NULL ring
    EXPECT_EQ(mb_iovec_from_ring(&list, NULL, 10, 0, 5), MB_ERR_INVALID_ARGUMENT);
    
    // Zero length
    EXPECT_EQ(mb_iovec_from_ring(&list, ring, 10, 0, 0), MB_ERR_INVALID_ARGUMENT);
    
    // Start beyond capacity
    EXPECT_EQ(mb_iovec_from_ring(&list, ring, 10, 15, 5), MB_ERR_INVALID_ARGUMENT);
}

// ============================================================================
// Statistics (if enabled)
// ============================================================================

#if MB_CONF_ENABLE_IOVEC_STATS
TEST_F(MbIovecTest, StatsTracking) {
    mb_iovec_t vectors[2];
    mb_iovec_list_t list;
    uint8_t data[] = {1, 2, 3, 4, 5};
    uint8_t dst[10];
    uint8_t ring[10] = {0};
    
    mb_iovec_list_init(&list, vectors, 2);
    mb_iovec_list_add(&list, data, sizeof(data));
    
    // Reset stats
    memset(&g_mb_iovec_stats, 0, sizeof(g_mb_iovec_stats));
    
    // Copyout should increment tx_memcpy
    mb_iovec_list_copyout(&list, dst, sizeof(dst));
    EXPECT_EQ(g_mb_iovec_stats.tx_memcpy, 1);
    
    // Copyin should increment rx_memcpy
    mb_iovec_list_copyin(&list, data, sizeof(data));
    EXPECT_EQ(g_mb_iovec_stats.rx_memcpy, 1);
    
    // Ring buffer should increment rx_zero_copy
    mb_iovec_from_ring(&list, ring, 10, 0, 5);
    EXPECT_EQ(g_mb_iovec_stats.rx_zero_copy, 1);
}
#endif

// ============================================================================
// Integration: Simulating actual usage
// ============================================================================

TEST_F(MbIovecTest, SimulateRingBufferTx) {
    // Simulate encoding a PDU directly into a ring buffer view
    uint8_t ring[256] = {0};
    mb_iovec_t vectors[2];
    mb_iovec_list_t list;
    
    // Simulate ring buffer with data at offset 200 (will wrap)
    size_t start = 200;
    size_t len = 100; // 56 bytes wrap around to start
    
    mb_iovec_list_init(&list, vectors, 2);
    mb_err_t err = mb_iovec_from_ring(&list, ring, 256, start, len);
    
    EXPECT_EQ(err, MB_OK);
    EXPECT_EQ(list.total_len, 100);
    
    // Should have 2 vectors due to wrap
    EXPECT_EQ(list.count, 2);
    EXPECT_EQ(list.vectors[0].len, 56); // 256 - 200 = 56
    EXPECT_EQ(list.vectors[1].len, 44); // 100 - 56 = 44
}

TEST_F(MbIovecTest, SimulatePDUFragmentation) {
    // Simulate a PDU split across multiple buffers
    uint8_t header[7] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5};
    uint8_t data[20] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
    uint8_t crc[2] = {0xAB, 0xCD};
    
    mb_iovec_t vectors[3];
    mb_iovec_list_t list;
    
    mb_iovec_list_init(&list, vectors, 3);
    mb_iovec_list_add(&list, header, sizeof(header));
    mb_iovec_list_add(&list, data, sizeof(data));
    mb_iovec_list_add(&list, crc, sizeof(crc));
    
    EXPECT_EQ(list.count, 3);
    EXPECT_EQ(list.total_len, 29);
    
    // Verify we can reconstruct the full frame
    uint8_t reconstructed[50];
    size_t copied = mb_iovec_list_copyout(&list, reconstructed, sizeof(reconstructed));
    
    EXPECT_EQ(copied, 29);
    EXPECT_EQ(reconstructed[0], 0x01); // Function code
    EXPECT_EQ(reconstructed[6], 0xC5);
    EXPECT_EQ(reconstructed[7], 1);    // First data byte
    EXPECT_EQ(reconstructed[27], 0xAB); // First CRC byte
    EXPECT_EQ(reconstructed[28], 0xCD); // Second CRC byte
}
