#include <trace2d/agent/Workspace.hpp>

#include <gtest/gtest.h>

#include <string_view>

namespace
{
TEST(WorkspaceInspectionTests, CarriesExistingAgentInspectionSnapshotWithoutBecomingAuthoritativeState)
{
    constexpr std::string_view specText = R"toml(
format_version = 1
[work]
id = "inspection-work"
intent = "Review live semantic world state alongside result evidence."
state = "implemented"
constraints = []

[[deliverables]]
id = "world"
description = "World review"
state = "implemented"
)toml";
    constexpr std::string_view resultText = R"toml(
format_version = 1
[result]
work_id = "inspection-work"

[[revisions]]
id = "r1"
changed_paths = []
limitations = []
)toml";

    const auto spec = trace2d::agent::ParseWorkSpecToml(specText);
    const auto result = trace2d::agent::ParseWorkResultToml(resultText);
    ASSERT_TRUE(spec.Succeeded());
    ASSERT_TRUE(result.Succeeded());

    trace2d::agent::InspectionSnapshot inspection{};
    inspection.runtime.frame = 42U;
    inspection.scene.semanticId = "scene/main";
    inspection.scene.name = "Main";
    trace2d::agent::EntitySnapshot player{};
    player.semanticId = "player";
    player.name = "Player";
    inspection.scene.entities.push_back(player);

    const auto snapshot = trace2d::agent::BuildWorkspaceSnapshot(*spec.spec, *result.result, &inspection);
    ASSERT_TRUE(snapshot.inspection.has_value());
    EXPECT_EQ(snapshot.inspection->runtime.frame, 42U);
    EXPECT_EQ(snapshot.inspection->scene.semanticId, "scene/main");
    ASSERT_EQ(snapshot.inspection->scene.entities.size(), 1U);
    EXPECT_EQ(snapshot.inspection->scene.entities[0].semanticId, "player");

    inspection.scene.entities[0].semanticId = "mutated-after-snapshot";
    EXPECT_EQ(snapshot.inspection->scene.entities[0].semanticId, "player");
}
} // namespace
