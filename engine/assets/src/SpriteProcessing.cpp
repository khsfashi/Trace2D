#include <trace2d/assets/SpriteProcessing.hpp>

#include <algorithm>
#include <charconv>
#include <limits>
#include <map>
#include <numeric>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>

namespace trace2d::assets
{
namespace
{
constexpr std::uint64_t kBytesPerPixel = 4U;

SpriteProcessingResult Failure(
    const SpriteProcessingErrorCode code,
    std::string id,
    std::string message)
{
    SpriteProcessingResult result{};
    result.diagnostics.push_back(SpriteProcessingDiagnostic{
        .code = code,
        .id = std::move(id),
        .message = std::move(message),
    });
    return result;
}

void AddFinding(
    SpriteProcessingReport& report,
    const SpriteProcessingSeverity severity,
    const SpriteProcessingFindingCode code,
    std::string primaryId,
    std::string secondaryId,
    const std::uint64_t valueA,
    const std::uint64_t valueB,
    std::string message)
{
    report.findings.push_back(SpriteProcessingFinding{
        .severity = severity,
        .code = code,
        .primaryId = std::move(primaryId),
        .secondaryId = std::move(secondaryId),
        .valueA = valueA,
        .valueB = valueB,
        .message = std::move(message),
    });
}

std::uint32_t PackRgba(
    const std::uint8_t red,
    const std::uint8_t green,
    const std::uint8_t blue,
    const std::uint8_t alpha) noexcept
{
    return static_cast<std::uint32_t>(red) |
           (static_cast<std::uint32_t>(green) << 8U) |
           (static_cast<std::uint32_t>(blue) << 16U) |
           (static_cast<std::uint32_t>(alpha) << 24U);
}

std::uint32_t PackRgb(
    const std::uint8_t red,
    const std::uint8_t green,
    const std::uint8_t blue) noexcept
{
    return static_cast<std::uint32_t>(red) |
           (static_cast<std::uint32_t>(green) << 8U) |
           (static_cast<std::uint32_t>(blue) << 16U);
}

std::uint64_t AbsMagnitude(const std::int64_t value) noexcept
{
    return value < 0 ? static_cast<std::uint64_t>(-value) : static_cast<std::uint64_t>(value);
}

bool RatioLess(
    std::uint64_t leftNumerator,
    std::uint64_t leftDenominator,
    std::uint64_t rightNumerator,
    std::uint64_t rightDenominator) noexcept
{
    bool inverted = false;

    for (;;)
    {
        const std::uint64_t leftQuotient = leftNumerator / leftDenominator;
        const std::uint64_t rightQuotient = rightNumerator / rightDenominator;
        if (leftQuotient != rightQuotient)
        {
            return inverted ? leftQuotient > rightQuotient : leftQuotient < rightQuotient;
        }

        const std::uint64_t leftRemainder = leftNumerator % leftDenominator;
        const std::uint64_t rightRemainder = rightNumerator % rightDenominator;
        if (leftRemainder == 0U || rightRemainder == 0U)
        {
            if (leftRemainder == 0U && rightRemainder == 0U)
            {
                return false;
            }

            const bool normalLess = leftRemainder == 0U;
            return inverted ? !normalLess : normalLess;
        }

        leftNumerator = leftDenominator;
        leftDenominator = leftRemainder;
        rightNumerator = rightDenominator;
        rightDenominator = rightRemainder;
        inverted = !inverted;
    }
}

bool RectanglesOverlap(const SpritePixelRect& left, const SpritePixelRect& right) noexcept
{
    const std::uint64_t leftRight = static_cast<std::uint64_t>(left.x) + left.width;
    const std::uint64_t leftBottom = static_cast<std::uint64_t>(left.y) + left.height;
    const std::uint64_t rightRight = static_cast<std::uint64_t>(right.x) + right.width;
    const std::uint64_t rightBottom = static_cast<std::uint64_t>(right.y) + right.height;

    return static_cast<std::uint64_t>(left.x) < rightRight &&
           static_cast<std::uint64_t>(right.x) < leftRight &&
           static_cast<std::uint64_t>(left.y) < rightBottom &&
           static_cast<std::uint64_t>(right.y) < leftBottom;
}

bool FramesEqual(const SpriteProcessingFrameView& left, const SpriteProcessingFrameView& right)
{
    return left.width == right.width && left.height == right.height &&
           left.rgba8.size() == right.rgba8.size() &&
           std::equal(left.rgba8.begin(), left.rgba8.end(), right.rgba8.begin());
}

void AppendUnsigned(std::string& output, const std::uint64_t value)
{
    char buffer[32]{};
    const auto conversion = std::to_chars(std::begin(buffer), std::end(buffer), value);
    output.append(buffer, conversion.ptr);
}

void AppendSigned(std::string& output, const std::int64_t value)
{
    char buffer[32]{};
    const auto conversion = std::to_chars(std::begin(buffer), std::end(buffer), value);
    output.append(buffer, conversion.ptr);
}

void AppendBool(std::string& output, const bool value)
{
    output.append(value ? "true" : "false");
}

void AppendJsonString(std::string& output, const std::string_view value)
{
    constexpr char hex[] = "0123456789abcdef";

    output.push_back('"');
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '"':
            output.append("\\\"");
            break;
        case '\\':
            output.append("\\\\");
            break;
        case '\b':
            output.append("\\b");
            break;
        case '\f':
            output.append("\\f");
            break;
        case '\n':
            output.append("\\n");
            break;
        case '\r':
            output.append("\\r");
            break;
        case '\t':
            output.append("\\t");
            break;
        default:
            if (character < 0x20U)
            {
                output.append("\\u00");
                output.push_back(hex[(character >> 4U) & 0x0FU]);
                output.push_back(hex[character & 0x0FU]);
            }
            else
            {
                output.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    output.push_back('"');
}

void AppendPixelSize(std::string& output, const SpritePixelSize size)
{
    output.append("{\"width\":");
    AppendUnsigned(output, size.width);
    output.append(",\"height\":");
    AppendUnsigned(output, size.height);
    output.push_back('}');
}

void AppendPixelRect(std::string& output, const SpritePixelRect rect)
{
    output.append("{\"x\":");
    AppendUnsigned(output, rect.x);
    output.append(",\"y\":");
    AppendUnsigned(output, rect.y);
    output.append(",\"width\":");
    AppendUnsigned(output, rect.width);
    output.append(",\"height\":");
    AppendUnsigned(output, rect.height);
    output.push_back('}');
}

void AppendPivot(std::string& output, const SpriteRationalPivot pivot)
{
    output.append("{\"x_numerator\":");
    AppendSigned(output, pivot.xNumerator);
    output.append(",\"y_numerator\":");
    AppendSigned(output, pivot.yNumerator);
    output.append(",\"denominator\":");
    AppendSigned(output, pivot.denominator);
    output.push_back('}');
}
} // namespace

std::string_view ToString(const SpriteProcessingSeverity value) noexcept
{
    switch (value)
    {
    case SpriteProcessingSeverity::Info:
        return "info";
    case SpriteProcessingSeverity::Warning:
        return "warning";
    case SpriteProcessingSeverity::Error:
        return "error";
    }
    return "unknown";
}

std::string_view ToString(const SpriteProcessingFindingCode value) noexcept
{
    switch (value)
    {
    case SpriteProcessingFindingCode::EmptyFrame:
        return "empty_frame";
    case SpriteProcessingFindingCode::VisibleTouchesEdge:
        return "visible_touches_edge";
    case SpriteProcessingFindingCode::TransparentRgbResidue:
        return "transparent_rgb_residue";
    case SpriteProcessingFindingCode::InconsistentDimensions:
        return "inconsistent_dimensions";
    case SpriteProcessingFindingCode::PivotInconsistent:
        return "pivot_inconsistent";
    case SpriteProcessingFindingCode::DuplicateFrame:
        return "duplicate_frame";
    case SpriteProcessingFindingCode::AdjacentNoChange:
        return "adjacent_no_change";
    case SpriteProcessingFindingCode::BoundsDisplacement:
        return "bounds_displacement";
    case SpriteProcessingFindingCode::AtlasOutOfBounds:
        return "atlas_out_of_bounds";
    case SpriteProcessingFindingCode::AtlasOverlap:
        return "atlas_overlap";
    case SpriteProcessingFindingCode::LowAtlasUtilization:
        return "low_atlas_utilization";
    }
    return "unknown";
}

std::string_view ToString(const SpriteProcessingErrorCode value) noexcept
{
    switch (value)
    {
    case SpriteProcessingErrorCode::EmptyFrameId:
        return "empty_frame_id";
    case SpriteProcessingErrorCode::DuplicateFrameId:
        return "duplicate_frame_id";
    case SpriteProcessingErrorCode::InvalidDimensions:
        return "invalid_dimensions";
    case SpriteProcessingErrorCode::InvalidByteCount:
        return "invalid_byte_count";
    case SpriteProcessingErrorCode::SizeOverflow:
        return "size_overflow";
    case SpriteProcessingErrorCode::InvalidGrid:
        return "invalid_grid";
    case SpriteProcessingErrorCode::InvalidThreshold:
        return "invalid_threshold";
    case SpriteProcessingErrorCode::EmptyAtlasId:
        return "empty_atlas_id";
    case SpriteProcessingErrorCode::DuplicateAtlasId:
        return "duplicate_atlas_id";
    case SpriteProcessingErrorCode::EmptyAtlasRectId:
        return "empty_atlas_rect_id";
    }
    return "unknown";
}

SpriteProcessingResult AnalyzeSpriteProcessing(
    const std::span<const SpriteProcessingFrameView> frames,
    const std::span<const SpriteProcessingAtlasPageView> atlases,
    const SpriteProcessingOptions& options)
{
    if (options.gridColumns.has_value() && *options.gridColumns == 0U)
    {
        return Failure(
            SpriteProcessingErrorCode::InvalidGrid,
            {},
            "gridColumns must be greater than zero when supplied.");
    }

    if (options.minimumAtlasUtilization.has_value())
    {
        const SpriteProcessingRatio threshold = *options.minimumAtlasUtilization;
        if (threshold.denominator == 0U || threshold.numerator > threshold.denominator)
        {
            return Failure(
                SpriteProcessingErrorCode::InvalidThreshold,
                {},
                "minimumAtlasUtilization must be a ratio in the inclusive range [0, 1].");
        }
    }

    std::unordered_set<std::string> frameIds{};
    frameIds.reserve(frames.size());
    for (const SpriteProcessingFrameView& frame : frames)
    {
        if (frame.id.empty())
        {
            return Failure(
                SpriteProcessingErrorCode::EmptyFrameId,
                {},
                "Every processing frame requires a stable non-empty ID.");
        }
        if (!frameIds.emplace(frame.id).second)
        {
            return Failure(
                SpriteProcessingErrorCode::DuplicateFrameId,
                std::string{frame.id},
                "Processing frame IDs must be unique within one report.");
        }
        if (frame.width == 0U || frame.height == 0U)
        {
            return Failure(
                SpriteProcessingErrorCode::InvalidDimensions,
                std::string{frame.id},
                "Frame width and height must both be greater than zero.");
        }

        const std::uint64_t pixelCount =
            static_cast<std::uint64_t>(frame.width) * static_cast<std::uint64_t>(frame.height);
        if (pixelCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) / kBytesPerPixel)
        {
            return Failure(
                SpriteProcessingErrorCode::SizeOverflow,
                std::string{frame.id},
                "Frame byte count cannot be represented by size_t.");
        }

        const std::size_t expectedBytes = static_cast<std::size_t>(pixelCount * kBytesPerPixel);
        if (frame.rgba8.size() != expectedBytes)
        {
            return Failure(
                SpriteProcessingErrorCode::InvalidByteCount,
                std::string{frame.id},
                "RGBA8 byte count must equal width * height * 4 exactly.");
        }
    }

    std::unordered_set<std::string> atlasIds{};
    atlasIds.reserve(atlases.size());
    for (const SpriteProcessingAtlasPageView& atlas : atlases)
    {
        if (atlas.id.empty())
        {
            return Failure(
                SpriteProcessingErrorCode::EmptyAtlasId,
                {},
                "Every atlas page requires a stable non-empty ID.");
        }
        if (!atlasIds.emplace(atlas.id).second)
        {
            return Failure(
                SpriteProcessingErrorCode::DuplicateAtlasId,
                std::string{atlas.id},
                "Atlas page IDs must be unique within one report.");
        }
        if (atlas.size.width == 0U || atlas.size.height == 0U)
        {
            return Failure(
                SpriteProcessingErrorCode::InvalidDimensions,
                std::string{atlas.id},
                "Atlas page width and height must both be greater than zero.");
        }

        for (const SpriteProcessingAtlasRectView& rect : atlas.rects)
        {
            if (rect.id.empty())
            {
                return Failure(
                    SpriteProcessingErrorCode::EmptyAtlasRectId,
                    std::string{atlas.id},
                    "Every atlas rectangle requires a stable non-empty ID.");
            }
            if (rect.rect.width == 0U || rect.rect.height == 0U)
            {
                return Failure(
                    SpriteProcessingErrorCode::InvalidDimensions,
                    std::string{atlas.id} + "/" + std::string{rect.id},
                    "Atlas rectangle width and height must both be greater than zero.");
            }
        }
    }

    SpriteProcessingReport report{};
    report.frames.reserve(frames.size());
    report.adjacentPairs.reserve(frames.empty() ? 0U : frames.size() - 1U);
    report.atlases.reserve(atlases.size());

    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint64_t> dimensionHistogram{};
    std::map<std::tuple<std::int64_t, std::int64_t, std::int64_t>, std::uint64_t> pivotHistogram{};

    for (const SpriteProcessingFrameView& frame : frames)
    {
        SpriteProcessingFrameMetrics metrics{};
        metrics.id = std::string{frame.id};
        metrics.width = frame.width;
        metrics.height = frame.height;
        metrics.pixelCount = static_cast<std::uint64_t>(frame.width) * frame.height;
        metrics.pivot = frame.pivot;

        std::unordered_set<std::uint32_t> rgbaColors{};
        std::unordered_set<std::uint32_t> visibleRgbColors{};
        const std::size_t boundedReserve = std::min<std::size_t>(frame.rgba8.size() / 4U, 4096U);
        rgbaColors.reserve(boundedReserve);
        visibleRgbColors.reserve(boundedReserve);

        std::uint32_t minX = frame.width;
        std::uint32_t minY = frame.height;
        std::uint32_t maxX = 0U;
        std::uint32_t maxY = 0U;
        bool hasVisiblePixel = false;

        for (std::uint32_t y = 0U; y < frame.height; ++y)
        {
            for (std::uint32_t x = 0U; x < frame.width; ++x)
            {
                const std::uint64_t pixelIndex =
                    static_cast<std::uint64_t>(y) * frame.width + static_cast<std::uint64_t>(x);
                const std::size_t byteIndex = static_cast<std::size_t>(pixelIndex * kBytesPerPixel);
                const std::uint8_t red = frame.rgba8[byteIndex + 0U];
                const std::uint8_t green = frame.rgba8[byteIndex + 1U];
                const std::uint8_t blue = frame.rgba8[byteIndex + 2U];
                const std::uint8_t alpha = frame.rgba8[byteIndex + 3U];

                rgbaColors.emplace(PackRgba(red, green, blue, alpha));

                if (alpha == 0U)
                {
                    ++metrics.fullyTransparentPixels;
                    if (red != 0U || green != 0U || blue != 0U)
                    {
                        ++metrics.transparentRgbResiduePixels;
                    }
                    continue;
                }

                visibleRgbColors.emplace(PackRgb(red, green, blue));
                hasVisiblePixel = true;
                minX = std::min(minX, x);
                minY = std::min(minY, y);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);

                if (x == 0U)
                {
                    ++metrics.visibleEdgePixels.left;
                }
                if (y == 0U)
                {
                    ++metrics.visibleEdgePixels.top;
                }
                if (x + 1U == frame.width)
                {
                    ++metrics.visibleEdgePixels.right;
                }
                if (y + 1U == frame.height)
                {
                    ++metrics.visibleEdgePixels.bottom;
                }

                if (alpha == std::numeric_limits<std::uint8_t>::max())
                {
                    ++metrics.fullyOpaquePixels;
                }
                else
                {
                    ++metrics.partiallyTransparentPixels;
                }
            }
        }

        metrics.uniqueRgbaColors = rgbaColors.size();
        metrics.uniqueVisibleRgbColors = visibleRgbColors.size();
        metrics.empty = !hasVisiblePixel;
        if (hasVisiblePixel)
        {
            metrics.visibleAlphaBounds = SpritePixelRect{
                .x = minX,
                .y = minY,
                .width = maxX - minX + 1U,
                .height = maxY - minY + 1U,
            };
        }

        ++dimensionHistogram[{frame.width, frame.height}];
        if (frame.pivot.has_value())
        {
            ++pivotHistogram[{
                frame.pivot->xNumerator,
                frame.pivot->yNumerator,
                frame.pivot->denominator,
            }];
        }

        if (metrics.empty)
        {
            AddFinding(
                report,
                SpriteProcessingSeverity::Warning,
                SpriteProcessingFindingCode::EmptyFrame,
                metrics.id,
                {},
                metrics.pixelCount,
                0U,
                "Frame contains no pixel with alpha greater than zero.");
        }

        const std::uint64_t touchedEdges =
            (metrics.visibleEdgePixels.left > 0U ? 1U : 0U) +
            (metrics.visibleEdgePixels.top > 0U ? 1U : 0U) +
            (metrics.visibleEdgePixels.right > 0U ? 1U : 0U) +
            (metrics.visibleEdgePixels.bottom > 0U ? 1U : 0U);
        if (touchedEdges > 0U)
        {
            AddFinding(
                report,
                SpriteProcessingSeverity::Info,
                SpriteProcessingFindingCode::VisibleTouchesEdge,
                metrics.id,
                {},
                touchedEdges,
                0U,
                "Visible pixels touch one or more outer source-image edges.");
        }

        if (metrics.transparentRgbResiduePixels > 0U)
        {
            AddFinding(
                report,
                SpriteProcessingSeverity::Info,
                SpriteProcessingFindingCode::TransparentRgbResidue,
                metrics.id,
                {},
                metrics.transparentRgbResiduePixels,
                metrics.fullyTransparentPixels,
                "Fully transparent pixels contain non-zero RGB channel data.");
        }

        report.frames.push_back(std::move(metrics));
    }

    for (const auto& [key, count] : dimensionHistogram)
    {
        report.dimensionHistogram.push_back(SpriteProcessingDimensionHistogramEntry{
            .size = SpritePixelSize{.width = key.first, .height = key.second},
            .count = count,
        });
    }
    report.uniformFrameDimensions = report.dimensionHistogram.size() <= 1U;
    if (!report.uniformFrameDimensions)
    {
        AddFinding(
            report,
            SpriteProcessingSeverity::Warning,
            SpriteProcessingFindingCode::InconsistentDimensions,
            {},
            {},
            report.dimensionHistogram.size(),
            frames.size(),
            "Frame set contains more than one source dimension.");
    }

    for (const auto& [key, count] : pivotHistogram)
    {
        report.pivotHistogram.push_back(SpriteProcessingPivotHistogramEntry{
            .pivot = SpriteRationalPivot{
                .xNumerator = std::get<0>(key),
                .yNumerator = std::get<1>(key),
                .denominator = std::get<2>(key),
            },
            .count = count,
        });
    }
    if (options.requireUniformPivot && report.pivotHistogram.size() > 1U)
    {
        AddFinding(
            report,
            SpriteProcessingSeverity::Warning,
            SpriteProcessingFindingCode::PivotInconsistent,
            {},
            {},
            report.pivotHistogram.size(),
            0U,
            "Explicit frame pivots are not uniform across the analyzed frame set.");
    }

    std::vector<bool> duplicateMember(frames.size(), false);
    for (std::size_t leftIndex = 0U; leftIndex < frames.size(); ++leftIndex)
    {
        if (duplicateMember[leftIndex])
        {
            continue;
        }

        SpriteProcessingDuplicateGroup group{};
        group.frameIds.push_back(std::string{frames[leftIndex].id});
        for (std::size_t rightIndex = leftIndex + 1U; rightIndex < frames.size(); ++rightIndex)
        {
            if (duplicateMember[rightIndex] || !FramesEqual(frames[leftIndex], frames[rightIndex]))
            {
                continue;
            }

            duplicateMember[rightIndex] = true;
            group.frameIds.push_back(std::string{frames[rightIndex].id});
            AddFinding(
                report,
                SpriteProcessingSeverity::Info,
                SpriteProcessingFindingCode::DuplicateFrame,
                std::string{frames[leftIndex].id},
                std::string{frames[rightIndex].id},
                frames[leftIndex].rgba8.size(),
                0U,
                "Frames are byte-identical after exact dimension and RGBA8 comparison.");
        }

        if (group.frameIds.size() > 1U)
        {
            duplicateMember[leftIndex] = true;
            report.duplicateGroups.push_back(std::move(group));
        }
    }

    for (std::size_t index = 1U; index < frames.size(); ++index)
    {
        const SpriteProcessingFrameView& previous = frames[index - 1U];
        const SpriteProcessingFrameView& current = frames[index];
        SpriteProcessingAdjacentMetrics adjacent{};
        adjacent.fromFrameId = std::string{previous.id};
        adjacent.toFrameId = std::string{current.id};
        adjacent.comparable = previous.width == current.width && previous.height == current.height;

        if (adjacent.comparable)
        {
            const std::uint64_t pixelCount =
                static_cast<std::uint64_t>(previous.width) * previous.height;
            for (std::uint64_t pixelIndex = 0U; pixelIndex < pixelCount; ++pixelIndex)
            {
                const std::size_t byteIndex = static_cast<std::size_t>(pixelIndex * kBytesPerPixel);
                const bool changed =
                    previous.rgba8[byteIndex + 0U] != current.rgba8[byteIndex + 0U] ||
                    previous.rgba8[byteIndex + 1U] != current.rgba8[byteIndex + 1U] ||
                    previous.rgba8[byteIndex + 2U] != current.rgba8[byteIndex + 2U] ||
                    previous.rgba8[byteIndex + 3U] != current.rgba8[byteIndex + 3U];
                if (changed)
                {
                    ++adjacent.changedPixels;
                }
            }

            const auto& previousBounds = report.frames[index - 1U].visibleAlphaBounds;
            const auto& currentBounds = report.frames[index].visibleAlphaBounds;
            if (previousBounds.has_value() && currentBounds.has_value())
            {
                adjacent.hasBoundsOriginDisplacement = true;
                adjacent.boundsOriginDeltaX =
                    static_cast<std::int64_t>(currentBounds->x) - previousBounds->x;
                adjacent.boundsOriginDeltaY =
                    static_cast<std::int64_t>(currentBounds->y) - previousBounds->y;
            }

            if (adjacent.changedPixels == 0U)
            {
                AddFinding(
                    report,
                    SpriteProcessingSeverity::Info,
                    SpriteProcessingFindingCode::AdjacentNoChange,
                    adjacent.fromFrameId,
                    adjacent.toFrameId,
                    0U,
                    pixelCount,
                    "Adjacent equal-dimension frames contain no changed RGBA8 pixels.");
            }

            if (options.maxBoundsOriginDisplacementPixels.has_value() &&
                adjacent.hasBoundsOriginDisplacement)
            {
                const std::uint64_t absoluteX = AbsMagnitude(adjacent.boundsOriginDeltaX);
                const std::uint64_t absoluteY = AbsMagnitude(adjacent.boundsOriginDeltaY);
                const std::uint64_t threshold = *options.maxBoundsOriginDisplacementPixels;
                if (absoluteX > threshold || absoluteY > threshold)
                {
                    AddFinding(
                        report,
                        SpriteProcessingSeverity::Warning,
                        SpriteProcessingFindingCode::BoundsDisplacement,
                        adjacent.fromFrameId,
                        adjacent.toFrameId,
                        absoluteX,
                        absoluteY,
                        "Visible alpha-bounds origin displacement exceeds the explicit processing threshold.");
                }
            }
        }

        report.adjacentPairs.push_back(std::move(adjacent));
    }

    if (options.gridColumns.has_value())
    {
        const std::uint64_t columns = *options.gridColumns;
        const std::uint64_t rows = frames.empty() ? 0U :
            (static_cast<std::uint64_t>(frames.size()) + columns - 1U) / columns;
        if (rows > std::numeric_limits<std::uint32_t>::max())
        {
            return Failure(
                SpriteProcessingErrorCode::SizeOverflow,
                {},
                "Requested grid row count exceeds the report representation.");
        }

        report.grid.requested = true;
        report.grid.columns = *options.gridColumns;
        report.grid.rows = static_cast<std::uint32_t>(rows);
        report.grid.complete = frames.empty() || frames.size() % *options.gridColumns == 0U;
        report.grid.uniformCellSize = report.uniformFrameDimensions && !frames.empty();
        if (report.grid.uniformCellSize)
        {
            report.grid.cellSize = SpritePixelSize{
                .width = frames.front().width,
                .height = frames.front().height,
            };
        }
    }

    for (const SpriteProcessingAtlasPageView& atlas : atlases)
    {
        SpriteProcessingAtlasMetrics metrics{};
        metrics.id = std::string{atlas.id};
        metrics.size = atlas.size;
        metrics.pageArea = static_cast<std::uint64_t>(atlas.size.width) * atlas.size.height;
        metrics.packedRectCount = atlas.rects.size();

        for (const SpriteProcessingAtlasRectView& rect : atlas.rects)
        {
            const std::uint64_t rectArea =
                static_cast<std::uint64_t>(rect.rect.width) * rect.rect.height;
            if (metrics.occupiedPackedArea > std::numeric_limits<std::uint64_t>::max() - rectArea)
            {
                return Failure(
                    SpriteProcessingErrorCode::SizeOverflow,
                    metrics.id,
                    "Summed atlas rectangle area exceeds the report representation.");
            }
            metrics.occupiedPackedArea += rectArea;

            const std::uint64_t right = static_cast<std::uint64_t>(rect.rect.x) + rect.rect.width;
            const std::uint64_t bottom = static_cast<std::uint64_t>(rect.rect.y) + rect.rect.height;
            if (right > atlas.size.width || bottom > atlas.size.height)
            {
                ++metrics.outOfBoundsRectCount;
                AddFinding(
                    report,
                    SpriteProcessingSeverity::Error,
                    SpriteProcessingFindingCode::AtlasOutOfBounds,
                    metrics.id,
                    std::string{rect.id},
                    right,
                    bottom,
                    "Packed atlas rectangle extends beyond the declared page dimensions.");
            }
        }

        for (std::size_t leftIndex = 0U; leftIndex < atlas.rects.size(); ++leftIndex)
        {
            for (std::size_t rightIndex = leftIndex + 1U; rightIndex < atlas.rects.size(); ++rightIndex)
            {
                if (!RectanglesOverlap(atlas.rects[leftIndex].rect, atlas.rects[rightIndex].rect))
                {
                    continue;
                }

                ++metrics.overlappingRectPairCount;
                AddFinding(
                    report,
                    SpriteProcessingSeverity::Error,
                    SpriteProcessingFindingCode::AtlasOverlap,
                    metrics.id,
                    std::string{atlas.rects[leftIndex].id} + ":" + std::string{atlas.rects[rightIndex].id},
                    leftIndex,
                    rightIndex,
                    "Packed atlas rectangles overlap in declared page space.");
            }
        }

        metrics.utilization = SpriteProcessingRatio{
            .numerator = metrics.occupiedPackedArea,
            .denominator = metrics.pageArea,
        };

        if (options.minimumAtlasUtilization.has_value() &&
            RatioLess(
                metrics.utilization.numerator,
                metrics.utilization.denominator,
                options.minimumAtlasUtilization->numerator,
                options.minimumAtlasUtilization->denominator))
        {
            AddFinding(
                report,
                SpriteProcessingSeverity::Info,
                SpriteProcessingFindingCode::LowAtlasUtilization,
                metrics.id,
                {},
                metrics.utilization.numerator,
                metrics.utilization.denominator,
                "Atlas packed-area utilization is below the explicit processing threshold.");
        }

        report.atlases.push_back(std::move(metrics));
    }

    SpriteProcessingResult result{};
    result.report = std::move(report);
    return result;
}

std::string SerializeSpriteProcessingReportJson(const SpriteProcessingReport& report)
{
    std::string output{};
    output.reserve(2048U + report.frames.size() * 512U + report.findings.size() * 256U);
    output.append("{\"schema_version\":");
    AppendUnsigned(output, report.schemaVersion);

    output.append(",\"frames\":[");
    for (std::size_t index = 0U; index < report.frames.size(); ++index)
    {
        if (index > 0U)
        {
            output.push_back(',');
        }
        const SpriteProcessingFrameMetrics& frame = report.frames[index];
        output.append("{\"id\":");
        AppendJsonString(output, frame.id);
        output.append(",\"width\":");
        AppendUnsigned(output, frame.width);
        output.append(",\"height\":");
        AppendUnsigned(output, frame.height);
        output.append(",\"pixel_count\":");
        AppendUnsigned(output, frame.pixelCount);
        output.append(",\"fully_transparent_pixels\":");
        AppendUnsigned(output, frame.fullyTransparentPixels);
        output.append(",\"partially_transparent_pixels\":");
        AppendUnsigned(output, frame.partiallyTransparentPixels);
        output.append(",\"fully_opaque_pixels\":");
        AppendUnsigned(output, frame.fullyOpaquePixels);
        output.append(",\"visible_alpha_bounds\":");
        if (frame.visibleAlphaBounds.has_value())
        {
            AppendPixelRect(output, *frame.visibleAlphaBounds);
        }
        else
        {
            output.append("null");
        }
        output.append(",\"empty\":");
        AppendBool(output, frame.empty);
        output.append(",\"visible_edge_pixels\":{\"left\":");
        AppendUnsigned(output, frame.visibleEdgePixels.left);
        output.append(",\"top\":");
        AppendUnsigned(output, frame.visibleEdgePixels.top);
        output.append(",\"right\":");
        AppendUnsigned(output, frame.visibleEdgePixels.right);
        output.append(",\"bottom\":");
        AppendUnsigned(output, frame.visibleEdgePixels.bottom);
        output.append("},\"transparent_rgb_residue_pixels\":");
        AppendUnsigned(output, frame.transparentRgbResiduePixels);
        output.append(",\"unique_rgba_colors\":");
        AppendUnsigned(output, frame.uniqueRgbaColors);
        output.append(",\"unique_visible_rgb_colors\":");
        AppendUnsigned(output, frame.uniqueVisibleRgbColors);
        output.append(",\"pivot\":");
        if (frame.pivot.has_value())
        {
            AppendPivot(output, *frame.pivot);
        }
        else
        {
            output.append("null");
        }
        output.push_back('}');
    }
    output.push_back(']');

    output.append(",\"uniform_frame_dimensions\":");
    AppendBool(output, report.uniformFrameDimensions);
    output.append(",\"dimension_histogram\":[");
    for (std::size_t index = 0U; index < report.dimensionHistogram.size(); ++index)
    {
        if (index > 0U)
        {
            output.push_back(',');
        }
        output.append("{\"size\":");
        AppendPixelSize(output, report.dimensionHistogram[index].size);
        output.append(",\"count\":");
        AppendUnsigned(output, report.dimensionHistogram[index].count);
        output.push_back('}');
    }
    output.push_back(']');

    output.append(",\"pivot_histogram\":[");
    for (std::size_t index = 0U; index < report.pivotHistogram.size(); ++index)
    {
        if (index > 0U)
        {
            output.push_back(',');
        }
        output.append("{\"pivot\":");
        AppendPivot(output, report.pivotHistogram[index].pivot);
        output.append(",\"count\":");
        AppendUnsigned(output, report.pivotHistogram[index].count);
        output.push_back('}');
    }
    output.push_back(']');

    output.append(",\"duplicate_groups\":[");
    for (std::size_t groupIndex = 0U; groupIndex < report.duplicateGroups.size(); ++groupIndex)
    {
        if (groupIndex > 0U)
        {
            output.push_back(',');
        }
        output.append("{\"frame_ids\":[");
        const auto& ids = report.duplicateGroups[groupIndex].frameIds;
        for (std::size_t idIndex = 0U; idIndex < ids.size(); ++idIndex)
        {
            if (idIndex > 0U)
            {
                output.push_back(',');
            }
            AppendJsonString(output, ids[idIndex]);
        }
        output.append("]}");
    }
    output.push_back(']');

    output.append(",\"adjacent_pairs\":[");
    for (std::size_t index = 0U; index < report.adjacentPairs.size(); ++index)
    {
        if (index > 0U)
        {
            output.push_back(',');
        }
        const SpriteProcessingAdjacentMetrics& pair = report.adjacentPairs[index];
        output.append("{\"from\":");
        AppendJsonString(output, pair.fromFrameId);
        output.append(",\"to\":");
        AppendJsonString(output, pair.toFrameId);
        output.append(",\"comparable\":");
        AppendBool(output, pair.comparable);
        output.append(",\"changed_pixels\":");
        AppendUnsigned(output, pair.changedPixels);
        output.append(",\"has_bounds_origin_displacement\":");
        AppendBool(output, pair.hasBoundsOriginDisplacement);
        output.append(",\"bounds_origin_delta_x\":");
        AppendSigned(output, pair.boundsOriginDeltaX);
        output.append(",\"bounds_origin_delta_y\":");
        AppendSigned(output, pair.boundsOriginDeltaY);
        output.push_back('}');
    }
    output.push_back(']');

    output.append(",\"grid\":{\"requested\":");
    AppendBool(output, report.grid.requested);
    output.append(",\"columns\":");
    AppendUnsigned(output, report.grid.columns);
    output.append(",\"rows\":");
    AppendUnsigned(output, report.grid.rows);
    output.append(",\"complete\":");
    AppendBool(output, report.grid.complete);
    output.append(",\"uniform_cell_size\":");
    AppendBool(output, report.grid.uniformCellSize);
    output.append(",\"cell_size\":");
    AppendPixelSize(output, report.grid.cellSize);
    output.push_back('}');

    output.append(",\"atlases\":[");
    for (std::size_t index = 0U; index < report.atlases.size(); ++index)
    {
        if (index > 0U)
        {
            output.push_back(',');
        }
        const SpriteProcessingAtlasMetrics& atlas = report.atlases[index];
        output.append("{\"id\":");
        AppendJsonString(output, atlas.id);
        output.append(",\"size\":");
        AppendPixelSize(output, atlas.size);
        output.append(",\"page_area\":");
        AppendUnsigned(output, atlas.pageArea);
        output.append(",\"packed_rect_count\":");
        AppendUnsigned(output, atlas.packedRectCount);
        output.append(",\"occupied_packed_area\":");
        AppendUnsigned(output, atlas.occupiedPackedArea);
        output.append(",\"utilization\":{\"numerator\":");
        AppendUnsigned(output, atlas.utilization.numerator);
        output.append(",\"denominator\":");
        AppendUnsigned(output, atlas.utilization.denominator);
        output.append("},\"out_of_bounds_rect_count\":");
        AppendUnsigned(output, atlas.outOfBoundsRectCount);
        output.append(",\"overlapping_rect_pair_count\":");
        AppendUnsigned(output, atlas.overlappingRectPairCount);
        output.push_back('}');
    }
    output.push_back(']');

    output.append(",\"findings\":[");
    for (std::size_t index = 0U; index < report.findings.size(); ++index)
    {
        if (index > 0U)
        {
            output.push_back(',');
        }
        const SpriteProcessingFinding& finding = report.findings[index];
        output.append("{\"severity\":");
        AppendJsonString(output, ToString(finding.severity));
        output.append(",\"code\":");
        AppendJsonString(output, ToString(finding.code));
        output.append(",\"primary_id\":");
        AppendJsonString(output, finding.primaryId);
        output.append(",\"secondary_id\":");
        AppendJsonString(output, finding.secondaryId);
        output.append(",\"value_a\":");
        AppendUnsigned(output, finding.valueA);
        output.append(",\"value_b\":");
        AppendUnsigned(output, finding.valueB);
        output.append(",\"message\":");
        AppendJsonString(output, finding.message);
        output.push_back('}');
    }
    output.append("]}");
    return output;
}
} // namespace trace2d::assets
