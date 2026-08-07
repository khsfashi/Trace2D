#include <trace2d/scene/SceneText.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using trace2d::scene::Entity;
using trace2d::scene::EntityDescriptor;
using trace2d::scene::LoadSceneToml;
using trace2d::scene::SaveSceneToml;
using trace2d::scene::Scene;
using trace2d::scene::SceneLoadResult;
using trace2d::scene::SceneMetadata;
using trace2d::scene::SceneSaveResult;
using trace2d::scene::SceneTextDiagnostic;

const SceneTextDiagnostic* FindDiagnostic(
    const SceneLoadResult& result,
    const std::string_view path)
{
    const auto iterator = std::find_if(
        result.diagnostics.begin(),
        result.diagnostics.end(),
        [path](const SceneTextDiagnostic& diagnostic)
        {
            return std::string_view{diagnostic.path} == path;
        });
    return iterator == result.diagnostics.end() ? nullptr : &*iterator;
}

TEST(SceneTextTests, LoadsMinimalAuthoredSceneFromToml)
{
    static constexpr std::string_view SceneText = R"toml(format_version = 1

[scene]
id = "arena"
name = "Arena"

[[entities]]
id = "player"
name = "Player"
tags = ["hero", "controllable", "hero"]

[entities.transform]
position = [10, -4.5]
rotation_radians = 0.25
scale = [2, 3]
)toml";

    SceneLoadResult result = LoadSceneToml(SceneText, "minimal.trace2d.toml");

    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.scene.has_value());
    EXPECT_EQ(result.scene->Metadata().semanticId, "arena");
    EXPECT_EQ(result.scene->Metadata().name, "Arena");
    EXPECT_EQ(result.scene->EntityCount(), 1U);

    const auto playerId = result.scene->FindBySemanticId("player");
    ASSERT_TRUE(playerId.has_value());
    const Entity* player = result.scene->TryGet(*playerId);
    ASSERT_NE(player, nullptr);
    EXPECT_EQ(player->Name(), "Player");
    EXPECT_EQ(player->Tags().size(), 2U);
    EXPECT_EQ(player->Tags()[0], "controllable");
    EXPECT_EQ(player->Tags()[1], "hero");
    EXPECT_FLOAT_EQ(player->Transform().position.x, 10.0F);
    EXPECT_FLOAT_EQ(player->Transform().position.y, -4.5F);
    EXPECT_FLOAT_EQ(player->Transform().rotationRadians, 0.25F);
    EXPECT_FLOAT_EQ(player->Transform().scale.x, 2.0F);
    EXPECT_FLOAT_EQ(player->Transform().scale.y, 3.0F);
}

TEST(SceneTextTests, ReportsActionableParseLocation)
{
    const SceneLoadResult result = LoadSceneToml("format_version =\n", "broken.trace2d.toml");

    ASSERT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.diagnostics.empty());
    EXPECT_EQ(result.diagnostics.front().path, "$");
    EXPECT_GT(result.diagnostics.front().line, 0U);
    EXPECT_GT(result.diagnostics.front().column, 0U);
    EXPECT_FALSE(result.diagnostics.front().message.empty());
}

TEST(SceneTextTests, RejectsDuplicateSemanticIdsAtSecondEntity)
{
    static constexpr std::string_view SceneText = R"toml(format_version = 1

[scene]
id = "duplicate-test"

[[entities]]
id = "enemy"

[[entities]]
id = "enemy"
)toml";

    const SceneLoadResult result = LoadSceneToml(SceneText, "duplicate.trace2d.toml");

    ASSERT_FALSE(result.Succeeded());
    const SceneTextDiagnostic* diagnostic = FindDiagnostic(result, "entities[1].id");
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_NE(diagnostic->message.find("Duplicate entity semantic ID"), std::string::npos);
    EXPECT_GT(diagnostic->line, 0U);
    EXPECT_GT(diagnostic->column, 0U);
}

TEST(SceneTextTests, RejectsUnknownFieldsInsteadOfIgnoringTypos)
{
    static constexpr std::string_view SceneText = R"toml(format_version = 1

[scene]
id = "typo-test"

[[entities]]
id = "player"
postion = [1, 2]
)toml";

    const SceneLoadResult result = LoadSceneToml(SceneText, "typo.trace2d.toml");

    ASSERT_FALSE(result.Succeeded());
    const SceneTextDiagnostic* diagnostic = FindDiagnostic(result, "entities[0].postion");
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_EQ(diagnostic->message, "Unknown field.");
    EXPECT_GT(diagnostic->line, 0U);
}

TEST(SceneTextTests, SaveLoadSaveProducesIdenticalCanonicalText)
{
    static constexpr std::string_view SceneText = R"toml(format_version = 1

[scene]
id = "round-trip"
name = "Round Trip"

[[entities]]
id = "z_enemy"
name = "Enemy"
tags = ["enemy", "damageable"]

[entities.transform]
position = [8.25, 2]
rotation_radians = 0
scale = [1, 1]

[[entities]]
id = "a_player"
name = "Player"
tags = ["hero", "controllable"]

[entities.transform]
position = [-1, 3.5]
rotation_radians = 0.125
scale = [1.5, 1.5]
)toml";

    SceneLoadResult firstLoad = LoadSceneToml(SceneText, "round-trip.trace2d.toml");
    ASSERT_TRUE(firstLoad.Succeeded());

    SceneSaveResult firstSave = SaveSceneToml(*firstLoad.scene);
    ASSERT_TRUE(firstSave.Succeeded());

    SceneLoadResult secondLoad = LoadSceneToml(firstSave.text, "canonical.trace2d.toml");
    ASSERT_TRUE(secondLoad.Succeeded());

    SceneSaveResult secondSave = SaveSceneToml(*secondLoad.scene);
    ASSERT_TRUE(secondSave.Succeeded());
    EXPECT_EQ(firstSave.text, secondSave.text);

    const std::size_t playerPosition = firstSave.text.find("id = \"a_player\"");
    const std::size_t enemyPosition = firstSave.text.find("id = \"z_enemy\"");
    ASSERT_NE(playerPosition, std::string::npos);
    ASSERT_NE(enemyPosition, std::string::npos);
    EXPECT_LT(playerPosition, enemyPosition);
}

TEST(SceneTextTests, SerializationOrderDoesNotDependOnCreationOrder)
{
    Scene first{SceneMetadata{"stable", "Stable"}};
    EntityDescriptor firstB{};
    firstB.semanticId = "b";
    static_cast<void>(first.CreateEntity(std::move(firstB)));
    EntityDescriptor firstA{};
    firstA.semanticId = "a";
    static_cast<void>(first.CreateEntity(std::move(firstA)));

    Scene second{SceneMetadata{"stable", "Stable"}};
    EntityDescriptor secondA{};
    secondA.semanticId = "a";
    static_cast<void>(second.CreateEntity(std::move(secondA)));
    EntityDescriptor secondB{};
    secondB.semanticId = "b";
    static_cast<void>(second.CreateEntity(std::move(secondB)));

    const SceneSaveResult firstSave = SaveSceneToml(first);
    const SceneSaveResult secondSave = SaveSceneToml(second);

    ASSERT_TRUE(firstSave.Succeeded());
    ASSERT_TRUE(secondSave.Succeeded());
    EXPECT_EQ(firstSave.text, secondSave.text);
}

TEST(SceneTextTests, RefusesRuntimeOnlyEntitiesWithoutSemanticIdentity)
{
    Scene scene{SceneMetadata{"runtime-only-test", "Runtime Only Test"}};
    EntityDescriptor descriptor{};
    descriptor.name = "Transient";
    static_cast<void>(scene.CreateEntity(std::move(descriptor)));

    const SceneSaveResult result = SaveSceneToml(scene);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_NE(result.diagnostics.front().message.find("Runtime-only entity"), std::string::npos);
}

TEST(SceneTextTests, RefusesNonFiniteTransforms)
{
    Scene scene{SceneMetadata{"finite-test", "Finite Test"}};
    EntityDescriptor descriptor{};
    descriptor.semanticId = "player";
    descriptor.transform.position.x = std::numeric_limits<float>::infinity();
    static_cast<void>(scene.CreateEntity(std::move(descriptor)));

    const SceneSaveResult result = SaveSceneToml(scene);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_NE(result.diagnostics.front().message.find("finite"), std::string::npos);
}
} // namespace
