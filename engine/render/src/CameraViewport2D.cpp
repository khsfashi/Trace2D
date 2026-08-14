#include <trace2d/render/CameraViewport2D.hpp>

#include <trace2d/scene/CameraSelection2D.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace trace2d::render
{
namespace
{
[[nodiscard]] bool IsFinite(const Float2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool IsValidScaleMode(const ViewportScaleMode2D mode) noexcept
{
    switch (mode)
    {
    case ViewportScaleMode2D::Fit:
    case ViewportScaleMode2D::Fill:
    case ViewportScaleMode2D::Stretch:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidFrameState(const CameraFrameState2D& state) noexcept
{
    return state.entity.IsValid() && IsFinite(state.center) && std::isfinite(state.verticalSize) &&
        state.verticalSize > 0.0F;
}

[[nodiscard]] bool IsValidResolvedViewport(const ResolvedViewport2D& viewport) noexcept
{
    return viewport.logicalWidth != 0U && viewport.logicalHeight != 0U &&
        viewport.targetWidth != 0U && viewport.targetHeight != 0U &&
        IsValidScaleMode(viewport.scaleMode) &&
        IsFinite(viewport.contentRect.origin) && IsFinite(viewport.contentRect.size) &&
        viewport.contentRect.size.x > 0.0F && viewport.contentRect.size.y > 0.0F &&
        IsFinite(viewport.viewportToPresentationScale) &&
        viewport.viewportToPresentationScale.x > 0.0F &&
        viewport.viewportToPresentationScale.y > 0.0F &&
        IsFinite(viewport.presentationToViewportScale) &&
        viewport.presentationToViewportScale.x > 0.0F &&
        viewport.presentationToViewportScale.y > 0.0F;
}

[[nodiscard]] bool IsValidInverseView(const ResolvedPresentationView2D& view) noexcept
{
    return IsValidResolvedViewport(view.viewport) &&
        IsFinite(view.logicalView.center) &&
        IsFinite(view.logicalView.halfExtents) &&
        view.logicalView.halfExtents.x > 0.0F && view.logicalView.halfExtents.y > 0.0F &&
        IsFinite(view.logicalView.clipScale) &&
        view.logicalView.clipScale.x > 0.0F && view.logicalView.clipScale.y > 0.0F;
}

[[nodiscard]] Float2 Lerp(const Float2 previous, const Float2 current, const float alpha) noexcept
{
    return Float2{
        previous.x + ((current.x - previous.x) * alpha),
        previous.y + ((current.y - previous.y) * alpha),
    };
}
} // namespace

std::string_view ToString(const ViewportScaleMode2D mode) noexcept
{
    switch (mode)
    {
    case ViewportScaleMode2D::Fit:
        return "fit";
    case ViewportScaleMode2D::Fill:
        return "fill";
    case ViewportScaleMode2D::Stretch:
        return "stretch";
    }
    return "invalid";
}

std::string_view ToString(const ViewportResolveError2D error) noexcept
{
    switch (error)
    {
    case ViewportResolveError2D::None:
        return "none";
    case ViewportResolveError2D::EmptySemanticId:
        return "empty_semantic_id";
    case ViewportResolveError2D::InvalidLogicalSize:
        return "invalid_logical_size";
    case ViewportResolveError2D::InvalidTargetSize:
        return "invalid_target_size";
    case ViewportResolveError2D::InvalidScaleMode:
        return "invalid_scale_mode";
    case ViewportResolveError2D::InvalidMapping:
        return "invalid_mapping";
    }
    return "invalid";
}

ViewportResolveResult2D ResolveViewport2D(
    const Viewport2D& viewport,
    const std::uint32_t targetWidth,
    const std::uint32_t targetHeight) noexcept
{
    ViewportResolveResult2D result{};
    if (viewport.semanticId.empty())
    {
        result.error = ViewportResolveError2D::EmptySemanticId;
        return result;
    }
    if (viewport.logicalWidth == 0U || viewport.logicalHeight == 0U)
    {
        result.error = ViewportResolveError2D::InvalidLogicalSize;
        return result;
    }
    if (targetWidth == 0U || targetHeight == 0U)
    {
        result.error = ViewportResolveError2D::InvalidTargetSize;
        return result;
    }

    const float logicalWidth = static_cast<float>(viewport.logicalWidth);
    const float logicalHeight = static_cast<float>(viewport.logicalHeight);
    const float presentationWidth = static_cast<float>(targetWidth);
    const float presentationHeight = static_cast<float>(targetHeight);
    const float widthScale = presentationWidth / logicalWidth;
    const float heightScale = presentationHeight / logicalHeight;

    Float2 scale{};
    switch (viewport.scaleMode)
    {
    case ViewportScaleMode2D::Fit:
    {
        const float uniform = std::min(widthScale, heightScale);
        scale = Float2{uniform, uniform};
        break;
    }
    case ViewportScaleMode2D::Fill:
    {
        const float uniform = std::max(widthScale, heightScale);
        scale = Float2{uniform, uniform};
        break;
    }
    case ViewportScaleMode2D::Stretch:
        scale = Float2{widthScale, heightScale};
        break;
    default:
        result.error = ViewportResolveError2D::InvalidScaleMode;
        return result;
    }

    const Float2 contentSize{logicalWidth * scale.x, logicalHeight * scale.y};
    const Float2 contentOrigin{
        (presentationWidth - contentSize.x) * 0.5F,
        (presentationHeight - contentSize.y) * 0.5F,
    };
    const Float2 inverseScale{1.0F / scale.x, 1.0F / scale.y};
    if (!IsFinite(scale) || scale.x <= 0.0F || scale.y <= 0.0F ||
        !IsFinite(contentSize) || contentSize.x <= 0.0F || contentSize.y <= 0.0F ||
        !IsFinite(contentOrigin) || !IsFinite(inverseScale))
    {
        result.error = ViewportResolveError2D::InvalidMapping;
        return result;
    }

    result.viewport.logicalWidth = viewport.logicalWidth;
    result.viewport.logicalHeight = viewport.logicalHeight;
    result.viewport.targetWidth = targetWidth;
    result.viewport.targetHeight = targetHeight;
    result.viewport.scaleMode = viewport.scaleMode;
    result.viewport.contentRect = PresentationRect2D{contentOrigin, contentSize};
    result.viewport.viewportToPresentationScale = scale;
    result.viewport.presentationToViewportScale = inverseScale;
    return result;
}

std::string_view ToString(const ActiveCameraSelectionError2D error) noexcept
{
    switch (error)
    {
    case ActiveCameraSelectionError2D::None:
        return "none";
    case ActiveCameraSelectionError2D::InvalidCameraType:
        return "invalid_camera_type";
    case ActiveCameraSelectionError2D::InvalidViewport:
        return "invalid_viewport";
    case ActiveCameraSelectionError2D::NoActiveCamera:
        return "no_active_camera";
    }
    return "invalid";
}

ActiveCameraSelectionResult2D ResolveActiveCamera2D(
    const scene::Scene& worldScene,
    const scene::ComponentTypeHandle<scene::Camera2D> cameraType,
    const Viewport2D& viewport) noexcept
{
    ActiveCameraSelectionResult2D result{};
    const scene::CameraSelection2D selection =
        scene::ResolveCameraSelection2D(worldScene, cameraType, viewport.semanticId);
    switch (selection.error)
    {
    case scene::CameraSelectionError2D::None:
        result.camera.entity = selection.entity;
        result.camera.priority = selection.priority;
        return result;
    case scene::CameraSelectionError2D::InvalidCameraType:
        result.error = ActiveCameraSelectionError2D::InvalidCameraType;
        return result;
    case scene::CameraSelectionError2D::InvalidViewport:
        result.error = ActiveCameraSelectionError2D::InvalidViewport;
        return result;
    case scene::CameraSelectionError2D::NoActiveCamera:
        result.error = ActiveCameraSelectionError2D::NoActiveCamera;
        return result;
    }

    result.error = ActiveCameraSelectionError2D::NoActiveCamera;
    return result;
}

std::string_view ToString(const CameraFrameStateError2D error) noexcept
{
    switch (error)
    {
    case CameraFrameStateError2D::None:
        return "none";
    case CameraFrameStateError2D::InvalidCameraType:
        return "invalid_camera_type";
    case CameraFrameStateError2D::StaleSelection:
        return "stale_selection";
    case CameraFrameStateError2D::InactiveSelection:
        return "inactive_selection";
    case CameraFrameStateError2D::InvalidCameraState:
        return "invalid_camera_state";
    case CameraFrameStateError2D::InvalidWorldTransform:
        return "invalid_world_transform";
    }
    return "invalid";
}

CameraFrameStateResult2D ResolveCameraFrameState2D(
    const scene::Scene& worldScene,
    const scene::ComponentTypeHandle<scene::Camera2D> cameraType,
    const ActiveCamera2D activeCamera) noexcept
{
    CameraFrameStateResult2D result{};
    if (!cameraType.IsValid())
    {
        result.error = CameraFrameStateError2D::InvalidCameraType;
        return result;
    }
    if (!activeCamera.IsValid())
    {
        result.error = CameraFrameStateError2D::StaleSelection;
        return result;
    }

    const scene::Camera2D* const camera = worldScene.TryGetComponent(activeCamera.entity, cameraType);
    if (camera == nullptr)
    {
        result.error = CameraFrameStateError2D::StaleSelection;
        return result;
    }
    if (!camera->enabled)
    {
        result.error = CameraFrameStateError2D::InactiveSelection;
        return result;
    }
    if (!std::isfinite(camera->verticalSize) || camera->verticalSize <= 0.0F)
    {
        result.error = CameraFrameStateError2D::InvalidCameraState;
        return result;
    }

    scene::Transform2D world{};
    if (!worldScene.TryGetWorldTransform(activeCamera.entity, world) ||
        !std::isfinite(world.position.x) || !std::isfinite(world.position.y))
    {
        result.error = CameraFrameStateError2D::InvalidWorldTransform;
        return result;
    }

    result.state.entity = activeCamera.entity;
    result.state.center = Float2{world.position.x, world.position.y};
    result.state.verticalSize = camera->verticalSize;
    return result;
}

std::string_view ToString(const PresentationSamplingMode2D mode) noexcept
{
    switch (mode)
    {
    case PresentationSamplingMode2D::AuthoritativeCurrent:
        return "authoritative_current";
    case PresentationSamplingMode2D::Interpolated:
        return "interpolated";
    }
    return "invalid";
}

std::string_view ToString(const PresentationViewError2D error) noexcept
{
    switch (error)
    {
    case PresentationViewError2D::None:
        return "none";
    case PresentationViewError2D::InvalidViewport:
        return "invalid_viewport";
    case PresentationViewError2D::InvalidCurrentCamera:
        return "invalid_current_camera";
    case PresentationViewError2D::MissingPreviousCamera:
        return "missing_previous_camera";
    case PresentationViewError2D::CameraHistoryMismatch:
        return "camera_history_mismatch";
    case PresentationViewError2D::InvalidInterpolationAlpha:
        return "invalid_interpolation_alpha";
    case PresentationViewError2D::InvalidSamplingMode:
        return "invalid_sampling_mode";
    case PresentationViewError2D::InvalidProjection:
        return "invalid_projection";
    }
    return "invalid";
}

PresentationViewResult2D ResolvePresentationView2D(
    const CameraFrameState2D& current,
    const CameraFrameState2D* const previous,
    const ResolvedViewport2D& viewport,
    const PresentationSamplingMode2D samplingMode,
    const float interpolationAlpha) noexcept
{
    PresentationViewResult2D result{};
    if (!IsValidResolvedViewport(viewport))
    {
        result.error = PresentationViewError2D::InvalidViewport;
        return result;
    }
    if (!IsValidFrameState(current))
    {
        result.error = PresentationViewError2D::InvalidCurrentCamera;
        return result;
    }

    Float2 sampledCenter = current.center;
    float sampledVerticalSize = current.verticalSize;
    float recordedAlpha = 1.0F;
    switch (samplingMode)
    {
    case PresentationSamplingMode2D::AuthoritativeCurrent:
        break;
    case PresentationSamplingMode2D::Interpolated:
        if (previous == nullptr)
        {
            result.error = PresentationViewError2D::MissingPreviousCamera;
            return result;
        }
        if (!IsValidFrameState(*previous) || previous->entity != current.entity)
        {
            result.error = PresentationViewError2D::CameraHistoryMismatch;
            return result;
        }
        if (!std::isfinite(interpolationAlpha) || interpolationAlpha < 0.0F || interpolationAlpha > 1.0F)
        {
            result.error = PresentationViewError2D::InvalidInterpolationAlpha;
            return result;
        }
        sampledCenter = Lerp(previous->center, current.center, interpolationAlpha);
        sampledVerticalSize = previous->verticalSize +
            ((current.verticalSize - previous->verticalSize) * interpolationAlpha);
        recordedAlpha = interpolationAlpha;
        break;
    default:
        result.error = PresentationViewError2D::InvalidSamplingMode;
        return result;
    }

    OrthographicCamera logicalCamera{};
    logicalCamera.center = sampledCenter;
    logicalCamera.verticalSize = sampledVerticalSize;

    OrthographicView logicalView{};
    if (!TryBuildOrthographicView(
            logicalCamera, viewport.logicalWidth, viewport.logicalHeight, logicalView))
    {
        result.error = PresentationViewError2D::InvalidProjection;
        return result;
    }

    const float logicalAspect =
        static_cast<float>(viewport.logicalWidth) / static_cast<float>(viewport.logicalHeight);
    const float targetAspect =
        static_cast<float>(viewport.targetWidth) / static_cast<float>(viewport.targetHeight);
    const Float2 contentToTarget{
        viewport.contentRect.size.x / static_cast<float>(viewport.targetWidth),
        viewport.contentRect.size.y / static_cast<float>(viewport.targetHeight),
    };

    OrthographicCamera rendererCamera = logicalCamera;
    rendererCamera.presentationScale = Float2{
        (targetAspect / logicalAspect) * contentToTarget.x,
        contentToTarget.y,
    };
    rendererCamera.rasterViewport.enabled = true;
    rendererCamera.rasterViewport.targetWidth = viewport.targetWidth;
    rendererCamera.rasterViewport.targetHeight = viewport.targetHeight;
    rendererCamera.rasterViewport.origin = viewport.contentRect.origin;
    rendererCamera.rasterViewport.size = viewport.contentRect.size;
    if (!IsFinite(rendererCamera.presentationScale) || rendererCamera.presentationScale.x <= 0.0F ||
        rendererCamera.presentationScale.y <= 0.0F)
    {
        result.error = PresentationViewError2D::InvalidProjection;
        return result;
    }

    OrthographicView presentationView{};
    if (!TryBuildOrthographicView(
            rendererCamera, viewport.targetWidth, viewport.targetHeight, presentationView))
    {
        result.error = PresentationViewError2D::InvalidProjection;
        return result;
    }

    result.view.cameraEntity = current.entity;
    result.view.viewport = viewport;
    result.view.samplingMode = samplingMode;
    result.view.interpolationAlpha = recordedAlpha;
    result.view.rendererCamera = rendererCamera;
    result.view.logicalView = logicalView;
    result.view.presentationView = presentationView;
    return result;
}

std::string_view ToString(const CoordinateConversionError2D error) noexcept
{
    switch (error)
    {
    case CoordinateConversionError2D::None:
        return "none";
    case CoordinateConversionError2D::InvalidResolvedView:
        return "invalid_resolved_view";
    case CoordinateConversionError2D::NonFiniteInput:
        return "non_finite_input";
    }
    return "invalid";
}

Float2 WorldToViewport(
    const ResolvedPresentationView2D& view,
    const Float2 worldPosition) noexcept
{
    const Float2 clip = WorldToClip(view.logicalView, worldPosition);
    return Float2{
        (clip.x + 1.0F) * 0.5F * static_cast<float>(view.viewport.logicalWidth),
        (1.0F - clip.y) * 0.5F * static_cast<float>(view.viewport.logicalHeight),
    };
}

CoordinateConversionResult2D ViewportToWorld(
    const ResolvedPresentationView2D& view,
    const Float2 viewportPosition) noexcept
{
    CoordinateConversionResult2D result{};
    if (!IsValidInverseView(view))
    {
        result.error = CoordinateConversionError2D::InvalidResolvedView;
        return result;
    }
    if (!IsFinite(viewportPosition))
    {
        result.error = CoordinateConversionError2D::NonFiniteInput;
        return result;
    }

    const Float2 clip{
        ((viewportPosition.x / static_cast<float>(view.viewport.logicalWidth)) * 2.0F) - 1.0F,
        1.0F - ((viewportPosition.y / static_cast<float>(view.viewport.logicalHeight)) * 2.0F),
    };
    result.value = Float2{
        view.logicalView.center.x + (clip.x / view.logicalView.clipScale.x),
        view.logicalView.center.y + (clip.y / view.logicalView.clipScale.y),
    };
    if (!IsFinite(result.value))
    {
        result.error = CoordinateConversionError2D::InvalidResolvedView;
        result.value = {};
    }
    return result;
}

Float2 ViewportToPresentation(
    const ResolvedPresentationView2D& view,
    const Float2 viewportPosition) noexcept
{
    return Float2{
        view.viewport.contentRect.origin.x +
            (viewportPosition.x * view.viewport.viewportToPresentationScale.x),
        view.viewport.contentRect.origin.y +
            (viewportPosition.y * view.viewport.viewportToPresentationScale.y),
    };
}

CoordinateConversionResult2D PresentationToViewport(
    const ResolvedPresentationView2D& view,
    const Float2 presentationPosition) noexcept
{
    CoordinateConversionResult2D result{};
    if (!IsValidInverseView(view))
    {
        result.error = CoordinateConversionError2D::InvalidResolvedView;
        return result;
    }
    if (!IsFinite(presentationPosition))
    {
        result.error = CoordinateConversionError2D::NonFiniteInput;
        return result;
    }

    result.value = Float2{
        (presentationPosition.x - view.viewport.contentRect.origin.x) *
            view.viewport.presentationToViewportScale.x,
        (presentationPosition.y - view.viewport.contentRect.origin.y) *
            view.viewport.presentationToViewportScale.y,
    };
    if (!IsFinite(result.value))
    {
        result.error = CoordinateConversionError2D::InvalidResolvedView;
        result.value = {};
    }
    return result;
}

Float2 WorldToPresentation(
    const ResolvedPresentationView2D& view,
    const Float2 worldPosition) noexcept
{
    return ViewportToPresentation(view, WorldToViewport(view, worldPosition));
}

CoordinateConversionResult2D PresentationToWorld(
    const ResolvedPresentationView2D& view,
    const Float2 presentationPosition) noexcept
{
    const CoordinateConversionResult2D viewport =
        PresentationToViewport(view, presentationPosition);
    if (!viewport.Succeeded())
    {
        return viewport;
    }
    return ViewportToWorld(view, viewport.value);
}

bool IsPresentationPointInsideViewport(
    const ResolvedPresentationView2D& view,
    const Float2 presentationPosition) noexcept
{
    if (!IsFinite(presentationPosition))
    {
        return false;
    }

    const float targetWidth = static_cast<float>(view.viewport.targetWidth);
    const float targetHeight = static_cast<float>(view.viewport.targetHeight);
    if (presentationPosition.x < 0.0F || presentationPosition.x > targetWidth ||
        presentationPosition.y < 0.0F || presentationPosition.y > targetHeight)
    {
        return false;
    }

    const float right = view.viewport.contentRect.origin.x + view.viewport.contentRect.size.x;
    const float bottom = view.viewport.contentRect.origin.y + view.viewport.contentRect.size.y;
    return presentationPosition.x >= view.viewport.contentRect.origin.x &&
        presentationPosition.x <= right &&
        presentationPosition.y >= view.viewport.contentRect.origin.y &&
        presentationPosition.y <= bottom;
}
} // namespace trace2d::render
