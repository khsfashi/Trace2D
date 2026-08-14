#pragma once

#include <trace2d/scene/Camera2D.hpp>
#include <trace2d/scene/Scene.hpp>

#include <cstdint>
#include <string_view>

namespace trace2d::scene
{
enum class CameraSelectionError2D : std::uint8_t
{
    None = 0,
    InvalidCameraType,
    InvalidViewport,
    NoActiveCamera,
};

[[nodiscard]] std::string_view ToString(CameraSelectionError2D error) noexcept;

struct CameraSelection2D final
{
    CameraSelectionError2D error{CameraSelectionError2D::None};
    EntityId entity{};
    std::int32_t priority{0};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == CameraSelectionError2D::None && entity.IsValid();
    }

    [[nodiscard]] bool operator==(const CameraSelection2D&) const noexcept = default;
};

// Single authority for C0 active-camera selection. Selection is deliberately explicit rather than
// a per-frame hot-path query: scan after camera structural/enabled/priority/target changes, cache
// the generation-safe EntityId, and resolve typed state directly during frames.
//
// Deterministic order: highest priority, then lexicographically smallest semantic entity id, then
// EntityId index/generation as a final stable fallback for entities without distinct semantic ids.
[[nodiscard]] CameraSelection2D ResolveCameraSelection2D(
    const Scene& scene,
    ComponentTypeHandle<Camera2D> cameraType,
    std::string_view viewportSemanticId) noexcept;
} // namespace trace2d::scene
