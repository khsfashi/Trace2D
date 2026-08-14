#pragma once

#include <trace2d/scene/CameraSelection2D.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace trace2d::agent
{
enum class CameraInspectionErrorCode : std::uint8_t
{
    SceneUnavailable = 0,
    InvalidCameraType,
    InvalidViewport,
    NoActiveCamera,
    CameraUnavailable,
    InvalidWorldTransform,
};

[[nodiscard]] std::string_view ToString(CameraInspectionErrorCode code) noexcept;

struct CameraVector2Snapshot final
{
    float x{0.0F};
    float y{0.0F};

    [[nodiscard]] bool operator==(const CameraVector2Snapshot&) const noexcept = default;
};

struct ActiveCameraViewportSnapshot2D final
{
    std::string viewportSemanticId{};
    std::string cameraEntitySemanticId{};
    std::uint32_t entityIndex{0U};
    std::uint32_t entityGeneration{0U};
    std::int32_t priority{0};
    bool enabled{false};
    float verticalSize{0.0F};
    std::string targetViewport{};
    CameraVector2Snapshot worldCenter{};

    [[nodiscard]] bool operator==(const ActiveCameraViewportSnapshot2D&) const noexcept = default;
};

struct CameraInspectionError final
{
    CameraInspectionErrorCode code{CameraInspectionErrorCode::SceneUnavailable};
    std::string message{};

    [[nodiscard]] bool operator==(const CameraInspectionError&) const noexcept = default;
};

struct CameraViewportInspectionResult2D final
{
    std::optional<ActiveCameraViewportSnapshot2D> snapshot{};
    std::optional<CameraInspectionError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return snapshot.has_value() && !error.has_value();
    }
};

// Explicit Agent query surface for the resolved active camera identity and authoritative camera
// state. This is not a render-frame hot path; string copies are confined to the inspection result.
[[nodiscard]] CameraViewportInspectionResult2D InspectActiveCameraViewport2D(
    const scene::Scene* scene,
    scene::ComponentTypeHandle<scene::Camera2D> cameraType,
    std::string_view viewportSemanticId);
} // namespace trace2d::agent
