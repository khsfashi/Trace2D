#pragma once

#include <trace2d/scene/Components.hpp>

#include <cstdint>
#include <string>

namespace trace2d::scene
{
// Authoritative C0 camera state. Position comes from the owning entity world transform.
// Rotation, bounds/follow/smoothing and shake are intentionally deferred by C0; verticalSize is
// the single authoritative orthographic zoom/size value.
struct Camera2D final
{
    bool enabled{true};
    std::int32_t priority{0};
    float verticalSize{10.0F};
    std::string targetViewport{"main"};

    [[nodiscard]] bool operator==(const Camera2D&) const noexcept = default;
};

// Registers the authored `trace2d.camera2d` schema. Call during the same setup phase as the other
// #71 component registrations and before ComponentRegistry::Freeze().
[[nodiscard]] ComponentTypeHandle<Camera2D> RegisterCamera2DComponent(ComponentRegistry& registry);
} // namespace trace2d::scene
