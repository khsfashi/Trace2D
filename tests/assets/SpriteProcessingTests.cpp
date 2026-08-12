#include <trace2d/assets/SpriteProcessing.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <span>
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

bool HasFinding(const SpriteProcessingReport& report, const SpriteProcessingFindingCode code)
{
    for (const SpriteProcessingFinding& finding : report.findings)
    {
        if (finding.code == code)
        {
            return true;
        }
    }
    return false;
}
} // namespace

TEST(SpriteProcessingTests, MeasuresAlphaBoundsEdgesResidueAndColorsExactly)
{
    std::vector<std::uint8_t> pixels(3U * 3U * 4U, 0U);
    SetPixel(pixels, 3U, 0U, 0U, {5U, 0U, 0U, 0U});
    SetPixel(pixels, 3U, 1U, 1U, {10U, 20U, 30U, 128U});
    SetPixel(pixels, 3U, 2U, 2U, {10U, 20U, 30U, 255U});

    const SpriteProcessingFrameView frame{
        .id = "frame_a",
        .width = 3U,
        .height = 3U,
        .rgba8 = pixels,
    };
    const std::array frames{frame};
    const SpriteProcessingResult result = AnalyzeSpriteProcessing(frames, {});

    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.report.has_value());
    ASSERT_EQ(result.report->frames.size(), 1U);

    const SpriteProcessingFrameMetrics& metrics = result.report->frames.front();
    EXPECT_EQ(metrics.pixelCount, 9U);
    EXPECT_EQ(metrics.fullyTransparentPixels, 7U);
    EXPECT_EQ(metrics.partiallyTransparentPixels, 1U);
    EXPECT_EQ(metrics.fullyOpaquePixels, 1U);
    EXPECT_FALSE(metrics.empty);
    ASSERT_TRUE(metrics.visibleAlphaBounds.has_value());
    EXPECT_EQ(*metrics.visibleAlphaBounds, (SpritePixelRect{1U, 1U, 2U, 2U}));
    EXPECT_EQ(metrics.visibleEdgePixels.left, 0U);
    EXPECT_EQ(metrics.visibleEdgePixels.top, 0U);
    EXPECT_EQ(metrics.visibleEdgePixels.right, 1U);
    EXPECT_EQ(metrics.visibleEdgePixels.bottom, 1U);
    EXPECT_EQ(metrics.transparentRgbResiduePixels, 1U);
    EXPECT_EQ(metrics.uniqueRgbaColors, 4U);
    EXPECT_EQ(metrics.uniqueVisibleRgbColors, 1U);
    EXPECT_TRUE(HasFinding(*result.report, SpriteProcessingFindingCode::VisibleTouchesEdge));
    EXPECT_TRUE(HasFinding(*result.report, SpriteProcessingFindingCode::TransparentRgbResidue));
}

TEST(SpriteProcessingTests, GroupsOnlyByteIdenticalFramesAndMeasuresAdjacentMotion)
{
    std::vector<std::uint8_t> first(2U * 1U * 4U, 0U);
    SetPixel(first, 2U, 0U, 0U, {255U, 0U, 0U, 255U});
    const std::vector<std::uint8_t> duplicate = first;

    std::vector<std::uint8_t> shifted(2U * 1U * 4U, 0U);
    SetPixel(shifted, 2U, 1U, 0U, {255U, 0U, 0U, 255U});

    const std::array frames{
        SpriteProcessingFrameView{.id = "a", .width = 2U, .height = 1U, .rgba8 = first},
        SpriteProcessingFrameView{.id = "b", .width = 2U, .height = 1U, .rgba8 = duplicate},
        SpriteProcessingFrameView{.id = "c", .width = 2U, .height = 1U, .rgba8 = shifted},
    };
    SpriteProcessingOptions options{};
    options.gridColumns = 2U;
    options.maxBoundsOriginDisplacementPixels = 0U;

    const SpriteProcessingResult result = AnalyzeSpriteProcessing(frames, {}, options);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.report->duplicateGroups.size(), 1U);
    ASSERT_EQ(result.report->duplicateGroups.front().frameIds.size(), 2U);
    EXPECT_EQ(result.report->duplicateGroups.front().frameIds[0U], "a");
    EXPECT_EQ(result.report->duplicateGroups.front().frameIds[1U], "b");

    ASSERT_EQ(result.report->adjacentPairs.size(), 2U);
    EXPECT_TRUE(result.report->adjacentPairs[0U].comparable);
    EXPECT_EQ(result.report->adjacentPairs[0U].changedPixels, 0U);
    EXPECT_TRUE(result.report->adjacentPairs[1U].comparable);
    EXPECT_EQ(result.report->adjacentPairs[1U].changedPixels, 2U);
    EXPECT_EQ(result.report->adjacentPairs[1U].boundsOriginDeltaX, 1);
    EXPECT_EQ(result.report->adjacentPairs[1U].boundsOriginDeltaY, 0);

    EXPECT_TRUE(result.report->grid.requested);
    EXPECT_EQ(result.report->grid.columns, 2U);
    EXPECT_EQ(result.report->grid.rows, 2U);
    EXPECT_FALSE(result.report->grid.complete);
    EXPECT_TRUE(result.report->grid.uniformCellSize);
    EXPECT_EQ(result.report->grid.cellSize, (SpritePixelSize{2U, 1U}));
    EXPECT_TRUE(HasFinding(*result.report, SpriteProcessingFindingCode::DuplicateFrame));
    EXPECT_TRUE(HasFinding(*result.report, SpriteProcessingFindingCode::AdjacentNoChange));
    EXPECT_TRUE(HasFinding(*result.report, SpriteProcessingFindingCode::BoundsDisplacement));
}

TEST(SpriteProcessingTests, ReportsDimensionAndExplicitPivotInconsistencyInStableOrder)
{
    const std::vector<std::uint8_t> twoByTwo(2U * 2U * 4U, 0U);
    const std::vector<std::uint8_t> oneByTwo(1U * 2U * 4U, 0U);
    const std::array frames{
        SpriteProcessingFrameView{
            .id = "wide",
            .width = 2U,
            .height = 2U,
            .rgba8 = twoByTwo,
            .pivot = SpriteRationalPivot{1, 1, 2},
        },
        SpriteProcessingFrameView{
            .id = "narrow",
            .width = 1U,
            .height = 2U,
            .rgba8 = oneByTwo,
            .pivot = SpriteRationalPivot{0, 1, 2},
        },
    };
    SpriteProcessingOptions options{};
    options.requireUniformPivot = true;

    const SpriteProcessingResult result = AnalyzeSpriteProcessing(frames, {}, options);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_FALSE(result.report->uniformFrameDimensions);
    ASSERT_EQ(result.report->dimensionHistogram.size(), 2U);
    EXPECT_EQ(result.report->dimensionHistogram[0U].size, (SpritePixelSize{1U, 2U}));
    EXPECT_EQ(result.report->dimensionHistogram[1U].size, (SpritePixelSize{2U, 2U}));
    ASSERT_EQ(result.report->pivotHistogram.size(), 2U);
    EXPECT_TRUE(HasFinding(*result.report, SpriteProcessingFindingCode::InconsistentDimensions));
    EXPECT_TRUE(HasFinding(*result.report, SpriteProcessingFindingCode::PivotInconsistent));
}

TEST(SpriteProcessingTests, MeasuresAtlasUtilizationOverlapAndOutOfBoundsWithoutFloatingAuthority)
{
    const std::array rects{
        SpriteProcessingAtlasRectView{
            .id = "a",
            .rect = SpritePixelRect{0U, 0U, 4U, 4U},
        },
        SpriteProcessingAtlasRectView{
            .id = "b",
            .rect = SpritePixelRect{2U, 2U, 4U, 4U},
            .packedRotation = SpritePackedRotation::Cw90,
        },
        SpriteProcessingAtlasRectView{
            .id = "c",
            .rect = SpritePixelRect{7U, 7U, 2U, 2U},
        },
    };
    const std::array atlases{
        SpriteProcessingAtlasPageView{
            .id = "page",
            .size = SpritePixelSize{8U, 8U},
            .rects = rects,
        },
    };
    SpriteProcessingOptions options{};
    options.minimumAtlasUtilization = SpriteProcessingRatio{3U, 4U};

    const SpriteProcessingResult result = AnalyzeSpriteProcessing({}, atlases, options);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.report->atlases.size(), 1U);
    const SpriteProcessingAtlasMetrics& metrics = result.report->atlases.front();
    EXPECT_EQ(metrics.pageArea, 64U);
    EXPECT_EQ(metrics.packedRectCount, 3U);
    EXPECT_EQ(metrics.occupiedPackedArea, 36U);
    EXPECT_EQ(metrics.utilization, (SpriteProcessingRatio{36U, 64U}));
    EXPECT_EQ(metrics.outOfBoundsRectCount, 1U);
    EXPECT_EQ(metrics.overlappingRectPairCount, 1U);
    EXPECT_TRUE(HasFinding(*result.report, SpriteProcessingFindingCode::AtlasOutOfBounds));
    EXPECT_TRUE(HasFinding(*result.report, SpriteProcessingFindingCode::AtlasOverlap));
    EXPECT_TRUE(HasFinding(*result.report, SpriteProcessingFindingCode::LowAtlasUtilization));
}

TEST(SpriteProcessingTests, RejectsMalformedInputWithoutPartialReport)
{
    const std::array<std::uint8_t, 3U> invalidBytes{1U, 2U, 3U};
    const std::array frames{
        SpriteProcessingFrameView{
            .id = "bad",
            .width = 1U,
            .height = 1U,
            .rgba8 = invalidBytes,
        },
    };

    const SpriteProcessingResult result = AnalyzeSpriteProcessing(frames, {});

    EXPECT_FALSE(result.Succeeded());
    EXPECT_FALSE(result.report.has_value());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, SpriteProcessingErrorCode::InvalidByteCount);
    EXPECT_EQ(result.diagnostics.front().id, "bad");
}

TEST(SpriteProcessingTests, SerializesIdenticalReportToByteIdenticalVersionedJson)
{
    std::vector<std::uint8_t> pixels(1U * 1U * 4U, 0U);
    SetPixel(pixels, 1U, 0U, 0U, {1U, 2U, 3U, 255U});
    const std::array frames{
        SpriteProcessingFrameView{
            .id = "quote_\"_frame",
            .width = 1U,
            .height = 1U,
            .rgba8 = pixels,
            .pivot = SpriteRationalPivot{1, -1, 2},
        },
    };

    const SpriteProcessingResult first = AnalyzeSpriteProcessing(frames, {});
    const SpriteProcessingResult second = AnalyzeSpriteProcessing(frames, {});
    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(second.Succeeded());

    const std::string firstJson = SerializeSpriteProcessingReportJson(*first.report);
    const std::string secondJson = SerializeSpriteProcessingReportJson(*second.report);
    EXPECT_EQ(firstJson, secondJson);
    EXPECT_NE(firstJson.find("\"schema_version\":1"), std::string::npos);
    EXPECT_NE(firstJson.find("quote_\\\"_frame"), std::string::npos);
}
} // namespace trace2d::assets
