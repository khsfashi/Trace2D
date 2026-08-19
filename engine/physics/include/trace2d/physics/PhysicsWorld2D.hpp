#pragma once

#include <trace2d/physics/PhysicsComponents2D.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace trace2d::physics
{
struct PhysicsWorldConfig2D final
{
    scene::Vector2 gravity{0.0F, -9.8F};
    int subStepCount{4};
};

enum class PhysicsAttachResult2D : std::uint8_t
{
    Success = 0,
    EntityNotFound,
    MissingRigidBody,
    MissingCollider,
    AlreadyAttached,
    ParentedEntityUnsupported,
    NonUnitScaleUnsupported,
    InvalidTransform,
    InvalidComponent,
    BackendFailure,
};
[[nodiscard]] std::string_view ToString(PhysicsAttachResult2D result) noexcept;

enum class PhysicsStepResult2D : std::uint8_t
{
    Success = 0,
    InvalidDelta,
    BackendInvalid,
};
[[nodiscard]] std::string_view ToString(PhysicsStepResult2D result) noexcept;

enum class PhysicsQueryResult2D : std::uint8_t
{
    Success = 0,
    InvalidInput,
    CapacityExceeded,
    BackendInvalid,
};
[[nodiscard]] std::string_view ToString(PhysicsQueryResult2D result) noexcept;

struct PhysicsRaycastQuery2D final
{
    scene::Vector2 origin{};
    scene::Vector2 translation{};
    std::uint32_t layerBits{1U};
    std::uint32_t maskBits{0xFFFFFFFFU};
};

struct PhysicsRaycastHit2D final
{
    scene::EntityId entity{};
    scene::Vector2 point{};
    scene::Vector2 normal{};
    float fraction{0.0F};
    std::array<char, PhysicsSemanticIdCapacity2D> colliderSemanticId{};
    std::uint8_t colliderSemanticIdLength{0U};

    [[nodiscard]] std::string_view ColliderSemanticId() const noexcept
    {
        return {colliderSemanticId.data(), colliderSemanticIdLength};
    }
};

struct PhysicsRaycastReport2D final
{
    PhysicsQueryResult2D result{PhysicsQueryResult2D::Success};
    std::size_t hitCount{0U};
    std::size_t requiredCapacity{0U};
};

struct PhysicsBodyState2D final
{
    scene::Vector2 position{};
    float rotationRadians{0.0F};
    scene::Vector2 linearVelocity{};
    float angularVelocity{0.0F};
    bool awake{false};
};

struct PhysicsMetrics2D final
{
    std::size_t attachedBodyCount{0U};
    std::size_t retainedBodyCapacity{0U};
    std::size_t retainedRayHitCapacity{0U};
    std::uint64_t fixedStepCount{0U};
    std::uint64_t stalePruneCount{0U};
    std::uint64_t unsupportedTransformPruneCount{0U};
    std::uint64_t rayQueryCount{0U};
    std::uint64_t rayCapacityFailureCount{0U};
};

class PhysicsWorld2D final
{
public:
    PhysicsWorld2D(
        scene::Scene& scene,
        PhysicsComponentTypes2D componentTypes,
        PhysicsWorldConfig2D config = {});
    PhysicsWorld2D(const PhysicsWorld2D&) = delete;
    PhysicsWorld2D& operator=(const PhysicsWorld2D&) = delete;
    PhysicsWorld2D(PhysicsWorld2D&&) = delete;
    PhysicsWorld2D& operator=(PhysicsWorld2D&&) = delete;
    ~PhysicsWorld2D();

    void Reserve(std::size_t bodyCapacity, std::size_t rayHitCapacity);

    [[nodiscard]] PhysicsAttachResult2D AttachEntity(scene::EntityId entity);
    [[nodiscard]] bool DetachEntity(scene::EntityId entity) noexcept;
    [[nodiscard]] bool Contains(scene::EntityId entity) const noexcept;

    [[nodiscard]] PhysicsStepResult2D Step(float fixedDeltaSeconds) noexcept;
    [[nodiscard]] bool TryGetBodyState(scene::EntityId entity, PhysicsBodyState2D& outState) const noexcept;

    [[nodiscard]] PhysicsRaycastReport2D Raycast(
        const PhysicsRaycastQuery2D& query,
        std::span<PhysicsRaycastHit2D> output) noexcept;

    [[nodiscard]] PhysicsMetrics2D Metrics() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
} // namespace trace2d::physics
