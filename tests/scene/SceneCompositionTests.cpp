#include <trace2d/scene/Scene.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
struct Health final
{
    std::int64_t current{100};
    std::int64_t maximum{100};
};

trace2d::scene::SemanticValue IntValue(const std::int64_t value)
{
    trace2d::scene::SemanticValue result{};
    result.kind = trace2d::scene::SemanticValueKind::SignedInteger;
    result.signedIntegerValue = value;
    return result;
}

trace2d::scene::ComponentTypeHandle<Health> RegisterHealth(
    trace2d::scene::ComponentRegistry& registry,
    std::string typeId = "game.health")
{
    trace2d::scene::ComponentRegistration<Health> registration{};
    registration.typeId = std::move(typeId);
    registration.schemaVersion = 1;
    registration.componentClass = trace2d::scene::ComponentClass::Authored;
    registration.parseAuthored = [](
        const trace2d::scene::ComponentAuthoringObject& authored,
        Health& health,
        std::string& error)
    {
        if (authored.fields.size() != 2U)
        {
            error = "game.health requires current and maximum.";
            return false;
        }
        const auto* current = authored.Find("current");
        const auto* maximum = authored.Find("maximum");
        if (current == nullptr || maximum == nullptr ||
            current->kind != trace2d::scene::SemanticValueKind::SignedInteger ||
            maximum->kind != trace2d::scene::SemanticValueKind::SignedInteger)
        {
            error = "game.health current/maximum must be signed integers.";
            return false;
        }
        health.current = current->signedIntegerValue;
        health.maximum = maximum->signedIntegerValue;
        return true;
    };
    registration.validate = [](const Health& health, std::string& error)
    {
        if (health.maximum <= 0 || health.current < 0 || health.current > health.maximum)
        {
            error = "game.health requires 0 <= current <= maximum and maximum > 0.";
            return false;
        }
        return true;
    };
    registration.serializeAuthored = [](const Health& health)
    {
        trace2d::scene::ComponentAuthoringObject authored{};
        authored.fields.push_back({"current", IntValue(health.current)});
        authored.fields.push_back({"maximum", IntValue(health.maximum)});
        return authored;
    };
    registration.inspect = [](const Health& health)
    {
        return std::vector<trace2d::scene::ComponentInspectionField>{
            {"current", IntValue(health.current)},
            {"maximum", IntValue(health.maximum)},
        };
    };
    return registry.Register(std::move(registration));
}

trace2d::scene::EntityDescriptor MakeEntity(std::string id, const float x = 0.0F, const float y = 0.0F)
{
    trace2d::scene::EntityDescriptor descriptor{};
    descriptor.semanticId = std::move(id);
    descriptor.transform.position = {x, y};
    return descriptor;
}

TEST(SceneCompositionTests, HierarchyUsesLocalTransformsAndDeterministicChildren)
{
    trace2d::scene::Scene scene{};
    const auto root = scene.CreateEntity(MakeEntity("root", 10.0F, 5.0F));
    const auto zChild = scene.CreateEntity(MakeEntity("z_child", 2.0F, 0.0F));
    const auto aChild = scene.CreateEntity(MakeEntity("a_child", 1.0F, 0.0F));

    ASSERT_EQ(scene.SetParent(zChild, root), trace2d::scene::HierarchyResult::Success);
    ASSERT_EQ(scene.SetParent(aChild, root), trace2d::scene::HierarchyResult::Success);

    const auto* rootEntity = scene.TryGet(root);
    ASSERT_NE(rootEntity, nullptr);
    ASSERT_EQ(rootEntity->Children().size(), 2U);
    EXPECT_EQ(scene.TryGet(rootEntity->Children()[0])->SemanticId(), "a_child");
    EXPECT_EQ(scene.TryGet(rootEntity->Children()[1])->SemanticId(), "z_child");

    trace2d::scene::Transform2D world{};
    ASSERT_TRUE(scene.TryGetWorldTransform(aChild, world));
    EXPECT_FLOAT_EQ(world.position.x, 11.0F);
    EXPECT_FLOAT_EQ(world.position.y, 5.0F);

    ASSERT_EQ(scene.SetParent(root, aChild), trace2d::scene::HierarchyResult::Cycle);
    EXPECT_FALSE(scene.TryGet(root)->Parent().has_value());
}

TEST(SceneCompositionTests, KeepWorldReparentPreservesWorldTransform)
{
    trace2d::scene::Scene scene{};
    const auto left = scene.CreateEntity(MakeEntity("left", 10.0F, 0.0F));
    const auto right = scene.CreateEntity(MakeEntity("right", -3.0F, 0.0F));
    const auto child = scene.CreateEntity(MakeEntity("child", 2.0F, 0.0F));

    ASSERT_EQ(scene.SetParent(child, left), trace2d::scene::HierarchyResult::Success);
    trace2d::scene::Transform2D before{};
    ASSERT_TRUE(scene.TryGetWorldTransform(child, before));
    EXPECT_FLOAT_EQ(before.position.x, 12.0F);

    ASSERT_EQ(
        scene.SetParent(child, right, trace2d::scene::ReparentMode::KeepWorld),
        trace2d::scene::HierarchyResult::Success);
    trace2d::scene::Transform2D after{};
    ASSERT_TRUE(scene.TryGetWorldTransform(child, after));
    EXPECT_FLOAT_EQ(after.position.x, before.position.x);
    EXPECT_FLOAT_EQ(scene.TryGet(child)->LocalTransform().position.x, 15.0F);
}

TEST(SceneCompositionTests, QuarterTurnUnderNonUniformScaleRemainsExactTrs)
{
    trace2d::scene::Scene scene{};
    auto parentDescriptor = MakeEntity("parent");
    parentDescriptor.transform.scale = {2.0F, 1.0F};
    const auto parent = scene.CreateEntity(std::move(parentDescriptor));

    auto childDescriptor = MakeEntity("child");
    childDescriptor.transform.rotationRadians = std::numbers::pi_v<float> * 0.5F;
    const auto child = scene.CreateEntity(std::move(childDescriptor));
    ASSERT_EQ(scene.SetParent(child, parent), trace2d::scene::HierarchyResult::Success);

    trace2d::scene::Transform2D world{};
    ASSERT_TRUE(scene.TryGetWorldTransform(child, world));
    EXPECT_NEAR(world.rotationRadians, std::numbers::pi_v<float> * 0.5F, 1.0e-6F);
    EXPECT_NEAR(world.scale.x, 1.0F, 1.0e-5F);
    EXPECT_NEAR(world.scale.y, 2.0F, 1.0e-5F);
}

TEST(SceneCompositionTests, HierarchyRejectsWorldTrsWhenCompositionWouldIntroduceShear)
{
    trace2d::scene::Scene scene{};
    auto parentDescriptor = MakeEntity("parent");
    parentDescriptor.transform.scale = {2.0F, 1.0F};
    const auto parent = scene.CreateEntity(std::move(parentDescriptor));

    auto childDescriptor = MakeEntity("child");
    childDescriptor.transform.rotationRadians = 0.3F;
    const auto child = scene.CreateEntity(std::move(childDescriptor));
    ASSERT_EQ(scene.SetParent(child, parent), trace2d::scene::HierarchyResult::Success);

    trace2d::scene::Transform2D world{};
    EXPECT_FALSE(scene.TryGetWorldTransform(child, world));
}

TEST(SceneCompositionTests, KeepWorldRejectsAParentThatWouldRequireShearedLocalState)
{
    trace2d::scene::Scene scene{};
    auto parentDescriptor = MakeEntity("parent");
    parentDescriptor.transform.scale = {2.0F, 1.0F};
    const auto parent = scene.CreateEntity(std::move(parentDescriptor));

    auto childDescriptor = MakeEntity("child");
    childDescriptor.transform.rotationRadians = 0.3F;
    const auto child = scene.CreateEntity(std::move(childDescriptor));

    EXPECT_EQ(
        scene.SetParent(child, parent, trace2d::scene::ReparentMode::KeepWorld),
        trace2d::scene::HierarchyResult::InvalidWorldTransform);
    ASSERT_NE(scene.TryGet(child), nullptr);
    EXPECT_FALSE(scene.TryGet(child)->Parent().has_value());
}

TEST(SceneCompositionTests, DestroyingParentInvalidatesWholeSubtreeGenerations)
{
    trace2d::scene::Scene scene{};
    const auto parent = scene.CreateEntity(MakeEntity("parent"));
    const auto child = scene.CreateEntity(MakeEntity("child"));
    ASSERT_EQ(scene.SetParent(child, parent), trace2d::scene::HierarchyResult::Success);

    ASSERT_TRUE(scene.DestroyEntity(parent));
    EXPECT_FALSE(scene.Contains(parent));
    EXPECT_FALSE(scene.Contains(child));
    EXPECT_EQ(scene.EntityCount(), 0U);
}

TEST(SceneCompositionTests, RegistryFreezesAndTypedHandlesInvalidateWithEntityGeneration)
{
    trace2d::scene::ComponentRegistry registry{};
    const auto builtins = trace2d::scene::RegisterSceneComponents(registry);
    const auto healthType = RegisterHealth(registry);
    EXPECT_THROW((void)RegisterHealth(registry), std::invalid_argument);
    registry.Freeze();

    trace2d::scene::ComponentRegistration<int> late{};
    late.typeId = "game.late";
    EXPECT_THROW((void)registry.Register(std::move(late)), std::logic_error);

    trace2d::scene::Scene scene{registry, {.semanticId = "typed"}};
    const auto entity = scene.CreateEntity(MakeEntity("player"));
    auto& visibility = scene.AddComponent(entity, builtins.visibility, trace2d::scene::Visibility2D{false});
    auto& health = scene.AddComponent(entity, healthType, Health{75, 100});
    EXPECT_FALSE(visibility.visible);
    EXPECT_EQ(health.current, 75);

    const auto healthHandle = scene.MakeComponentHandle(entity, healthType);
    ASSERT_TRUE(healthHandle.IsValid());
    ASSERT_NE(scene.Resolve(healthHandle), nullptr);
    EXPECT_EQ(scene.Resolve(healthHandle)->maximum, 100);

    ASSERT_TRUE(scene.DestroyEntity(entity));
    const auto replacement = scene.CreateEntity(MakeEntity("replacement"));
    ASSERT_EQ(replacement.index, entity.index);
    EXPECT_EQ(scene.Resolve(healthHandle), nullptr);
}

TEST(SceneCompositionTests, AuthoredAdapterValidatesAndInspectionUsesStableTypeIdentity)
{
    trace2d::scene::ComponentRegistry registry{};
    (void)trace2d::scene::RegisterSceneComponents(registry);
    const auto healthType = RegisterHealth(registry);
    registry.Freeze();

    trace2d::scene::Scene scene{registry, {.semanticId = "authored"}};
    const auto entity = scene.CreateEntity(MakeEntity("player"));

    trace2d::scene::ComponentAuthoringObject authored{};
    authored.fields.push_back({"maximum", IntValue(100)});
    authored.fields.push_back({"current", IntValue(80)});
    std::string error{};
    EXPECT_EQ(
        scene.AddAuthoredComponent(entity, "game.health", 1, authored, error),
        trace2d::scene::ComponentAttachResult::Success);
    ASSERT_NE(scene.TryGetComponent(entity, healthType), nullptr);
    EXPECT_EQ(scene.TryGetComponent(entity, healthType)->current, 80);

    const auto inspected = scene.InspectComponents(entity);
    ASSERT_EQ(inspected.size(), 1U);
    EXPECT_EQ(inspected[0].typeId, "game.health");
    EXPECT_EQ(inspected[0].schemaVersion, 1U);
    EXPECT_EQ(inspected[0].fields.size(), 2U);

    EXPECT_EQ(
        scene.AddAuthoredComponent(entity, "game.health", 1, authored, error),
        trace2d::scene::ComponentAttachResult::DuplicateComponent);
}
} // namespace
