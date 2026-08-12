#include <trace2d/render/SpritePixelPerfect2D.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace trace2d::render
{
namespace
{
[[nodiscard]] SpritePixelPerfectStatus Success() noexcept
{
    return SpritePixelPerfectStatus{};
}

[[nodiscard]] SpritePixelPerfectStatus Failure(
    const SpritePixelPerfectError error,
    const SpritePixelPerfectField field) noexcept
{
    return SpritePixelPerfectStatus{error, field};
}

[[nodiscard]] bool IsFinite(const Float2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
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

[[nodiscard]] bool NearlyEqual(
    const double left,
    const double right,
    const double absoluteEpsilon = 1.0e-4) noexcept
{
    const double scale = std::max({1.0, std::abs(left), std::abs(right)});
    return std::abs(left - right) <= absoluteEpsilon * scale;
}

[[nodiscard]] bool TryWorldToLogicalPixel(
    const SpritePixelPerfectViewport2D& viewport,
    const Float2 world,
    Float2& outLogical) noexcept
{
    const OrthographicView& view = viewport.logicalView;
    const double halfWidth = static_cast<double>(view.halfExtents.x);
    const double halfHeight = static_cast<double>(view.halfExtents.y);
    if (!std::isfinite(halfWidth) || !std::isfinite(halfHeight) ||
        halfWidth <= 0.0 || halfHeight <= 0.0)
    {
        return false;
    }

    const double left = static_cast<double>(view.center.x) - halfWidth;
    const double top = static_cast<double>(view.center.y) + halfHeight;
    const double pixelsPerWorldX =
        static_cast<double>(viewport.logicalWidth) / (2.0 * halfWidth);
    const double pixelsPerWorldY =
        static_cast<double>(viewport.logicalHeight) / (2.0 * halfHeight);
    const double logicalX = (static_cast<double>(world.x) - left) * pixelsPerWorldX;
    const double logicalY = (top - static_cast<double>(world.y)) * pixelsPerWorldY;

    return TryFloat(logicalX, outLogical.x) && TryFloat(logicalY, outLogical.y);
}

enum class LogicalAxis : std::uint8_t
{
    Invalid = 0,
    X,
    Y,
};

struct IntegerBasis final
{
    LogicalAxis axis{LogicalAxis::Invalid};
    std::uint32_t magnitude{0U};
};

[[nodiscard]] IntegerBasis ResolveIntegerBasis(const Float2 basis) noexcept
{
    if (!IsFinite(basis))
    {
        return {};
    }

    const bool xZero = NearlyEqual(static_cast<double>(basis.x), 0.0);
    const bool yZero = NearlyEqual(static_cast<double>(basis.y), 0.0);
    if (xZero == yZero)
    {
        return {};
    }

    const double component = xZero
        ? static_cast<double>(basis.y)
        : static_cast<double>(basis.x);
    const double rounded = std::round(component);
    if (!NearlyEqual(component, rounded) || std::abs(rounded) < 1.0 ||
        std::abs(rounded) > static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
    {
        return {};
    }

    return IntegerBasis{
        xZero ? LogicalAxis::Y : LogicalAxis::X,
        static_cast<std::uint32_t>(std::abs(rounded)),
    };
}

[[nodiscard]] SpritePixelPerfectStatus MapGeometryFailure(
    const SpriteGeometryStatus geometryStatus) noexcept
{
    switch (geometryStatus.error)
    {
    case SpriteGeometryError::InvalidPose:
        return Failure(SpritePixelPerfectError::InvalidPose, SpritePixelPerfectField::Pose);
    case SpriteGeometryError::InvalidPixelsPerUnit:
        return Failure(
            SpritePixelPerfectError::InvalidPixelsPerUnit,
            SpritePixelPerfectField::PixelsPerUnit);
    case SpriteGeometryError::GeometryOverflow:
        return Failure(
            SpritePixelPerfectError::MappingOverflow,
            SpritePixelPerfectField::SourcePixelBasis);
    case SpriteGeometryError::None:
        break;
    default:
        return Failure(
            SpritePixelPerfectError::InvalidSourcePixelGrid,
            SpritePixelPerfectField::SourcePixelBasis);
    }

    return Failure(
        SpritePixelPerfectError::InvalidSourcePixelGrid,
        SpritePixelPerfectField::SourcePixelBasis);
}
} // namespace

std::string_view ToString(const SpritePixelPerfectError value) noexcept
{
    switch (value)
    {
    case SpritePixelPerfectError::None: return "none";
    case SpritePixelPerfectError::InvalidLogicalSize: return "invalid_logical_size";
    case SpritePixelPerfectError::InvalidTargetSize: return "invalid_target_size";
    case SpritePixelPerfectError::TargetTooSmall: return "target_too_small";
    case SpritePixelPerfectError::InvalidCamera: return "invalid_camera";
    case SpritePixelPerfectError::InvalidViewport: return "invalid_viewport";
    case SpritePixelPerfectError::InvalidPose: return "invalid_pose";
    case SpritePixelPerfectError::InvalidPixelsPerUnit: return "invalid_pixels_per_unit";
    case SpritePixelPerfectError::InvalidSourcePixelGrid: return "invalid_source_pixel_grid";
    case SpritePixelPerfectError::MappingOverflow: return "mapping_overflow";
    }
    return "unknown";
}

std::string_view ToString(const SpritePixelPerfectField value) noexcept
{
    switch (value)
    {
    case SpritePixelPerfectField::None: return "none";
    case SpritePixelPerfectField::LogicalViewport: return "logical_viewport";
    case SpritePixelPerfectField::Target: return "target";
    case SpritePixelPerfectField::Camera: return "camera";
    case SpritePixelPerfectField::View: return "view";
    case SpritePixelPerfectField::Pose: return "pose";
    case SpritePixelPerfectField::PixelsPerUnit: return "pixels_per_unit";
    case SpritePixelPerfectField::SourcePixelBasis: return "source_pixel_basis";
    case SpritePixelPerfectField::SourceOrigin: return "source_origin";
    }
    return "unknown";
}

std::string_view ToString(const SpritePresentationTimeMode value) noexcept
{
    switch (value)
    {
    case SpritePresentationTimeMode::AuthoritativeCurrent: return "authoritative_current";
    case SpritePresentationTimeMode::Interpolated: return "interpolated";
    }
    return "unknown";
}

SpritePixelPerfectStatus BuildSpritePixelPerfectViewport(
    const OrthographicCamera& camera,
    const std::uint32_t logicalWidth,
    const std::uint32_t logicalHeight,
    const std::uint32_t targetWidth,
    const std::uint32_t targetHeight,
    SpritePixelPerfectViewport2D& outViewport) noexcept
{
    outViewport = SpritePixelPerfectViewport2D{};

    if (logicalWidth == 0U || logicalHeight == 0U)
    {
        return Failure(
            SpritePixelPerfectError::InvalidLogicalSize,
            SpritePixelPerfectField::LogicalViewport);
    }
    if (targetWidth == 0U || targetHeight == 0U ||
        targetWidth > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
        targetHeight > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
    {
        return Failure(SpritePixelPerfectError::InvalidTargetSize, SpritePixelPerfectField::Target);
    }

    const std::uint32_t scale =
        std::min(targetWidth / logicalWidth, targetHeight / logicalHeight);
    if (scale == 0U)
    {
        return Failure(SpritePixelPerfectError::TargetTooSmall, SpritePixelPerfectField::Target);
    }

    const std::uint64_t contentWidth64 =
        static_cast<std::uint64_t>(logicalWidth) * static_cast<std::uint64_t>(scale);
    const std::uint64_t contentHeight64 =
        static_cast<std::uint64_t>(logicalHeight) * static_cast<std::uint64_t>(scale);
    if (contentWidth64 > targetWidth || contentHeight64 > targetHeight)
    {
        return Failure(SpritePixelPerfectError::MappingOverflow, SpritePixelPerfectField::Target);
    }

    OrthographicView logicalView{};
    if (!TryBuildOrthographicView(camera, logicalWidth, logicalHeight, logicalView))
    {
        return Failure(SpritePixelPerfectError::InvalidCamera, SpritePixelPerfectField::Camera);
    }

    const std::uint32_t contentWidth = static_cast<std::uint32_t>(contentWidth64);
    const std::uint32_t contentHeight = static_cast<std::uint32_t>(contentHeight64);
    outViewport.logicalWidth = logicalWidth;
    outViewport.logicalHeight = logicalHeight;
    outViewport.targetWidth = targetWidth;
    outViewport.targetHeight = targetHeight;
    outViewport.integerScale = scale;
    outViewport.contentRect = SpritePixelRect2D{
        (targetWidth - contentWidth) / 2U,
        (targetHeight - contentHeight) / 2U,
        contentWidth,
        contentHeight,
    };
    outViewport.logicalView = logicalView;

    const SpritePixelPerfectStatus validation = ValidateSpritePixelPerfectViewport(outViewport);
    if (!validation.Succeeded())
    {
        outViewport = SpritePixelPerfectViewport2D{};
        return validation;
    }
    return Success();
}

SpritePixelPerfectStatus ValidateSpritePixelPerfectViewport(
    const SpritePixelPerfectViewport2D& viewport) noexcept
{
    if (viewport.logicalWidth == 0U || viewport.logicalHeight == 0U)
    {
        return Failure(
            SpritePixelPerfectError::InvalidLogicalSize,
            SpritePixelPerfectField::LogicalViewport);
    }
    if (viewport.targetWidth == 0U || viewport.targetHeight == 0U ||
        viewport.targetWidth > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
        viewport.targetHeight > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
    {
        return Failure(SpritePixelPerfectError::InvalidTargetSize, SpritePixelPerfectField::Target);
    }

    const std::uint32_t expectedScale = std::min(
        viewport.targetWidth / viewport.logicalWidth,
        viewport.targetHeight / viewport.logicalHeight);
    if (expectedScale == 0U)
    {
        return Failure(SpritePixelPerfectError::TargetTooSmall, SpritePixelPerfectField::Target);
    }
    if (viewport.integerScale != expectedScale)
    {
        return Failure(
            SpritePixelPerfectError::InvalidViewport,
            SpritePixelPerfectField::LogicalViewport);
    }

    const std::uint64_t expectedWidth64 =
        static_cast<std::uint64_t>(viewport.logicalWidth) * expectedScale;
    const std::uint64_t expectedHeight64 =
        static_cast<std::uint64_t>(viewport.logicalHeight) * expectedScale;
    const std::uint32_t expectedWidth = static_cast<std::uint32_t>(expectedWidth64);
    const std::uint32_t expectedHeight = static_cast<std::uint32_t>(expectedHeight64);
    const SpritePixelRect2D expectedRect{
        (viewport.targetWidth - expectedWidth) / 2U,
        (viewport.targetHeight - expectedHeight) / 2U,
        expectedWidth,
        expectedHeight,
    };
    if (viewport.contentRect != expectedRect)
    {
        return Failure(
            SpritePixelPerfectError::InvalidViewport,
            SpritePixelPerfectField::LogicalViewport);
    }

    const OrthographicView& view = viewport.logicalView;
    if (!IsFinite(view.center) || !IsFinite(view.halfExtents) || !IsFinite(view.clipScale) ||
        view.halfExtents.x <= 0.0F || view.halfExtents.y <= 0.0F ||
        view.clipScale.x <= 0.0F || view.clipScale.y <= 0.0F ||
        !NearlyEqual(
            static_cast<double>(view.halfExtents.x) * static_cast<double>(view.clipScale.x),
            1.0) ||
        !NearlyEqual(
            static_cast<double>(view.halfExtents.y) * static_cast<double>(view.clipScale.y),
            1.0))
    {
        return Failure(SpritePixelPerfectError::InvalidViewport, SpritePixelPerfectField::View);
    }

    const double logicalAspect =
        static_cast<double>(viewport.logicalWidth) / static_cast<double>(viewport.logicalHeight);
    const double viewAspect =
        static_cast<double>(view.halfExtents.x) / static_cast<double>(view.halfExtents.y);
    if (!NearlyEqual(logicalAspect, viewAspect))
    {
        return Failure(SpritePixelPerfectError::InvalidViewport, SpritePixelPerfectField::View);
    }

    return Success();
}

SpritePixelPerfectStatus ResolveSpritePixelPerfectPose(
    const ResolvedSpriteRegion& selection,
    const scene::SpritePoseHistory2D& history,
    const float pixelsPerUnit,
    const SpritePixelPerfectViewport2D& viewport,
    const SpritePixelPerfectPoseRequest& request,
    SpritePixelPerfectMapping2D& outMapping) noexcept
{
    outMapping = SpritePixelPerfectMapping2D{};

    const SpritePixelPerfectStatus viewportStatus = ValidateSpritePixelPerfectViewport(viewport);
    if (!viewportStatus.Succeeded())
    {
        return viewportStatus;
    }
    if (!std::isfinite(pixelsPerUnit) || pixelsPerUnit <= 0.0F)
    {
        return Failure(
            SpritePixelPerfectError::InvalidPixelsPerUnit,
            SpritePixelPerfectField::PixelsPerUnit);
    }

    scene::SpritePose2D presentationPose{};
    scene::SpritePoseStatus poseStatus{};
    switch (request.timeMode)
    {
    case SpritePresentationTimeMode::AuthoritativeCurrent:
        poseStatus = scene::ResolveSpriteAuthoritativeCurrent(history, presentationPose);
        break;
    case SpritePresentationTimeMode::Interpolated:
        poseStatus = scene::InterpolateSpritePose(
            history, request.interpolationAlpha, presentationPose);
        break;
    default:
        return Failure(SpritePixelPerfectError::InvalidPose, SpritePixelPerfectField::Pose);
    }
    if (!poseStatus.Succeeded())
    {
        return Failure(SpritePixelPerfectError::InvalidPose, SpritePixelPerfectField::Pose);
    }

    SpriteLogicalQuad logicalQuad{};
    const SpriteGeometryStatus geometryStatus =
        BuildSpriteLogicalQuad(selection, presentationPose, pixelsPerUnit, logicalQuad);
    if (!geometryStatus.Succeeded())
    {
        return MapGeometryFailure(geometryStatus);
    }
    if (selection.Region() == nullptr || selection.Region()->sourceSize.width == 0U ||
        selection.Region()->sourceSize.height == 0U)
    {
        return Failure(
            SpritePixelPerfectError::InvalidSourcePixelGrid,
            SpritePixelPerfectField::SourcePixelBasis);
    }

    Float2 originLogical{};
    Float2 rightLogical{};
    Float2 bottomLogical{};
    if (!TryWorldToLogicalPixel(viewport, logicalQuad.topLeft, originLogical) ||
        !TryWorldToLogicalPixel(viewport, logicalQuad.topRight, rightLogical) ||
        !TryWorldToLogicalPixel(viewport, logicalQuad.bottomLeft, bottomLogical))
    {
        return Failure(
            SpritePixelPerfectError::MappingOverflow,
            SpritePixelPerfectField::SourceOrigin);
    }

    const double sourceWidth = static_cast<double>(selection.Region()->sourceSize.width);
    const double sourceHeight = static_cast<double>(selection.Region()->sourceSize.height);
    Float2 basisX{};
    Float2 basisY{};
    if (!TryFloat(
            (static_cast<double>(rightLogical.x) - static_cast<double>(originLogical.x)) /
                sourceWidth,
            basisX.x) ||
        !TryFloat(
            (static_cast<double>(rightLogical.y) - static_cast<double>(originLogical.y)) /
                sourceWidth,
            basisX.y) ||
        !TryFloat(
            (static_cast<double>(bottomLogical.x) - static_cast<double>(originLogical.x)) /
                sourceHeight,
            basisY.x) ||
        !TryFloat(
            (static_cast<double>(bottomLogical.y) - static_cast<double>(originLogical.y)) /
                sourceHeight,
            basisY.y))
    {
        return Failure(
            SpritePixelPerfectError::MappingOverflow,
            SpritePixelPerfectField::SourcePixelBasis);
    }

    const IntegerBasis integerBasisX = ResolveIntegerBasis(basisX);
    const IntegerBasis integerBasisY = ResolveIntegerBasis(basisY);
    if (integerBasisX.axis == LogicalAxis::Invalid ||
        integerBasisY.axis == LogicalAxis::Invalid ||
        integerBasisX.axis == integerBasisY.axis)
    {
        return Failure(
            SpritePixelPerfectError::InvalidSourcePixelGrid,
            SpritePixelPerfectField::SourcePixelBasis);
    }

    const double snappedLogicalX =
        std::floor(static_cast<double>(originLogical.x) + 0.5);
    const double snappedLogicalY =
        std::floor(static_cast<double>(originLogical.y) + 0.5);
    const double deltaLogicalX = snappedLogicalX - static_cast<double>(originLogical.x);
    const double deltaLogicalY = snappedLogicalY - static_cast<double>(originLogical.y);
    const double worldPerLogicalX =
        2.0 * static_cast<double>(viewport.logicalView.halfExtents.x) /
        static_cast<double>(viewport.logicalWidth);
    const double worldPerLogicalY =
        2.0 * static_cast<double>(viewport.logicalView.halfExtents.y) /
        static_cast<double>(viewport.logicalHeight);
    const double worldDeltaX = deltaLogicalX * worldPerLogicalX;
    const double worldDeltaY = -deltaLogicalY * worldPerLogicalY;

    Float2 worldDelta{};
    float snappedPositionX = 0.0F;
    float snappedPositionY = 0.0F;
    if (!TryFloat(worldDeltaX, worldDelta.x) || !TryFloat(worldDeltaY, worldDelta.y) ||
        !TryFloat(
            static_cast<double>(presentationPose.transform.position.x) + worldDeltaX,
            snappedPositionX) ||
        !TryFloat(
            static_cast<double>(presentationPose.transform.position.y) + worldDeltaY,
            snappedPositionY))
    {
        return Failure(
            SpritePixelPerfectError::MappingOverflow,
            SpritePixelPerfectField::SourceOrigin);
    }

    presentationPose.transform.position.x = snappedPositionX;
    presentationPose.transform.position.y = snappedPositionY;

    SpriteLogicalQuad snappedLogicalQuad{};
    const SpriteGeometryStatus snappedStatus =
        BuildSpriteLogicalQuad(selection, presentationPose, pixelsPerUnit, snappedLogicalQuad);
    if (!snappedStatus.Succeeded())
    {
        return MapGeometryFailure(snappedStatus);
    }

    Float2 snappedOriginLogical{};
    if (!TryWorldToLogicalPixel(viewport, snappedLogicalQuad.topLeft, snappedOriginLogical) ||
        !NearlyEqual(static_cast<double>(snappedOriginLogical.x), snappedLogicalX) ||
        !NearlyEqual(static_cast<double>(snappedOriginLogical.y), snappedLogicalY))
    {
        return Failure(
            SpritePixelPerfectError::MappingOverflow,
            SpritePixelPerfectField::SourceOrigin);
    }

    outMapping.presentationPose = presentationPose;
    outMapping.sourceOriginLogicalBeforeSnap = originLogical;
    outMapping.sourceOriginLogicalAfterSnap = snappedOriginLogical;
    outMapping.sourcePixelBasisXLogical = basisX;
    outMapping.sourcePixelBasisYLogical = basisY;
    outMapping.worldSnapDelta = worldDelta;
    outMapping.sourcePixelScaleX = integerBasisX.magnitude;
    outMapping.sourcePixelScaleY = integerBasisY.magnitude;
    outMapping.axesSwapped = integerBasisX.axis == LogicalAxis::Y;
    return Success();
}
} // namespace trace2d::render
