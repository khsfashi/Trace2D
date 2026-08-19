#include <trace2d/physics/PhysicsComponents2D.hpp>
#include <trace2d/physics/PhysicsWorld2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>

namespace
{
using namespace trace2d;

[[nodiscard]] physics::Collider2D MakeCollider(
    std::string semanticId,
    const std::uint32_t layerBits = 1U,
    const bool sensor = false,
    const float restitution = 0.0F)
{
    physics::Collider2D collider{};
    collider.semanticId = std::move(semanticId);
    collider.shape = physics::ColliderShape2D::Circle;
    collider.radius = 0.5F;
    collider.layerBits = layerBits;
    collider.sensor = sensor;
    collider.restitution = restitution;
    return collider;
}

[[nodiscard]] scene::EntityId CreatePhysicalEntity(
    scene::Scene& scene,
    const physics::PhysicsComponentTypes2D types,
    std::string semanticId,
    const scene::Vector2 position,
    const physics::RigidBodyType2D bodyType,
    const scene::Vector2 linearVelocity,
    physics::Collider2D collider)
{
    scene::EntityDescriptor descriptor{};
    descriptor.semanticId = std::move(semanticId);
    descriptor.name = descriptor.semanticId;
    descriptor.transform.position = position;
    const scene::EntityId entity = scene.CreateEntity(std::move(descriptor));

    physics::RigidBody2D body{};
    body.type = bodyType;
    body.linearVelocity = linearVelocity;
    (void)scene.AddComponent(entity, types.rigidBody, body);
    (void)scene.AddComponent(entity, types.collider, std::move(collider));
    return entity;
}

[[nodiscard]] physics::PhysicsWorldConfig2D MakeZeroGravityConfig()
{
    physics::PhysicsWorldConfig2D config{};
    config.gravity = {0.0F, 0.0F};
    return config;
}
} // namespace

TEST(PhysicsWorld2DPHYS2, ContactBeginEndAreCanonicalStableAndSelfContained)
{
    scene::ComponentRegistry registry{};
    const physics::PhysicsComponentTypes2D types = physics::RegisterPhysics2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};

    const scene::EntityId right = CreatePhysicalEntity(
        scene,
        types,
        "right",
        {1.5F, 0.0F},
        physics::RigidBodyType2D::Dynamic,
        {-3.0F, 0.0F},
        MakeCollider("z_collider", 1U, false, 1.0F));
    const scene::EntityId left = CreatePhysicalEntity(
        scene,
        types,
        "left",
        {-1.5F, 0.0F},
        physics::RigidBodyType2D::Dynamic,
        {3.0F, 0.0F},
        MakeCollider("a_collider", 1U, false, 1.0F));

    physics::PhysicsWorld2D world{scene, types, MakeZeroGravityConfig()};
    world.Reserve(4U, 0U);
    world.ReserveEvents(8U, 8U);
    ASSERT_EQ(world.AttachEntity(right), physics::PhysicsAttachResult2D::Success);
    ASSERT_EQ(world.AttachEntity(left), physics::PhysicsAttachResult2D::Success);

    physics::PhysicsContactEvent2D copiedBegin{};
    bool sawBegin = false;
    bool sawEnd = false;

    for (int step = 0; step < 240 && !sawEnd; ++step)
    {
        const physics::PhysicsStepReport2D report = world.StepWithReport(1.0F / 120.0F);
        ASSERT_EQ(report.result, physics::PhysicsStepResult2D::Success);
        for (const physics::PhysicsContactEvent2D& event : world.ContactEvents())
        {
            if (event.kind == physics::PhysicsContactEventKind2D::Begin)
            {
                ASSERT_FALSE(sawBegin);
                sawBegin = true;
                copiedBegin = event;
                EXPECT_EQ(event.ColliderSemanticIdA(), "a_collider");
                EXPECT_EQ(event.ColliderSemanticIdB(), "z_collider");
                EXPECT_EQ(event.entityA, left);
                EXPECT_EQ(event.entityB, right);
                ASSERT_TRUE(event.hasContactGeometry);
                EXPECT_TRUE(std::isfinite(event.point.x));
                EXPECT_TRUE(std::isfinite(event.point.y));
                EXPECT_TRUE(std::isfinite(event.normal.x));
                EXPECT_TRUE(std::isfinite(event.normal.y));
            }
            else if (event.kind == physics::PhysicsContactEventKind2D::End)
            {
                ASSERT_TRUE(sawBegin);
                sawEnd = true;
                EXPECT_EQ(event.ColliderSemanticIdA(), "a_collider");
                EXPECT_EQ(event.ColliderSemanticIdB(), "z_collider");
                EXPECT_EQ(event.entityA, left);
                EXPECT_EQ(event.entityB, right);
                EXPECT_FALSE(event.hasContactGeometry);
                EXPECT_FLOAT_EQ(event.approachSpeed, 0.0F);
            }
            else
            {
                ASSERT_TRUE(sawBegin);
                EXPECT_EQ(event.ColliderSemanticIdA(), "a_collider");
                EXPECT_EQ(event.ColliderSemanticIdB(), "z_collider");
                EXPECT_GT(event.approachSpeed, 0.0F);
                EXPECT_TRUE(event.hasContactGeometry);
                EXPECT_TRUE(std::isfinite(event.point.x));
                EXPECT_TRUE(std::isfinite(event.normal.x));
            }
        }
    }

    ASSERT_TRUE(sawBegin);
    ASSERT_TRUE(sawEnd);
    EXPECT_EQ(copiedBegin.ColliderSemanticIdA(), "a_collider");
    EXPECT_EQ(copiedBegin.ColliderSemanticIdB(), "z_collider");
    EXPECT_EQ(copiedBegin.entityA, left);
    EXPECT_EQ(copiedBegin.entityB, right);
}

TEST(PhysicsWorld2DPHYS2, SensorBeginEndPreserveSensorVisitorRoles)
{
    scene::ComponentRegistry registry{};
    const physics::PhysicsComponentTypes2D types = physics::RegisterPhysics2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};

    physics::Collider2D sensorCollider = MakeCollider("pickup_trigger", 2U, true);
    sensorCollider.radius = 0.75F;
    const scene::EntityId sensor = CreatePhysicalEntity(
        scene,
        types,
        "pickup",
        {0.0F, 0.0F},
        physics::RigidBodyType2D::Static,
        {},
        std::move(sensorCollider));
    const scene::EntityId visitor = CreatePhysicalEntity(
        scene,
        types,
        "player",
        {-3.0F, 0.0F},
        physics::RigidBodyType2D::Dynamic,
        {4.0F, 0.0F},
        MakeCollider("player_hurtbox", 1U));

    physics::PhysicsWorld2D world{scene, types, MakeZeroGravityConfig()};
    world.Reserve(4U, 0U);
    world.ReserveEvents(4U, 4U);
    ASSERT_EQ(world.AttachEntity(sensor), physics::PhysicsAttachResult2D::Success);
    ASSERT_EQ(world.AttachEntity(visitor), physics::PhysicsAttachResult2D::Success);

    bool sawBegin = false;
    bool sawEnd = false;
    for (int step = 0; step < 240 && !sawEnd; ++step)
    {
        const physics::PhysicsStepReport2D report = world.StepWithReport(1.0F / 120.0F);
        ASSERT_EQ(report.result, physics::PhysicsStepResult2D::Success);
        for (const physics::PhysicsSensorEvent2D& event : world.SensorEvents())
        {
            EXPECT_EQ(event.sensorEntity, sensor);
            EXPECT_EQ(event.visitorEntity, visitor);
            EXPECT_EQ(event.SensorColliderSemanticId(), "pickup_trigger");
            EXPECT_EQ(event.VisitorColliderSemanticId(), "player_hurtbox");
            if (event.kind == physics::PhysicsSensorEventKind2D::Begin)
                sawBegin = true;
            else
                sawEnd = true;
        }
    }

    EXPECT_TRUE(sawBegin);
    EXPECT_TRUE(sawEnd);
}

TEST(PhysicsWorld2DPHYS2, EventCapacityOverflowIsExplicitAndPublishesNoPartialBatch)
{
    scene::ComponentRegistry registry{};
    const physics::PhysicsComponentTypes2D types = physics::RegisterPhysics2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};

    const scene::EntityId sensor = CreatePhysicalEntity(
        scene,
        types,
        "sensor",
        {},
        physics::RigidBodyType2D::Static,
        {},
        MakeCollider("sensor_collider", 1U, true));
    const scene::EntityId visitor = CreatePhysicalEntity(
        scene,
        types,
        "visitor",
        {},
        physics::RigidBodyType2D::Dynamic,
        {},
        MakeCollider("visitor_collider"));

    physics::PhysicsWorld2D world{scene, types, MakeZeroGravityConfig()};
    world.Reserve(2U, 0U);
    world.ReserveEvents(0U, 0U);
    ASSERT_EQ(world.AttachEntity(sensor), physics::PhysicsAttachResult2D::Success);
    ASSERT_EQ(world.AttachEntity(visitor), physics::PhysicsAttachResult2D::Success);

    const physics::PhysicsStepReport2D report = world.StepWithReport(1.0F / 60.0F);
    EXPECT_EQ(report.result, physics::PhysicsStepResult2D::EventCapacityExceeded);
    EXPECT_EQ(report.contactEventCount, 0U);
    EXPECT_EQ(report.sensorEventCount, 0U);
    EXPECT_GE(report.requiredSensorEventCapacity, 1U);
    EXPECT_TRUE(world.ContactEvents().empty());
    EXPECT_TRUE(world.SensorEvents().empty());
    EXPECT_EQ(world.Metrics().eventCapacityFailureCount, 1U);
    EXPECT_EQ(world.Metrics().fixedStepCount, 1U);
}

TEST(PhysicsWorld2DPHYS2, CircleAndBoxOverlapAreStableFilteredBoundedAndPruneStaleEntities)
{
    scene::ComponentRegistry registry{};
    const physics::PhysicsComponentTypes2D types = physics::RegisterPhysics2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};

    const scene::EntityId bEntity = CreatePhysicalEntity(
        scene,
        types,
        "body_b",
        {},
        physics::RigidBodyType2D::Static,
        {},
        MakeCollider("b_collider", 2U));
    const scene::EntityId aEntity = CreatePhysicalEntity(
        scene,
        types,
        "body_a",
        {},
        physics::RigidBodyType2D::Static,
        {},
        MakeCollider("a_collider", 2U));
    const scene::EntityId excludedEntity = CreatePhysicalEntity(
        scene,
        types,
        "excluded",
        {},
        physics::RigidBodyType2D::Static,
        {},
        MakeCollider("excluded_collider", 4U));

    physics::PhysicsWorld2D world{scene, types, MakeZeroGravityConfig()};
    world.Reserve(4U, 0U);
    world.ReserveOverlap(1U);
    ASSERT_EQ(world.AttachEntity(bEntity), physics::PhysicsAttachResult2D::Success);
    ASSERT_EQ(world.AttachEntity(aEntity), physics::PhysicsAttachResult2D::Success);
    ASSERT_EQ(world.AttachEntity(excludedEntity), physics::PhysicsAttachResult2D::Success);

    std::array<physics::PhysicsOverlapHit2D, 4U> hits{};
    const physics::PhysicsCircleOverlapQuery2D circle{{0.0F, 0.0F}, 1.0F, 1U, 2U};
    const physics::PhysicsOverlapReport2D small = world.OverlapCircle(circle, hits);
    EXPECT_EQ(small.result, physics::PhysicsQueryResult2D::CapacityExceeded);
    EXPECT_EQ(small.hitCount, 0U);
    EXPECT_EQ(small.requiredCapacity, 2U);

    world.ReserveOverlap(4U);
    const physics::PhysicsOverlapReport2D circleReport = world.OverlapCircle(circle, hits);
    ASSERT_EQ(circleReport.result, physics::PhysicsQueryResult2D::Success);
    ASSERT_EQ(circleReport.hitCount, 2U);
    EXPECT_EQ(hits[0].ColliderSemanticId(), "a_collider");
    EXPECT_EQ(hits[1].ColliderSemanticId(), "b_collider");
    EXPECT_EQ(hits[0].entity, aEntity);
    EXPECT_EQ(hits[1].entity, bEntity);
    EXPECT_NE(hits[0].entity, excludedEntity);

    const physics::PhysicsBoxOverlapQuery2D box{{0.0F, 0.0F}, {1.0F, 1.0F}, 0.35F, 1U, 2U};
    const physics::PhysicsOverlapReport2D boxReport = world.OverlapBox(box, hits);
    ASSERT_EQ(boxReport.result, physics::PhysicsQueryResult2D::Success);
    ASSERT_EQ(boxReport.hitCount, 2U);
    EXPECT_EQ(hits[0].ColliderSemanticId(), "a_collider");
    EXPECT_EQ(hits[1].ColliderSemanticId(), "b_collider");

    ASSERT_TRUE(scene.DestroyEntity(aEntity));
    const physics::PhysicsOverlapReport2D afterDestroy = world.OverlapCircle(circle, hits);
    ASSERT_EQ(afterDestroy.result, physics::PhysicsQueryResult2D::Success);
    ASSERT_EQ(afterDestroy.hitCount, 1U);
    EXPECT_EQ(hits[0].entity, bEntity);
    EXPECT_EQ(hits[0].ColliderSemanticId(), "b_collider");

    const physics::PhysicsMetrics2D metrics = world.Metrics();
    EXPECT_EQ(metrics.overlapQueryCount, 4U);
    EXPECT_EQ(metrics.overlapCapacityFailureCount, 1U);
    EXPECT_EQ(metrics.stalePruneCount, 1U);
    EXPECT_GE(metrics.retainedOverlapHitCapacity, 4U);
}
