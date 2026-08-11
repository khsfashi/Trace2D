#include <trace2d/render/SpriteGeometry2D.hpp>

#include <cmath>
#include <limits>

namespace trace2d::render
{
namespace
{
[[nodiscard]] SpriteGeometryStatus Success() noexcept
{
    return SpriteGeometryStatus{};
}

[[nodiscard]] SpriteGeometryStatus Failure(
    const SpriteGeometryError error,
    const SpriteGeometryField field) noexcept
{
    return SpriteGeometryStatus{error, field};
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

[[nodiscard]] bool FitsExtent(
    const std::uint32_t offset,
    const std::uint32_t extent,
    const std::uint32_t limit) noexcept
{
    return static_cast<std::uint64_t>(offset) + static_cast<std::uint64_t>(extent) <=
        static_cast<std::uint64_t>(limit);
}

struct GeometryContext final
{
    double pivotX{0.0};
    double pivotY{0.0};
    double inversePixelsPerUnit{1.0};
    double scaleX{1.0};
    double scaleY{1.0};
    double cosine{1.0};
    double sine{0.0};
    double positionX{0.0};
    double positionY{0.0};
};

[[nodiscard]] bool BuildCorner(
    const GeometryContext& context,
    const double sourceX,
    const double sourceY,
    Float2& outCorner) noexcept
{
    double localX = (sourceX - context.pivotX) * context.inversePixelsPerUnit;
    double localY = -(sourceY - context.pivotY) * context.inversePixelsPerUnit;

    localX *= context.scaleX;
    localY *= context.scaleY;

    const double rotatedX = localX * context.cosine - localY * context.sine;
    const double rotatedY = localX * context.sine + localY * context.cosine;
    return TryFloat(rotatedX + context.positionX, outCorner.x) &&
        TryFloat(rotatedY + context.positionY, outCorner.y);
}

[[nodiscard]] SpriteGeometryStatus BuildGeometryContext(
    const ResolvedSpriteRegion& selection,
    const scene::SpritePose2D& pose,
    const float pixelsPerUnit,
    const assets::SpriteRegion*& outRegion,
    GeometryContext& outContext) noexcept
{
    outRegion = nullptr;
    outContext = GeometryContext{};

    if (!selection.Valid() || selection.Region() == nullptr)
    {
        return Failure(SpriteGeometryError::UnresolvedSelection, SpriteGeometryField::Selection);
    }
    if (!scene::ValidateSpritePose(pose).Succeeded())
    {
        return Failure(SpriteGeometryError::InvalidPose, SpriteGeometryField::Pose);
    }
    if (!std::isfinite(pixelsPerUnit) || pixelsPerUnit <= 0.0F)
    {
        return Failure(SpriteGeometryError::InvalidPixelsPerUnit, SpriteGeometryField::PixelsPerUnit);
    }

    const assets::SpriteRegion& region = *selection.Region();
    if (region.sourceSize.width == 0U || region.sourceSize.height == 0U)
    {
        return Failure(SpriteGeometryError::InvalidSourceSize, SpriteGeometryField::SourceSize);
    }
    if (region.pivot.denominator <= 0)
    {
        return Failure(SpriteGeometryError::InvalidPivot, SpriteGeometryField::Pivot);
    }

    const double denominator = static_cast<double>(region.pivot.denominator);
    const double pivotX = static_cast<double>(region.pivot.xNumerator) / denominator;
    const double pivotY = static_cast<double>(region.pivot.yNumerator) / denominator;
    if (!std::isfinite(pivotX) || !std::isfinite(pivotY))
    {
        return Failure(SpriteGeometryError::InvalidPivot, SpriteGeometryField::Pivot);
    }

    const double flipX = pose.flipX ? -1.0 : 1.0;
    const double flipY = pose.flipY ? -1.0 : 1.0;
    const double rotation = static_cast<double>(pose.transform.rotationRadians);
    outContext.pivotX = pivotX;
    outContext.pivotY = pivotY;
    outContext.inversePixelsPerUnit = 1.0 / static_cast<double>(pixelsPerUnit);
    outContext.scaleX = static_cast<double>(pose.transform.scale.x) * flipX;
    outContext.scaleY = static_cast<double>(pose.transform.scale.y) * flipY;
    outContext.cosine = std::cos(rotation);
    outContext.sine = std::sin(rotation);
    outContext.positionX = static_cast<double>(pose.transform.position.x);
    outContext.positionY = static_cast<double>(pose.transform.position.y);

    if (!std::isfinite(outContext.inversePixelsPerUnit) ||
        !std::isfinite(outContext.scaleX) || !std::isfinite(outContext.scaleY) ||
        !std::isfinite(outContext.cosine) || !std::isfinite(outContext.sine))
    {
        return Failure(SpriteGeometryError::GeometryOverflow, SpriteGeometryField::LogicalQuad);
    }

    outRegion = &region;
    return Success();
}

[[nodiscard]] SpriteGeometryStatus ValidateDrawStorage(
    const assets::SpriteAtlasPage& page,
    const assets::SpriteRegion& region) noexcept
{
    if (page.size.width == 0U || page.size.height == 0U)
    {
        return Failure(SpriteGeometryError::InvalidPageSize, SpriteGeometryField::PageSize);
    }
    if (region.trimSize.width == 0U || region.trimSize.height == 0U ||
        !FitsExtent(region.trimOffset.x, region.trimSize.width, region.sourceSize.width) ||
        !FitsExtent(region.trimOffset.y, region.trimSize.height, region.sourceSize.height))
    {
        return Failure(SpriteGeometryError::InvalidTrimRect, SpriteGeometryField::TrimRect);
    }
    if (region.packedRect.width == 0U || region.packedRect.height == 0U ||
        !FitsExtent(region.packedRect.x, region.packedRect.width, page.size.width) ||
        !FitsExtent(region.packedRect.y, region.packedRect.height, page.size.height))
    {
        return Failure(SpriteGeometryError::InvalidPackedRect, SpriteGeometryField::PackedRect);
    }

    switch (region.packedRotation)
    {
    case assets::SpritePackedRotation::None:
        if (region.packedRect.width != region.trimSize.width ||
            region.packedRect.height != region.trimSize.height)
        {
            return Failure(
                SpriteGeometryError::PackedExtentMismatch,
                SpriteGeometryField::PackedRect);
        }
        break;
    case assets::SpritePackedRotation::Cw90:
        if (region.packedRect.width != region.trimSize.height ||
            region.packedRect.height != region.trimSize.width)
        {
            return Failure(
                SpriteGeometryError::PackedExtentMismatch,
                SpriteGeometryField::PackedRect);
        }
        break;
    default:
        return Failure(
            SpriteGeometryError::UnsupportedPackedRotation,
            SpriteGeometryField::PackedRotation);
    }

    return Success();
}

[[nodiscard]] bool BuildPackedUvCorners(
    const assets::SpriteAtlasPage& page,
    const assets::SpriteRegion& region,
    Float2& packedTopLeft,
    Float2& packedTopRight,
    Float2& packedBottomRight,
    Float2& packedBottomLeft) noexcept
{
    const double pageWidth = static_cast<double>(page.size.width);
    const double pageHeight = static_cast<double>(page.size.height);
    const double u0 = static_cast<double>(region.packedRect.x) / pageWidth;
    const double v0 = static_cast<double>(region.packedRect.y) / pageHeight;
    const double u1 =
        (static_cast<double>(region.packedRect.x) + static_cast<double>(region.packedRect.width)) /
        pageWidth;
    const double v1 =
        (static_cast<double>(region.packedRect.y) + static_cast<double>(region.packedRect.height)) /
        pageHeight;

    return TryFloat(u0, packedTopLeft.x) && TryFloat(v0, packedTopLeft.y) &&
        TryFloat(u1, packedTopRight.x) && TryFloat(v0, packedTopRight.y) &&
        TryFloat(u1, packedBottomRight.x) && TryFloat(v1, packedBottomRight.y) &&
        TryFloat(u0, packedBottomLeft.x) && TryFloat(v1, packedBottomLeft.y);
}
} // namespace

std::string_view ToString(const SpriteGeometryError value) noexcept
{
    switch (value)
    {
    case SpriteGeometryError::None: return "none";
    case SpriteGeometryError::UnresolvedSelection: return "unresolved_selection";
    case SpriteGeometryError::InvalidPose: return "invalid_pose";
    case SpriteGeometryError::InvalidPixelsPerUnit: return "invalid_pixels_per_unit";
    case SpriteGeometryError::InvalidSourceSize: return "invalid_source_size";
    case SpriteGeometryError::InvalidPivot: return "invalid_pivot";
    case SpriteGeometryError::InvalidPageSize: return "invalid_page_size";
    case SpriteGeometryError::InvalidTrimRect: return "invalid_trim_rect";
    case SpriteGeometryError::InvalidPackedRect: return "invalid_packed_rect";
    case SpriteGeometryError::PackedExtentMismatch: return "packed_extent_mismatch";
    case SpriteGeometryError::UnsupportedPackedRotation: return "unsupported_packed_rotation";
    case SpriteGeometryError::GeometryOverflow: return "geometry_overflow";
    case SpriteGeometryError::UvOverflow: return "uv_overflow";
    }
    return "unknown";
}

std::string_view ToString(const SpriteGeometryField value) noexcept
{
    switch (value)
    {
    case SpriteGeometryField::None: return "none";
    case SpriteGeometryField::Selection: return "selection";
    case SpriteGeometryField::Pose: return "pose";
    case SpriteGeometryField::PixelsPerUnit: return "pixels_per_unit";
    case SpriteGeometryField::SourceSize: return "source_size";
    case SpriteGeometryField::Pivot: return "pivot";
    case SpriteGeometryField::PageSize: return "page_size";
    case SpriteGeometryField::TrimRect: return "trim_rect";
    case SpriteGeometryField::PackedRect: return "packed_rect";
    case SpriteGeometryField::PackedRotation: return "packed_rotation";
    case SpriteGeometryField::LogicalQuad: return "logical_quad";
    case SpriteGeometryField::DrawQuad: return "draw_quad";
    case SpriteGeometryField::Uv: return "uv";
    }
    return "unknown";
}

SpriteGeometryStatus BuildSpriteLogicalQuad(
    const ResolvedSpriteRegion& selection,
    const scene::SpritePose2D& pose,
    const float pixelsPerUnit,
    SpriteLogicalQuad& outQuad) noexcept
{
    outQuad = SpriteLogicalQuad{};

    const assets::SpriteRegion* region = nullptr;
    GeometryContext context{};
    const SpriteGeometryStatus contextStatus =
        BuildGeometryContext(selection, pose, pixelsPerUnit, region, context);
    if (!contextStatus.Succeeded())
    {
        return contextStatus;
    }

    const double width = static_cast<double>(region->sourceSize.width);
    const double height = static_cast<double>(region->sourceSize.height);
    if (!BuildCorner(context, 0.0, 0.0, outQuad.topLeft) ||
        !BuildCorner(context, width, 0.0, outQuad.topRight) ||
        !BuildCorner(context, width, height, outQuad.bottomRight) ||
        !BuildCorner(context, 0.0, height, outQuad.bottomLeft))
    {
        outQuad = SpriteLogicalQuad{};
        return Failure(SpriteGeometryError::GeometryOverflow, SpriteGeometryField::LogicalQuad);
    }

    return Success();
}

SpriteGeometryStatus BuildSpriteDrawQuad(
    const ResolvedSpriteRegion& selection,
    const scene::SpritePose2D& pose,
    const float pixelsPerUnit,
    SpriteDrawQuad& outQuad) noexcept
{
    outQuad = SpriteDrawQuad{};

    const assets::SpriteRegion* region = nullptr;
    GeometryContext context{};
    const SpriteGeometryStatus contextStatus =
        BuildGeometryContext(selection, pose, pixelsPerUnit, region, context);
    if (!contextStatus.Succeeded())
    {
        return contextStatus;
    }

    const assets::SpriteAtlasPage* page = selection.Page();
    if (page == nullptr)
    {
        return Failure(SpriteGeometryError::UnresolvedSelection, SpriteGeometryField::Selection);
    }

    const SpriteGeometryStatus storageStatus = ValidateDrawStorage(*page, *region);
    if (!storageStatus.Succeeded())
    {
        return storageStatus;
    }

    const double left = static_cast<double>(region->trimOffset.x);
    const double top = static_cast<double>(region->trimOffset.y);
    const double right = left + static_cast<double>(region->trimSize.width);
    const double bottom = top + static_cast<double>(region->trimSize.height);

    if (!BuildCorner(context, left, top, outQuad.topLeft.position) ||
        !BuildCorner(context, right, top, outQuad.topRight.position) ||
        !BuildCorner(context, right, bottom, outQuad.bottomRight.position) ||
        !BuildCorner(context, left, bottom, outQuad.bottomLeft.position))
    {
        outQuad = SpriteDrawQuad{};
        return Failure(SpriteGeometryError::GeometryOverflow, SpriteGeometryField::DrawQuad);
    }

    Float2 packedTopLeft{};
    Float2 packedTopRight{};
    Float2 packedBottomRight{};
    Float2 packedBottomLeft{};
    if (!BuildPackedUvCorners(
            *page,
            *region,
            packedTopLeft,
            packedTopRight,
            packedBottomRight,
            packedBottomLeft))
    {
        outQuad = SpriteDrawQuad{};
        return Failure(SpriteGeometryError::UvOverflow, SpriteGeometryField::Uv);
    }

    switch (region->packedRotation)
    {
    case assets::SpritePackedRotation::None:
        outQuad.topLeft.uv = packedTopLeft;
        outQuad.topRight.uv = packedTopRight;
        outQuad.bottomRight.uv = packedBottomRight;
        outQuad.bottomLeft.uv = packedBottomLeft;
        break;
    case assets::SpritePackedRotation::Cw90:
        outQuad.topLeft.uv = packedTopRight;
        outQuad.topRight.uv = packedBottomRight;
        outQuad.bottomRight.uv = packedBottomLeft;
        outQuad.bottomLeft.uv = packedTopLeft;
        break;
    default:
        outQuad = SpriteDrawQuad{};
        return Failure(
            SpriteGeometryError::UnsupportedPackedRotation,
            SpriteGeometryField::PackedRotation);
    }

    return Success();
}
} // namespace trace2d::render
