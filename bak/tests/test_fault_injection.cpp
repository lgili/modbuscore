/**
 * @file test_fault_injection.cpp
 * @brief Fault injection framework tests
 *
 * Copyright (c) 2025 ModbusCore
 * SPDX-License-Identifier: MIT
 */

#include <gtest/gtest.h>
#include "fault_inject.h"
#include <string.h>

class FaultInjectionTest : public ::testing::Test {
protected:
    mb_fault_inject_transport_t fit;
    mb_transport_if_t mock_transport;
    
    /* Mock transport stats */
    static size_t send_count;
    static size_t recv_count;
    static uint8_t last_sent[512];
    static size_t last_sent_len;
    
    void SetUp() override {
        /* Setup mock transport */
        mock_transport.ctx = this;
        mock_transport.send = mock_send;
        mock_transport.recv = mock_recv;
        mock_transport.poll = mock_poll;
        mock_transport.reset = mock_reset;
        
        /* Create fault injection wrapper */
        mb_fault_inject_create(&fit, &mock_transport, 12345);
        
        /* Reset stats */
        send_count = 0;
        recv_count = 0;
        last_sent_len = 0;
    }
    
    static mb_err_t mock_send(void *ctx, const uint8_t *data, size_t len) {
        (void)ctx;
        send_count++;
        if (len <= sizeof(last_sent)) {
            memcpy(last_sent, data, len);
            last_sent_len = len;
        }
        return MB_OK;
    }
    
    static mb_err_t mock_recv(void *ctx, uint8_t *buf, size_t buf_size, size_t *received) {
        (void)ctx;
        (void)buf;
        (void)buf_size;
        recv_count++;
        *received = 0;
        return MB_OK;
    }
    
    static mb_err_t mock_poll(void *ctx) {
        (void)ctx;
        return MB_OK;
    }
    
    static void mock_reset(void *ctx) {
        (void)ctx;
    }
};

/* Static member initialization */
size_t FaultInjectionTest::send_count = 0;
size_t FaultInjectionTest::recv_count = 0;
uint8_t FaultInjectionTest::last_sent[512] = {0};
size_t FaultInjectionTest::last_sent_len = 0;

TEST_F(FaultInjectionTest, CreateSuccess) {
    EXPECT_NE(fit.transport_if.send, nullptr);
    EXPECT_NE(fit.transport_if.recv, nullptr);
    EXPECT_NE(fit.transport_if.poll, nullptr);
    EXPECT_NE(fit.transport_if.reset, nullptr);
}

TEST_F(FaultInjectionTest, NoFaultsPassthrough) {
    /* No fault rules, should pass through unchanged */
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    
    mb_err_t err = fit.transport_if.send(fit.transport_if.ctx, data, sizeof(data));
    
    EXPECT_EQ(err, MB_OK);
    EXPECT_EQ(send_count, 1u);
    EXPECT_EQ(last_sent_len, sizeof(data));
    EXPECT_EQ(memcmp(last_sent, data, sizeof(data)), 0);
}

TEST_F(FaultInjectionTest, BitFlipFault) {
    /* Add bit flip rule with 100% probability */
    mb_fault_inject_add_rule(&fit, FAULT_BIT_FLIP, 1.0f, 10);  /* 10% bit flip rate */
    
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    uint8_t original[sizeof(data)];
    memcpy(original, data, sizeof(data));
    
    fit.transport_if.send(fit.transport_if.ctx, data, sizeof(data));
    
    /* Should have some bit flips (not guaranteed to be different, but stats should show) */
    mb_fault_stats_t stats;
    mb_fault_inject_get_stats(&fit, &stats);
    EXPECT_GT(stats.total_injected, 0u);
}

TEST_F(FaultInjectionTest, TruncateFault) {
    /* Add truncate rule with 100% probability */
    mb_fault_inject_add_rule(&fit, FAULT_TRUNCATE, 1.0f, 3);  /* Drop up to 3 bytes */
    
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xAA, 0xBB};
    
    fit.transport_if.send(fit.transport_if.ctx, data, sizeof(data));
    
    /* Sent frame should be shorter */
    EXPECT_LT(last_sent_len, sizeof(data));
    EXPECT_GE(last_sent_len, sizeof(data) - 3);
    
    mb_fault_stats_t stats;
    mb_fault_inject_get_stats(&fit, &stats);
    EXPECT_EQ(stats.truncations, 1u);
}

TEST_F(FaultInjectionTest, PhantomBytesFault) {
    /* Add phantom bytes rule */
    mb_fault_inject_add_rule(&fit, FAULT_PHANTOM_BYTES, 1.0f, 5);  /* Insert up to 5 bytes */
    
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    
    fit.transport_if.send(fit.transport_if.ctx, data, sizeof(data));
    
    /* Sent frame should be longer */
    EXPECT_GT(last_sent_len, sizeof(data));
    EXPECT_LE(last_sent_len, sizeof(data) + 5);
    
    mb_fault_stats_t stats;
    mb_fault_inject_get_stats(&fit, &stats);
    EXPECT_EQ(stats.phantom_insertions, 1u);
}

TEST_F(FaultInjectionTest, DropFrameFault) {
    /* Add drop frame rule */
    mb_fault_inject_add_rule(&fit, FAULT_DROP_FRAME, 1.0f, 0);
    
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    
    fit.transport_if.send(fit.transport_if.ctx, data, sizeof(data));
    
    /* No frame should be sent */
    EXPECT_EQ(send_count, 0u);
    
    mb_fault_stats_t stats;
    mb_fault_inject_get_stats(&fit, &stats);
    EXPECT_EQ(stats.drops, 1u);
}

TEST_F(FaultInjectionTest, DuplicateFault) {
    /* Add duplicate rule */
    mb_fault_inject_add_rule(&fit, FAULT_DUPLICATE, 1.0f, 0);
    
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    
    fit.transport_if.send(fit.transport_if.ctx, data, sizeof(data));
    
    /* Should send twice (original + duplicate) */
    EXPECT_EQ(send_count, 2u);
    
    mb_fault_stats_t stats;
    mb_fault_inject_get_stats(&fit, &stats);
    EXPECT_EQ(stats.duplications, 1u);
}

TEST_F(FaultInjectionTest, MultipleFaultRules) {
    /* Add multiple rules with low probability */
    mb_fault_inject_add_rule(&fit, FAULT_BIT_FLIP, 0.1f, 5);
    mb_fault_inject_add_rule(&fit, FAULT_TRUNCATE, 0.1f, 2);
    mb_fault_inject_add_rule(&fit, FAULT_DROP_FRAME, 0.05f, 0);
    
    /* Send many frames */
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    for (int i = 0; i < 100; i++) {
        fit.transport_if.send(fit.transport_if.ctx, data, sizeof(data));
    }
    
    mb_fault_stats_t stats;
    mb_fault_inject_get_stats(&fit, &stats);
    
    /* Should have some faults injected */
    EXPECT_GT(stats.total_injected, 0u);
    EXPECT_EQ(stats.total_frames, 100u);
}

TEST_F(FaultInjectionTest, StatisticsTracking) {
    mb_fault_inject_add_rule(&fit, FAULT_BIT_FLIP, 1.0f, 10);
    mb_fault_inject_add_rule(&fit, FAULT_TRUNCATE, 1.0f, 2);
    
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    
    fit.transport_if.send(fit.transport_if.ctx, data, sizeof(data));
    fit.transport_if.send(fit.transport_if.ctx, data, sizeof(data));
    
    mb_fault_stats_t stats;
    mb_fault_inject_get_stats(&fit, &stats);
    
    EXPECT_EQ(stats.total_frames, 2u);
    EXPECT_GT(stats.total_injected, 0u);
}

TEST_F(FaultInjectionTest, ResetStatistics) {
    mb_fault_inject_add_rule(&fit, FAULT_BIT_FLIP, 1.0f, 10);
    
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    fit.transport_if.send(fit.transport_if.ctx, data, sizeof(data));
    
    mb_fault_stats_t stats;
    mb_fault_inject_get_stats(&fit, &stats);
    EXPECT_GT(stats.total_frames, 0u);
    
    /* Reset */
    mb_fault_inject_reset_stats(&fit);
    
    mb_fault_inject_get_stats(&fit, &stats);
    EXPECT_EQ(stats.total_frames, 0u);
    EXPECT_EQ(stats.total_injected, 0u);
}

TEST_F(FaultInjectionTest, PRNG_Deterministic) {
    uint32_t seed = 12345;
    uint32_t state1 = seed;
    uint32_t state2 = seed;
    
    /* Same seed should produce same sequence */
    uint32_t val1 = mb_fault_prng(&state1);
    uint32_t val2 = mb_fault_prng(&state2);
    
    EXPECT_EQ(val1, val2);
    EXPECT_EQ(state1, state2);
}

TEST_F(FaultInjectionTest, PRNG_FloatRange) {
    uint32_t state = 12345;
    
    /* Generate 1000 random floats, all should be in [0, 1) */
    for (int i = 0; i < 1000; i++) {
        float val = mb_fault_prng_float(&state);
        EXPECT_GE(val, 0.0f);
        EXPECT_LT(val, 1.0f);
    }
}

/* Chaos test scenario */
TEST_F(FaultInjectionTest, ChaosScenario) {
    /* Simulate harsh environment */
    mb_fault_inject_add_rule(&fit, FAULT_BIT_FLIP, 0.10f, 5);    /* 10% frames with bit flips */
    mb_fault_inject_add_rule(&fit, FAULT_TRUNCATE, 0.10f, 3);    /* 10% truncated */
    mb_fault_inject_add_rule(&fit, FAULT_PHANTOM_BYTES, 0.05f, 4); /* 5% phantom */
    mb_fault_inject_add_rule(&fit, FAULT_DROP_FRAME, 0.20f, 0);  /* 20% dropped */
    mb_fault_inject_add_rule(&fit, FAULT_DUPLICATE, 0.05f, 0);   /* 5% duplicated */
    
    uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A};
    
    /* Send 1000 frames */
    for (int i = 0; i < 1000; i++) {
        fit.transport_if.send(fit.transport_if.ctx, data, sizeof(data));
    }
    
    mb_fault_stats_t stats;
    mb_fault_inject_get_stats(&fit, &stats);
    
    EXPECT_EQ(stats.total_frames, 1000u);
    
    /* Should have significant faults */
    EXPECT_GT(stats.total_injected, 100u);  /* At least 10% */
    EXPECT_GT(stats.drops, 100u);            /* ~20% drop rate */
    
    /* Print stats for verification */
    printf("\nChaos test stats:\n");
    printf("  Total frames: %u\n", stats.total_frames);
    printf("  Total faults: %u\n", stats.total_injected);
    printf("  Bit flips: %u\n", stats.bit_flips);
    printf("  Truncations: %u\n", stats.truncations);
    printf("  Phantom: %u\n", stats.phantom_insertions);
    printf("  Drops: %u\n", stats.drops);
    printf("  Duplications: %u\n", stats.duplications);
}
