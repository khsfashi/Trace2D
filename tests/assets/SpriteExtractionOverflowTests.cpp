#include <trace2d/assets/SpriteExtraction.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

namespace trace2d::assets
{
TEST(SpriteExtractionOverflowTests, RejectsGridCoordinateOverflowBeforeProducingFrames)
{
    const std::vector<std::uint8_t> pixels(4U, 0U);
    const SpriteExtractionSheetView sheet{
        .id = "overflow",
        .width = 1U,
        .height = 1U,
        .rgba8 = pixels,
    };

    SpriteExtractionSpec spec{};
    spec.mode = SpriteExtractionMode::UniformGrid;
    spec.expectedFrameCount = 3U;
    spec.grid = SpriteExtractionGridSpec{
        .originX = 0U,
        .originY = 0U,
        .cellWidth = 1U,
        .cellHeight = 1U,
        .columns = 3U,
        .rows = 1U,
        .spacingX = std::numeric_limits<std::uint32_t>::max(),
        .spacingY = 0U,
        .order = SpriteExtractionOrder::RowMajor,
    };

    const SpriteExtractionResult result = ExtractSpriteFrames(sheet, spec);

    EXPECT_FALSE(result.Succeeded());
    EXPECT_TRUE(result.frames.empty());
    EXPECT_FALSE(result.processingReport.has_value());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, SpriteExtractionErrorCode::SizeOverflow);
}
} // namespace trace2d::assets
