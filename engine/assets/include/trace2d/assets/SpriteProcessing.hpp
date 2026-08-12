#pragma once

#include <trace2d/assets/SpriteAssets.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::assets
{
enum class SpriteProcessingSeverity : std::uint8_t
{
    Info = 0,
    Warning,
    Error,
};

enum class SpriteProcessingFindingCode : std::uint8_t
{
    EmptyFrame = 0,
    VisibleTouchesEdge,
    TransparentRgbResidue,
    InconsistentDimensions,
    PivotInconsistent,
    DuplicateFrame,
    AdjacentNoChange,
    BoundsDisplacement,
    AtlasOutOfBounds,
    AtlasOverlap,
    LowAtlasUtilization,
};

enum class SpriteProcessingErrorCode : std::uint8_t
{
    EmptyFrameId = 0,
    DuplicateFrameId,
    InvalidDimensions,
    InvalidByteCount,
    SizeOverflow,
    InvalidGrid,
    InvalidThreshold,
    EmptyAtlasId,
    DuplicateAtlasId,
    EmptyAtlasRectId,
};

[[nodiscard]] std::string_view ToString(SpriteProcessingSeverity value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteProcessingFindingCode value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteProcessingErrorCode value) noexcept;

struct SpriteProcessingRatio final
{
    std::uint64_t numerator{0};
    std::uint64_t denominator{1};

    [[nodiscard]] bool operator==(const SpriteProcessingRatio&) const noexcept = default;
};

struct SpriteProcessingFrameView final
{
    std::string_view id{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::span<const std::uint8_t> rgba8{};
    std::optional<SpriteRationalPivot> pivot{};
};

struct SpriteProcessingAtlasRectView final
{
    std::string_view id{};
    SpritePixelRect rect{};
    SpritePackedRotation packedRotation{SpritePackedRotation::None};
};

struct SpriteProcessingAtlasPageView final
{
    std::string_view id{};
    SpritePixelSize size{};
    std::span<const SpriteProcessingAtlasRectView> rects{};
};

struct SpriteProcessingOptions final
{
    std::optional<std::uint32_t> gridColumns{};
    std::optional<std::uint32_t> maxBoundsOriginDisplacementPixels{};
    std::optional<SpriteProcessingRatio> minimumAtlasUtilization{};
    bool requireUniformPivot{false};
};

struct SpriteProcessingEdgeCounts final
{
    std::uint64_t left{0};
    std::uint64_t top{0};
    std::uint64_t right{0};
    std::uint64_t bottom{0};

    [[nodiscard]] bool operator==(const SpriteProcessingEdgeCounts&) const noexcept = default;
};

struct SpriteProcessingFrameMetrics final
{
    std::string id{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::uint64_t pixelCount{0};
    std::uint64_t fullyTransparentPixels{0};
    std::uint64_t partiallyTransparentPixels{0};
    std::uint64_t fullyOpaquePixels{0};
    std::optional<SpritePixelRect> visibleAlphaBounds{};
    bool empty{true};
    SpriteProcessingEdgeCounts visibleEdgePixels{};
    std::uint64_t transparentRgbResiduePixels{0};
    std::uint64_t uniqueRgbaColors{0};
    std::uint64_t uniqueVisibleRgbColors{0};
    std::optional<SpriteRationalPivot> pivot{};
};

struct SpriteProcessingDimensionHistogramEntry final
{
    SpritePixelSize size{};
    std::uint64_t count{0};
};

struct SpriteProcessingPivotHistogramEntry final
{
    SpriteRationalPivot pivot{};
    std::uint64_t count{0};
};

struct SpriteProcessingDuplicateGroup final
{
    std::vector<std::string> frameIds{};
};

struct SpriteProcessingAdjacentMetrics final
{
    std::string fromFrameId{};
    std::string toFrameId{};
    bool comparable{false};
    std::uint64_t changedPixels{0};
    bool hasBoundsOriginDisplacement{false};
    std::int64_t boundsOriginDeltaX{0};
    std::int64_t boundsOriginDeltaY{0};
};

struct SpriteProcessingGridMetrics final
{
    bool requested{false};
    std::uint32_t columns{0};
    std::uint32_t rows{0};
    bool complete{false};
    bool uniformCellSize{false};
    SpritePixelSize cellSize{};
};

struct SpriteProcessingAtlasMetrics final
{
    std::string id{};
    SpritePixelSize size{};
    std::uint64_t pageArea{0};
    std::uint64_t packedRectCount{0};
    std::uint64_t occupiedPackedArea{0};
    SpriteProcessingRatio utilization{};
    std::uint64_t outOfBoundsRectCount{0};
    std::uint64_t overlappingRectPairCount{0};
};

struct SpriteProcessingFinding final
{
    SpriteProcessingSeverity severity{SpriteProcessingSeverity::Info};
    SpriteProcessingFindingCode code{SpriteProcessingFindingCode::EmptyFrame};
    std::string primaryId{};
    std::string secondaryId{};
    std::uint64_t valueA{0};
    std::uint64_t valueB{0};
    std::string message{};
};

struct SpriteProcessingDiagnostic final
{
    SpriteProcessingErrorCode code{SpriteProcessingErrorCode::InvalidDimensions};
    std::string id{};
    std::string message{};
};

struct SpriteProcessingReport final
{
    std::uint32_t schemaVersion{1};
    std::vector<SpriteProcessingFrameMetrics> frames{};
    bool uniformFrameDimensions{true};
    std::vector<SpriteProcessingDimensionHistogramEntry> dimensionHistogram{};
    std::vector<SpriteProcessingPivotHistogramEntry> pivotHistogram{};
    std::vector<SpriteProcessingDuplicateGroup> duplicateGroups{};
    std::vector<SpriteProcessingAdjacentMetrics> adjacentPairs{};
    SpriteProcessingGridMetrics grid{};
    std::vector<SpriteProcessingAtlasMetrics> atlases{};
    std::vector<SpriteProcessingFinding> findings{};
};

struct SpriteProcessingResult final
{
    std::optional<SpriteProcessingReport> report{};
    std::vector<SpriteProcessingDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return report.has_value() && diagnostics.empty();
    }
};

[[nodiscard]] SpriteProcessingResult AnalyzeSpriteProcessing(
    std::span<const SpriteProcessingFrameView> frames,
    std::span<const SpriteProcessingAtlasPageView> atlases,
    const SpriteProcessingOptions& options = {});

[[nodiscard]] std::string SerializeSpriteProcessingReportJson(const SpriteProcessingReport& report);
} // namespace trace2d::assets
