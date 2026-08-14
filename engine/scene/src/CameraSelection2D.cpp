#include <trace2d/scene/CameraSelection2D.hpp>

#include <string_view>

namespace trace2d::scene
{
std::string_view ToString(const CameraSelectionError2D error) noexcept
{
    switch (error)
    {
    case CameraSelectionError2D::None:
        return "none";
    case CameraSelectionError2D::InvalidCameraType:
        return "invalid_camera_type";
    case CameraSelectionError2D::InvalidViewport:
        return "invalid_viewport";
    case CameraSelectionError2D::NoActiveCamera:
        return "no_active_camera";
    }
    return "invalid";
}

CameraSelection2D ResolveCameraSelection2D(
    const Scene& scene,
    const ComponentTypeHandle<Camera2D> cameraType,
    const std::string_view viewportSemanticId) noexcept
{
    CameraSelection2D result{};
    if (!cameraType.IsValid())
    {
        result.error = CameraSelectionError2D::InvalidCameraType;
        return result;
    }
    if (viewportSemanticId.empty())
    {
        result.error = CameraSelectionError2D::InvalidViewport;
        return result;
    }

    bool found = false;
    std::string_view selectedSemanticId{};
    scene.ForEachEntity([&](const EntityId entityId, const Entity& entity)
    {
        const Camera2D* const camera = scene.TryGetComponent(entityId, cameraType);
        if (camera == nullptr || !camera->enabled || camera->targetViewport != viewportSemanticId)
        {
            return;
        }

        const std::string_view semanticId = entity.SemanticId();
        const bool stableTieBreakWins =
            found && camera->priority == result.priority &&
            (semanticId < selectedSemanticId ||
             (semanticId == selectedSemanticId &&
              (entityId.index < result.entity.index ||
               (entityId.index == result.entity.index &&
                entityId.generation < result.entity.generation))));
        if (!found || camera->priority > result.priority || stableTieBreakWins)
        {
            found = true;
            selectedSemanticId = semanticId;
            result.entity = entityId;
            result.priority = camera->priority;
        }
    });

    if (!found)
    {
        result.error = CameraSelectionError2D::NoActiveCamera;
    }
    return result;
}
} // namespace trace2d::scene
