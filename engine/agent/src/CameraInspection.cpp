#include <trace2d/agent/CameraInspection.hpp>

#include <utility>

namespace trace2d::agent
{
namespace
{
[[nodiscard]] CameraViewportInspectionResult2D MakeError(
    const CameraInspectionErrorCode code,
    std::string message)
{
    CameraViewportInspectionResult2D result{};
    result.error = CameraInspectionError{code, std::move(message)};
    return result;
}
} // namespace

std::string_view ToString(const CameraInspectionErrorCode code) noexcept
{
    switch (code)
    {
    case CameraInspectionErrorCode::SceneUnavailable:
        return "scene_unavailable";
    case CameraInspectionErrorCode::InvalidCameraType:
        return "invalid_camera_type";
    case CameraInspectionErrorCode::InvalidViewport:
        return "invalid_viewport";
    case CameraInspectionErrorCode::NoActiveCamera:
        return "no_active_camera";
    case CameraInspectionErrorCode::CameraUnavailable:
        return "camera_unavailable";
    case CameraInspectionErrorCode::InvalidWorldTransform:
        return "invalid_world_transform";
    }
    return "invalid";
}

CameraViewportInspectionResult2D InspectActiveCameraViewport2D(
    const scene::Scene* const scene,
    const scene::ComponentTypeHandle<scene::Camera2D> cameraType,
    const std::string_view viewportSemanticId)
{
    if (scene == nullptr)
    {
        return MakeError(
            CameraInspectionErrorCode::SceneUnavailable,
            "No active scene is available for Camera2D inspection.");
    }

    const scene::CameraSelection2D selection =
        scene::ResolveCameraSelection2D(*scene, cameraType, viewportSemanticId);
    if (!selection.Succeeded())
    {
        switch (selection.error)
        {
        case scene::CameraSelectionError2D::InvalidCameraType:
            return MakeError(
                CameraInspectionErrorCode::InvalidCameraType,
                "Camera2D inspection requires a valid component type handle for this scene registry.");
        case scene::CameraSelectionError2D::InvalidViewport:
            return MakeError(
                CameraInspectionErrorCode::InvalidViewport,
                "Camera2D inspection requires a non-empty viewport semantic id.");
        case scene::CameraSelectionError2D::NoActiveCamera:
            return MakeError(
                CameraInspectionErrorCode::NoActiveCamera,
                "No enabled Camera2D targets the requested viewport.");
        case scene::CameraSelectionError2D::None:
            break;
        }
    }

    const scene::Entity* const entity = scene->TryGet(selection.entity);
    const scene::Camera2D* const camera = scene->TryGetComponent(selection.entity, cameraType);
    if (entity == nullptr || camera == nullptr || !camera->enabled)
    {
        return MakeError(
            CameraInspectionErrorCode::CameraUnavailable,
            "The selected Camera2D became unavailable before inspection completed.");
    }

    scene::Transform2D world{};
    if (!scene->TryGetWorldTransform(selection.entity, world))
    {
        return MakeError(
            CameraInspectionErrorCode::InvalidWorldTransform,
            "The selected Camera2D does not have a finite resolvable world transform.");
    }

    ActiveCameraViewportSnapshot2D snapshot{};
    snapshot.viewportSemanticId = viewportSemanticId;
    snapshot.cameraEntitySemanticId = entity->SemanticId();
    snapshot.entityIndex = selection.entity.index;
    snapshot.entityGeneration = selection.entity.generation;
    snapshot.priority = selection.priority;
    snapshot.enabled = camera->enabled;
    snapshot.verticalSize = camera->verticalSize;
    snapshot.targetViewport = camera->targetViewport;
    snapshot.worldCenter = CameraVector2Snapshot{world.position.x, world.position.y};

    CameraViewportInspectionResult2D result{};
    result.snapshot = std::move(snapshot);
    return result;
}
} // namespace trace2d::agent
