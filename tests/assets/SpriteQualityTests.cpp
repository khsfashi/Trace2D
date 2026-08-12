#include <trace2d/assets/SpriteQuality.hpp>

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

SpriteQualityFrameView MakeFrame(
    const std::string_view id,
    const std::uint32_t width,
    const std::uint32_t height,
    const std::vector<std::uint8_t>& pixels,
    const std::optional<SpriteRationalPivot> pivot = std::nullopt)
{
    return SpriteQualityFrameView{
        .id = id,
        .width = width,
        .height = height,
        .rgba8 = pixels,
        .pivot = pivot,
    };
}
} // namespace

TEST(SpriteQualityTests, PixelGridReportsExactViolationsAndCanonicalizesModeColor)
{
    std::vector<std::uint8_t> pixels(4U * 2U * 4U, 0U);
    for (std::uint32_t y = 0U; y < 2U; ++y)
    {
        for (std::uint32_t x = 0U; x < 4U; ++x)
        {
            SetPixel(pixels, 4U, x, y, {10U, 0U, 0U, 255U});
        }
    }
    SetPixel(pixels, 4U, 1U, 1U, {20U, 0U, 0U, 255U});

    const std::array frames{MakeFrame("frame", 4U, 2U, pixels)};
    SpriteQualityOptions options{};
    options.pixelGrid = SpriteQualityPixelGrid{2U, 2U};
    options.repair.canonicalizePixelBlocks = true;

    const SpriteQualityResult result = AnalyzeAndRepairSpriteQuality(frames, options);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.report.has_value());
    ASSERT_EQ(result.report->frames.size(), 1U);
    const SpriteQualityPixelGridMetrics& grid = result.report->frames.front().pixelGrid;
    EXPECT_TRUE(grid.requested);
    EXPECT_EQ(grid.checkedBlocks, 2U);
    EXPECT_EQ(grid.uniformBlocks, 1U);
    EXPECT_EQ(grid.violatingBlocks, 1U);
    EXPECT_EQ(grid.violatingBlockPixels, 4U);

    ASSERT_EQ(result.repairs.size(), 1U);
    EXPECT_EQ(result.repairs.front().kind, SpriteQualityRepairKind::PixelBlockCanonicalization);
    EXPECT_EQ(result.repairs.front().changedBlocks, 1U);
    EXPECT_EQ(result.repairs.front().changedPixels, 1U);
    ASSERT_EQ(result.repairedFrames.size(), 1U);
    EXPECT_EQ(result.repairedFrames.front().rgba8[12U], 10U);
    EXPECT_TRUE(result.postRepairProcessingReport.has_value());
}

TEST(SpriteQualityTests, PixelGridCanonicalizationUsesLowestNumericRgbaOnFrequencyTie)
{
    std::vector<std::uint8_t> pixels(2U * 1U * 4U, 0U);
    SetPixel(pixels, 2U, 0U, 0U, {10U, 0U, 0U, 255U});
    SetPixel(pixels, 2U, 1U, 0U, {5U, 0U, 0U, 255U});

    const std::array frames{MakeFrame("tie", 2U, 1U, pixels)};
    SpriteQualityOptions options{};
    options.pixelGrid = SpriteQualityPixelGrid{2U, 1U};
    options.repair.canonicalizePixelBlocks = true;

    const SpriteQualityResult result = AnalyzeAndRepairSpriteQuality(frames, options);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.repairedFrames.size(), 1U);
    const std::vector<std::uint8_t>& repaired = result.repairedFrames.front().rgba8;
    EXPECT_EQ(repaired[0U], 5U);
    EXPECT_EQ(repaired[4U], 5U);
    ASSERT_EQ(result.repairs.size(), 1U);
    EXPECT_EQ(result.repairs.front().changedPixels, 1U);
}

TEST(SpriteQualityTests, PaletteEvidenceAndRepairUseStableIndexTieBreakAndPreserveAlpha)
{
    std::vector<std::uint8_t> pixels(1U * 1U * 4U, 0U);
    SetPixel(pixels, 1U, 0U, 0U, {1U, 0U, 0U, 77U});

    const std::array frames{MakeFrame("palette", 1U, 1U, pixels)};
    const std::array palette{
        SpriteQualityRgb{0U, 0U, 0U},
        SpriteQualityRgb{2U, 0U, 0U},
    };
    SpriteQualityOptions options{};
    options.palette = palette;
    options.measureNearestPaletteDistance = true;
    options.repair.remapToPalette = true;
    options.repair.maximumPaletteDistanceSquared = 1U;

    const SpriteQualityResult result = AnalyzeAndRepairSpriteQuality(frames, options);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.report.has_value());
    const SpriteQualityPaletteMetrics& metrics = result.report->frames.front().palette;
    EXPECT_EQ(metrics.visiblePixels, 1U);
    EXPECT_EQ(metrics.exactInPalettePixels, 0U);
    EXPECT_EQ(metrics.exactOffPalettePixels, 1U);
    EXPECT_EQ(metrics.distinctVisibleRgb, 1U);
    EXPECT_TRUE(metrics.nearestDistanceMeasured);
    EXPECT_EQ(metrics.maximumNearestDistanceSquared, 1U);

    ASSERT_EQ(result.repairedFrames.size(), 1U);
    const std::vector<std::uint8_t>& repaired = result.repairedFrames.front().rgba8;
    EXPECT_EQ(repaired[0U], 0U);
    EXPECT_EQ(repaired[1U], 0U);
    EXPECT_EQ(repaired[2U], 0U);
    EXPECT_EQ(repaired[3U], 77U);
}

TEST(SpriteQualityTests, PaletteDistanceFailureIsTransactional)
{
    std::vector<std::uint8_t> pixels(1U * 1U * 4U, 0U);
    SetPixel(pixels, 1U, 0U, 0U, {10U, 0U, 0U, 255U});

    const std::array frames{MakeFrame("too-far", 1U, 1U, pixels)};
    const std::array palette{SpriteQualityRgb{0U, 0U, 0U}};
    SpriteQualityOptions options{};
    options.palette = palette;
    options.repair.remapToPalette = true;
    options.repair.maximumPaletteDistanceSquared = 4U;

    const SpriteQualityResult result = AnalyzeAndRepairSpriteQuality(frames, options);

    EXPECT_FALSE(result.Succeeded());
    EXPECT_TRUE(result.report.has_value());
    EXPECT_TRUE(result.repairedFrames.empty());
    EXPECT_TRUE(result.repairs.empty());
    EXPECT_FALSE(result.postRepairProcessingReport.has_value());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics.front().code, SpriteQualityErrorCode::PaletteDistanceExceeded);
}

TEST(SpriteQualityTests, PivotNormalizationIsMetadataOnlyAndReusesSpp0)
{
    std::vector<std::uint8_t> pixels(1U * 1U * 4U, 0U);
    SetPixel(pixels, 1U, 0U, 0U, {1U, 2U, 3U, 255U});

    const SpriteRationalPivot authored{1, 1, 2};
    const SpriteRationalPivot target{0, 0, 1};
    const std::array frames{MakeFrame("pivot", 1U, 1U, pixels, authored)};
    SpriteQualityOptions options{};
    options.targetPivot = target;
    options.repair.normalizePivotTo = target;

    const SpriteQualityResult result = AnalyzeAndRepairSpriteQuality(frames, options);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.report.has_value());
    EXPECT_FALSE(result.report->frames.front().matchesTargetPivot);
    ASSERT_EQ(result.repairedFrames.size(), 1U);
    EXPECT_EQ(result.repairedFrames.front().rgba8, pixels);
    ASSERT_TRUE(result.repairedFrames.front().pivot.has_value());
    EXPECT_EQ(*result.repairedFrames.front().pivot, target);
    ASSERT_TRUE(result.postRepairProcessingReport.has_value());
    ASSERT_EQ(result.postRepairProcessingReport->frames.size(), 1U);
    ASSERT_TRUE(result.postRepairProcessingReport->frames.front().pivot.has_value());
    EXPECT_EQ(*result.postRepairProcessingReport->frames.front().pivot, target);
}

TEST(SpriteQualityTests, AdjacentSilhouetteColorAndCentroidEvidenceAreExact)
{
    std::vector<std::uint8_t> first(3U * 1U * 4U, 0U);
    std::vector<std::uint8_t> recolored(3U * 1U * 4U, 0U);
    std::vector<std::uint8_t> moved(3U * 1U * 4U, 0U);
    SetPixel(first, 3U, 0U, 0U, {255U, 0U, 0U, 255U});
    SetPixel(recolored, 3U, 0U, 0U, {0U, 0U, 255U, 255U});
    SetPixel(moved, 3U, 1U, 0U, {0U, 0U, 255U, 255U});

    const std::array frames{
        MakeFrame("first", 3U, 1U, first),
        MakeFrame("recolored", 3U, 1U, recolored),
        MakeFrame("moved", 3U, 1U, moved),
    };
    SpriteQualityOptions options{};
    options.maximumCentroidDeltaPixels = 0U;

    const SpriteQualityResult result = AnalyzeAndRepairSpriteQuality(frames, options);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.report.has_value());
    ASSERT_EQ(result.report->adjacentPairs.size(), 2U);

    const SpriteQualityAdjacentMetrics& recolor = result.report->adjacentPairs[0U];
    EXPECT_TRUE(recolor.comparable);
    EXPECT_EQ(recolor.rgbaChangedPixels, 1U);
    EXPECT_EQ(recolor.visibleMaskChangedPixels, 0U);
    EXPECT_TRUE(recolor.colorChangedWithStableMask);
    EXPECT_TRUE(recolor.centroidComparable);
    EXPECT_EQ(recolor.centroidDeltaXAbsNumerator, 0U);
    EXPECT_EQ(recolor.centroidDeltaDenominator, 1U);

    const SpriteQualityAdjacentMetrics& move = result.report->adjacentPairs[1U];
    EXPECT_EQ(move.rgbaChangedPixels, 2U);
    EXPECT_EQ(move.visibleMaskChangedPixels, 2U);
    EXPECT_FALSE(move.colorChangedWithStableMask);
    EXPECT_TRUE(move.centroidComparable);
    EXPECT_FALSE(move.centroidDeltaXNegative);
    EXPECT_EQ(move.centroidDeltaXAbsNumerator, 1U);
    EXPECT_EQ(move.centroidDeltaYAbsNumerator, 0U);
    EXPECT_EQ(move.centroidDeltaDenominator, 1U);

    const auto finding = std::find_if(
        result.report->findings.begin(),
        result.report->findings.end(),
        [](const SpriteQualityFinding& value)
        {
            return value.code == SpriteQualityFindingCode::MotionCentroidThresholdExceeded;
        });
    EXPECT_NE(finding, result.report->findings.end());
}

TEST(SpriteQualityTests, RejectsAmbiguousPartialPixelGridAndDuplicatePaletteEntries)
{
    std::vector<std::uint8_t> pixels(3U * 1U * 4U, 0U);
    const std::array frames{MakeFrame("frame", 3U, 1U, pixels)};

    SpriteQualityOptions gridOptions{};
    gridOptions.pixelGrid = SpriteQualityPixelGrid{2U, 1U};
    const SpriteQualityResult gridResult = AnalyzeAndRepairSpriteQuality(frames, gridOptions);
    EXPECT_FALSE(gridResult.Succeeded());
    ASSERT_EQ(gridResult.diagnostics.size(), 1U);
    EXPECT_EQ(gridResult.diagnostics.front().code, SpriteQualityErrorCode::InvalidPixelGrid);

    const std::array palette{
        SpriteQualityRgb{1U, 2U, 3U},
        SpriteQualityRgb{1U, 2U, 3U},
    };
    SpriteQualityOptions paletteOptions{};
    paletteOptions.palette = palette;
    const SpriteQualityResult paletteResult = AnalyzeAndRepairSpriteQuality(frames, paletteOptions);
    EXPECT_FALSE(paletteResult.Succeeded());
    ASSERT_EQ(paletteResult.diagnostics.size(), 1U);
    EXPECT_EQ(paletteResult.diagnostics.front().code, SpriteQualityErrorCode::DuplicatePaletteColor);
}

TEST(SpriteQualityTests, IdenticalRequestsProduceByteIdenticalStructuralJson)
{
    std::vector<std::uint8_t> pixels(2U * 1U * 4U, 0U);
    SetPixel(pixels, 2U, 0U, 0U, {1U, 0U, 0U, 255U});
    SetPixel(pixels, 2U, 1U, 0U, {2U, 0U, 0U, 255U});

    const std::array frames{MakeFrame("stable", 2U, 1U, pixels)};
    const std::array palette{
        SpriteQualityRgb{0U, 0U, 0U},
        SpriteQualityRgb{2U, 0U, 0U},
    };
    SpriteQualityOptions options{};
    options.pixelGrid = SpriteQualityPixelGrid{2U, 1U};
    options.palette = palette;
    options.measureNearestPaletteDistance = true;
    options.repair.canonicalizePixelBlocks = true;
    options.repair.remapToPalette = true;
    options.repair.maximumPaletteDistanceSquared = 1U;

    const SpriteQualityResult first = AnalyzeAndRepairSpriteQuality(frames, options);
    const SpriteQualityResult second = AnalyzeAndRepairSpriteQuality(frames, options);

    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(second.Succeeded());
    EXPECT_EQ(SerializeSpriteQualityResultJson(first), SerializeSpriteQualityResultJson(second));
}
} // namespace trace2d::assets
