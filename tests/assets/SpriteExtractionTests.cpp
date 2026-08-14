#include <trace2d/assets/SpriteExtraction.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace trace2d::assets
{
namespace
{
void SetPixel(
    std::vector<std::uint8_t>& pixels,
    const std::uint32_t width,
    const std::uint32_t x,
    const std::uint32_t y,
    const std::array<std::uint8_t, 4U> rgba)
{
    const std::size_t index = (static_cast<std::size_t>(y) * width + x) * 4U;
    pixels[index + 0U] = rgba[0U];
    pixels[index + 1U] = rgba[1U];
    pixels[index + 2U] = rgba[2U];
    pixels[index + 3U] = rgba[3U];
}

SpriteExtractionSheetView MakeSheet(
    const std::string_view id,
    const std::uint32_t width,
    const std::uint32_t height,
    const std::vector<std::uint8_t>& pixels)
{
    return SpriteExtractionSheetView{
        .id = id,
        .width = width,
        .height = height,
        .rgba8 = pixels,
    };
}
} // namespace

TEST(SpriteExtractionTests, AppliesOnlyExplicitExactCleanupRules)
{
    std::vector<std::uint8_t> pixels(4U * 1U * 4U, 0U);
    SetPixel(pixels, 4U, 0U, 0U, {10U, 20U, 30U, 255U});
    SetPixel(pixels, 4U, 1U, 0U, {50U, 60U, 70U, 10U});
    SetPixel(pixels, 4U, 2U, 0U, {7U, 8U, 9U, 0U});
    SetPixel(pixels, 4U, 3U, 0U, {1U, 2U, 3U, 128U});

    const std::array rects{
        SpriteExtractionRectView{
            .id = "all",
            .rect = SpritePixelRect{0U, 0U, 4U, 1U},
        },
    };
    SpriteExtractionSpec spec{};
    spec.mode = SpriteExtractionMode::ExplicitRects;
    spec.explicitRects = rects;
    spec.expectedFrameCount = 1U;
    spec.cleanup.exactBackgroundRgb = SpriteExtractionRgbKey{10U, 20U, 30U};
    spec.cleanup.alphaCutoff = std::uint8_t{10};
    spec.cleanup.zeroTransparentRgb = true;

    const SpriteExtractionResult result =
        ExtractSpriteFrames(MakeSheet("sheet", 4U, 1U, pixels), spec);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.frames.size(), 1U);
    const std::vector<std::uint8_t>& output = result.frames.front().rgba8;
    const std::vector<std::uint8_t> expected{
        0U, 0U, 0U, 0U,
        0U, 0U, 0U, 0U,
        0U, 0U, 0U, 0U,
        1U, 2U, 3U, 128U,
    };
    EXPECT_EQ(output, expected);
}

TEST(SpriteExtractionTests, ExplicitRectsPreserveCallerOrderAndCopyExactRegions)
{
    std::vector<std::uint8_t> pixels(4U * 2U * 4U, 0U);
    SetPixel(pixels, 4U, 0U, 0U, {1U, 0U, 0U, 255U});
    SetPixel(pixels, 4U, 1U, 0U, {2U, 0U, 0U, 255U});
    SetPixel(pixels, 4U, 2U, 1U, {3U, 0U, 0U, 255U});
    SetPixel(pixels, 4U, 3U, 1U, {4U, 0U, 0U, 255U});

    const std::array rects{
        SpriteExtractionRectView{
            .id = "right",
            .rect = SpritePixelRect{2U, 1U, 2U, 1U},
        },
        SpriteExtractionRectView{
            .id = "left",
            .rect = SpritePixelRect{0U, 0U, 2U, 1U},
        },
    };
    SpriteExtractionSpec spec{};
    spec.mode = SpriteExtractionMode::ExplicitRects;
    spec.explicitRects = rects;
    spec.expectedFrameCount = 2U;

    const SpriteExtractionResult result =
        ExtractSpriteFrames(MakeSheet("sheet", 4U, 2U, pixels), spec);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.frames.size(), 2U);
    EXPECT_EQ(result.frames[0U].id, "right");
    EXPECT_EQ(result.frames[0U].sourceRect, (SpritePixelRect{2U, 1U, 2U, 1U}));
    EXPECT_EQ(result.frames[0U].rgba8[0U], 3U);
    EXPECT_EQ(result.frames[0U].rgba8[4U], 4U);
    EXPECT_EQ(result.frames[1U].id, "left");
    EXPECT_EQ(result.frames[1U].sourceRect, (SpritePixelRect{0U, 0U, 2U, 1U}));
    EXPECT_EQ(result.frames[1U].rgba8[0U], 1U);
    EXPECT_EQ(result.frames[1U].rgba8[4U], 2U);
}

TEST(SpriteExtractionTests, GridUsesExplicitGeometryAndStableRequestedOrder)
{
    std::vector<std::uint8_t> pixels(5U * 3U * 4U, 0U);
    SetPixel(pixels, 5U, 0U, 0U, {1U, 0U, 0U, 255U});
    SetPixel(pixels, 5U, 2U, 0U, {2U, 0U, 0U, 255U});
    SetPixel(pixels, 5U, 0U, 2U, {3U, 0U, 0U, 255U});
    SetPixel(pixels, 5U, 2U, 2U, {4U, 0U, 0U, 255U});

    SpriteExtractionSpec spec{};
    spec.mode = SpriteExtractionMode::UniformGrid;
    spec.expectedFrameCount = 4U;
    spec.grid = SpriteExtractionGridSpec{
        .originX = 0U,
        .originY = 0U,
        .cellWidth = 1U,
        .cellHeight = 1U,
        .columns = 2U,
        .rows = 2U,
        .spacingX = 1U,
        .spacingY = 1U,
        .order = SpriteExtractionOrder::ColumnMajor,
    };

    const SpriteExtractionResult result =
        ExtractSpriteFrames(MakeSheet("grid", 5U, 3U, pixels), spec);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.frames.size(), 4U);
    EXPECT_EQ(result.frames[0U].sourceRect, (SpritePixelRect{0U, 0U, 1U, 1U}));
    EXPECT_EQ(result.frames[1U].sourceRect, (SpritePixelRect{0U, 2U, 1U, 1U}));
    EXPECT_EQ(result.frames[2U].sourceRect, (SpritePixelRect{2U, 0U, 1U, 1U}));
    EXPECT_EQ(result.frames[3U].sourceRect, (SpritePixelRect{2U, 2U, 1U, 1U}));
    EXPECT_EQ(result.frames[0U].id, "grid#frame-0");
    EXPECT_EQ(result.frames[3U].id, "grid#frame-3");
}

TEST(SpriteExtractionTests, AlphaComponentsUseFourConnectivityAndRowMajorSeedOrder)
{
    std::vector<std::uint8_t> pixels(4U * 3U * 4U, 0U);
    SetPixel(pixels, 4U, 0U, 0U, {255U, 255U, 255U, 255U});
    SetPixel(pixels, 4U, 1U, 1U, {255U, 255U, 255U, 255U});
    SetPixel(pixels, 4U, 3U, 1U, {255U, 255U, 255U, 255U});
    SetPixel(pixels, 4U, 3U, 2U, {255U, 255U, 255U, 255U});

    SpriteExtractionSpec spec{};
    spec.mode = SpriteExtractionMode::AlphaComponents;
    spec.expectedFrameCount = 3U;

    const SpriteExtractionResult result =
        ExtractSpriteFrames(MakeSheet("components", 4U, 3U, pixels), spec);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.frames.size(), 3U);
    EXPECT_EQ(result.frames[0U].sourceRect, (SpritePixelRect{0U, 0U, 1U, 1U}));
    EXPECT_EQ(result.frames[1U].sourceRect, (SpritePixelRect{1U, 1U, 1U, 1U}));
    EXPECT_EQ(result.frames[2U].sourceRect, (SpritePixelRect{3U, 1U, 1U, 2U}));
}

TEST(SpriteExtractionTests, ExpectedFrameMismatchFailsWithoutPartialOutput)
{
    std::vector<std::uint8_t> pixels(2U * 1U * 4U, 0U);
    SetPixel(pixels, 2U, 0U, 0U, {255U, 255U, 255U, 255U});
    SetPixel(pixels, 2U, 1U, 0U, {255U, 255U, 255U, 255U});

    SpriteExtractionSpec spec{};
    spec.mode = SpriteExtractionMode::AlphaComponents;
    spec.expectedFrameCount = 2U;

    const SpriteExtractionResult result =
        ExtractSpriteFrames(MakeSheet("joined", 2U, 1U, pixels), spec);

    EXPECT_FALSE(result.Succeeded());
    EXPECT_TRUE(result.frames.empty());
    EXPECT_FALSE(result.processingReport.has_value());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(
        result.diagnostics.front().code,
        SpriteExtractionErrorCode::ExpectedFrameCountMismatch);
}

TEST(SpriteExtractionTests, TrimPreservesAbsoluteSourceRectAndRejectsEmptyTrim)
{
    std::vector<std::uint8_t> pixels(5U * 4U * 4U, 0U);
    SetPixel(pixels, 5U, 2U, 2U, {1U, 2U, 3U, 255U});

    const std::array rects{
        SpriteExtractionRectView{
            .id = "trimmed",
            .rect = SpritePixelRect{1U, 1U, 3U, 3U},
        },
    };
    SpriteExtractionSpec spec{};
    spec.mode = SpriteExtractionMode::ExplicitRects;
    spec.explicitRects = rects;
    spec.expectedFrameCount = 1U;
    spec.trimToVisibleAlphaBounds = true;

    const SpriteExtractionResult result =
        ExtractSpriteFrames(MakeSheet("trim", 5U, 4U, pixels), spec);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.frames.size(), 1U);
    EXPECT_EQ(result.frames.front().sourceRect, (SpritePixelRect{2U, 2U, 1U, 1U}));
    EXPECT_EQ(result.frames.front().width, 1U);
    EXPECT_EQ(result.frames.front().height, 1U);

    const std::vector<std::uint8_t> emptyPixels(2U * 2U * 4U, 0U);
    const std::array emptyRects{
        SpriteExtractionRectView{
            .id = "empty",
            .rect = SpritePixelRect{0U, 0U, 2U, 2U},
        },
    };
    SpriteExtractionSpec emptySpec{};
    emptySpec.mode = SpriteExtractionMode::ExplicitRects;
    emptySpec.explicitRects = emptyRects;
    emptySpec.expectedFrameCount = 1U;
    emptySpec.trimToVisibleAlphaBounds = true;

    const SpriteExtractionResult emptyResult =
        ExtractSpriteFrames(MakeSheet("empty_sheet", 2U, 2U, emptyPixels), emptySpec);

    EXPECT_FALSE(emptyResult.Succeeded());
    EXPECT_TRUE(emptyResult.frames.empty());
    ASSERT_EQ(emptyResult.diagnostics.size(), 1U);
    EXPECT_EQ(
        emptyResult.diagnostics.front().code,
        SpriteExtractionErrorCode::EmptyFrameAfterTrim);
}

TEST(SpriteExtractionTests, SuccessfulExtractionReusesSpp0QaForDuplicateEvidence)
{
    std::vector<std::uint8_t> pixels(1U * 1U * 4U, 0U);
    SetPixel(pixels, 1U, 0U, 0U, {9U, 8U, 7U, 255U});

    const std::array rects{
        SpriteExtractionRectView{
            .id = "first",
            .rect = SpritePixelRect{0U, 0U, 1U, 1U},
        },
        SpriteExtractionRectView{
            .id = "second",
            .rect = SpritePixelRect{0U, 0U, 1U, 1U},
        },
    };
    SpriteExtractionSpec spec{};
    spec.mode = SpriteExtractionMode::ExplicitRects;
    spec.explicitRects = rects;
    spec.expectedFrameCount = 2U;

    const SpriteExtractionResult result =
        ExtractSpriteFrames(MakeSheet("duplicate", 1U, 1U, pixels), spec);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.processingReport.has_value());
    ASSERT_EQ(result.processingReport->duplicateGroups.size(), 1U);
    ASSERT_EQ(result.processingReport->duplicateGroups.front().frameIds.size(), 2U);
    EXPECT_EQ(result.processingReport->duplicateGroups.front().frameIds[0U], "first");
    EXPECT_EQ(result.processingReport->duplicateGroups.front().frameIds[1U], "second");
}

TEST(SpriteExtractionTests, SerializesRepeatedIdenticalRequestsByteIdentically)
{
    std::vector<std::uint8_t> pixels(1U * 1U * 4U, 0U);
    SetPixel(pixels, 1U, 0U, 0U, {1U, 2U, 3U, 255U});

    const std::array rects{
        SpriteExtractionRectView{
            .id = "quote_\"_frame",
            .rect = SpritePixelRect{0U, 0U, 1U, 1U},
        },
    };
    SpriteExtractionSpec spec{};
    spec.mode = SpriteExtractionMode::ExplicitRects;
    spec.explicitRects = rects;
    spec.expectedFrameCount = 1U;

    const SpriteExtractionSheetView sheet = MakeSheet("sheet", 1U, 1U, pixels);
    const SpriteExtractionResult first = ExtractSpriteFrames(sheet, spec);
    const SpriteExtractionResult second = ExtractSpriteFrames(sheet, spec);
    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(second.Succeeded());

    const std::string firstJson = SerializeSpriteExtractionResultJson(first);
    const std::string secondJson = SerializeSpriteExtractionResultJson(second);
    EXPECT_EQ(firstJson, secondJson);
    EXPECT_NE(firstJson.find("\"schema_version\":1"), std::string::npos);
    EXPECT_NE(firstJson.find("quote_\\\"_frame"), std::string::npos);
    EXPECT_NE(firstJson.find("\"processing\":{\"schema_version\":1"), std::string::npos);
}
} // namespace trace2d::assets
