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
struct SemanticProbe final {};

trace2d::scene::SemanticValue IntValue(const std::int64_t value)
{
    trace2d::scene::SemanticValue result{};
    result.kind = trace2d::scene::SemanticValueKind::SignedInteger;
    result.signedIntegerValue = value;
    return result;
}

trace2d::scene::SemanticValue TextValue(
    const trace2d::scene::SemanticValueKind kind,
    std::string text)
{
    trace2d::scene::SemanticValue result{};
    result.kind = kind;
    result.textValue = std::move(text);
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

trace2d::scene::ComponentTypeHandle<SemanticProbe> RegisterSemanticProbe(trace2d::scene::ComponentRegistry& registry)
{
    trace2d::scene::ComponentRegistration<SemanticProbe> registration{};
    registration.typeId = "game.semantic_probe";
    registration.componentClass = trace2d::scene::ComponentClass::RuntimeOnly;
    registration.inspect = [](const SemanticProbe&)
    {
        trace2d::scene::SemanticValue boolean{};
        boolean.kind = trace2d::scene::SemanticValueKind::Boolean;
        boolean.booleanValue = true;

        trace2d::scene::SemanticValue unsignedInteger{};
        unsignedInteger.kind = trace2d::scene::SemanticValueKind::UnsignedInteger;
        unsignedInteger.unsignedIntegerValue = 42;

        trace2d::scene::SemanticValue scalar{};
        scalar.kind = trace2d::scene::SemanticValueKind::Float;
        scalar.floatValue = 1.25;

        trace2d::scene::SemanticValue float2{};
        float2.kind = trace2d::scene::SemanticValueKind::Float2;
        float2.vectorValue[0] = 2.0;
        float2.vectorValue[1] = 3.0;

        trace2d::scene::SemanticValue float4{};
        float4.kind = trace2d::scene::SemanticValueKind::Float4;
        float4.vectorValue = {4.0, 5.0, 6.0, 7.0};

        return std::vector<trace2d::scene::ComponentInspectionField>{
            {"bool", boolean},
            {"enum", TextValue(trace2d::scene::SemanticValueKind::EnumName, "idle")},
            {"entity", TextValue(trace2d::scene::SemanticValueKind::EntityReference, "player")},
            {"float", scalar},
            {"float2", float2},
            {"float4", float4},
            {"int", IntValue(-7)},
            {"resource", TextValue(trace2d::scene::SemanticValueKind::ResourceReference, "texture:hero")},
            {"text", TextValue(trace2d::scene::SemanticValueKind::Text, "hello")},
            {"uint", unsignedInteger},
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
    ASSERT_TRUE(weaponSnapshot.worldTransform.has_value());
    EXPECT_FLOAT_EQ(weaponSnapshot.worldTransform->position.x, 12.0F);

    const auto* hierarchy = FindComponent(weaponSnapshot, "Hierarchy2D");
    ASSERT_NE(hierarchy, nullptr);
    const auto* hierarchyParent = FindField(*hierarchy, "parent");
    ASSERT_NE(hierarchyParent, nullptr);
    EXPECT_EQ(hierarchyParent->value.kind, trace2d::agent::FieldValueKind::String);
    EXPECT_EQ(hierarchyParent->value.stringValue, "player");
    const auto* worldAvailable = FindField(*hierarchy, "world_trs_available");
    ASSERT_NE(worldAvailable, nullptr);
    EXPECT_EQ(worldAvailable->value.kind, trace2d::agent::FieldValueKind::Boolean);
    EXPECT_TRUE(worldAvailable->value.booleanValue);
    const auto* worldX = FindField(*hierarchy, "world.position.x");
    ASSERT_NE(worldX, nullptr);
    EXPECT_EQ(worldX->value.kind, trace2d::agent::FieldValueKind::Float);
    EXPECT_FLOAT_EQ(worldX->value.floatValue, 12.0F);

    const auto byType = agent.QueryOne("type:game.health");
    ASSERT_TRUE(byType.Succeeded());
    EXPECT_EQ(byType.match->semanticId, "player");
    EXPECT_NE(FindComponent(*byType.match, "game.health"), nullptr);

    const auto bySemanticIdAfterType = agent.QueryOne("#weapon");
    ASSERT_TRUE(bySemanticIdAfterType.Succeeded());
    EXPECT_EQ(bySemanticIdAfterType.match->semanticId, "weapon");
    ASSERT_TRUE(bySemanticIdAfterType.match->parentSemanticId.has_value());
    EXPECT_EQ(*bySemanticIdAfterType.match->parentSemanticId, "player");

    const auto hierarchyQuery = agent.Query("type:Hierarchy2D");
    ASSERT_TRUE(hierarchyQuery.Succeeded());
    EXPECT_EQ(hierarchyQuery.matches.size(), 2U);
}

TEST(SceneCompositionInspectionTests, AgentDoesNotInventWorldTrsWhenHierarchyIntroducesShear)
{
    trace2d::scene::Scene scene{{.semanticId = "shear", .name = "Shear"}};
    trace2d::scene::EntityDescriptor parentDescriptor{};
    parentDescriptor.semanticId = "parent";
    parentDescriptor.transform.scale = {2.0F, 1.0F};
    const auto parent = scene.CreateEntity(std::move(parentDescriptor));

    trace2d::scene::EntityDescriptor childDescriptor{};
    childDescriptor.semanticId = "child";
    childDescriptor.transform.rotationRadians = 0.3F;
    const auto child = scene.CreateEntity(std::move(childDescriptor));
    ASSERT_EQ(scene.SetParent(child, parent), trace2d::scene::HierarchyResult::Success);

    trace2d::runtime::FixedStepRuntime runtime{};
    trace2d::agent::AgentFacade agent{&runtime, &scene};
    const auto result = agent.QueryOne("#child");
    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.match.has_value());
    EXPECT_FALSE(result.match->worldTransform.has_value());

    const auto* hierarchy = FindComponent(*result.match, "Hierarchy2D");
    ASSERT_NE(hierarchy, nullptr);
    const auto* worldAvailable = FindField(*hierarchy, "world_trs_available");
    ASSERT_NE(worldAvailable, nullptr);
    EXPECT_EQ(worldAvailable->value.kind, trace2d::agent::FieldValueKind::Boolean);
    EXPECT_FALSE(worldAvailable->value.booleanValue);
    EXPECT_EQ(FindField(*hierarchy, "world.position.x"), nullptr);
}

TEST(SceneCompositionInspectionTests, AgentPreservesBoundedSemanticValueKinds)
{
    trace2d::scene::ComponentRegistry registry{};
    const auto probeType = RegisterSemanticProbe(registry);
    registry.Freeze();

    trace2d::scene::Scene scene{registry, {.semanticId = "semantic", .name = "Semantic"}};
    trace2d::scene::EntityDescriptor descriptor{};
    descriptor.semanticId = "probe";
    const auto entity = scene.CreateEntity(std::move(descriptor));
    (void)scene.AddComponent(entity, probeType, SemanticProbe{});

    trace2d::runtime::FixedStepRuntime runtime{};
    trace2d::agent::AgentFacade agent{&runtime, &scene};
    const auto result = agent.QueryOne("type:game.semantic_probe");
    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.match.has_value());
    const auto* probe = FindComponent(*result.match, "game.semantic_probe");
    ASSERT_NE(probe, nullptr);

    EXPECT_EQ(FindField(*probe, "bool")->value.kind, trace2d::agent::FieldValueKind::Boolean);
    EXPECT_EQ(FindField(*probe, "int")->value.kind, trace2d::agent::FieldValueKind::SignedInteger);
    EXPECT_EQ(FindField(*probe, "uint")->value.kind, trace2d::agent::FieldValueKind::UnsignedInteger);
    EXPECT_EQ(FindField(*probe, "float")->value.kind, trace2d::agent::FieldValueKind::Float);
    EXPECT_EQ(FindField(*probe, "text")->value.kind, trace2d::agent::FieldValueKind::String);
    EXPECT_EQ(FindField(*probe, "float2")->value.kind, trace2d::agent::FieldValueKind::Float2);
    EXPECT_EQ(FindField(*probe, "float4")->value.kind, trace2d::agent::FieldValueKind::Float4);
    EXPECT_EQ(FindField(*probe, "entity")->value.kind, trace2d::agent::FieldValueKind::EntityReference);
    EXPECT_EQ(FindField(*probe, "resource")->value.kind, trace2d::agent::FieldValueKind::ResourceReference);
    EXPECT_EQ(FindField(*probe, "enum")->value.kind, trace2d::agent::FieldValueKind::EnumName);

    const auto* float2 = FindField(*probe, "float2");
    ASSERT_NE(float2, nullptr);
    EXPECT_FLOAT_EQ(float2->value.vectorValue[0], 2.0F);
    EXPECT_FLOAT_EQ(float2->value.vectorValue[1], 3.0F);
    const auto* entityReference = FindField(*probe, "entity");
    ASSERT_NE(entityReference, nullptr);
    EXPECT_EQ(entityReference->value.stringValue, "player");
    const auto* resourceReference = FindField(*probe, "resource");
    ASSERT_NE(resourceReference, nullptr);
    EXPECT_EQ(resourceReference->value.stringValue, "texture:hero");
    const auto* enumName = FindField(*probe, "enum");
    ASSERT_NE(enumName, nullptr);
    EXPECT_EQ(enumName->value.stringValue, "idle");
}
} // namespace
