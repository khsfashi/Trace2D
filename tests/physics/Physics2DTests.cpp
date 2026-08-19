#include <trace2d/physics/PhysicsComponents2D.hpp>
#include <trace2d/physics/PhysicsWorld2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace
{
using namespace trace2d;

[[nodiscard]] scene::SemanticValue TextValue(std::string value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::Text;
    semantic.textValue = std::move(value);
    return semantic;
}

[[nodiscard]] scene::SemanticValue FloatValue(const double value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::Float;
    semantic.floatValue = value;
    return semantic;
}

[[nodiscard]] scene::SemanticValue Float2Value(const double x, const double y)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::Float2;
    semantic.vectorValue[0] = x;
    semantic.vectorValue[1] = y;
    return semantic;
}

[[nodiscard]] scene::SemanticValue BooleanValue(const bool value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::Boolean;
    semantic.booleanValue = value;
    return semantic;
}

[[nodiscard]] scene::SemanticValue IntegerValue(const std::int64_t value)
{
    scene::SemanticValue semantic{};
    semantic.kind = scene::SemanticValueKind::SignedInteger;
    semantic.signedIntegerValue = value;
    return semantic;
}

[[nodiscard]] physics::Collider2D MakeCollider(std::string semanticId, const std::uint32_t layerBits = 1U)
{
    physics::Collider2D collider{};
    collider.semanticId = std::move(semanticId);
    collider.layerBits = layerBits;
    return collider;
}

[[nodiscard]] scene::EntityId CreatePhysicalEntity(
    scene::Scene& scene,
    const physics::PhysicsComponentTypes2D types,
    std::string semanticId,
    const scene::Vector2 position,
    const physics::RigidBodyType2D bodyType,
    physics::Collider2D collider)
{
    scene::EntityDescriptor descriptor{};
    descriptor.semanticId = std::move(semanticId);
    descriptor.name = descriptor.semanticId;
    descriptor.transform.position = position;
    const scene::EntityId entity = scene.CreateEntity(std::move(descriptor));
    physics::RigidBody2D body{};
    body.type = bodyType;
    (void)scene.AddComponent(entity, types.rigidBody, body);
    (void)scene.AddComponent(entity, types.collider, std::move(collider));
    return entity;
}
} // namespace

TEST(Physics2DComponents, AuthoredBodyAndColliderParseSerializeAndInspect)
{
    scene::ComponentRegistry registry{};
    const physics::PhysicsComponentTypes2D types = physics::RegisterPhysics2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};
    const scene::EntityId entity = scene.CreateEntity(scene::EntityDescriptor{"player", "Player"});

    scene::ComponentAuthoringObject body{};
    body.fields = {
        {"body_type", TextValue("dynamic")},
        {"linear_velocity", Float2Value(3.0, -2.0)},
        {"angular_velocity", FloatValue(0.25)},
        {"linear_damping", FloatValue(0.1)},
        {"angular_damping", FloatValue(0.2)},
        {"gravity_scale", FloatValue(0.75)},
        {"fixed_rotation", BooleanValue(true)},
        {"bullet", BooleanValue(true)},
    };
    std::string error{};
    EXPECT_EQ(
        scene.AddAuthoredComponent(entity, "trace2d.rigidbody2d", 1U, body, error),
        scene::ComponentAttachResult::Success) << error;

    scene::ComponentAuthoringObject collider{};
    collider.fields = {
        {"semantic_id", TextValue("player_hurtbox")},
        {"shape", TextValue("circle")},
        {"local_offset", Float2Value(0.0, 0.25)},
        {"half_extents", Float2Value(0.5, 1.0)},
        {"radius", FloatValue(0.75)},
        {"layer_bits", IntegerValue(8)},
        {"mask_bits", IntegerValue(0xFFFFFFFFLL)},
        {"sensor", BooleanValue(false)},
        {"density", FloatValue(2.0)},
        {"friction", FloatValue(0.4)},
        {"restitution", FloatValue(0.15)},
    };
    error.clear();
    EXPECT_EQ(
        scene.AddAuthoredComponent(entity, "trace2d.collider2d", 1U, collider, error),
        scene::ComponentAttachResult::Success) << error;

    const physics::RigidBody2D* const parsedBody = scene.TryGetComponent(entity, types.rigidBody);
    const physics::Collider2D* const parsedCollider = scene.TryGetComponent(entity, types.collider);
    ASSERT_NE(parsedBody, nullptr);
    ASSERT_NE(parsedCollider, nullptr);
    EXPECT_EQ(parsedBody->type, physics::RigidBodyType2D::Dynamic);
    EXPECT_FLOAT_EQ(parsedBody->linearVelocity.x, 3.0F);
    EXPECT_TRUE(parsedBody->fixedRotation);
    EXPECT_TRUE(parsedBody->bullet);
    EXPECT_EQ(parsedCollider->shape, physics::ColliderShape2D::Circle);
    EXPECT_EQ(parsedCollider->semanticId, "player_hurtbox");
    EXPECT_EQ(parsedCollider->layerBits, 8U);
    EXPECT_EQ(parsedCollider->maskBits, 0xFFFFFFFFU);

    error.clear();
    const auto authored = scene.SerializeAuthoredComponents(entity, error);
    EXPECT_TRUE(error.empty()) << error;
    EXPECT_EQ(authored.size(), 2U);
    const auto inspected = scene.InspectComponents(entity);
    EXPECT_EQ(inspected.size(), 2U);
}

TEST(PhysicsWorld2D, FixedStepUpdatesAuthoritativeRootTransform)
{
    scene::ComponentRegistry registry{};
    const physics::PhysicsComponentTypes2D types = physics::RegisterPhysics2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};
    const scene::EntityId entity = CreatePhysicalEntity(
        scene, types, "falling_ball", {0.0F, 5.0F}, physics::RigidBodyType2D::Dynamic,
        MakeCollider("ball_collider"));

    physics::PhysicsWorld2D world{scene, types};
    world.Reserve(4U, 8U);
    ASSERT_EQ(world.AttachEntity(entity), physics::PhysicsAttachResult2D::Success);
    for (int step = 0; step < 60; ++step)
    {
        ASSERT_EQ(world.Step(1.0F / 60.0F), physics::PhysicsStepResult2D::Success);
    }

    const scene::Entity* const sceneEntity = scene.TryGet(entity);
    ASSERT_NE(sceneEntity, nullptr);
    EXPECT_LT(sceneEntity->Transform().position.y, 1.0F);

    physics::PhysicsBodyState2D state{};
    ASSERT_TRUE(world.TryGetBodyState(entity, state));
    EXPECT_NEAR(state.position.y, sceneEntity->Transform().position.y, 0.0001F);
    EXPECT_LT(state.linearVelocity.y, 0.0F);

    const physics::PhysicsMetrics2D metrics = world.Metrics();
    EXPECT_EQ(metrics.attachedBodyCount, 1U);
    EXPECT_GE(metrics.retainedBodyCapacity, 4U);
    EXPECT_EQ(metrics.fixedStepCount, 60U);
}

TEST(PhysicsWorld2D, PHYS1RejectsHierarchyAndNonUnitScale)
{
    scene::ComponentRegistry registry{};
    const physics::PhysicsComponentTypes2D types = physics::RegisterPhysics2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};

    const scene::EntityId parent = scene.CreateEntity(scene::EntityDescriptor{"parent", "Parent"});
    const scene::EntityId child = CreatePhysicalEntity(
        scene, types, "child", {}, physics::RigidBodyType2D::Dynamic, MakeCollider("child_collider"));
    ASSERT_EQ(scene.SetParent(child, parent), scene::HierarchyResult::Success);

    const scene::EntityId scaled = CreatePhysicalEntity(
        scene, types, "scaled", {}, physics::RigidBodyType2D::Dynamic, MakeCollider("scaled_collider"));
    scene::Entity* const scaledEntity = scene.TryGet(scaled);
    ASSERT_NE(scaledEntity, nullptr);
    scaledEntity->Transform().scale = {2.0F, 1.0F};

    physics::PhysicsWorld2D world{scene, types};
    EXPECT_EQ(world.AttachEntity(child), physics::PhysicsAttachResult2D::ParentedEntityUnsupported);
    EXPECT_EQ(world.AttachEntity(scaled), physics::PhysicsAttachResult2D::NonUnitScaleUnsupported);
}

TEST(PhysicsWorld2D, DestroyedSceneEntityIsPrunedBeforeStepping)
{
    scene::ComponentRegistry registry{};
    const physics::PhysicsComponentTypes2D types = physics::RegisterPhysics2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};
    const scene::EntityId entity = CreatePhysicalEntity(
        scene, types, "temporary", {}, physics::RigidBodyType2D::Dynamic, MakeCollider("temporary_collider"));

    physics::PhysicsWorld2D world{scene, types};
    ASSERT_EQ(world.AttachEntity(entity), physics::PhysicsAttachResult2D::Success);
    ASSERT_TRUE(scene.DestroyEntity(entity));
    EXPECT_FALSE(world.Contains(entity));
    ASSERT_EQ(world.Step(1.0F / 60.0F), physics::PhysicsStepResult2D::Success);
    EXPECT_FALSE(world.Contains(entity));
    EXPECT_EQ(world.Metrics().stalePruneCount, 1U);
}

TEST(PhysicsWorld2D, RaycastIsFilterableStableSortedAndFailClosedWhenCapacityIsSmall)
{
    scene::ComponentRegistry registry{};
    const physics::PhysicsComponentTypes2D types = physics::RegisterPhysics2DComponents(registry);
    registry.Freeze();
    scene::Scene scene{registry};

    const scene::EntityId bEntity = CreatePhysicalEntity(
        scene, types, "body_b", {0.0F, 0.0F}, physics::RigidBodyType2D::Static,
        MakeCollider("b_collider", 2U));
    const scene::EntityId aEntity = CreatePhysicalEntity(
        scene, types, "body_a", {0.0F, 0.0F}, physics::RigidBodyType2D::Static,
        MakeCollider("a_collider", 2U));
    const scene::EntityId excludedEntity = CreatePhysicalEntity(
        scene, types, "excluded", {2.0F, 0.0F}, physics::RigidBodyType2D::Static,
        MakeCollider("excluded_collider", 4U));

    physics::PhysicsWorld2D world{scene, types};
    world.Reserve(4U, 1U);
    ASSERT_EQ(world.AttachEntity(bEntity), physics::PhysicsAttachResult2D::Success);
    ASSERT_EQ(world.AttachEntity(aEntity), physics::PhysicsAttachResult2D::Success);
    ASSERT_EQ(world.AttachEntity(excludedEntity), physics::PhysicsAttachResult2D::Success);

    const physics::PhysicsRaycastQuery2D query{{-5.0F, 0.0F}, {10.0F, 0.0F}, 1U, 2U};
    std::array<physics::PhysicsRaycastHit2D, 4U> hits{};
    const physics::PhysicsRaycastReport2D small = world.Raycast(query, hits);
    EXPECT_EQ(small.result, physics::PhysicsQueryResult2D::CapacityExceeded);
    EXPECT_EQ(small.hitCount, 0U);
    EXPECT_EQ(small.requiredCapacity, 2U);

    world.Reserve(4U, 4U);
    const physics::PhysicsRaycastReport2D report = world.Raycast(query, hits);
    ASSERT_EQ(report.result, physics::PhysicsQueryResult2D::Success);
    ASSERT_EQ(report.hitCount, 2U);
    EXPECT_EQ(hits[0].ColliderSemanticId(), "a_collider");
    EXPECT_EQ(hits[1].ColliderSemanticId(), "b_collider");
    EXPECT_EQ(hits[0].entity, aEntity);
    EXPECT_EQ(hits[1].entity, bEntity);
    EXPECT_NE(hits[0].entity, excludedEntity);
    EXPECT_EQ(world.Metrics().rayCapacityFailureCount, 1U);
}
