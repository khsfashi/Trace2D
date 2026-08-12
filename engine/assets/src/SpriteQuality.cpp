#include <trace2d/assets/SpriteQuality.hpp>

#include <algorithm>
#include <charconv>
#include <limits>
#include <numeric>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace trace2d::assets
{
namespace
{
constexpr std::uint64_t kBytesPerPixel = 4U;
constexpr std::size_t kMaximumPaletteSize = 256U;

struct NearestPaletteResult final
{
    std::size_t index{0U};
    std::uint32_t distanceSquared{0U};
};

bool CheckedByteCount(
    const std::uint32_t width,
    const std::uint32_t height,
    std::size_t& outBytes) noexcept
{
    const std::uint64_t pixels =
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    if (pixels > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) / kBytesPerPixel)
    {
        return false;
    }
    outBytes = static_cast<std::size_t>(pixels * kBytesPerPixel);
    return true;
}

bool CheckedAdd(std::uint64_t& value, const std::uint64_t addend) noexcept
{
    if (value > std::numeric_limits<std::uint64_t>::max() - addend)
    {
        return false;
    }
    value += addend;
    return true;
}

bool CheckedMultiply(
    const std::uint64_t left,
    const std::uint64_t right,
    std::uint64_t& out) noexcept
{
    if (left != 0U && right > std::numeric_limits<std::uint64_t>::max() / left)
    {
        return false;
    }
    out = left * right;
    return true;
}

std::uint32_t PackRgb(
    const std::uint8_t red,
    const std::uint8_t green,
    const std::uint8_t blue) noexcept
{
    return (static_cast<std::uint32_t>(red) << 16U) |
           (static_cast<std::uint32_t>(green) << 8U) |
           static_cast<std::uint32_t>(blue);
}

std::uint32_t PackRgba(
    const std::uint8_t red,
    const std::uint8_t green,
    const std::uint8_t blue,
    const std::uint8_t alpha) noexcept
{
    return (static_cast<std::uint32_t>(red) << 24U) |
           (static_cast<std::uint32_t>(green) << 16U) |
           (static_cast<std::uint32_t>(blue) << 8U) |
           static_cast<std::uint32_t>(alpha);
}

void UnpackRgba(
    const std::uint32_t packed,
    std::uint8_t& red,
    std::uint8_t& green,
    std::uint8_t& blue,
    std::uint8_t& alpha) noexcept
{
    red = static_cast<std::uint8_t>((packed >> 24U) & 0xFFU);
    green = static_cast<std::uint8_t>((packed >> 16U) & 0xFFU);
    blue = static_cast<std::uint8_t>((packed >> 8U) & 0xFFU);
    alpha = static_cast<std::uint8_t>(packed & 0xFFU);
}

bool IsValidPivot(const SpriteRationalPivot& pivot) noexcept
{
    return pivot.denominator > 0;
}

bool PaletteContains(
    const std::span<const SpriteQualityRgb> palette,
    const std::uint32_t packedRgb) noexcept
{
    for (const SpriteQualityRgb color : palette)
    {
        if (PackRgb(color.red, color.green, color.blue) == packedRgb)
        {
            return true;
        }
    }
    return false;
}

NearestPaletteResult FindNearestPalette(
    const std::span<const SpriteQualityRgb> palette,
    const std::uint8_t red,
    const std::uint8_t green,
    const std::uint8_t blue) noexcept
{
    NearestPaletteResult best{};
    best.distanceSquared = std::numeric_limits<std::uint32_t>::max();

    for (std::size_t index = 0U; index < palette.size(); ++index)
    {
        const SpriteQualityRgb candidate = palette[index];
        const std::int32_t dr = static_cast<std::int32_t>(red) - candidate.red;
        const std::int32_t dg = static_cast<std::int32_t>(green) - candidate.green;
        const std::int32_t db = static_cast<std::int32_t>(blue) - candidate.blue;
        const std::uint32_t distance = static_cast<std::uint32_t>(dr * dr + dg * dg + db * db);
        if (distance < best.distanceSquared)
        {
            best.index = index;
            best.distanceSquared = distance;
        }
    }
    return best;
}

SpriteQualityResult Fail(
    const SpriteQualityErrorCode code,
    std::string id,
    std::string message,
    std::optional<SpriteQualityReport> report = std::nullopt)
{
    SpriteQualityResult result{};
    result.report = std::move(report);
    result.diagnostics.push_back({code, std::move(id), std::move(message)});
    return result;
}

bool BuildCentroidDelta(
    const SpriteQualityCentroid& from,
    const SpriteQualityCentroid& to,
    const bool useX,
    bool& negative,
    std::uint64_t& absNumerator,
    std::uint64_t& denominator) noexcept
{
    const std::uint64_t gcd = std::gcd(from.visibleCount, to.visibleCount);
    const std::uint64_t fromScale = to.visibleCount / gcd;
    const std::uint64_t toScale = from.visibleCount / gcd;
    const std::uint64_t fromSum = useX ? from.sumX : from.sumY;
    const std::uint64_t toSum = useX ? to.sumX : to.sumY;

    std::uint64_t fromTerm = 0U;
    std::uint64_t toTerm = 0U;
    if (!CheckedMultiply(fromSum, fromScale, fromTerm) ||
        !CheckedMultiply(toSum, toScale, toTerm) ||
        !CheckedMultiply(from.visibleCount, fromScale, denominator))
    {
        return false;
    }

    negative = toTerm < fromTerm;
    absNumerator = negative ? fromTerm - toTerm : toTerm - fromTerm;
    return true;
}

bool ExceedsIntegerThreshold(
    const std::uint64_t absNumerator,
    const std::uint64_t denominator,
    const std::uint32_t threshold) noexcept
{
    if (threshold == 0U)
    {
        return absNumerator > 0U;
    }
    if (denominator > std::numeric_limits<std::uint64_t>::max() / threshold)
    {
        return false;
    }
    return absNumerator > denominator * threshold;
}

void AppendUnsigned(std::string& out, const std::uint64_t value)
{
    char buffer[32]{};
    const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
    out.append(buffer, converted.ptr);
}

void AppendSigned(std::string& out, const std::int64_t value)
{
    char buffer[32]{};
    const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
    out.append(buffer, converted.ptr);
}

void AppendBool(std::string& out, const bool value)
{
    out.append(value ? "true" : "false");
}

void AppendJsonString(std::string& out, const std::string_view value)
{
    constexpr char hex[] = "0123456789abcdef";
    out.push_back('"');
    for (const unsigned char c : value)
    {
        switch (c)
        {
        case '"': out.append("\\\""); break;
        case '\\': out.append("\\\\"); break;
        case '\b': out.append("\\b"); break;
        case '\f': out.append("\\f"); break;
        case '\n': out.append("\\n"); break;
        case '\r': out.append("\\r"); break;
        case '\t': out.append("\\t"); break;
        default:
            if (c < 0x20U)
            {
                out.append("\\u00");
                out.push_back(hex[(c >> 4U) & 0x0FU]);
                out.push_back(hex[c & 0x0FU]);
            }
            else
            {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out.push_back('"');
}

void AppendPivot(std::string& out, const SpriteRationalPivot& pivot)
{
    out.append("{\"x_numerator\":");
    AppendSigned(out, pivot.xNumerator);
    out.append(",\"y_numerator\":");
    AppendSigned(out, pivot.yNumerator);
    out.append(",\"denominator\":");
    AppendSigned(out, pivot.denominator);
    out.push_back('}');
}

bool AnyRepairRequested(const SpriteQualityRepairOptions& repair) noexcept
{
    return repair.canonicalizePixelBlocks ||
           repair.remapToPalette ||
           repair.normalizePivotTo.has_value();
}
} // namespace

std::string_view ToString(const SpriteQualityFindingCode value) noexcept
{
    switch (value)
    {
    case SpriteQualityFindingCode::PixelGridViolation: return "pixel_grid_violation";
    case SpriteQualityFindingCode::OffPalettePixels: return "off_palette_pixels";
    case SpriteQualityFindingCode::PivotTargetMismatch: return "pivot_target_mismatch";
    case SpriteQualityFindingCode::MotionCentroidThresholdExceeded: return "motion_centroid_threshold_exceeded";
    }
    return "unknown";
}

std::string_view ToString(const SpriteQualityRepairKind value) noexcept
{
    switch (value)
    {
    case SpriteQualityRepairKind::PixelBlockCanonicalization: return "pixel_block_canonicalization";
    case SpriteQualityRepairKind::PaletteRemap: return "palette_remap";
    case SpriteQualityRepairKind::PivotNormalization: return "pivot_normalization";
    }
    return "unknown";
}

std::string_view ToString(const SpriteQualityErrorCode value) noexcept
{
    switch (value)
    {
    case SpriteQualityErrorCode::NoFrames: return "no_frames";
    case SpriteQualityErrorCode::EmptyFrameId: return "empty_frame_id";
    case SpriteQualityErrorCode::DuplicateFrameId: return "duplicate_frame_id";
    case SpriteQualityErrorCode::InvalidDimensions: return "invalid_dimensions";
    case SpriteQualityErrorCode::InvalidByteCount: return "invalid_byte_count";
    case SpriteQualityErrorCode::SizeOverflow: return "size_overflow";
    case SpriteQualityErrorCode::InvalidPixelGrid: return "invalid_pixel_grid";
    case SpriteQualityErrorCode::InvalidPalette: return "invalid_palette";
    case SpriteQualityErrorCode::DuplicatePaletteColor: return "duplicate_palette_color";
    case SpriteQualityErrorCode::InvalidPivot: return "invalid_pivot";
    case SpriteQualityErrorCode::MissingPaletteRepairLimit: return "missing_palette_repair_limit";
    case SpriteQualityErrorCode::PaletteDistanceExceeded: return "palette_distance_exceeded";
    case SpriteQualityErrorCode::ProcessingFailure: return "processing_failure";
    }
    return "unknown";
}

SpriteQualityResult AnalyzeAndRepairSpriteQuality(
    const std::span<const SpriteQualityFrameView> frames,
    const SpriteQualityOptions& options)
{
    if (frames.empty())
    {
        return Fail(SpriteQualityErrorCode::NoFrames, {}, "At least one frame is required.");
    }

    if (options.pixelGrid.has_value())
    {
        const SpriteQualityPixelGrid grid = *options.pixelGrid;
        if (grid.blockWidth == 0U || grid.blockHeight == 0U)
        {
            return Fail(SpriteQualityErrorCode::InvalidPixelGrid, {}, "Pixel-grid block dimensions must be positive.");
        }
    }
    if (options.palette.size() > kMaximumPaletteSize)
    {
        return Fail(SpriteQualityErrorCode::InvalidPalette, {}, "Palette size must be at most 256 RGB entries.");
    }
    if (options.measureNearestPaletteDistance && options.palette.empty())
    {
        return Fail(SpriteQualityErrorCode::InvalidPalette, {}, "Nearest-palette analysis requires an explicit palette.");
    }
    if (options.targetPivot.has_value() && !IsValidPivot(*options.targetPivot))
    {
        return Fail(SpriteQualityErrorCode::InvalidPivot, {}, "Target pivot denominator must be positive.");
    }
    if (options.repair.canonicalizePixelBlocks && !options.pixelGrid.has_value())
    {
        return Fail(SpriteQualityErrorCode::InvalidPixelGrid, {}, "Pixel-block repair requires an explicit pixel grid.");
    }
    if (options.repair.remapToPalette && options.palette.empty())
    {
        return Fail(SpriteQualityErrorCode::InvalidPalette, {}, "Palette repair requires an explicit palette.");
    }
    if (options.repair.remapToPalette && !options.repair.maximumPaletteDistanceSquared.has_value())
    {
        return Fail(SpriteQualityErrorCode::MissingPaletteRepairLimit, {}, "Palette repair requires maximumPaletteDistanceSquared.");
    }
    if (options.repair.normalizePivotTo.has_value() && !IsValidPivot(*options.repair.normalizePivotTo))
    {
        return Fail(SpriteQualityErrorCode::InvalidPivot, {}, "Repair pivot denominator must be positive.");
    }

    std::unordered_set<std::uint32_t> paletteColors{};
    for (const SpriteQualityRgb color : options.palette)
    {
        const std::uint32_t packed = PackRgb(color.red, color.green, color.blue);
        if (!paletteColors.emplace(packed).second)
        {
            return Fail(SpriteQualityErrorCode::DuplicatePaletteColor, {}, "Palette RGB entries must be unique.");
        }
    }

    std::unordered_set<std::string> frameIds{};
    frameIds.reserve(frames.size());

    SpriteQualityReport report{};
    report.frames.reserve(frames.size());
    if (frames.size() > 1U)
    {
        report.adjacentPairs.reserve(frames.size() - 1U);
    }

    for (const SpriteQualityFrameView& frame : frames)
    {
        if (frame.id.empty())
        {
            return Fail(SpriteQualityErrorCode::EmptyFrameId, {}, "Frame ID must be non-empty.");
        }
        if (!frameIds.emplace(frame.id).second)
        {
            return Fail(SpriteQualityErrorCode::DuplicateFrameId, std::string{frame.id}, "Frame IDs must be unique.");
        }
        if (frame.width == 0U || frame.height == 0U)
        {
            return Fail(SpriteQualityErrorCode::InvalidDimensions, std::string{frame.id}, "Frame dimensions must be positive.");
        }
        std::size_t expectedBytes = 0U;
        if (!CheckedByteCount(frame.width, frame.height, expectedBytes))
        {
            return Fail(SpriteQualityErrorCode::SizeOverflow, std::string{frame.id}, "Frame byte count overflow.");
        }
        if (frame.rgba8.size() != expectedBytes)
        {
            return Fail(SpriteQualityErrorCode::InvalidByteCount, std::string{frame.id}, "RGBA8 byte count must equal width * height * 4.");
        }
        if (frame.pivot.has_value() && !IsValidPivot(*frame.pivot))
        {
            return Fail(SpriteQualityErrorCode::InvalidPivot, std::string{frame.id}, "Frame pivot denominator must be positive.");
        }
        if (options.pixelGrid.has_value())
        {
            const SpriteQualityPixelGrid grid = *options.pixelGrid;
            if (frame.width % grid.blockWidth != 0U || frame.height % grid.blockHeight != 0U)
            {
                return Fail(SpriteQualityErrorCode::InvalidPixelGrid, std::string{frame.id}, "Frame dimensions must be divisible by the explicit pixel-grid block dimensions.");
            }
        }

        SpriteQualityFrameMetrics metrics{};
        metrics.id = std::string{frame.id};
        metrics.width = frame.width;
        metrics.height = frame.height;
        metrics.pivot = frame.pivot;
        metrics.targetPivotRequested = options.targetPivot.has_value();
        metrics.matchesTargetPivot = options.targetPivot.has_value() && frame.pivot == options.targetPivot;

        if (options.pixelGrid.has_value())
        {
            const SpriteQualityPixelGrid grid = *options.pixelGrid;
            metrics.pixelGrid.requested = true;
            metrics.pixelGrid.blockWidth = grid.blockWidth;
            metrics.pixelGrid.blockHeight = grid.blockHeight;
            const std::uint64_t blockPixels =
                static_cast<std::uint64_t>(grid.blockWidth) * grid.blockHeight;

            for (std::uint32_t blockY = 0U; blockY < frame.height; blockY += grid.blockHeight)
            {
                for (std::uint32_t blockX = 0U; blockX < frame.width; blockX += grid.blockWidth)
                {
                    ++metrics.pixelGrid.checkedBlocks;
                    const std::size_t firstIndex =
                        (static_cast<std::size_t>(blockY) * frame.width + blockX) * 4U;
                    const std::uint32_t first = PackRgba(
                        frame.rgba8[firstIndex + 0U],
                        frame.rgba8[firstIndex + 1U],
                        frame.rgba8[firstIndex + 2U],
                        frame.rgba8[firstIndex + 3U]);
                    bool uniform = true;
                    for (std::uint32_t y = 0U; y < grid.blockHeight && uniform; ++y)
                    {
                        for (std::uint32_t x = 0U; x < grid.blockWidth; ++x)
                        {
                            const std::size_t index =
                                (static_cast<std::size_t>(blockY + y) * frame.width + blockX + x) * 4U;
                            const std::uint32_t packed = PackRgba(
                                frame.rgba8[index + 0U],
                                frame.rgba8[index + 1U],
                                frame.rgba8[index + 2U],
                                frame.rgba8[index + 3U]);
                            if (packed != first)
                            {
                                uniform = false;
                                break;
                            }
                        }
                    }
                    if (uniform)
                    {
                        ++metrics.pixelGrid.uniformBlocks;
                    }
                    else
                    {
                        ++metrics.pixelGrid.violatingBlocks;
                        if (!CheckedAdd(metrics.pixelGrid.violatingBlockPixels, blockPixels))
                        {
                            return Fail(SpriteQualityErrorCode::SizeOverflow, std::string{frame.id}, "Pixel-grid violation count overflow.");
                        }
                    }
                }
            }
        }

        std::unordered_set<std::uint32_t> distinctVisibleRgb{};
        if (!options.palette.empty())
        {
            metrics.palette.requested = true;
            metrics.palette.paletteSize = static_cast<std::uint32_t>(options.palette.size());
            metrics.palette.nearestDistanceMeasured = options.measureNearestPaletteDistance;
        }

        for (std::uint32_t y = 0U; y < frame.height; ++y)
        {
            for (std::uint32_t x = 0U; x < frame.width; ++x)
            {
                const std::size_t index =
                    (static_cast<std::size_t>(y) * frame.width + x) * 4U;
                const std::uint8_t red = frame.rgba8[index + 0U];
                const std::uint8_t green = frame.rgba8[index + 1U];
                const std::uint8_t blue = frame.rgba8[index + 2U];
                const std::uint8_t alpha = frame.rgba8[index + 3U];
                if (alpha == 0U)
                {
                    continue;
                }

                ++metrics.centroid.visibleCount;
                if (!CheckedAdd(metrics.centroid.sumX, x) || !CheckedAdd(metrics.centroid.sumY, y))
                {
                    return Fail(SpriteQualityErrorCode::SizeOverflow, std::string{frame.id}, "Centroid sum overflow.");
                }

                if (!options.palette.empty())
                {
                    ++metrics.palette.visiblePixels;
                    const std::uint32_t packedRgb = PackRgb(red, green, blue);
                    distinctVisibleRgb.emplace(packedRgb);
                    if (PaletteContains(options.palette, packedRgb))
                    {
                        ++metrics.palette.exactInPalettePixels;
                    }
                    else
                    {
                        ++metrics.palette.exactOffPalettePixels;
                        if (options.measureNearestPaletteDistance)
                        {
                            const NearestPaletteResult nearest =
                                FindNearestPalette(options.palette, red, green, blue);
                            metrics.palette.maximumNearestDistanceSquared = std::max(
                                metrics.palette.maximumNearestDistanceSquared,
                                nearest.distanceSquared);
                        }
                    }
                }
            }
        }
        metrics.centroid.present = metrics.centroid.visibleCount != 0U;
        metrics.palette.distinctVisibleRgb = distinctVisibleRgb.size();

        if (metrics.pixelGrid.requested && metrics.pixelGrid.violatingBlocks != 0U)
        {
            report.findings.push_back({
                SpriteProcessingSeverity::Warning,
                SpriteQualityFindingCode::PixelGridViolation,
                metrics.id,
                {},
                metrics.pixelGrid.violatingBlocks,
                metrics.pixelGrid.violatingBlockPixels,
                "Explicit pixel-grid blocks contain non-uniform RGBA8 pixels.",
            });
        }
        if (metrics.palette.requested && metrics.palette.exactOffPalettePixels != 0U)
        {
            report.findings.push_back({
                SpriteProcessingSeverity::Warning,
                SpriteQualityFindingCode::OffPalettePixels,
                metrics.id,
                {},
                metrics.palette.exactOffPalettePixels,
                metrics.palette.visiblePixels,
                "Visible pixels contain RGB values outside the explicit palette.",
            });
        }
        if (options.targetPivot.has_value() && !metrics.matchesTargetPivot)
        {
            report.findings.push_back({
                SpriteProcessingSeverity::Warning,
                SpriteQualityFindingCode::PivotTargetMismatch,
                metrics.id,
                {},
                0U,
                0U,
                "Frame pivot does not exactly match the explicit rational target pivot.",
            });
        }

        report.frames.push_back(std::move(metrics));
    }

    for (std::size_t index = 1U; index < frames.size(); ++index)
    {
        const SpriteQualityFrameView& from = frames[index - 1U];
        const SpriteQualityFrameView& to = frames[index];
        const SpriteQualityFrameMetrics& fromMetrics = report.frames[index - 1U];
        const SpriteQualityFrameMetrics& toMetrics = report.frames[index];

        SpriteQualityAdjacentMetrics adjacent{};
        adjacent.fromFrameId = std::string{from.id};
        adjacent.toFrameId = std::string{to.id};
        adjacent.comparable = from.width == to.width && from.height == to.height;
        if (adjacent.comparable)
        {
            const std::size_t pixels = from.rgba8.size() / 4U;
            for (std::size_t pixel = 0U; pixel < pixels; ++pixel)
            {
                const std::size_t byte = pixel * 4U;
                bool rgbaChanged = false;
                for (std::size_t channel = 0U; channel < 4U; ++channel)
                {
                    rgbaChanged = rgbaChanged || from.rgba8[byte + channel] != to.rgba8[byte + channel];
                }
                if (rgbaChanged)
                {
                    ++adjacent.rgbaChangedPixels;
                }
                const bool fromVisible = from.rgba8[byte + 3U] > 0U;
                const bool toVisible = to.rgba8[byte + 3U] > 0U;
                if (fromVisible != toVisible)
                {
                    ++adjacent.visibleMaskChangedPixels;
                }
            }
            adjacent.colorChangedWithStableMask =
                adjacent.visibleMaskChangedPixels == 0U && adjacent.rgbaChangedPixels != 0U;
        }

        if (fromMetrics.centroid.present && toMetrics.centroid.present)
        {
            adjacent.centroidComparable = true;
            std::uint64_t xDenominator = 1U;
            std::uint64_t yDenominator = 1U;
            if (!BuildCentroidDelta(
                    fromMetrics.centroid,
                    toMetrics.centroid,
                    true,
                    adjacent.centroidDeltaXNegative,
                    adjacent.centroidDeltaXAbsNumerator,
                    xDenominator) ||
                !BuildCentroidDelta(
                    fromMetrics.centroid,
                    toMetrics.centroid,
                    false,
                    adjacent.centroidDeltaYNegative,
                    adjacent.centroidDeltaYAbsNumerator,
                    yDenominator) ||
                xDenominator != yDenominator)
            {
                return Fail(SpriteQualityErrorCode::SizeOverflow, adjacent.toFrameId, "Centroid delta arithmetic overflow.", report);
            }
            adjacent.centroidDeltaDenominator = xDenominator;

            if (options.maximumCentroidDeltaPixels.has_value())
            {
                const std::uint32_t threshold = *options.maximumCentroidDeltaPixels;
                if (ExceedsIntegerThreshold(
                        adjacent.centroidDeltaXAbsNumerator,
                        adjacent.centroidDeltaDenominator,
                        threshold) ||
                    ExceedsIntegerThreshold(
                        adjacent.centroidDeltaYAbsNumerator,
                        adjacent.centroidDeltaDenominator,
                        threshold))
                {
                    report.findings.push_back({
                        SpriteProcessingSeverity::Warning,
                        SpriteQualityFindingCode::MotionCentroidThresholdExceeded,
                        adjacent.fromFrameId,
                        adjacent.toFrameId,
                        threshold,
                        adjacent.centroidDeltaDenominator,
                        "Adjacent visible-pixel centroid delta exceeds the explicit per-axis pixel threshold.",
                    });
                }
            }
        }

        report.adjacentPairs.push_back(std::move(adjacent));
    }

    SpriteQualityResult result{};
    result.report = report;
    if (!AnyRepairRequested(options.repair))
    {
        return result;
    }

    result.repairedFrames.reserve(frames.size());
    for (const SpriteQualityFrameView& frame : frames)
    {
        SpriteQualityRepairedFrame repaired{};
        repaired.id = std::string{frame.id};
        repaired.width = frame.width;
        repaired.height = frame.height;
        repaired.rgba8.assign(frame.rgba8.begin(), frame.rgba8.end());
        repaired.pivot = frame.pivot;
        result.repairedFrames.push_back(std::move(repaired));
    }

    if (options.repair.canonicalizePixelBlocks)
    {
        const SpriteQualityPixelGrid grid = *options.pixelGrid;
        const std::size_t blockPixels =
            static_cast<std::size_t>(grid.blockWidth) * grid.blockHeight;
        std::vector<std::uint32_t> blockValues{};
        blockValues.reserve(blockPixels);

        for (SpriteQualityRepairedFrame& frame : result.repairedFrames)
        {
            SpriteQualityRepairRecord record{};
            record.kind = SpriteQualityRepairKind::PixelBlockCanonicalization;
            record.frameId = frame.id;

            for (std::uint32_t blockY = 0U; blockY < frame.height; blockY += grid.blockHeight)
            {
                for (std::uint32_t blockX = 0U; blockX < frame.width; blockX += grid.blockWidth)
                {
                    blockValues.clear();
                    for (std::uint32_t y = 0U; y < grid.blockHeight; ++y)
                    {
                        for (std::uint32_t x = 0U; x < grid.blockWidth; ++x)
                        {
                            const std::size_t byte =
                                (static_cast<std::size_t>(blockY + y) * frame.width + blockX + x) * 4U;
                            blockValues.push_back(PackRgba(
                                frame.rgba8[byte + 0U],
                                frame.rgba8[byte + 1U],
                                frame.rgba8[byte + 2U],
                                frame.rgba8[byte + 3U]));
                        }
                    }
                    std::sort(blockValues.begin(), blockValues.end());
                    std::uint32_t bestValue = blockValues.front();
                    std::size_t bestCount = 1U;
                    std::size_t runStart = 0U;
                    while (runStart < blockValues.size())
                    {
                        std::size_t runEnd = runStart + 1U;
                        while (runEnd < blockValues.size() && blockValues[runEnd] == blockValues[runStart])
                        {
                            ++runEnd;
                        }
                        const std::size_t runCount = runEnd - runStart;
                        if (runCount > bestCount)
                        {
                            bestCount = runCount;
                            bestValue = blockValues[runStart];
                        }
                        runStart = runEnd;
                    }
                    if (bestCount == blockValues.size())
                    {
                        continue;
                    }

                    ++record.changedBlocks;
                    std::uint8_t red = 0U;
                    std::uint8_t green = 0U;
                    std::uint8_t blue = 0U;
                    std::uint8_t alpha = 0U;
                    UnpackRgba(bestValue, red, green, blue, alpha);
                    for (std::uint32_t y = 0U; y < grid.blockHeight; ++y)
                    {
                        for (std::uint32_t x = 0U; x < grid.blockWidth; ++x)
                        {
                            const std::size_t byte =
                                (static_cast<std::size_t>(blockY + y) * frame.width + blockX + x) * 4U;
                            const std::uint32_t oldValue = PackRgba(
                                frame.rgba8[byte + 0U],
                                frame.rgba8[byte + 1U],
                                frame.rgba8[byte + 2U],
                                frame.rgba8[byte + 3U]);
                            if (oldValue != bestValue)
                            {
                                ++record.changedPixels;
                                frame.rgba8[byte + 0U] = red;
                                frame.rgba8[byte + 1U] = green;
                                frame.rgba8[byte + 2U] = blue;
                                frame.rgba8[byte + 3U] = alpha;
                            }
                        }
                    }
                }
            }
            result.repairs.push_back(std::move(record));
        }
    }

    if (options.repair.remapToPalette)
    {
        const std::uint32_t maximumDistance = *options.repair.maximumPaletteDistanceSquared;
        for (SpriteQualityRepairedFrame& frame : result.repairedFrames)
        {
            SpriteQualityRepairRecord record{};
            record.kind = SpriteQualityRepairKind::PaletteRemap;
            record.frameId = frame.id;

            const std::size_t pixels = frame.rgba8.size() / 4U;
            for (std::size_t pixel = 0U; pixel < pixels; ++pixel)
            {
                const std::size_t byte = pixel * 4U;
                if (frame.rgba8[byte + 3U] == 0U)
                {
                    continue;
                }
                const std::uint32_t packedRgb = PackRgb(
                    frame.rgba8[byte + 0U],
                    frame.rgba8[byte + 1U],
                    frame.rgba8[byte + 2U]);
                if (PaletteContains(options.palette, packedRgb))
                {
                    continue;
                }

                const NearestPaletteResult nearest = FindNearestPalette(
                    options.palette,
                    frame.rgba8[byte + 0U],
                    frame.rgba8[byte + 1U],
                    frame.rgba8[byte + 2U]);
                if (nearest.distanceSquared > maximumDistance)
                {
                    result.repairedFrames.clear();
                    result.repairs.clear();
                    result.postRepairProcessingReport.reset();
                    result.diagnostics.push_back({
                        SpriteQualityErrorCode::PaletteDistanceExceeded,
                        frame.id,
                        "Off-palette RGB exceeds the explicit maximum squared palette-remap distance.",
                    });
                    return result;
                }

                const SpriteQualityRgb replacement = options.palette[nearest.index];
                frame.rgba8[byte + 0U] = replacement.red;
                frame.rgba8[byte + 1U] = replacement.green;
                frame.rgba8[byte + 2U] = replacement.blue;
                ++record.changedPixels;
            }
            result.repairs.push_back(std::move(record));
        }
    }

    if (options.repair.normalizePivotTo.has_value())
    {
        for (SpriteQualityRepairedFrame& frame : result.repairedFrames)
        {
            SpriteQualityRepairRecord record{};
            record.kind = SpriteQualityRepairKind::PivotNormalization;
            record.frameId = frame.id;
            if (frame.pivot != options.repair.normalizePivotTo)
            {
                frame.pivot = options.repair.normalizePivotTo;
                record.changedPixels = 0U;
            }
            result.repairs.push_back(std::move(record));
        }
    }

    std::vector<SpriteProcessingFrameView> processingViews{};
    processingViews.reserve(result.repairedFrames.size());
    for (const SpriteQualityRepairedFrame& frame : result.repairedFrames)
    {
        processingViews.push_back({
            .id = frame.id,
            .width = frame.width,
            .height = frame.height,
            .rgba8 = frame.rgba8,
            .pivot = frame.pivot,
        });
    }
    const SpriteProcessingResult processing = AnalyzeSpriteProcessing(processingViews, {});
    if (!processing.Succeeded())
    {
        result.repairedFrames.clear();
        result.repairs.clear();
        result.postRepairProcessingReport.reset();
        result.diagnostics.push_back({
            SpriteQualityErrorCode::ProcessingFailure,
            {},
            "SPP0 post-repair analysis failed.",
        });
        return result;
    }
    result.postRepairProcessingReport = *processing.report;
    return result;
}

std::string SerializeSpriteQualityResultJson(const SpriteQualityResult& result)
{
    std::string out{};
    out.reserve(2048U);
    out.append("{\"schema_version\":1,\"succeeded\":");
    AppendBool(out, result.Succeeded());
    out.append(",\"report\":");
    if (!result.report.has_value())
    {
        out.append("null");
    }
    else
    {
        const SpriteQualityReport& report = *result.report;
        out.append("{\"frames\":[");
        for (std::size_t index = 0U; index < report.frames.size(); ++index)
        {
            if (index != 0U) out.push_back(',');
            const SpriteQualityFrameMetrics& frame = report.frames[index];
            out.append("{\"id\":");
            AppendJsonString(out, frame.id);
            out.append(",\"width\":"); AppendUnsigned(out, frame.width);
            out.append(",\"height\":"); AppendUnsigned(out, frame.height);
            out.append(",\"pixel_grid\":{\"requested\":"); AppendBool(out, frame.pixelGrid.requested);
            out.append(",\"block_width\":"); AppendUnsigned(out, frame.pixelGrid.blockWidth);
            out.append(",\"block_height\":"); AppendUnsigned(out, frame.pixelGrid.blockHeight);
            out.append(",\"checked_blocks\":"); AppendUnsigned(out, frame.pixelGrid.checkedBlocks);
            out.append(",\"uniform_blocks\":"); AppendUnsigned(out, frame.pixelGrid.uniformBlocks);
            out.append(",\"violating_blocks\":"); AppendUnsigned(out, frame.pixelGrid.violatingBlocks);
            out.append(",\"violating_block_pixels\":"); AppendUnsigned(out, frame.pixelGrid.violatingBlockPixels);
            out.append("},\"palette\":{\"requested\":"); AppendBool(out, frame.palette.requested);
            out.append(",\"palette_size\":"); AppendUnsigned(out, frame.palette.paletteSize);
            out.append(",\"visible_pixels\":"); AppendUnsigned(out, frame.palette.visiblePixels);
            out.append(",\"exact_in_palette_pixels\":"); AppendUnsigned(out, frame.palette.exactInPalettePixels);
            out.append(",\"exact_off_palette_pixels\":"); AppendUnsigned(out, frame.palette.exactOffPalettePixels);
            out.append(",\"distinct_visible_rgb\":"); AppendUnsigned(out, frame.palette.distinctVisibleRgb);
            out.append(",\"nearest_distance_measured\":"); AppendBool(out, frame.palette.nearestDistanceMeasured);
            out.append(",\"maximum_nearest_distance_squared\":"); AppendUnsigned(out, frame.palette.maximumNearestDistanceSquared);
            out.append("},\"centroid\":{\"present\":"); AppendBool(out, frame.centroid.present);
            out.append(",\"sum_x\":"); AppendUnsigned(out, frame.centroid.sumX);
            out.append(",\"sum_y\":"); AppendUnsigned(out, frame.centroid.sumY);
            out.append(",\"visible_count\":"); AppendUnsigned(out, frame.centroid.visibleCount);
            out.append("},\"pivot\":");
            if (frame.pivot.has_value()) AppendPivot(out, *frame.pivot); else out.append("null");
            out.append(",\"target_pivot_requested\":"); AppendBool(out, frame.targetPivotRequested);
            out.append(",\"matches_target_pivot\":"); AppendBool(out, frame.matchesTargetPivot);
            out.push_back('}');
        }
        out.append("],\"adjacent_pairs\":[");
        for (std::size_t index = 0U; index < report.adjacentPairs.size(); ++index)
        {
            if (index != 0U) out.push_back(',');
            const SpriteQualityAdjacentMetrics& pair = report.adjacentPairs[index];
            out.append("{\"from\":"); AppendJsonString(out, pair.fromFrameId);
            out.append(",\"to\":"); AppendJsonString(out, pair.toFrameId);
            out.append(",\"comparable\":"); AppendBool(out, pair.comparable);
            out.append(",\"rgba_changed_pixels\":"); AppendUnsigned(out, pair.rgbaChangedPixels);
            out.append(",\"visible_mask_changed_pixels\":"); AppendUnsigned(out, pair.visibleMaskChangedPixels);
            out.append(",\"color_changed_with_stable_mask\":"); AppendBool(out, pair.colorChangedWithStableMask);
            out.append(",\"centroid_comparable\":"); AppendBool(out, pair.centroidComparable);
            out.append(",\"centroid_delta_x_negative\":"); AppendBool(out, pair.centroidDeltaXNegative);
            out.append(",\"centroid_delta_x_abs_numerator\":"); AppendUnsigned(out, pair.centroidDeltaXAbsNumerator);
            out.append(",\"centroid_delta_y_negative\":"); AppendBool(out, pair.centroidDeltaYNegative);
            out.append(",\"centroid_delta_y_abs_numerator\":"); AppendUnsigned(out, pair.centroidDeltaYAbsNumerator);
            out.append(",\"centroid_delta_denominator\":"); AppendUnsigned(out, pair.centroidDeltaDenominator);
            out.push_back('}');
        }
        out.append("],\"findings\":[");
        for (std::size_t index = 0U; index < report.findings.size(); ++index)
        {
            if (index != 0U) out.push_back(',');
            const SpriteQualityFinding& finding = report.findings[index];
            out.append("{\"severity\":"); AppendJsonString(out, ToString(finding.severity));
            out.append(",\"code\":"); AppendJsonString(out, ToString(finding.code));
            out.append(",\"primary_id\":"); AppendJsonString(out, finding.primaryId);
            out.append(",\"secondary_id\":"); AppendJsonString(out, finding.secondaryId);
            out.append(",\"value_a\":"); AppendUnsigned(out, finding.valueA);
            out.append(",\"value_b\":"); AppendUnsigned(out, finding.valueB);
            out.append(",\"message\":"); AppendJsonString(out, finding.message);
            out.push_back('}');
        }
        out.append("]}");
    }

    out.append(",\"repaired_frames\":[");
    for (std::size_t index = 0U; index < result.repairedFrames.size(); ++index)
    {
        if (index != 0U) out.push_back(',');
        const SpriteQualityRepairedFrame& frame = result.repairedFrames[index];
        out.append("{\"id\":"); AppendJsonString(out, frame.id);
        out.append(",\"width\":"); AppendUnsigned(out, frame.width);
        out.append(",\"height\":"); AppendUnsigned(out, frame.height);
        out.append(",\"byte_count\":"); AppendUnsigned(out, frame.rgba8.size());
        out.append(",\"pivot\":");
        if (frame.pivot.has_value()) AppendPivot(out, *frame.pivot); else out.append("null");
        out.push_back('}');
    }
    out.append("],\"repairs\":[");
    for (std::size_t index = 0U; index < result.repairs.size(); ++index)
    {
        if (index != 0U) out.push_back(',');
        const SpriteQualityRepairRecord& repair = result.repairs[index];
        out.append("{\"kind\":"); AppendJsonString(out, ToString(repair.kind));
        out.append(",\"frame_id\":"); AppendJsonString(out, repair.frameId);
        out.append(",\"changed_blocks\":"); AppendUnsigned(out, repair.changedBlocks);
        out.append(",\"changed_pixels\":"); AppendUnsigned(out, repair.changedPixels);
        out.push_back('}');
    }
    out.append("],\"post_repair_processing\":");
    if (result.postRepairProcessingReport.has_value())
    {
        out.append(SerializeSpriteProcessingReportJson(*result.postRepairProcessingReport));
    }
    else
    {
        out.append("null");
    }

    out.append(",\"diagnostics\":[");
    for (std::size_t index = 0U; index < result.diagnostics.size(); ++index)
    {
        if (index != 0U) out.push_back(',');
        const SpriteQualityDiagnostic& diagnostic = result.diagnostics[index];
        out.append("{\"code\":"); AppendJsonString(out, ToString(diagnostic.code));
        out.append(",\"id\":"); AppendJsonString(out, diagnostic.id);
        out.append(",\"message\":"); AppendJsonString(out, diagnostic.message);
        out.push_back('}');
    }
    out.append("]}");
    return out;
}
} // namespace trace2d::assets
