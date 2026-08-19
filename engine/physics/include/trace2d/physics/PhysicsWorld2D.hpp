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
    EventCapacityExceeded,
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

struct PhysicsStepReport2D final
{
    PhysicsStepResult2D result{PhysicsStepResult2D::Success};
    std::size_t contactEventCount{0U};
    std::size_t sensorEventCount{0U};
    std::size_t requiredContactEventCapacity{0U};
    std::size_t requiredSensorEventCapacity{0U};
};

enum class PhysicsContactEventKind2D : std::uint8_t
{
    Begin = 0,
    End,
};

struct PhysicsContactEvent2D final
{
    PhysicsContactEventKind2D kind{PhysicsContactEventKind2D::Begin};
    scene::EntityId entityA{};
    scene::EntityId entityB{};
    std::array<char, PhysicsSemanticIdCapacity2D> colliderSemanticIdA{};
    std::array<char, PhysicsSemanticIdCapacity2D> colliderSemanticIdB{};
    std::uint8_t colliderSemanticIdALength{0U};
    std::uint8_t colliderSemanticIdBLength{0U};
    scene::Vector2 point{};
    scene::Vector2 normal{};
    bool hasContactGeometry{false};

    [[nodiscard]] std::string_view ColliderSemanticIdA() const noexcept
    {
        return {colliderSemanticIdA.data(), colliderSemanticIdALength};
    }

    [[nodiscard]] std::string_view ColliderSemanticIdB() const noexcept
    {
        return {colliderSemanticIdB.data(), colliderSemanticIdBLength};
    }
};

enum class PhysicsSensorEventKind2D : std::uint8_t
{
    Begin = 0,
    End,
};

struct PhysicsSensorEvent2D final
{
    PhysicsSensorEventKind2D kind{PhysicsSensorEventKind2D::Begin};
    scene::EntityId sensorEntity{};
    scene::EntityId visitorEntity{};
    std::array<char, PhysicsSemanticIdCapacity2D> sensorColliderSemanticId{};
    std::array<char, PhysicsSemanticIdCapacity2D> visitorColliderSemanticId{};
    std::uint8_t sensorColliderSemanticIdLength{0U};
    std::uint8_t visitorColliderSemanticIdLength{0U};

    [[nodiscard]] std::string_view SensorColliderSemanticId() const noexcept
    {
        return {sensorColliderSemanticId.data(), sensorColliderSemanticIdLength};
    }

    [[nodiscard]] std::string_view VisitorColliderSemanticId() const noexcept
    {
        return {visitorColliderSemanticId.data(), visitorColliderSemanticIdLength};
    }
};

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

struct PhysicsCircleOverlapQuery2D final
{
    scene::Vector2 center{};
    float radius{0.5F};
    std::uint32_t layerBits{1U};
    std::uint32_t maskBits{0xFFFFFFFFU};
};

struct PhysicsBoxOverlapQuery2D final
{
    scene::Vector2 center{};
    scene::Vector2 halfExtents{0.5F, 0.5F};
    float rotationRadians{0.0F};
    std::uint32_t layerBits{1U};
    std::uint32_t maskBits{0xFFFFFFFFU};
};

struct PhysicsOverlapHit2D final
{
    scene::EntityId entity{};
    std::array<char, PhysicsSemanticIdCapacity2D> colliderSemanticId{};
    std::uint8_t colliderSemanticIdLength{0U};

    [[nodiscard]] std::string_view ColliderSemanticId() const noexcept
    {
        return {colliderSemanticId.data(), colliderSemanticIdLength};
    }
};

struct PhysicsOverlapReport2D final
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
    std::size_t retainedOverlapHitCapacity{0U};
    std::size_t retainedContactEventCapacity{0U};
    std::size_t retainedSensorEventCapacity{0U};
    std::size_t publishedContactEventCount{0U};
    std::size_t publishedSensorEventCount{0U};
    std::uint64_t fixedStepCount{0U};
    std::uint64_t stalePruneCount{0U};
    std::uint64_t unsupportedTransformPruneCount{0U};
    std::uint64_t rayQueryCount{0U};
    std::uint64_t rayCapacityFailureCount{0U};
    std::uint64_t overlapQueryCount{0U};
    std::uint64_t overlapCapacityFailureCount{0U};
    std::uint64_t eventCapacityFailureCount{0U};
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
    void ReserveEvents(std::size_t contactEventCapacity, std::size_t sensorEventCapacity);
    void ReserveOverlap(std::size_t overlapHitCapacity);

    [[nodiscard]] PhysicsAttachResult2D AttachEntity(scene::EntityId entity);
    [[nodiscard]] bool DetachEntity(scene::EntityId entity) noexcept;
    [[nodiscard]] bool Contains(scene::EntityId entity) const noexcept;

    [[nodiscard]] PhysicsStepResult2D Step(float fixedDeltaSeconds) noexcept;
    [[nodiscard]] PhysicsStepReport2D StepWithReport(float fixedDeltaSeconds) noexcept;
    [[nodiscard]] std::span<const PhysicsContactEvent2D> ContactEvents() const noexcept;
    [[nodiscard]] std::span<const PhysicsSensorEvent2D> SensorEvents() const noexcept;
    [[nodiscard]] bool TryGetBodyState(scene::EntityId entity, PhysicsBodyState2D& outState) const noexcept;

    [[nodiscard]] PhysicsRaycastReport2D Raycast(
        const PhysicsRaycastQuery2D& query,
        std::span<PhysicsRaycastHit2D> output) noexcept;
    [[nodiscard]] PhysicsOverlapReport2D OverlapCircle(
        const PhysicsCircleOverlapQuery2D& query,
        std::span<PhysicsOverlapHit2D> output) noexcept;
    [[nodiscard]] PhysicsOverlapReport2D OverlapBox(
        const PhysicsBoxOverlapQuery2D& query,
        std::span<PhysicsOverlapHit2D> output) noexcept;

    [[nodiscard]] PhysicsMetrics2D Metrics() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_{};
};
} // namespace trace2d::physics
