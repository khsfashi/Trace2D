#pragma once

#include <trace2d/scene/Scene.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace trace2d::physics
{
inline constexpr std::size_t PhysicsSemanticIdCapacity2D = 64U;

enum class RigidBodyType2D : std::uint8_t
{
    Static = 0,
    Kinematic,
    Dynamic,
};

struct RigidBody2D final
{
    RigidBodyType2D type{RigidBodyType2D::Static};
    scene::Vector2 linearVelocity{};
    float angularVelocity{0.0F};
    float linearDamping{0.0F};
    float angularDamping{0.0F};
    float gravityScale{1.0F};
    bool fixedRotation{false};
    bool bullet{false};
    [[nodiscard]] bool operator==(const RigidBody2D&) const noexcept = default;
};

enum class ColliderShape2D : std::uint8_t
{
    Box = 0,
    Circle,
};

struct Collider2D final
{
    std::string semanticId{};
    ColliderShape2D shape{ColliderShape2D::Box};
    scene::Vector2 localOffset{};
    scene::Vector2 halfExtents{0.5F, 0.5F};
    float radius{0.5F};
    std::uint32_t layerBits{1U};
    std::uint32_t maskBits{0xFFFFFFFFU};
    bool sensor{false};
    float density{1.0F};
    float friction{0.6F};
    float restitution{0.0F};
    [[nodiscard]] bool operator==(const Collider2D&) const noexcept = default;
};

struct PhysicsComponentTypes2D final
{
    scene::ComponentTypeHandle<RigidBody2D> rigidBody{};
    scene::ComponentTypeHandle<Collider2D> collider{};
};

[[nodiscard]] PhysicsComponentTypes2D RegisterPhysics2DComponents(scene::ComponentRegistry& registry);
} // namespace trace2d::physics
