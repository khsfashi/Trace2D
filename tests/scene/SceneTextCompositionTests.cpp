#include <trace2d/scene/SceneText.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
struct Health final { std::int64_t current{100}; std::int64_t maximum{100}; };

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
    registration.parseAuthored = [](const auto& authored, Health& health, std::string& error)
    {
        if (authored.fields.size() != 2U)
        {
            error = "game.health requires exactly current and maximum.";
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
        authored.fields.push_back({"maximum", IntValue(health.maximum)});
        authored.fields.push_back({"current", IntValue(health.current)});
        return authored;
    };
    registration.inspect = [](const Health& health)
    {
        return std::vector<trace2d::scene::ComponentInspectionField>{
            {"maximum", IntValue(health.maximum)}, {"current", IntValue(health.current)}};
    };
    return registry.Register(std::move(registration));
}

const trace2d::scene::SceneTextDiagnostic* FindDiagnostic(
    const trace2d::scene::SceneLoadResult& result,
    const std::string_view path)
{
    const auto iterator = std::find_if(result.diagnostics.begin(), result.diagnostics.end(), [path](const auto& diagnostic)
    {
        return std::string_view{diagnostic.path} == path;
    });
    return iterator == result.diagnostics.end() ? nullptr : &*iterator;
}

constexpr std::string_view AuthoredScene = R"toml(format_version = 2

[scene]
id = "e2"
name = "E2"

[[entities]]
id = "player"
name = "Player"
tags = ["hero"]

[entities.transform]
position = [10, 0]
rotation_radians = 0
scale = [1, 1]

[[entities.components]]
type = "trace2d.visibility2d"
version = 1

[entities.components.data]
visible = true

[[entities.components]]
type = "game.health"
version = 1

[entities.components.data]
current = 75
maximum = 100

[[entities]]
id = "weapon"
name = "Weapon"
parent = "player"

[entities.transform]
position = [2, 0]
rotation_radians = 0
scale = [1, 1]
)toml";

TEST(SceneTextCompositionTests, LoadsHierarchyAndExternalAuthoredComponentThenRoundTripsCanonically)
{
    trace2d::scene::ComponentRegistry registry{};
    (void)trace2d::scene::RegisterSceneComponents(registry);
    const auto healthType = RegisterHealth(registry);
    registry.Freeze();

    auto first = trace2d::scene::LoadSceneToml(AuthoredScene, registry, "e2.trace2d.toml");
    ASSERT_TRUE(first.Succeeded());
    const auto player = first.scene->FindBySemanticId("player");
    const auto weapon = first.scene->FindBySemanticId("weapon");
    ASSERT_TRUE(player.has_value());
    ASSERT_TRUE(weapon.has_value());
    ASSERT_EQ(first.scene->TryGet(*weapon)->Parent(), player);

    trace2d::scene::Transform2D weaponWorld{};
    ASSERT_TRUE(first.scene->TryGetWorldTransform(*weapon, weaponWorld));
    EXPECT_FLOAT_EQ(weaponWorld.position.x, 12.0F);

    const Health* health = first.scene->TryGetComponent(*player, healthType);
    ASSERT_NE(health, nullptr);
    EXPECT_EQ(health->current, 75);
    EXPECT_EQ(health->maximum, 100);

    const auto firstSave = trace2d::scene::SaveSceneToml(*first.scene);
    ASSERT_TRUE(firstSave.Succeeded());
    EXPECT_NE(firstSave.text.find("format_version = 2"), std::string::npos);
    EXPECT_NE(firstSave.text.find("parent = \"player\""), std::string::npos);
    EXPECT_NE(firstSave.text.find("type = \"game.health\""), std::string::npos);

    auto second = trace2d::scene::LoadSceneToml(firstSave.text, registry, "canonical.trace2d.toml");
    ASSERT_TRUE(second.Succeeded());
    const auto secondSave = trace2d::scene::SaveSceneToml(*second.scene);
    ASSERT_TRUE(secondSave.Succeeded());
    EXPECT_EQ(firstSave.text, secondSave.text);
}

TEST(SceneTextCompositionTests, RequiresRegistryFreezeBeforeAuthoredLoad)
{
    trace2d::scene::ComponentRegistry registry{};
    (void)trace2d::scene::RegisterSceneComponents(registry);
    (void)RegisterHealth(registry);

    const auto result = trace2d::scene::LoadSceneToml(AuthoredScene, registry, "not-frozen.trace2d.toml");
    ASSERT_FALSE(result.Succeeded());
    const auto* diagnostic = FindDiagnostic(result, "$");
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_NE(diagnostic->message.find("frozen"), std::string::npos);
}

TEST(SceneTextCompositionTests, RejectsMissingParentWithSourceReferenceContext)
{
    trace2d::scene::ComponentRegistry registry{};
    (void)trace2d::scene::RegisterSceneComponents(registry);
    (void)RegisterHealth(registry);
    registry.Freeze();

    constexpr std::string_view text = R"toml(format_version = 2
[scene]
id = "bad-parent"
[[entities]]
id = "child"
parent = "missing"
)toml";
    const auto result = trace2d::scene::LoadSceneToml(text, registry, "bad-parent.trace2d.toml");
    ASSERT_FALSE(result.Succeeded());
    const auto* diagnostic = FindDiagnostic(result, "entities[0].parent");
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_NE(diagnostic->message.find("missing"), std::string::npos);
}

TEST(SceneTextCompositionTests, RejectsHierarchyCyclesDeterministically)
{
    trace2d::scene::ComponentRegistry registry{};
    registry.Freeze();
    constexpr std::string_view text = R"toml(format_version = 2
[scene]
id = "cycle"
[[entities]]
id = "a"
parent = "b"
[[entities]]
id = "b"
parent = "a"
)toml";
    const auto result = trace2d::scene::LoadSceneToml(text, registry, "cycle.trace2d.toml");
    ASSERT_FALSE(result.Succeeded());
    const auto* diagnostic = FindDiagnostic(result, "entities[1].parent");
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_NE(diagnostic->message.find("cycle"), std::string::npos);
}

TEST(SceneTextCompositionTests, RejectsDuplicateComponentAndInvalidGameplayState)
{
    trace2d::scene::ComponentRegistry registry{};
    (void)RegisterHealth(registry);
    registry.Freeze();
    constexpr std::string_view duplicate = R"toml(format_version = 2
[scene]
id = "duplicate-component"
[[entities]]
id = "player"
[[entities.components]]
type = "game.health"
version = 1
[entities.components.data]
current = 75
maximum = 100
[[entities.components]]
type = "game.health"
version = 1
[entities.components.data]
current = 50
maximum = 100
)toml";
    const auto duplicateResult = trace2d::scene::LoadSceneToml(duplicate, registry, "duplicate-component.trace2d.toml");
    ASSERT_FALSE(duplicateResult.Succeeded());
    const auto* duplicateDiagnostic = FindDiagnostic(duplicateResult, "entities[0].components[1]");
    ASSERT_NE(duplicateDiagnostic, nullptr);
    EXPECT_NE(duplicateDiagnostic->message.find("duplicate_component"), std::string::npos);

    constexpr std::string_view invalid = R"toml(format_version = 2
[scene]
id = "invalid-health"
[[entities]]
id = "player"
[[entities.components]]
type = "game.health"
version = 1
[entities.components.data]
current = 125
maximum = 100
)toml";
    const auto invalidResult = trace2d::scene::LoadSceneToml(invalid, registry, "invalid-health.trace2d.toml");
    ASSERT_FALSE(invalidResult.Succeeded());
    const auto* invalidDiagnostic = FindDiagnostic(invalidResult, "entities[0].components[0]");
    ASSERT_NE(invalidDiagnostic, nullptr);
    EXPECT_NE(invalidDiagnostic->message.find("validation_failed"), std::string::npos);
}

TEST(SceneTextCompositionTests, LegacyVersionOneSceneRemainsLoadable)
{
    constexpr std::string_view legacy = R"toml(format_version = 1
[scene]
id = "legacy"
[[entities]]
id = "player"
)toml";
    const auto result = trace2d::scene::LoadSceneToml(legacy, "legacy.trace2d.toml");
    ASSERT_TRUE(result.Succeeded());
    const auto saved = trace2d::scene::SaveSceneToml(*result.scene);
    ASSERT_TRUE(saved.Succeeded());
    EXPECT_NE(saved.text.find("format_version = 2"), std::string::npos);
}
} // namespace
