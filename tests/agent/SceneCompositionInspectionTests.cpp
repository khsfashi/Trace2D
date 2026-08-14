#include <trace2d/agent/Inspection.hpp>

#include <trace2d/runtime/FixedStepRuntime.hpp>
#include <trace2d/scene/Scene.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
struct Health final { std::int64_t current{90}; std::int64_t maximum{100}; };

trace2d::scene::SemanticValue IntValue(const std::int64_t value)
{
    trace2d::scene::SemanticValue result{};
    result.kind = trace2d::scene::SemanticValueKind::SignedInteger;
    result.signedIntegerValue = value;
    return result;
}

trace2d::scene::ComponentTypeHandle<Health> RegisterHealth(trace2d::scene::ComponentRegistry& registry)
{
    trace2d::scene::ComponentRegistration<Health> registration{};
    registration.typeId = "game.health";
    registration.schemaVersion = 1;
    registration.componentClass = trace2d::scene::ComponentClass::Authored;
    registration.parseAuthored = [](const auto&, Health&, std::string&) { return true; };
    registration.validate = [](const Health& health, std::string&) { return health.maximum > 0 && health.current >= 0 && health.current <= health.maximum; };
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

const trace2d::agent::ComponentSnapshot* FindComponent(
    const trace2d::agent::EntitySnapshot& entity,
    const std::string_view type)
{
    const auto iterator = std::find_if(entity.components.begin(), entity.components.end(), [type](const auto& component)
    {
        return std::string_view{component.type} == type;
    });
    return iterator == entity.components.end() ? nullptr : &*iterator;
}

const trace2d::agent::ComponentFieldSnapshot* FindField(
    const trace2d::agent::ComponentSnapshot& component,
    const std::string_view name)
{
    const auto iterator = std::find_if(component.fields.begin(), component.fields.end(), [name](const auto& field)
    {
        return std::string_view{field.name} == name;
    });
    return iterator == component.fields.end() ? nullptr : &*iterator;
}

TEST(SceneCompositionInspectionTests, AgentSeesHierarchyWorldTransformAndRegisteredGameplayComponent)
{
    trace2d::scene::ComponentRegistry registry{};
    const auto builtins = trace2d::scene::RegisterSceneComponents(registry);
    const auto healthType = RegisterHealth(registry);
    registry.Freeze();

    trace2d::scene::Scene scene{registry, {.semanticId = "e2", .name = "E2"}};
    trace2d::scene::EntityDescriptor playerDescriptor{};
    playerDescriptor.semanticId = "player";
    playerDescriptor.transform.position = {10.0F, 0.0F};
    const auto player = scene.CreateEntity(std::move(playerDescriptor));
    (void)scene.AddComponent(player, builtins.visibility, trace2d::scene::Visibility2D{true});
    (void)scene.AddComponent(player, healthType, Health{75, 100});

    trace2d::scene::EntityDescriptor weaponDescriptor{};
    weaponDescriptor.semanticId = "weapon";
    weaponDescriptor.transform.position = {2.0F, 0.0F};
    const auto weapon = scene.CreateEntity(std::move(weaponDescriptor));
    ASSERT_EQ(scene.SetParent(weapon, player), trace2d::scene::HierarchyResult::Success);

    trace2d::runtime::FixedStepRuntime runtime{};
    trace2d::agent::AgentFacade agent{&runtime, &scene};
    const auto inspection = agent.Inspect();
    ASSERT_TRUE(inspection.Succeeded());
    ASSERT_EQ(inspection.snapshot->scene.entities.size(), 2U);

    const auto& playerSnapshot = inspection.snapshot->scene.entities[0];
    const auto* health = FindComponent(playerSnapshot, "game.health");
    ASSERT_NE(health, nullptr);
    EXPECT_EQ(health->schemaVersion, 1U);
    EXPECT_TRUE(health->authored);
    ASSERT_EQ(health->fields.size(), 2U);
    EXPECT_EQ(health->fields[0].name, "current");
    EXPECT_EQ(health->fields[0].value.kind, trace2d::agent::FieldValueKind::SignedInteger);
    EXPECT_EQ(health->fields[0].value.signedIntegerValue, 75);

    const auto& weaponSnapshot = inspection.snapshot->scene.entities[1];
    ASSERT_TRUE(weaponSnapshot.parentSemanticId.has_value());
    EXPECT_EQ(*weaponSnapshot.parentSemanticId, "player");
    EXPECT_FLOAT_EQ(weaponSnapshot.transform.position.x, 2.0F);
    EXPECT_FLOAT_EQ(weaponSnapshot.worldTransform.position.x, 12.0F);

    const auto* hierarchy = FindComponent(weaponSnapshot, "Hierarchy2D");
    ASSERT_NE(hierarchy, nullptr);
    const auto* hierarchyParent = FindField(*hierarchy, "parent");
    ASSERT_NE(hierarchyParent, nullptr);
    EXPECT_EQ(hierarchyParent->value.kind, trace2d::agent::FieldValueKind::String);
    EXPECT_EQ(hierarchyParent->value.stringValue, "player");
    const auto* worldX = FindField(*hierarchy, "world.position.x");
    ASSERT_NE(worldX, nullptr);
    EXPECT_EQ(worldX->value.kind, trace2d::agent::FieldValueKind::Float);
    EXPECT_FLOAT_EQ(worldX->value.floatValue, 12.0F);

    const auto byType = agent.QueryOne("type:game.health");
    ASSERT_TRUE(byType.Succeeded());
    EXPECT_EQ(byType.match->semanticId, "player");
    EXPECT_NE(FindComponent(*byType.match, "game.health"), nullptr);

    const auto hierarchyQuery = agent.Query("type:Hierarchy2D");
    ASSERT_TRUE(hierarchyQuery.Succeeded());
    EXPECT_EQ(hierarchyQuery.matches.size(), 2U);
}
} // namespace
