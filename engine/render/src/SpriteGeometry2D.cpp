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
    case SpriteGeometryError::GeometryOverflow: return "geometry_overflow";
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
    case SpriteGeometryField::LogicalQuad: return "logical_quad";
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
    GeometryContext context{};
    context.pivotX = pivotX;
    context.pivotY = pivotY;
    context.inversePixelsPerUnit = 1.0 / static_cast<double>(pixelsPerUnit);
    context.scaleX = static_cast<double>(pose.transform.scale.x) * flipX;
    context.scaleY = static_cast<double>(pose.transform.scale.y) * flipY;
    context.cosine = std::cos(rotation);
    context.sine = std::sin(rotation);
    context.positionX = static_cast<double>(pose.transform.position.x);
    context.positionY = static_cast<double>(pose.transform.position.y);

    if (!std::isfinite(context.inversePixelsPerUnit) ||
        !std::isfinite(context.scaleX) || !std::isfinite(context.scaleY) ||
        !std::isfinite(context.cosine) || !std::isfinite(context.sine))
    {
        return Failure(SpriteGeometryError::GeometryOverflow, SpriteGeometryField::LogicalQuad);
    }

    const double width = static_cast<double>(region.sourceSize.width);
    const double height = static_cast<double>(region.sourceSize.height);
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
} // namespace trace2d::render
