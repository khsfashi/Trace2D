#include <trace2d/render/SpritePrimitive2D.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace trace2d::render
{
namespace
{
struct AxisPiece final
{
    double targetMinimum{0.0};
    double targetMaximum{0.0};
    double sourceMinimum{0.0};
    double sourceMaximum{0.0};
};

struct PrimitiveValidation final
{
    const assets::SpriteAtlasPage* page{nullptr};
    const assets::SpriteRegion* region{nullptr};
    std::array<double, 4U> sourceX{};
    std::array<double, 4U> sourceY{};
    std::array<double, 4U> targetX{};
    std::array<double, 4U> targetY{};
};

[[nodiscard]] SpritePrimitiveStatus Success() noexcept
{
    return SpritePrimitiveStatus{};
}

[[nodiscard]] SpritePrimitiveStatus Failure(
    const SpritePrimitiveError error,
    const SpritePrimitiveField field) noexcept
{
    return SpritePrimitiveStatus{error, field, SpriteGeometryStatus{}};
}

[[nodiscard]] SpritePrimitiveStatus GeometryFailure(
    const SpriteGeometryStatus geometry) noexcept
{
    return SpritePrimitiveStatus{
        SpritePrimitiveError::Geometry,
        SpritePrimitiveField::Geometry,
        geometry,
    };
}

[[nodiscard]] bool FitsExtent(
    const std::uint32_t offset,
    const std::uint32_t extent,
    const std::uint32_t limit) noexcept
{
    return static_cast<std::uint64_t>(offset) + static_cast<std::uint64_t>(extent) <=
        static_cast<std::uint64_t>(limit);
}

[[nodiscard]] bool TryFloat(const double value, float& outValue) noexcept
{
    constexpr double MaximumFloat = static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(value) || value > MaximumFloat || value < -MaximumFloat)
    {
        return false;
    }
    outValue = static_cast<float>(value);
    return std::isfinite(outValue);
}

void ResolveTargetAxis(
    const double sourceExtent,
    const double beforeBorder,
    const double afterBorder,
    const double targetExtent,
    std::array<double, 4U>& output) noexcept
{
    double targetBefore = beforeBorder;
    double targetAfter = afterBorder;
    const double borderSum = beforeBorder + afterBorder;
    if (borderSum > 0.0 && targetExtent < borderSum)
    {
        const double scale = targetExtent / borderSum;
        targetBefore *= scale;
        targetAfter *= scale;
    }

    output = std::array<double, 4U>{
        0.0,
        targetBefore,
        targetExtent - targetAfter,
        targetExtent,
    };

    // Avoid tiny negative/overlapping center spans from floating-point subtraction.
    if (output[2] < output[1])
    {
        output[2] = output[1];
    }

    static_cast<void>(sourceExtent);
}

[[nodiscard]] SpritePrimitiveStatus ValidatePrimitive(
    const ResolvedSpriteRegion& selection,
    const SpritePrimitive2D& primitive,
    PrimitiveValidation& outValidation) noexcept
{
    outValidation = PrimitiveValidation{};
    if (!selection.Valid() || selection.Page() == nullptr || selection.Region() == nullptr)
    {
        return Failure(
            SpritePrimitiveError::UnresolvedSelection,
            SpritePrimitiveField::Selection);
    }

    const assets::SpriteAtlasPage& page = *selection.Page();
    const assets::SpriteRegion& region = *selection.Region();
    if (region.sourceSize.width == 0U || region.sourceSize.height == 0U)
    {
        return Failure(
            SpritePrimitiveError::InvalidSourceSize,
            SpritePrimitiveField::SourceSize);
    }

    const std::uint64_t horizontalBorder =
        static_cast<std::uint64_t>(region.border.left) +
        static_cast<std::uint64_t>(region.border.right);
    const std::uint64_t verticalBorder =
        static_cast<std::uint64_t>(region.border.top) +
        static_cast<std::uint64_t>(region.border.bottom);
    if (horizontalBorder > static_cast<std::uint64_t>(region.sourceSize.width) ||
        verticalBorder > static_cast<std::uint64_t>(region.sourceSize.height))
    {
        return Failure(
            SpritePrimitiveError::InvalidBorder,
            SpritePrimitiveField::Border);
    }

    if (page.size.width == 0U || page.size.height == 0U)
    {
        return Failure(
            SpritePrimitiveError::InvalidPageSize,
            SpritePrimitiveField::PageSize);
    }
    if (region.trimSize.width == 0U || region.trimSize.height == 0U ||
        !FitsExtent(region.trimOffset.x, region.trimSize.width, region.sourceSize.width) ||
        !FitsExtent(region.trimOffset.y, region.trimSize.height, region.sourceSize.height))
    {
        return Failure(
            SpritePrimitiveError::InvalidTrimRect,
            SpritePrimitiveField::TrimRect);
    }
    if (region.packedRect.width == 0U || region.packedRect.height == 0U ||
        !FitsExtent(region.packedRect.x, region.packedRect.width, page.size.width) ||
        !FitsExtent(region.packedRect.y, region.packedRect.height, page.size.height))
    {
        return Failure(
            SpritePrimitiveError::InvalidPackedRect,
            SpritePrimitiveField::PackedRect);
    }

    switch (region.packedRotation)
    {
    case assets::SpritePackedRotation::None:
        if (region.packedRect.width != region.trimSize.width ||
            region.packedRect.height != region.trimSize.height)
        {
            return Failure(
                SpritePrimitiveError::PackedExtentMismatch,
                SpritePrimitiveField::PackedRect);
        }
        break;
    case assets::SpritePackedRotation::Cw90:
        if (region.packedRect.width != region.trimSize.height ||
            region.packedRect.height != region.trimSize.width)
        {
            return Failure(
                SpritePrimitiveError::PackedExtentMismatch,
                SpritePrimitiveField::PackedRect);
        }
        break;
    default:
        return Failure(
            SpritePrimitiveError::UnsupportedPackedRotation,
            SpritePrimitiveField::PackedRotation);
    }

    const double sourceWidth = static_cast<double>(region.sourceSize.width);
    const double sourceHeight = static_cast<double>(region.sourceSize.height);
    outValidation.sourceX = std::array<double, 4U>{
        0.0,
        static_cast<double>(region.border.left),
        sourceWidth - static_cast<double>(region.border.right),
        sourceWidth,
    };
    outValidation.sourceY = std::array<double, 4U>{
        0.0,
        static_cast<double>(region.border.top),
        sourceHeight - static_cast<double>(region.border.bottom),
        sourceHeight,
    };

    switch (primitive.mode)
    {
    case SpritePrimitiveMode::Quad:
        outValidation.targetX = outValidation.sourceX;
        outValidation.targetY = outValidation.sourceY;
        break;
    case SpritePrimitiveMode::Sliced:
    case SpritePrimitiveMode::Tiled:
        if (!std::isfinite(primitive.targetSizeSourcePixels.x) ||
            !std::isfinite(primitive.targetSizeSourcePixels.y) ||
            primitive.targetSizeSourcePixels.x <= 0.0F ||
            primitive.targetSizeSourcePixels.y <= 0.0F)
        {
            return Failure(
                SpritePrimitiveError::InvalidTargetSize,
                SpritePrimitiveField::TargetSize);
        }
        ResolveTargetAxis(
            sourceWidth,
            static_cast<double>(region.border.left),
            static_cast<double>(region.border.right),
            static_cast<double>(primitive.targetSizeSourcePixels.x),
            outValidation.targetX);
        ResolveTargetAxis(
            sourceHeight,
            static_cast<double>(region.border.top),
            static_cast<double>(region.border.bottom),
            static_cast<double>(primitive.targetSizeSourcePixels.y),
            outValidation.targetY);
        break;
    default:
        return Failure(
            SpritePrimitiveError::UnsupportedMode,
            SpritePrimitiveField::Mode);
    }

    outValidation.page = &page;
    outValidation.region = &region;
    return Success();
}

[[nodiscard]] std::size_t CountAxisPieces(
    const bool repeat,
    const double sourceMinimum,
    const double sourceMaximum,
    const double targetMinimum,
    const double targetMaximum,
    const double trimMinimum,
    const double trimMaximum) noexcept
{
    const double sourceSpan = sourceMaximum - sourceMinimum;
    const double targetSpan = targetMaximum - targetMinimum;
    if (!(sourceSpan > 0.0) || !(targetSpan > 0.0))
    {
        return 0U;
    }

    const double visibleMinimum = std::max(sourceMinimum, trimMinimum);
    const double visibleMaximum = std::min(sourceMaximum, trimMaximum);
    if (!(visibleMaximum > visibleMinimum))
    {
        return 0U;
    }
    if (!repeat)
    {
        return 1U;
    }

    const double quotient = targetSpan / sourceSpan;
    if (!std::isfinite(quotient) ||
        quotient > static_cast<double>(MaximumSpritePrimitiveQuads))
    {
        return MaximumSpritePrimitiveQuads + 1U;
    }

    const double fullTileCountDouble = std::floor(quotient);
    std::size_t count = static_cast<std::size_t>(fullTileCountDouble);
    const double consumed = fullTileCountDouble * sourceSpan;
    const double remainder = std::max(0.0, targetSpan - consumed);
    const double visibleLocalMinimum = visibleMinimum - sourceMinimum;
    if (remainder > visibleLocalMinimum)
    {
        ++count;
    }
    return count;
}

[[nodiscard]] SpritePrimitiveStatus CountValidatedPrimitive(
    const PrimitiveValidation& validation,
    const SpritePrimitive2D& primitive,
    std::size_t& outPatchCount) noexcept
{
    outPatchCount = 0U;
    if (primitive.mode == SpritePrimitiveMode::Quad)
    {
        outPatchCount = 1U;
        return Success();
    }

    const assets::SpriteRegion& region = *validation.region;
    const double trimLeft = static_cast<double>(region.trimOffset.x);
    const double trimTop = static_cast<double>(region.trimOffset.y);
    const double trimRight = trimLeft + static_cast<double>(region.trimSize.width);
    const double trimBottom = trimTop + static_cast<double>(region.trimSize.height);

    std::array<std::size_t, 3U> xCounts{};
    std::array<std::size_t, 3U> yCounts{};
    for (std::size_t index = 0U; index < 3U; ++index)
    {
        const bool repeat = primitive.mode == SpritePrimitiveMode::Tiled && index == 1U;
        xCounts[index] = CountAxisPieces(
            repeat,
            validation.sourceX[index],
            validation.sourceX[index + 1U],
            validation.targetX[index],
            validation.targetX[index + 1U],
            trimLeft,
            trimRight);
        yCounts[index] = CountAxisPieces(
            repeat,
            validation.sourceY[index],
            validation.sourceY[index + 1U],
            validation.targetY[index],
            validation.targetY[index + 1U],
            trimTop,
            trimBottom);
    }

    std::size_t total = 0U;
    for (const std::size_t yCount : yCounts)
    {
        for (const std::size_t xCount : xCounts)
        {
            if (xCount == 0U || yCount == 0U)
            {
                continue;
            }
            if (xCount > MaximumSpritePrimitiveQuads ||
                yCount > MaximumSpritePrimitiveQuads ||
                xCount > MaximumSpritePrimitiveQuads / yCount)
            {
                outPatchCount = MaximumSpritePrimitiveQuads + 1U;
                return Failure(
                    SpritePrimitiveError::ExpansionLimit,
                    SpritePrimitiveField::PatchCount);
            }
            const std::size_t cellCount = xCount * yCount;
            if (total > MaximumSpritePrimitiveQuads - cellCount)
            {
                outPatchCount = MaximumSpritePrimitiveQuads + 1U;
                return Failure(
                    SpritePrimitiveError::ExpansionLimit,
                    SpritePrimitiveField::PatchCount);
            }
            total += cellCount;
        }
    }

    outPatchCount = total;
    return Success();
}

template <typename Callback>
[[nodiscard]] bool ForEachAxisPiece(
    const bool repeat,
    const double sourceMinimum,
    const double sourceMaximum,
    const double targetMinimum,
    const double targetMaximum,
    const double trimMinimum,
    const double trimMaximum,
    Callback&& callback) noexcept
{
    const double sourceSpan = sourceMaximum - sourceMinimum;
    const double targetSpan = targetMaximum - targetMinimum;
    if (!(sourceSpan > 0.0) || !(targetSpan > 0.0))
    {
        return true;
    }

    const double visibleMinimum = std::max(sourceMinimum, trimMinimum);
    const double visibleMaximum = std::min(sourceMaximum, trimMaximum);
    if (!(visibleMaximum > visibleMinimum))
    {
        return true;
    }

    if (!repeat)
    {
        const double targetScale = targetSpan / sourceSpan;
        const AxisPiece piece{
            targetMinimum + (visibleMinimum - sourceMinimum) * targetScale,
            targetMinimum + (visibleMaximum - sourceMinimum) * targetScale,
            visibleMinimum,
            visibleMaximum,
        };
        return callback(piece);
    }

    const double visibleLocalMinimum = visibleMinimum - sourceMinimum;
    const double visibleLocalMaximum = visibleMaximum - sourceMinimum;
    const double quotient = targetSpan / sourceSpan;
    const std::size_t fullTileCount = static_cast<std::size_t>(std::floor(quotient));
    const double fullConsumed = static_cast<double>(fullTileCount) * sourceSpan;
    const double remainder = std::max(0.0, targetSpan - fullConsumed);
    const std::size_t tileCount = fullTileCount + (remainder > 0.0 ? 1U : 0U);

    for (std::size_t tileIndex = 0U; tileIndex < tileCount; ++tileIndex)
    {
        const double tileOffset = static_cast<double>(tileIndex) * sourceSpan;
        const double available = std::min(sourceSpan, targetSpan - tileOffset);
        const double localMinimum = std::max(0.0, visibleLocalMinimum);
        const double localMaximum = std::min(available, visibleLocalMaximum);
        if (!(localMaximum > localMinimum))
        {
            continue;
        }

        const AxisPiece piece{
            targetMinimum + tileOffset + localMinimum,
            targetMinimum + tileOffset + localMaximum,
            sourceMinimum + localMinimum,
            sourceMinimum + localMaximum,
        };
        if (!callback(piece))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool BuildTargetPosition(
    const SpriteLogicalQuad& sourceLogicalQuad,
    const assets::SpriteRegion& region,
    const double targetWidth,
    const double targetHeight,
    const double targetX,
    const double targetY,
    Float2& outPosition) noexcept
{
    const double sourceWidth = static_cast<double>(region.sourceSize.width);
    const double sourceHeight = static_cast<double>(region.sourceSize.height);
    const double denominator = static_cast<double>(region.pivot.denominator);
    if (!(sourceWidth > 0.0) || !(sourceHeight > 0.0) || !(denominator > 0.0))
    {
        return false;
    }

    const double pivotX = static_cast<double>(region.pivot.xNumerator) / denominator;
    const double pivotY = static_cast<double>(region.pivot.yNumerator) / denominator;
    if (!std::isfinite(pivotX) || !std::isfinite(pivotY))
    {
        return false;
    }

    const double targetPivotX = pivotX * targetWidth / sourceWidth;
    const double targetPivotY = pivotY * targetHeight / sourceHeight;
    const double equivalentSourceX = targetX + pivotX - targetPivotX;
    const double equivalentSourceY = targetY + pivotY - targetPivotY;
    const double u = equivalentSourceX / sourceWidth;
    const double v = equivalentSourceY / sourceHeight;

    const double topLeftX = static_cast<double>(sourceLogicalQuad.topLeft.x);
    const double topLeftY = static_cast<double>(sourceLogicalQuad.topLeft.y);
    const double basisXX =
        static_cast<double>(sourceLogicalQuad.topRight.x) - topLeftX;
    const double basisXY =
        static_cast<double>(sourceLogicalQuad.topRight.y) - topLeftY;
    const double basisYX =
        static_cast<double>(sourceLogicalQuad.bottomLeft.x) - topLeftX;
    const double basisYY =
        static_cast<double>(sourceLogicalQuad.bottomLeft.y) - topLeftY;

    return TryFloat(topLeftX + basisXX * u + basisYX * v, outPosition.x) &&
        TryFloat(topLeftY + basisXY * u + basisYY * v, outPosition.y);
}

[[nodiscard]] bool MapSourceToPackedPixel(
    const assets::SpriteRegion& region,
    const double sourceX,
    const double sourceY,
    double& outPackedX,
    double& outPackedY) noexcept
{
    const double localX = sourceX - static_cast<double>(region.trimOffset.x);
    const double localY = sourceY - static_cast<double>(region.trimOffset.y);
    const double trimWidth = static_cast<double>(region.trimSize.width);
    const double trimHeight = static_cast<double>(region.trimSize.height);
    if (!std::isfinite(localX) || !std::isfinite(localY) ||
        localX < 0.0 || localX > trimWidth || localY < 0.0 || localY > trimHeight)
    {
        return false;
    }

    switch (region.packedRotation)
    {
    case assets::SpritePackedRotation::None:
        outPackedX = static_cast<double>(region.packedRect.x) + localX;
        outPackedY = static_cast<double>(region.packedRect.y) + localY;
        return true;
    case assets::SpritePackedRotation::Cw90:
        outPackedX = static_cast<double>(region.packedRect.x) + trimHeight - localY;
        outPackedY = static_cast<double>(region.packedRect.y) + localX;
        return true;
    default:
        return false;
    }
}

void ResolveAtlasSafeSampleRange(
    const double minimum,
    const double maximum,
    double& outMinimum,
    double& outMaximum) noexcept
{
    const double extent = maximum - minimum;
    if (extent >= 1.0)
    {
        outMinimum = minimum + 0.5;
        outMaximum = maximum - 0.5;
        return;
    }

    // A linear-filter footprint is one texel wide. When a geometrically clipped tile leaves a
    // source interval narrower than one texel, no sample position can keep the whole footprint
    // inside that interval. Collapse the clamp to the nearest actual texel center represented by
    // the interval instead of its geometric midpoint; the latter still blends atlas neighbors.
    const double midpoint = (minimum + maximum) * 0.5;
    const double texelCenter = std::ceil(midpoint) - 0.5;
    outMinimum = texelCenter;
    outMaximum = texelCenter;
}

[[nodiscard]] bool BuildPatchUvAndBounds(
    const assets::SpriteAtlasPage& page,
    const assets::SpriteRegion& region,
    const double sourceLeft,
    const double sourceTop,
    const double sourceRight,
    const double sourceBottom,
    SpriteDrawQuad& outQuad,
    SpriteSampleBounds& outBounds) noexcept
{
    std::array<double, 4U> packedX{};
    std::array<double, 4U> packedY{};
    const std::array<double, 4U> sourceX{sourceLeft, sourceRight, sourceRight, sourceLeft};
    const std::array<double, 4U> sourceY{sourceTop, sourceTop, sourceBottom, sourceBottom};
    for (std::size_t index = 0U; index < 4U; ++index)
    {
        if (!MapSourceToPackedPixel(
                region,
                sourceX[index],
                sourceY[index],
                packedX[index],
                packedY[index]))
        {
            return false;
        }
    }

    const double pageWidth = static_cast<double>(page.size.width);
    const double pageHeight = static_cast<double>(page.size.height);
    if (!(pageWidth > 0.0) || !(pageHeight > 0.0))
    {
        return false;
    }

    std::array<Float2*, 4U> uvs{
        &outQuad.topLeft.uv,
        &outQuad.topRight.uv,
        &outQuad.bottomRight.uv,
        &outQuad.bottomLeft.uv,
    };
    for (std::size_t index = 0U; index < 4U; ++index)
    {
        if (!TryFloat(packedX[index] / pageWidth, uvs[index]->x) ||
            !TryFloat(packedY[index] / pageHeight, uvs[index]->y))
        {
            return false;
        }
    }

    const auto [minimumX, maximumX] = std::minmax_element(packedX.begin(), packedX.end());
    const auto [minimumY, maximumY] = std::minmax_element(packedY.begin(), packedY.end());
    const double extentX = *maximumX - *minimumX;
    const double extentY = *maximumY - *minimumY;
    if (!(extentX > 0.0) || !(extentY > 0.0))
    {
        return false;
    }

    double sampleMinimumX = 0.0;
    double sampleMaximumX = 0.0;
    double sampleMinimumY = 0.0;
    double sampleMaximumY = 0.0;
    ResolveAtlasSafeSampleRange(*minimumX, *maximumX, sampleMinimumX, sampleMaximumX);
    ResolveAtlasSafeSampleRange(*minimumY, *maximumY, sampleMinimumY, sampleMaximumY);
    return TryFloat(sampleMinimumX / pageWidth, outBounds.minimum.x) &&
        TryFloat(sampleMinimumY / pageHeight, outBounds.minimum.y) &&
        TryFloat(sampleMaximumX / pageWidth, outBounds.maximum.x) &&
        TryFloat(sampleMaximumY / pageHeight, outBounds.maximum.y);
}

[[nodiscard]] SpritePrimitiveStatus BuildPatch(
    const SpriteLogicalQuad& sourceLogicalQuad,
    const PrimitiveValidation& validation,
    const double targetWidth,
    const double targetHeight,
    const AxisPiece& xPiece,
    const AxisPiece& yPiece,
    SpritePrimitivePatch2D& outPatch) noexcept
{
    outPatch = SpritePrimitivePatch2D{};
    const assets::SpriteRegion& region = *validation.region;
    if (!BuildTargetPosition(
            sourceLogicalQuad,
            region,
            targetWidth,
            targetHeight,
            xPiece.targetMinimum,
            yPiece.targetMinimum,
            outPatch.quad.topLeft.position) ||
        !BuildTargetPosition(
            sourceLogicalQuad,
            region,
            targetWidth,
            targetHeight,
            xPiece.targetMaximum,
            yPiece.targetMinimum,
            outPatch.quad.topRight.position) ||
        !BuildTargetPosition(
            sourceLogicalQuad,
            region,
            targetWidth,
            targetHeight,
            xPiece.targetMaximum,
            yPiece.targetMaximum,
            outPatch.quad.bottomRight.position) ||
        !BuildTargetPosition(
            sourceLogicalQuad,
            region,
            targetWidth,
            targetHeight,
            xPiece.targetMinimum,
            yPiece.targetMaximum,
            outPatch.quad.bottomLeft.position))
    {
        outPatch = SpritePrimitivePatch2D{};
        return Failure(
            SpritePrimitiveError::Geometry,
            SpritePrimitiveField::Geometry);
    }

    if (!BuildPatchUvAndBounds(
            *validation.page,
            region,
            xPiece.sourceMinimum,
            yPiece.sourceMinimum,
            xPiece.sourceMaximum,
            yPiece.sourceMaximum,
            outPatch.quad,
            outPatch.sampleBounds))
    {
        outPatch = SpritePrimitivePatch2D{};
        return Failure(
            SpritePrimitiveError::UvOverflow,
            SpritePrimitiveField::Uv);
    }
    return Success();
}
} // namespace

std::string_view ToString(const SpritePrimitiveMode value) noexcept
{
    switch (value)
    {
    case SpritePrimitiveMode::Quad: return "quad";
    case SpritePrimitiveMode::Sliced: return "sliced";
    case SpritePrimitiveMode::Tiled: return "tiled";
    }
    return "unknown";
}

std::string_view ToString(const SpritePrimitiveError value) noexcept
{
    switch (value)
    {
    case SpritePrimitiveError::None: return "none";
    case SpritePrimitiveError::UnresolvedSelection: return "unresolved_selection";
    case SpritePrimitiveError::InvalidSourceSize: return "invalid_source_size";
    case SpritePrimitiveError::InvalidBorder: return "invalid_border";
    case SpritePrimitiveError::InvalidTargetSize: return "invalid_target_size";
    case SpritePrimitiveError::InvalidPageSize: return "invalid_page_size";
    case SpritePrimitiveError::InvalidTrimRect: return "invalid_trim_rect";
    case SpritePrimitiveError::InvalidPackedRect: return "invalid_packed_rect";
    case SpritePrimitiveError::PackedExtentMismatch: return "packed_extent_mismatch";
    case SpritePrimitiveError::UnsupportedPackedRotation: return "unsupported_packed_rotation";
    case SpritePrimitiveError::UnsupportedMode: return "unsupported_mode";
    case SpritePrimitiveError::CountOverflow: return "count_overflow";
    case SpritePrimitiveError::ExpansionLimit: return "expansion_limit";
    case SpritePrimitiveError::InsufficientCapacity: return "insufficient_capacity";
    case SpritePrimitiveError::Geometry: return "geometry";
    case SpritePrimitiveError::UvOverflow: return "uv_overflow";
    case SpritePrimitiveError::SampleBoundsOverflow: return "sample_bounds_overflow";
    }
    return "unknown";
}

std::string_view ToString(const SpritePrimitiveField value) noexcept
{
    switch (value)
    {
    case SpritePrimitiveField::None: return "none";
    case SpritePrimitiveField::Selection: return "selection";
    case SpritePrimitiveField::SourceSize: return "source_size";
    case SpritePrimitiveField::Border: return "border";
    case SpritePrimitiveField::TargetSize: return "target_size";
    case SpritePrimitiveField::PageSize: return "page_size";
    case SpritePrimitiveField::TrimRect: return "trim_rect";
    case SpritePrimitiveField::PackedRect: return "packed_rect";
    case SpritePrimitiveField::PackedRotation: return "packed_rotation";
    case SpritePrimitiveField::Mode: return "mode";
    case SpritePrimitiveField::PatchCount: return "patch_count";
    case SpritePrimitiveField::OutputCapacity: return "output_capacity";
    case SpritePrimitiveField::Geometry: return "geometry";
    case SpritePrimitiveField::Uv: return "uv";
    case SpritePrimitiveField::SampleBounds: return "sample_bounds";
    }
    return "unknown";
}

SpritePrimitiveStatus CountSpritePrimitivePatches(
    const ResolvedSpriteRegion& selection,
    const SpritePrimitive2D& primitive,
    std::size_t& outPatchCount) noexcept
{
    outPatchCount = 0U;
    PrimitiveValidation validation{};
    const SpritePrimitiveStatus validationStatus =
        ValidatePrimitive(selection, primitive, validation);
    if (!validationStatus.Succeeded())
    {
        return validationStatus;
    }
    return CountValidatedPrimitive(validation, primitive, outPatchCount);
}

SpritePrimitiveStatus BuildSpritePrimitivePatches(
    const ResolvedSpriteRegion& selection,
    const scene::SpritePose2D& pose,
    const float pixelsPerUnit,
    const SpritePrimitive2D& primitive,
    const std::span<SpritePrimitivePatch2D> output,
    std::size_t& outPatchCount) noexcept
{
    outPatchCount = 0U;
    PrimitiveValidation validation{};
    const SpritePrimitiveStatus validationStatus =
        ValidatePrimitive(selection, primitive, validation);
    if (!validationStatus.Succeeded())
    {
        return validationStatus;
    }

    std::size_t required = 0U;
    const SpritePrimitiveStatus countStatus =
        CountValidatedPrimitive(validation, primitive, required);
    outPatchCount = required;
    if (!countStatus.Succeeded())
    {
        return countStatus;
    }
    if (output.size() < required)
    {
        return Failure(
            SpritePrimitiveError::InsufficientCapacity,
            SpritePrimitiveField::OutputCapacity);
    }
    if (required == 0U)
    {
        return Success();
    }

    if (primitive.mode == SpritePrimitiveMode::Quad)
    {
        SpriteDrawQuad quad{};
        const SpriteGeometryStatus geometryStatus =
            BuildSpriteDrawQuad(selection, pose, pixelsPerUnit, quad);
        if (!geometryStatus.Succeeded())
        {
            outPatchCount = 0U;
            return GeometryFailure(geometryStatus);
        }

        SpritePrimitivePatch2D patch{};
        patch.quad = quad;
        const assets::SpriteRegion& region = *validation.region;
        const double sourceLeft = static_cast<double>(region.trimOffset.x);
        const double sourceTop = static_cast<double>(region.trimOffset.y);
        const double sourceRight = sourceLeft + static_cast<double>(region.trimSize.width);
        const double sourceBottom = sourceTop + static_cast<double>(region.trimSize.height);
        SpriteDrawQuad uvOnly{};
        if (!BuildPatchUvAndBounds(
                *validation.page,
                region,
                sourceLeft,
                sourceTop,
                sourceRight,
                sourceBottom,
                uvOnly,
                patch.sampleBounds))
        {
            outPatchCount = 0U;
            return Failure(
                SpritePrimitiveError::SampleBoundsOverflow,
                SpritePrimitiveField::SampleBounds);
        }
        output[0] = patch;
        outPatchCount = 1U;
        return Success();
    }

    SpriteLogicalQuad sourceLogicalQuad{};
    const SpriteGeometryStatus geometryStatus =
        BuildSpriteLogicalQuad(selection, pose, pixelsPerUnit, sourceLogicalQuad);
    if (!geometryStatus.Succeeded())
    {
        outPatchCount = 0U;
        return GeometryFailure(geometryStatus);
    }

    const assets::SpriteRegion& region = *validation.region;
    const double trimLeft = static_cast<double>(region.trimOffset.x);
    const double trimTop = static_cast<double>(region.trimOffset.y);
    const double trimRight = trimLeft + static_cast<double>(region.trimSize.width);
    const double trimBottom = trimTop + static_cast<double>(region.trimSize.height);
    const double targetWidth = static_cast<double>(primitive.targetSizeSourcePixels.x);
    const double targetHeight = static_cast<double>(primitive.targetSizeSourcePixels.y);

    std::size_t outputIndex = 0U;
    for (std::size_t row = 0U; row < 3U; ++row)
    {
        const bool repeatY = primitive.mode == SpritePrimitiveMode::Tiled && row == 1U;
        for (std::size_t column = 0U; column < 3U; ++column)
        {
            const bool repeatX = primitive.mode == SpritePrimitiveMode::Tiled && column == 1U;
            SpritePrimitiveStatus patchStatus = Success();
            const bool completedY = ForEachAxisPiece(
                repeatY,
                validation.sourceY[row],
                validation.sourceY[row + 1U],
                validation.targetY[row],
                validation.targetY[row + 1U],
                trimTop,
                trimBottom,
                [&](const AxisPiece& yPiece) noexcept
                {
                    return ForEachAxisPiece(
                        repeatX,
                        validation.sourceX[column],
                        validation.sourceX[column + 1U],
                        validation.targetX[column],
                        validation.targetX[column + 1U],
                        trimLeft,
                        trimRight,
                        [&](const AxisPiece& xPiece) noexcept
                        {
                            if (outputIndex >= required)
                            {
                                patchStatus = Failure(
                                    SpritePrimitiveError::CountOverflow,
                                    SpritePrimitiveField::PatchCount);
                                return false;
                            }
                            patchStatus = BuildPatch(
                                sourceLogicalQuad,
                                validation,
                                targetWidth,
                                targetHeight,
                                xPiece,
                                yPiece,
                                output[outputIndex]);
                            if (!patchStatus.Succeeded())
                            {
                                return false;
                            }
                            ++outputIndex;
                            return true;
                        });
                });
            if (!completedY || !patchStatus.Succeeded())
            {
                outPatchCount = 0U;
                return patchStatus.Succeeded()
                    ? Failure(SpritePrimitiveError::CountOverflow, SpritePrimitiveField::PatchCount)
                    : patchStatus;
            }
        }
    }

    if (outputIndex != required)
    {
        outPatchCount = 0U;
        return Failure(
            SpritePrimitiveError::CountOverflow,
            SpritePrimitiveField::PatchCount);
    }

    outPatchCount = outputIndex;
    return Success();
}
} // namespace trace2d::render
