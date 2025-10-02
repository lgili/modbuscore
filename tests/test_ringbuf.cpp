#include <algorithm>
#include <gtest/gtest.h>

extern "C" {
#include <modbus/ringbuf.h>
}

namespace {

constexpr size_t kCapacity = 16U;

struct RingbufFixture : public ::testing::Test {
    RingbufFixture()
    {
        std::fill(std::begin(storage), std::end(storage), 0xAA);
    }

    void SetUp() override
    {
        ASSERT_EQ(MODBUS_ERROR_NONE,
                  mb_ringbuf_init(&rb, storage, kCapacity));
    }

    mb_ringbuf_t rb{};
    uint8_t storage[kCapacity]{};
};

TEST(RingbufInit, RejectsInvalidArgs)
{
    mb_ringbuf_t rb{};
    uint8_t storage[kCapacity]{};

    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, mb_ringbuf_init(nullptr, storage, kCapacity));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, mb_ringbuf_init(&rb, nullptr, kCapacity));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, mb_ringbuf_init(&rb, storage, 0U));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, mb_ringbuf_init(&rb, storage, 3U));
}

TEST_F(RingbufFixture, ReportsCapacityAndSize)
{
    EXPECT_EQ(kCapacity, mb_ringbuf_capacity(&rb));
    EXPECT_TRUE(mb_ringbuf_is_empty(&rb));
    EXPECT_FALSE(mb_ringbuf_is_full(&rb));
    EXPECT_EQ(0U, mb_ringbuf_size(&rb));
    EXPECT_EQ(kCapacity, mb_ringbuf_free(&rb));
}

TEST_F(RingbufFixture, PushPopSingleByte)
{
    constexpr uint8_t value = 0x5AU;
    EXPECT_TRUE(mb_ringbuf_push(&rb, value));
    EXPECT_FALSE(mb_ringbuf_is_empty(&rb));
    EXPECT_EQ(1U, mb_ringbuf_size(&rb));

    uint8_t out = 0U;
    EXPECT_TRUE(mb_ringbuf_pop(&rb, &out));
    EXPECT_EQ(value, out);
    EXPECT_TRUE(mb_ringbuf_is_empty(&rb));
}

TEST_F(RingbufFixture, PushFailsWhenFull)
{
    for (size_t i = 0; i < kCapacity; ++i) {
        ASSERT_TRUE(mb_ringbuf_push(&rb, static_cast<uint8_t>(i)));
    }

    EXPECT_TRUE(mb_ringbuf_is_full(&rb));
    EXPECT_FALSE(mb_ringbuf_push(&rb, 0xFFU));
}

TEST_F(RingbufFixture, PopFailsWhenEmpty)
{
    uint8_t out = 0U;
    EXPECT_FALSE(mb_ringbuf_pop(&rb, &out));
}

TEST_F(RingbufFixture, BulkWriteRead)
{
    uint8_t input[kCapacity]{};
    for (size_t i = 0; i < kCapacity; ++i) {
        input[i] = static_cast<uint8_t>(i + 1U);
    }

    EXPECT_EQ(kCapacity, mb_ringbuf_write(&rb, input, kCapacity));
    EXPECT_TRUE(mb_ringbuf_is_full(&rb));

    uint8_t output[kCapacity]{};
    EXPECT_EQ(kCapacity, mb_ringbuf_read(&rb, output, kCapacity));
    EXPECT_TRUE(mb_ringbuf_is_empty(&rb));

    for (size_t i = 0; i < kCapacity; ++i) {
        EXPECT_EQ(input[i], output[i]) << "Mismatch at index " << i;
    }
}

TEST_F(RingbufFixture, PartialWriteWhenNoSpace)
{
    uint8_t payload[kCapacity];
    for (size_t i = 0; i < kCapacity; ++i) {
        payload[i] = static_cast<uint8_t>(i + 1U);
    }

    ASSERT_EQ(kCapacity - 2U, mb_ringbuf_write(&rb, payload, kCapacity - 2U));
    EXPECT_EQ(kCapacity - 2U, mb_ringbuf_size(&rb));

    size_t written = mb_ringbuf_write(&rb, payload + (kCapacity - 2U), 4U);
    EXPECT_EQ(2U, written);
    EXPECT_TRUE(mb_ringbuf_is_full(&rb));

    uint8_t output[kCapacity];
    EXPECT_EQ(kCapacity, mb_ringbuf_read(&rb, output, kCapacity));

    for (size_t i = 0; i < kCapacity; ++i) {
        EXPECT_EQ(payload[i], output[i]) << "Mismatch at index " << i;
    }
}

TEST_F(RingbufFixture, WrapAroundPreservesOrder)
{
    uint8_t first_half[] = {0x10U, 0x11U, 0x12U, 0x13U};
    ASSERT_EQ(sizeof first_half, mb_ringbuf_write(&rb, first_half, sizeof first_half));

    uint8_t drain[sizeof first_half];
    ASSERT_EQ(sizeof first_half, mb_ringbuf_read(&rb, drain, sizeof drain));

    uint8_t second_half[kCapacity];
    for (size_t i = 0; i < kCapacity; ++i) {
        second_half[i] = static_cast<uint8_t>(0x20U + i);
    }

    ASSERT_EQ(kCapacity, mb_ringbuf_write(&rb, second_half, kCapacity));

    uint8_t out[kCapacity];
    EXPECT_EQ(kCapacity, mb_ringbuf_read(&rb, out, kCapacity));
    for (size_t i = 0; i < kCapacity; ++i) {
        EXPECT_EQ(second_half[i], out[i]) << "Mismatch at index " << i;
    }
}

TEST_F(RingbufFixture, ResetClearsState)
{
    ASSERT_TRUE(mb_ringbuf_push(&rb, 0xAAU));
    ASSERT_FALSE(mb_ringbuf_is_empty(&rb));

    mb_ringbuf_reset(&rb);
    EXPECT_TRUE(mb_ringbuf_is_empty(&rb));
    uint8_t tmp = 0U;
    EXPECT_FALSE(mb_ringbuf_pop(&rb, &tmp));
}

TEST_F(RingbufFixture, GracefullyHandleInvalidArgs)
{
    uint8_t tmp[4]{};

    EXPECT_EQ(0U, mb_ringbuf_write(nullptr, tmp, sizeof tmp));
    EXPECT_EQ(0U, mb_ringbuf_write(&rb, nullptr, sizeof tmp));
    EXPECT_EQ(0U, mb_ringbuf_write(&rb, tmp, 0U));

    EXPECT_EQ(0U, mb_ringbuf_read(nullptr, tmp, sizeof tmp));
    EXPECT_EQ(0U, mb_ringbuf_read(&rb, nullptr, sizeof tmp));
    EXPECT_EQ(0U, mb_ringbuf_read(&rb, tmp, 0U));
}

} // namespace
