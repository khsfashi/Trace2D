#pragma once

#include <trace2d/assets/SpriteProcessing.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::assets
{
enum class SpriteQualityFindingCode : std::uint8_t
{
    PixelGridViolation = 0,
    OffPalettePixels,
    PivotTargetMismatch,
    MotionCentroidThresholdExceeded,
};

enum class SpriteQualityRepairKind : std::uint8_t
{
    PixelBlockCanonicalization = 0,
    PaletteRemap,
    PivotNormalization,
};

enum class SpriteQualityErrorCode : std::uint8_t
{
    NoFrames = 0,
    EmptyFrameId,
    DuplicateFrameId,
    InvalidDimensions,
    InvalidByteCount,
    SizeOverflow,
    InvalidPixelGrid,
    InvalidPalette,
    DuplicatePaletteColor,
    InvalidPivot,
    MissingPaletteRepairLimit,
    PaletteDistanceExceeded,
    ProcessingFailure,
};

[[nodiscard]] std::string_view ToString(SpriteQualityFindingCode value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteQualityRepairKind value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteQualityErrorCode value) noexcept;

struct SpriteQualityRgb final
{
    std::uint8_t red{0};
    std::uint8_t green{0};
    std::uint8_t blue{0};

    [[nodiscard]] bool operator==(const SpriteQualityRgb&) const noexcept = default;
};

struct SpriteQualityFrameView final
{
    std::string_view id{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::span<const std::uint8_t> rgba8{};
    std::optional<SpriteRationalPivot> pivot{};
};

struct SpriteQualityPixelGrid final
{
    std::uint32_t blockWidth{0};
    std::uint32_t blockHeight{0};
};

struct SpriteQualityRepairOptions final
{
    bool canonicalizePixelBlocks{false};
    bool remapToPalette{false};
    std::optional<std::uint32_t> maximumPaletteDistanceSquared{};
    std::optional<SpriteRationalPivot> normalizePivotTo{};
};

struct SpriteQualityOptions final
{
    std::optional<SpriteQualityPixelGrid> pixelGrid{};
    std::span<const SpriteQualityRgb> palette{};
    bool measureNearestPaletteDistance{false};
    std::optional<SpriteRationalPivot> targetPivot{};
    std::optional<std::uint32_t> maximumCentroidDeltaPixels{};
    SpriteQualityRepairOptions repair{};
};

struct SpriteQualityPixelGridMetrics final
{
    bool requested{false};
    std::uint32_t blockWidth{0};
    std::uint32_t blockHeight{0};
    std::uint64_t checkedBlocks{0};
    std::uint64_t uniformBlocks{0};
    std::uint64_t violatingBlocks{0};
    std::uint64_t violatingBlockPixels{0};
};

struct SpriteQualityPaletteMetrics final
{
    bool requested{false};
    std::uint32_t paletteSize{0};
    std::uint64_t visiblePixels{0};
    std::uint64_t exactInPalettePixels{0};
    std::uint64_t exactOffPalettePixels{0};
    std::uint64_t distinctVisibleRgb{0};
    bool nearestDistanceMeasured{false};
    std::uint32_t maximumNearestDistanceSquared{0};
};

struct SpriteQualityCentroid final
{
    bool present{false};
    std::uint64_t sumX{0};
    std::uint64_t sumY{0};
    std::uint64_t visibleCount{0};
};

struct SpriteQualityFrameMetrics final
{
    std::string id{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    SpriteQualityPixelGridMetrics pixelGrid{};
    SpriteQualityPaletteMetrics palette{};
    SpriteQualityCentroid centroid{};
    std::optional<SpriteRationalPivot> pivot{};
    bool targetPivotRequested{false};
    bool matchesTargetPivot{false};
};

struct SpriteQualityAdjacentMetrics final
{
    std::string fromFrameId{};
    std::string toFrameId{};
    bool comparable{false};
    std::uint64_t rgbaChangedPixels{0};
    std::uint64_t visibleMaskChangedPixels{0};
    bool colorChangedWithStableMask{false};
    bool centroidComparable{false};
    bool centroidDeltaXNegative{false};
    std::uint64_t centroidDeltaXAbsNumerator{0};
    bool centroidDeltaYNegative{false};
    std::uint64_t centroidDeltaYAbsNumerator{0};
    std::uint64_t centroidDeltaDenominator{1};
};

struct SpriteQualityFinding final
{
    SpriteProcessingSeverity severity{SpriteProcessingSeverity::Info};
    SpriteQualityFindingCode code{SpriteQualityFindingCode::PixelGridViolation};
    std::string primaryId{};
    std::string secondaryId{};
    std::uint64_t valueA{0};
    std::uint64_t valueB{0};
    std::string message{};
};

struct SpriteQualityRepairRecord final
{
    SpriteQualityRepairKind kind{SpriteQualityRepairKind::PixelBlockCanonicalization};
    std::string frameId{};
    std::uint64_t changedBlocks{0};
    std::uint64_t changedPixels{0};
};

struct SpriteQualityRepairedFrame final
{
    std::string id{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> rgba8{};
    std::optional<SpriteRationalPivot> pivot{};
};

struct SpriteQualityDiagnostic final
{
    SpriteQualityErrorCode code{SpriteQualityErrorCode::InvalidDimensions};
    std::string id{};
    std::string message{};
};

struct SpriteQualityReport final
{
    std::uint32_t schemaVersion{1};
    std::vector<SpriteQualityFrameMetrics> frames{};
    std::vector<SpriteQualityAdjacentMetrics> adjacentPairs{};
    std::vector<SpriteQualityFinding> findings{};
};

struct SpriteQualityResult final
{
    std::optional<SpriteQualityReport> report{};
    std::vector<SpriteQualityRepairedFrame> repairedFrames{};
    std::vector<SpriteQualityRepairRecord> repairs{};
    std::optional<SpriteProcessingReport> postRepairProcessingReport{};
    std::vector<SpriteQualityDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return report.has_value() && diagnostics.empty();
    }
};

[[nodiscard]] SpriteQualityResult AnalyzeAndRepairSpriteQuality(
    std::span<const SpriteQualityFrameView> frames,
    const SpriteQualityOptions& options = {});

[[nodiscard]] std::string SerializeSpriteQualityResultJson(const SpriteQualityResult& result);
} // namespace trace2d::assets
