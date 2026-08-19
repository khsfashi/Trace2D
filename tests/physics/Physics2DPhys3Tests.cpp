#include <trace2d/physics/PhysicsComponents2D.hpp>
#include <trace2d/physics/PhysicsWorld2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace
{
using namespace trace2d;

[[nodiscard]] physics::Collider2D MakeCircleCollider(
    std::string semanticId,
    const std::uint32_t layerBits = 1U)
{
    physics::Collider2D collider{};
    collider.semanticId = std::move(semanticId);
    collider.shape = physics::ColliderShape2D::Circle;
    collider.radius = 0.5F;
    collider.layerBits = layerBits;
    return collider;
}

[[nodiscard]] scene::EntityId CreatePhysicalEntity(
    scene::Scene& scene,
    const physics::PhysicsComponentTypes2D types,
    std::string semanticId,
    const scene::Vector2 position,
    const physics::RigidBodyType2D bodyType,
    physics::Collider2D collider,
    const bool bullet = false)
{
    scene::EntityDescriptor descriptor{};
    descriptor.semanticId = std::move(semanticId);
    descriptor.name = descriptor.semanticId;
    descriptor.transform.position = position;
    const scene::EntityId entity = scene.CreateEntity(std::move(descriptor));

    physics::RigidBody2D body{};
    body.type = bodyType;
    body.bullet = bullet;
    (void)scene.AddComponent(entity, types.rigidBody, body);
    (void)scene.AddComponent(entity, types.collider, std::move(collider));
    return entity;
}

[[nodiscard]] physics::PhysicsWorldConfig2D ZeroGravityConfig()
{
    physics::PhysicsWorldConfig2D config{};
    config.gravity = {0.0F, 0.0F};
    return config;
}
} // namespace

TEST(PhysicsWorld2DPHYS3, RuntimeCommandsWakeBodiesAndTeleportSceneAuthorityImmediately)
{
    scene::ComponentRegistry registry{};
    const physics::PhysicsComponentTypes2D types = physics::RegisterPhysics2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};

    const scene::EntityId dynamic = CreatePhysicalEntity(
        scene,
        types,
        "dynamic",
        {},
        physics::RigidBodyType2D::Dynamic,
        MakeCircleCollider("dynamic_collider"));

    physics::PhysicsWorld2D world{scene, types, ZeroGravityConfig()};
    world.Reserve(2U, 0U);
    ASSERT_EQ(world.AttachEntity(dynamic), physics::PhysicsAttachResult2D::Success);

    EXPECT_EQ(
        world.SetLinearVelocity(dynamic, {3.0F, -2.0F}),
        physics::PhysicsBodyCommandResult2D::Success);
    EXPECT_EQ(
        world.SetAngularVelocity(dynamic, 0.75F),
        physics::PhysicsBodyCommandResult2D::Success);

    physics::PhysicsBodyState2D state{};
    ASSERT_TRUE(world.TryGetBodyState(dynamic, state));
    EXPECT_FLOAT_EQ(state.linearVelocity.x, 3.0F);
    EXPECT_FLOAT_EQ(state.linearVelocity.y, -2.0F);
    EXPECT_FLOAT_EQ(state.angularVelocity, 0.75F);
    EXPECT_TRUE(state.awake);

    EXPECT_EQ(
        world.ApplyLinearImpulseToCenter(dynamic, {2.0F, 0.0F}),
        physics::PhysicsBodyCommandResult2D::Success);
    ASSERT_TRUE(world.TryGetBodyState(dynamic, state));
    EXPECT_GT(state.linearVelocity.x, 3.0F);

    EXPECT_EQ(
        world.ApplyForceToCenter(dynamic, {4.0F, 0.0F}),
        physics::PhysicsBodyCommandResult2D::Success);
    const float beforeForceStepX = state.linearVelocity.x;
    ASSERT_EQ(world.Step(1.0F / 60.0F), physics::PhysicsStepResult2D::Success);
    ASSERT_TRUE(world.TryGetBodyState(dynamic, state));
    EXPECT_GT(state.linearVelocity.x, beforeForceStepX);

    constexpr scene::Vector2 teleportedPosition{7.0F, -3.0F};
    constexpr float teleportedRotation = 0.35F;
    EXPECT_EQ(
        world.Teleport(dynamic, teleportedPosition, teleportedRotation),
        physics::PhysicsBodyCommandResult2D::Success);

    const scene::Entity* const sceneEntity = scene.TryGet(dynamic);
    ASSERT_NE(sceneEntity, nullptr);
    EXPECT_FLOAT_EQ(sceneEntity->Transform().position.x, teleportedPosition.x);
    EXPECT_FLOAT_EQ(sceneEntity->Transform().position.y, teleportedPosition.y);
    EXPECT_FLOAT_EQ(sceneEntity->Transform().rotationRadians, teleportedRotation);

    ASSERT_TRUE(world.TryGetBodyState(dynamic, state));
    EXPECT_FLOAT_EQ(state.position.x, teleportedPosition.x);
    EXPECT_FLOAT_EQ(state.position.y, teleportedPosition.y);
    EXPECT_NEAR(state.rotationRadians, teleportedRotation, 0.0001F);
    EXPECT_TRUE(state.awake);

    const physics::PhysicsMetrics2D metrics = world.Metrics();
    EXPECT_EQ(metrics.bodyCommandCount, 5U);
    EXPECT_EQ(metrics.bodyCommandFailureCount, 0U);
}

TEST(PhysicsWorld2DPHYS3, RuntimeCommandsRejectInvalidBodyTypesInputsAndStaleTargetsExplicitly)
{
    scene::ComponentRegistry registry{};
    const physics::PhysicsComponentTypes2D types = physics::RegisterPhysics2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};

    const scene::EntityId staticBody = CreatePhysicalEntity(
        scene,
        types,
        "static",
        {},
        physics::RigidBodyType2D::Static,
        MakeCircleCollider("static_collider"));
    const scene::EntityId kinematic = CreatePhysicalEntity(
        scene,
        types,
        "kinematic",
        {2.0F, 0.0F},
        physics::RigidBodyType2D::Kinematic,
        MakeCircleCollider("kinematic_collider"));
    const scene::EntityId unattached = CreatePhysicalEntity(
        scene,
        types,
        "unattached",
        {4.0F, 0.0F},
        physics::RigidBodyType2D::Dynamic,
        MakeCircleCollider("unattached_collider"));
    const scene::EntityId stale = CreatePhysicalEntity(
        scene,
        types,
        "stale",
        {6.0F, 0.0F},
        physics::RigidBodyType2D::Dynamic,
        MakeCircleCollider("stale_collider"));

    physics::PhysicsWorld2D world{scene, types, ZeroGravityConfig()};
    world.Reserve(4U, 0U);
    ASSERT_EQ(world.AttachEntity(staticBody), physics::PhysicsAttachResult2D::Success);
    ASSERT_EQ(world.AttachEntity(kinematic), physics::PhysicsAttachResult2D::Success);
    ASSERT_EQ(world.AttachEntity(stale), physics::PhysicsAttachResult2D::Success);

    EXPECT_EQ(
        world.SetLinearVelocity(staticBody, {1.0F, 0.0F}),
        physics::PhysicsBodyCommandResult2D::UnsupportedBodyType);
    EXPECT_EQ(
        world.ApplyForceToCenter(staticBody, {1.0F, 0.0F}),
        physics::PhysicsBodyCommandResult2D::UnsupportedBodyType);
    EXPECT_EQ(
        world.ApplyLinearImpulseToCenter(kinematic, {1.0F, 0.0F}),
        physics::PhysicsBodyCommandResult2D::UnsupportedBodyType);
    EXPECT_EQ(
        world.SetLinearVelocity(kinematic, {1.0F, 0.0F}),
        physics::PhysicsBodyCommandResult2D::Success);
    EXPECT_EQ(
        world.SetAngularVelocity(unattached, 1.0F),
        physics::PhysicsBodyCommandResult2D::NotAttached);
    EXPECT_EQ(
        world.SetAngularVelocity(kinematic, std::numeric_limits<float>::quiet_NaN()),
        physics::PhysicsBodyCommandResult2D::InvalidInput);

    ASSERT_TRUE(scene.DestroyEntity(stale));
    EXPECT_EQ(
        world.Teleport(stale, {0.0F, 0.0F}, 0.0F),
        physics::PhysicsBodyCommandResult2D::EntityNotFound);
    EXPECT_FALSE(world.Contains(stale));

    const physics::PhysicsMetrics2D metrics = world.Metrics();
    EXPECT_EQ(metrics.bodyCommandCount, 7U);
    EXPECT_EQ(metrics.bodyCommandFailureCount, 6U);
    EXPECT_EQ(metrics.stalePruneCount, 1U);
}

TEST(PhysicsWorld2DPHYS3, CircleAndRotatedBoxCastsAreStableFilteredBoundedAndPruneStaleEntities)
{
    scene::ComponentRegistry registry{};
    const physics::PhysicsComponentTypes2D types = physics::RegisterPhysics2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};

    const scene::EntityId bEntity = CreatePhysicalEntity(
        scene,
        types,
        "body_b",
        {2.0F, 0.0F},
        physics::RigidBodyType2D::Static,
        MakeCircleCollider("b_collider", 2U));
    const scene::EntityId aEntity = CreatePhysicalEntity(
        scene,
        types,
        "body_a",
        {2.0F, 0.0F},
        physics::RigidBodyType2D::Static,
        MakeCircleCollider("a_collider", 2U));
    const scene::EntityId excludedEntity = CreatePhysicalEntity(
        scene,
        types,
        "excluded",
        {4.0F, 0.0F},
        physics::RigidBodyType2D::Static,
        MakeCircleCollider("excluded_collider", 4U));

    physics::PhysicsWorld2D world{scene, types, ZeroGravityConfig()};
    world.Reserve(4U, 0U);
    world.ReserveShapeCast(1U);
    ASSERT_EQ(world.AttachEntity(bEntity), physics::PhysicsAttachResult2D::Success);
    ASSERT_EQ(world.AttachEntity(aEntity), physics::PhysicsAttachResult2D::Success);
    ASSERT_EQ(world.AttachEntity(excludedEntity), physics::PhysicsAttachResult2D::Success);

    std::array<physics::PhysicsShapeCastHit2D, 4U> hits{};
    const physics::PhysicsCircleCastQuery2D circle{
        {-3.0F, 0.0F},
        0.25F,
        {10.0F, 0.0F},
        1U,
        2U,
    };

    const physics::PhysicsShapeCastReport2D small = world.CastCircle(circle, hits);
    EXPECT_EQ(small.result, physics::PhysicsQueryResult2D::CapacityExceeded);
    EXPECT_EQ(small.hitCount, 0U);
    EXPECT_EQ(small.requiredCapacity, 2U);

    world.ReserveShapeCast(4U);
    const physics::PhysicsShapeCastReport2D circleReport = world.CastCircle(circle, hits);
    ASSERT_EQ(circleReport.result, physics::PhysicsQueryResult2D::Success);
    ASSERT_EQ(circleReport.hitCount, 2U);
    EXPECT_EQ(hits[0].ColliderSemanticId(), "a_collider");
    EXPECT_EQ(hits[1].ColliderSemanticId(), "b_collider");
    EXPECT_EQ(hits[0].entity, aEntity);
    EXPECT_EQ(hits[1].entity, bEntity);
    EXPECT_NE(hits[0].entity, excludedEntity);
    EXPECT_FLOAT_EQ(hits[0].fraction, hits[1].fraction);
    EXPECT_TRUE(std::isfinite(hits[0].point.x));
    EXPECT_TRUE(std::isfinite(hits[0].normal.x));

    const physics::PhysicsBoxCastQuery2D box{
        {-3.0F, 0.0F},
        {0.25F, 0.5F},
        0.35F,
        {10.0F, 0.0F},
        1U,
        2U,
    };
    const physics::PhysicsShapeCastReport2D boxReport = world.CastBox(box, hits);
    ASSERT_EQ(boxReport.result, physics::PhysicsQueryResult2D::Success);
    ASSERT_EQ(boxReport.hitCount, 2U);
    EXPECT_EQ(hits[0].ColliderSemanticId(), "a_collider");
    EXPECT_EQ(hits[1].ColliderSemanticId(), "b_collider");

    ASSERT_TRUE(scene.DestroyEntity(aEntity));
    const physics::PhysicsShapeCastReport2D afterDestroy = world.CastCircle(circle, hits);
    ASSERT_EQ(afterDestroy.result, physics::PhysicsQueryResult2D::Success);
    ASSERT_EQ(afterDestroy.hitCount, 1U);
    EXPECT_EQ(hits[0].entity, bEntity);
    EXPECT_EQ(hits[0].ColliderSemanticId(), "b_collider");

    const physics::PhysicsMetrics2D metrics = world.Metrics();
    EXPECT_EQ(metrics.shapeCastQueryCount, 4U);
    EXPECT_EQ(metrics.shapeCastCapacityFailureCount, 1U);
    EXPECT_EQ(metrics.stalePruneCount, 1U);
    EXPECT_GE(metrics.retainedShapeCastHitCapacity, 4U);
}

TEST(PhysicsWorld2DPHYS3, TeleportSupportsStaticBodiesButVelocityAndForceDoNot)
{
    scene::ComponentRegistry registry{};
    const physics::PhysicsComponentTypes2D types = physics::RegisterPhysics2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};

    const scene::EntityId staticBody = CreatePhysicalEntity(
        scene,
        types,
        "wall",
        {},
        physics::RigidBodyType2D::Static,
        MakeCircleCollider("wall_collider"));

    physics::PhysicsWorld2D world{scene, types, ZeroGravityConfig()};
    ASSERT_EQ(world.AttachEntity(staticBody), physics::PhysicsAttachResult2D::Success);

    EXPECT_EQ(
        world.Teleport(staticBody, {5.0F, 2.0F}, 0.2F),
        physics::PhysicsBodyCommandResult2D::Success);
    EXPECT_EQ(
        world.SetLinearVelocity(staticBody, {1.0F, 0.0F}),
        physics::PhysicsBodyCommandResult2D::UnsupportedBodyType);
    EXPECT_EQ(
        world.ApplyForceToCenter(staticBody, {1.0F, 0.0F}),
        physics::PhysicsBodyCommandResult2D::UnsupportedBodyType);

    physics::PhysicsBodyState2D state{};
    ASSERT_TRUE(world.TryGetBodyState(staticBody, state));
    EXPECT_FLOAT_EQ(state.position.x, 5.0F);
    EXPECT_FLOAT_EQ(state.position.y, 2.0F);

    const scene::Entity* const sceneEntity = scene.TryGet(staticBody);
    ASSERT_NE(sceneEntity, nullptr);
    EXPECT_FLOAT_EQ(sceneEntity->Transform().position.x, 5.0F);
    EXPECT_FLOAT_EQ(sceneEntity->Transform().position.y, 2.0F);
}
