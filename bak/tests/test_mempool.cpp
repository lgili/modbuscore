#include <algorithm>
#include <array>
#include <gtest/gtest.h>

extern "C" {
#include <modbus/internal/mempool.h>
}

namespace {

constexpr mb_size_t kBlockSize = MB_ALIGN_UP(sizeof(void *), sizeof(void *));
constexpr mb_size_t kBlockCount = 8U;

struct MempoolFixture : public ::testing::Test {
    void SetUp() override
    {
        ASSERT_EQ(MODBUS_ERROR_NONE,
                  mb_mempool_init(&pool, storage.data(), kBlockSize, kBlockCount));
    }

    mb_mempool_t pool{};
    std::array<mb_u8, kBlockSize * kBlockCount> storage{};
};

TEST(MempoolInit, RejectsInvalidArgs)
{
    mb_mempool_t pool{};
    std::array<mb_u8, kBlockSize * kBlockCount> buffer{};

    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_mempool_init(nullptr, buffer.data(), kBlockSize, kBlockCount));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_mempool_init(&pool, nullptr, kBlockSize, kBlockCount));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_mempool_init(&pool, buffer.data(), 0U, kBlockCount));
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_mempool_init(&pool, buffer.data(), kBlockSize, 0U));
}

TEST(MempoolInit, RejectsBlockTooSmall)
{
    mb_mempool_t pool{};
    std::array<mb_u8, 32U> buffer{};

    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT,
              mb_mempool_init(&pool, buffer.data(), sizeof(void *) - 1U, 4U));
}

TEST_F(MempoolFixture, CapacityAndFreeCount)
{
    EXPECT_EQ(kBlockCount, mb_mempool_capacity(&pool));
    EXPECT_EQ(kBlockCount, mb_mempool_free_count(&pool));
}

TEST_F(MempoolFixture, AcquireAllThenEmpty)
{
    std::array<void *, kBlockCount> blocks{};
    for (mb_size_t i = 0; i < kBlockCount; ++i) {
        blocks[i] = mb_mempool_acquire(&pool);
        ASSERT_NE(nullptr, blocks[i]);
    }

    EXPECT_EQ(0U, mb_mempool_free_count(&pool));
    EXPECT_EQ(nullptr, mb_mempool_acquire(&pool));

    std::sort(blocks.begin(), blocks.end());
    EXPECT_TRUE(std::adjacent_find(blocks.begin(), blocks.end()) == blocks.end());
}

TEST_F(MempoolFixture, ReleaseAndReuse)
{
    void *block = mb_mempool_acquire(&pool);
    ASSERT_NE(nullptr, block);
    EXPECT_EQ(kBlockCount - 1U, mb_mempool_free_count(&pool));

    EXPECT_EQ(MODBUS_ERROR_NONE, mb_mempool_release(&pool, block));
    EXPECT_EQ(kBlockCount, mb_mempool_free_count(&pool));

    void *again = mb_mempool_acquire(&pool);
    EXPECT_EQ(block, again);
}

TEST_F(MempoolFixture, ReleaseRejectsForeignPointer)
{
    mb_u8 foreign[kBlockSize]{};
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, mb_mempool_release(&pool, foreign));
}

TEST_F(MempoolFixture, ReleaseRejectsMisalignedPointer)
{
    mb_u8 *block = static_cast<mb_u8 *>(mb_mempool_acquire(&pool));
    ASSERT_NE(nullptr, block);

    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, mb_mempool_release(&pool, block + 1U));

    EXPECT_EQ(MODBUS_ERROR_NONE, mb_mempool_release(&pool, block));
}

TEST_F(MempoolFixture, ReleaseRejectsNull)
{
    EXPECT_EQ(MODBUS_ERROR_INVALID_ARGUMENT, mb_mempool_release(&pool, nullptr));
}

TEST_F(MempoolFixture, DoubleFreeDetected)
{
    void *block = mb_mempool_acquire(&pool);
    ASSERT_NE(nullptr, block);
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_mempool_release(&pool, block));
    EXPECT_EQ(MODBUS_ERROR_OTHER, mb_mempool_release(&pool, block));
}

TEST_F(MempoolFixture, ResetRestoresFreeList)
{
    for (mb_size_t i = 0; i < kBlockCount; ++i) {
        ASSERT_NE(nullptr, mb_mempool_acquire(&pool));
    }
    EXPECT_EQ(0U, mb_mempool_free_count(&pool));

    mb_mempool_reset(&pool);
    EXPECT_EQ(kBlockCount, mb_mempool_free_count(&pool));
}

TEST_F(MempoolFixture, ContainsMarksValidBlocks)
{
    void *block = mb_mempool_acquire(&pool);
    ASSERT_NE(nullptr, block);
    EXPECT_TRUE(mb_mempool_contains(&pool, block));
    mb_u8 foreign[kBlockSize]{};
    EXPECT_FALSE(mb_mempool_contains(&pool, foreign));
    EXPECT_EQ(MODBUS_ERROR_NONE, mb_mempool_release(&pool, block));
}

} // namespace
