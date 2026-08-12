#include <trace2d/render/SpriteOrderMask2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace trace2d::render
{
namespace
{
SpriteOrderMaskEntry2D Entry(
    const std::uint32_t sourceIndex,
    const std::int32_t layer,
    const std::int32_t order,
    const std::uint64_t stableOrder)
{
    SpriteOrderMaskEntry2D entry{};
    entry.sourceIndex = sourceIndex;
    entry.order.layer = layer;
    entry.order.order = order;
    entry.order.stableOrder = stableOrder;
    return entry;
}

TEST(SpriteOrderMask2DTests, OrdersByLayerOrderStableIdentityAndExactInputTie)
{
    std::array<SpriteOrderMaskEntry2D, 6U> entries{
        Entry(0U, 2, 0, 7U),
        Entry(1U, -1, 5, 3U),
        Entry(2U, 2, -1, 8U),
        Entry(3U, 2, -1, 2U),
        Entry(4U, 2, -1, 2U),
        Entry(5U, 2, 0, 1U),
    };

    ASSERT_TRUE(ResolveSpriteOrderMask2D(entries).Succeeded());

    constexpr std::array<std::uint32_t, 6U> Expected{1U, 3U, 4U, 2U, 5U, 0U};
    for (std::size_t index = 0U; index < entries.size(); ++index)
    {
        EXPECT_EQ(entries[index].sourceIndex, Expected[index]);
    }
}

TEST(SpriteOrderMask2DTests, SortingGroupIsOneAtomicSemanticUnitBeforeChildOrder)
{
    std::array<SpriteOrderMaskEntry2D, 5U> entries{
        Entry(0U, 0, 2, 2U),
        Entry(1U, 99, 3, 30U),
        Entry(2U, -99, 1, 10U),
        Entry(3U, 0, 0, 1U),
        Entry(4U, 0, 4, 4U),
    };

    const SpriteSortingGroup2D group{7U, 0, 3, 3U};
    entries[1].order.group = group;
    entries[2].order.group = group;

    ASSERT_TRUE(ResolveSpriteOrderMask2D(entries).Succeeded());

    // Ungrouped top-level stable 1/2, then the group anchored at stable 3, then stable 4.
    // Group children use their local layer/order/stable tuple only after the group anchor.
    constexpr std::array<std::uint32_t, 5U> Expected{3U, 0U, 2U, 1U, 4U};
    for (std::size_t index = 0U; index < entries.size(); ++index)
    {
        EXPECT_EQ(entries[index].sourceIndex, Expected[index]);
    }
}

TEST(SpriteOrderMask2DTests, EqualGroupAnchorCollisionStillKeepsEachGroupContiguous)
{
    std::array<SpriteOrderMaskEntry2D, 5U> entries{
        Entry(0U, 0, 2, 5U),
        Entry(1U, 0, 0, 0U),
        Entry(2U, 0, 1, 1U),
        Entry(3U, 0, 0, 0U),
        Entry(4U, 0, 1, 1U),
    };
    entries[1].order.group = SpriteSortingGroup2D{2U, 0, 2, 5U};
    entries[2].order.group = SpriteSortingGroup2D{2U, 0, 2, 5U};
    entries[3].order.group = SpriteSortingGroup2D{3U, 0, 2, 5U};
    entries[4].order.group = SpriteSortingGroup2D{3U, 0, 2, 5U};

    ASSERT_TRUE(ResolveSpriteOrderMask2D(entries).Succeeded());

    constexpr std::array<std::uint32_t, 5U> Expected{0U, 1U, 2U, 3U, 4U};
    for (std::size_t index = 0U; index < entries.size(); ++index)
    {
        EXPECT_EQ(entries[index].sourceIndex, Expected[index]);
    }
}

TEST(SpriteOrderMask2DTests, RejectsInconsistentSortingGroupAnchor)
{
    std::array<SpriteOrderMaskEntry2D, 2U> entries{
        Entry(0U, 0, 0, 0U),
        Entry(1U, 0, 0, 1U),
    };
    entries[0].order.group = SpriteSortingGroup2D{4U, 1, 2, 3U};
    entries[1].order.group = SpriteSortingGroup2D{4U, 1, 9, 3U};

    const SpriteOrderMaskStatus status = ResolveSpriteOrderMask2D(entries);

    EXPECT_EQ(status.error, SpriteOrderMaskError::InconsistentSortingGroup);
    EXPECT_EQ(status.sourceIndex, 1U);
    EXPECT_EQ(status.sortingGroupId, 4U);
}

TEST(SpriteOrderMask2DTests, RejectsNonCanonicalUngroupedMetadataAndReservedStableOrder)
{
    std::array<SpriteOrderMaskEntry2D, 1U> entries{Entry(0U, 0, 0, 0U)};
    entries[0].order.group.layer = 1;
    EXPECT_EQ(
        ResolveSpriteOrderMask2D(entries).error,
        SpriteOrderMaskError::InvalidSortingGroup);

    entries[0] = Entry(0U, 0, 0, InvalidSpriteStableOrder);
    EXPECT_EQ(
        ResolveSpriteOrderMask2D(entries).error,
        SpriteOrderMaskError::InvalidStableOrder);
}

TEST(SpriteOrderMask2DTests, RejectsNonSequentialScratchSourceIdentity)
{
    std::array<SpriteOrderMaskEntry2D, 2U> entries{
        Entry(1U, 0, 0, 0U),
        Entry(0U, 0, 0, 1U),
    };

    EXPECT_EQ(
        ResolveSpriteOrderMask2D(entries).error,
        SpriteOrderMaskError::InvalidSourceIndex);
}

TEST(SpriteOrderMask2DTests, ResolvesWriterThenInsideAndOutsideTesters)
{
    std::array<SpriteOrderMaskEntry2D, 4U> entries{
        Entry(0U, 0, 0, 0U),
        Entry(1U, 0, 1, 1U),
        Entry(2U, 0, 2, 2U),
        Entry(3U, 0, 3, 3U),
    };
    entries[0].mask = SpriteMask2D{SpriteMaskMode::Write, 11U};
    entries[1].mask = SpriteMask2D{SpriteMaskMode::Write, 11U};
    entries[2].mask = SpriteMask2D{SpriteMaskMode::TestInside, 11U};
    entries[3].mask = SpriteMask2D{SpriteMaskMode::TestOutside, 11U};

    EXPECT_TRUE(ResolveSpriteOrderMask2D(entries).Succeeded());
}

TEST(SpriteOrderMask2DTests, RejectsTesterWithoutActiveWriter)
{
    std::array<SpriteOrderMaskEntry2D, 1U> entries{Entry(0U, 0, 0, 0U)};
    entries[0].mask = SpriteMask2D{SpriteMaskMode::TestInside, 7U};

    const SpriteOrderMaskStatus status = ResolveSpriteOrderMask2D(entries);

    EXPECT_EQ(status.error, SpriteOrderMaskError::MaskTesterWithoutWriter);
    EXPECT_EQ(status.maskId, 7U);
}

TEST(SpriteOrderMask2DTests, RejectsWriterAfterTesterInSamePhase)
{
    std::array<SpriteOrderMaskEntry2D, 3U> entries{
        Entry(0U, 0, 0, 0U),
        Entry(1U, 0, 1, 1U),
        Entry(2U, 0, 2, 2U),
    };
    entries[0].mask = SpriteMask2D{SpriteMaskMode::Write, 1U};
    entries[1].mask = SpriteMask2D{SpriteMaskMode::TestInside, 1U};
    entries[2].mask = SpriteMask2D{SpriteMaskMode::Write, 1U};

    EXPECT_EQ(
        ResolveSpriteOrderMask2D(entries).error,
        SpriteOrderMaskError::MaskWriterAfterTester);
}

TEST(SpriteOrderMask2DTests, RejectsClosedMaskPhaseReentry)
{
    std::array<SpriteOrderMaskEntry2D, 3U> entries{
        Entry(0U, 0, 0, 0U),
        Entry(1U, 0, 1, 1U),
        Entry(2U, 0, 2, 2U),
    };
    entries[0].mask = SpriteMask2D{SpriteMaskMode::Write, 1U};
    entries[1].mask = SpriteMask2D{SpriteMaskMode::Write, 2U};
    entries[2].mask = SpriteMask2D{SpriteMaskMode::Write, 1U};

    EXPECT_EQ(
        ResolveSpriteOrderMask2D(entries).error,
        SpriteOrderMaskError::MaskPhaseReentry);
}

TEST(SpriteOrderMask2DTests, RejectsInvalidNoneAndInvalidEnumMaskState)
{
    std::array<SpriteOrderMaskEntry2D, 1U> entries{Entry(0U, 0, 0, 0U)};
    entries[0].mask = SpriteMask2D{SpriteMaskMode::None, 1U};
    EXPECT_EQ(ResolveSpriteOrderMask2D(entries).error, SpriteOrderMaskError::InvalidMask);

    entries[0].mask = SpriteMask2D{static_cast<SpriteMaskMode>(255U), 1U};
    EXPECT_EQ(ResolveSpriteOrderMask2D(entries).error, SpriteOrderMaskError::InvalidMask);
}
} // namespace
} // namespace trace2d::render
