#include <trace2d/agent/Inspection.hpp>

#include <trace2d/runtime/FixedStepRuntime.hpp>
#include <trace2d/scene/Scene.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace
{
using namespace std::chrono_literals;

trace2d::scene::EntityDescriptor MakeEntity(
    std::string semanticId,
    std::string name,
    std::vector<std::string> tags = {})
{
    trace2d::scene::EntityDescriptor descriptor{};
    descriptor.semanticId = std::move(semanticId);
    descriptor.name = std::move(name);
    descriptor.tags = std::move(tags);
    return descriptor;
}

TEST(AgentInspectionTests, ReportsUnavailableRuntimeWithStableCode)
{
    trace2d::scene::Scene scene{{.semanticId = "arena", .name = "Arena"}};
    const trace2d::agent::AgentFacade facade{nullptr, &scene};

    const trace2d::agent::InspectionResult result = facade.Inspect();

    ASSERT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.snapshot.has_value());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->code, trace2d::agent::InspectionErrorCode::RuntimeUnavailable);
    EXPECT_EQ(trace2d::agent::ToString(result.error->code), "runtime_unavailable");
    EXPECT_FALSE(result.error->message.empty());
}

TEST(AgentInspectionTests, ReportsUnavailableSceneWithStableCode)
{
    trace2d::runtime::FixedStepRuntime runtime{};
    const trace2d::agent::AgentFacade facade{&runtime, nullptr};

    const trace2d::agent::InspectionResult result = facade.Inspect();

    ASSERT_FALSE(result.Succeeded());
    ASSERT_FALSE(result.snapshot.has_value());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->code, trace2d::agent::InspectionErrorCode::SceneUnavailable);
    EXPECT_EQ(trace2d::agent::ToString(result.error->code), "scene_unavailable");
    EXPECT_FALSE(result.error->message.empty());
}

TEST(AgentInspectionTests, CapturesDeterministicRuntimeAndSceneState)
{
    trace2d::runtime::RuntimeConfig runtimeConfig{};
    runtimeConfig.fixedTimestep = 5ms;
    runtimeConfig.seed = 77;
    trace2d::runtime::FixedStepRuntime runtime{runtimeConfig};
    runtime.Step(3);

    trace2d::scene::Scene scene{{.semanticId = "arena", .name = "Arena"}};

    trace2d::scene::EntityDescriptor player =
        MakeEntity("player", "Player", {"hero", "controllable", "hero"});
    player.transform.position = {1.5F, -2.0F};
    player.transform.rotationRadians = 0.25F;
    player.transform.scale = {2.0F, 3.0F};
    const trace2d::scene::EntityId playerId = scene.CreateEntity(std::move(player));

    trace2d::scene::EntityDescriptor enemy = MakeEntity("enemy", "Enemy", {"enemy"});
    enemy.transform.position = {8.0F, 4.0F};
    const trace2d::scene::EntityId enemyId = scene.CreateEntity(std::move(enemy));

    const trace2d::agent::AgentFacade facade{&runtime, &scene};

    const trace2d::agent::InspectionResult first = facade.Inspect();
    const trace2d::agent::InspectionResult second = facade.Inspect();

    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(first.snapshot.has_value());
    ASSERT_TRUE(second.Succeeded());
    ASSERT_TRUE(second.snapshot.has_value());
    EXPECT_EQ(first.snapshot, second.snapshot);

    const trace2d::agent::InspectionSnapshot& snapshot = *first.snapshot;
    EXPECT_EQ(snapshot.runtime.frame, 3U);
    EXPECT_EQ(snapshot.runtime.seed, 77U);
    EXPECT_EQ(snapshot.runtime.fixedStepNanoseconds, 5'000'000);
    EXPECT_EQ(snapshot.runtime.simulationTimeNanoseconds, 15'000'000);

    EXPECT_EQ(snapshot.scene.semanticId, "arena");
    EXPECT_EQ(snapshot.scene.name, "Arena");
    ASSERT_EQ(snapshot.scene.entities.size(), 2U);

    const trace2d::agent::EntitySnapshot& playerSnapshot = snapshot.scene.entities[0];
    EXPECT_EQ(playerSnapshot.handle.index, playerId.index);
    EXPECT_EQ(playerSnapshot.handle.generation, playerId.generation);
    EXPECT_EQ(playerSnapshot.semanticId, "player");
    EXPECT_EQ(playerSnapshot.name, "Player");
    EXPECT_EQ(playerSnapshot.tags, (std::vector<std::string>{"controllable", "hero"}));
    EXPECT_FLOAT_EQ(playerSnapshot.transform.position.x, 1.5F);
    EXPECT_FLOAT_EQ(playerSnapshot.transform.position.y, -2.0F);
    EXPECT_FLOAT_EQ(playerSnapshot.transform.rotationRadians, 0.25F);
    EXPECT_FLOAT_EQ(playerSnapshot.transform.scale.x, 2.0F);
    EXPECT_FLOAT_EQ(playerSnapshot.transform.scale.y, 3.0F);
    EXPECT_FALSE(playerSnapshot.bounds.has_value());

    ASSERT_EQ(playerSnapshot.components.size(), 1U);
    const trace2d::agent::ComponentSnapshot& transformComponent = playerSnapshot.components[0];
    EXPECT_EQ(transformComponent.type, "Transform2D");
    ASSERT_EQ(transformComponent.fields.size(), 5U);
    EXPECT_EQ(transformComponent.fields[0].name, "position.x");
    EXPECT_EQ(transformComponent.fields[0].value.kind, trace2d::agent::FieldValueKind::Float);
    EXPECT_FLOAT_EQ(transformComponent.fields[0].value.floatValue, 1.5F);
    EXPECT_EQ(transformComponent.fields[1].name, "position.y");
    EXPECT_EQ(transformComponent.fields[2].name, "rotation_radians");
    EXPECT_EQ(transformComponent.fields[3].name, "scale.x");
    EXPECT_EQ(transformComponent.fields[4].name, "scale.y");

    const trace2d::agent::EntitySnapshot& enemySnapshot = snapshot.scene.entities[1];
    EXPECT_EQ(enemySnapshot.handle.index, enemyId.index);
    EXPECT_EQ(enemySnapshot.handle.generation, enemyId.generation);
    EXPECT_EQ(enemySnapshot.semanticId, "enemy");
    EXPECT_EQ(enemySnapshot.name, "Enemy");
}

TEST(AgentInspectionTests, RebindingChangesTheAuthoritativeInspectionSource)
{
    trace2d::runtime::FixedStepRuntime firstRuntime{};
    trace2d::runtime::FixedStepRuntime secondRuntime{};
    secondRuntime.Step(9);

    trace2d::scene::Scene firstScene{{.semanticId = "first", .name = "First"}};
    trace2d::scene::Scene secondScene{{.semanticId = "second", .name = "Second"}};

    trace2d::agent::AgentFacade facade{&firstRuntime, &firstScene};
    facade.BindRuntime(&secondRuntime);
    facade.BindScene(&secondScene);

    const trace2d::agent::InspectionResult result = facade.Inspect();

    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.snapshot.has_value());
    EXPECT_EQ(result.snapshot->runtime.frame, 9U);
    EXPECT_EQ(result.snapshot->scene.semanticId, "second");
}
} // namespace
