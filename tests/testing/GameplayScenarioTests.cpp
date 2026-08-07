#include <trace2d/testing/GameplayScenario.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string_view>

namespace
{
using trace2d::input::InputControl;
using trace2d::scene::Entity;
using trace2d::testing::GameplayAssertionFailureCode;
using trace2d::testing::GameplayFrameContext;
using trace2d::testing::GameplayScenario;
using trace2d::testing::GameplayScenarioReport;

constexpr std::string_view GameplaySceneText = R"toml(format_version = 1

[scene]
id = "gameplay-test"
name = "Gameplay Test"

[[entities]]
id = "player"
name = "Player"
tags = ["hero", "controllable"]

[entities.transform]
position = [0.0, 0.0]
rotation_radians = 0.0
scale = [1.0, 1.0]
)toml";

constexpr std::string_view AmbiguousSceneText = R"toml(format_version = 1

[scene]
id = "ambiguous-test"
name = "Ambiguous Test"

[[entities]]
id = "enemy_a"
name = "Enemy A"
tags = ["enemy"]

[[entities]]
id = "enemy_b"
name = "Enemy B"
tags = ["enemy"]
)toml";

void MovePlayerWhileRightHeld(GameplayFrameContext& context)
{
    if (!context.input.Held(InputControl::KeyD))
    {
        return;
    }

    const auto playerId = context.scene.FindBySemanticId("player");
    if (!playerId.has_value())
    {
        throw std::logic_error{"Gameplay fixture is missing #player."};
    }

    Entity* const player = context.scene.TryGet(*playerId);
    if (player == nullptr)
    {
        throw std::logic_error{"Gameplay fixture returned a stale #player handle."};
    }

    player->Transform().position.x += 1.0F;
}

GameplayScenarioReport RunDeterministicFailure()
{
    trace2d::runtime::RuntimeConfig config{};
    config.seed = 123U;
    GameplayScenario scenario{config};

    const auto load = scenario.LoadSceneToml(GameplaySceneText, "repeat.trace2d.toml");
    if (!load.Succeeded())
    {
        throw std::runtime_error{"Deterministic gameplay fixture failed to load."};
    }

    scenario.Reset(123U);
    scenario.SchedulePress(1U, InputControl::KeyD);
    scenario.ScheduleRelease(3U, InputControl::KeyD);
    scenario.RunFrames(2U, MovePlayerWhileRightHeld);
    static_cast<void>(scenario.AssertFloatFieldEquals(
        "#player",
        "Transform2D",
        "position.x",
        99.0F));
    return scenario.Report();
}

TEST(GameplayScenarioTests, LoadsResetsSchedulesRunsAndAssertsAuthoritativeState)
{
    trace2d::runtime::RuntimeConfig config{};
    config.seed = 42U;
    GameplayScenario scenario{config};

    const auto load = scenario.LoadSceneToml(GameplaySceneText, "gameplay.trace2d.toml");
    ASSERT_TRUE(load.Succeeded());

    scenario.Reset(42U);
    scenario.SchedulePress(1U, InputControl::KeyD);
    scenario.ScheduleRelease(4U, InputControl::KeyD);
    scenario.RunFrames(4U, MovePlayerWhileRightHeld);

    EXPECT_TRUE(scenario.AssertFloatFieldEquals(
        "#player",
        "Transform2D",
        "position.x",
        3.0F));

    const GameplayScenarioReport& report = scenario.Report();
    EXPECT_TRUE(report.Succeeded());
    EXPECT_EQ(report.frame, 4U);
    EXPECT_EQ(report.seed, 42U);
    ASSERT_EQ(report.inputEvents.size(), 2U);
    EXPECT_EQ(report.inputEvents[0].frame, 1U);
    EXPECT_EQ(report.inputEvents[1].frame, 4U);
    EXPECT_TRUE(report.inputEvents[0].scheduled);
    EXPECT_TRUE(report.inputEvents[1].scheduled);
    EXPECT_EQ(scenario.Input().CurrentFrame(), scenario.Runtime().State().frame);
}

TEST(GameplayScenarioTests, FailureReportsExpectedObservedFrameSeedSelectorAndSnapshot)
{
    trace2d::runtime::RuntimeConfig config{};
    config.seed = 77U;
    GameplayScenario scenario{config};

    ASSERT_TRUE(scenario.LoadSceneToml(GameplaySceneText, "failure.trace2d.toml").Succeeded());
    scenario.Reset(77U);
    scenario.SchedulePress(1U, InputControl::KeyD);
    scenario.RunFrames(2U, MovePlayerWhileRightHeld);

    EXPECT_FALSE(scenario.AssertFloatFieldEquals(
        "#player",
        "Transform2D",
        "position.x",
        99.0F));

    const GameplayScenarioReport& report = scenario.Report();
    ASSERT_EQ(report.failures.size(), 1U);
    const auto& failure = report.failures.front();

    EXPECT_EQ(failure.code, GameplayAssertionFailureCode::ValueMismatch);
    EXPECT_EQ(trace2d::testing::ToString(failure.code), "value_mismatch");
    EXPECT_EQ(failure.selector, "#player");
    EXPECT_EQ(failure.componentType, "Transform2D");
    EXPECT_EQ(failure.fieldName, "position.x");
    EXPECT_EQ(failure.expected.kind, trace2d::agent::FieldValueKind::Float);
    EXPECT_FLOAT_EQ(failure.expected.floatValue, 99.0F);
    ASSERT_TRUE(failure.observed.has_value());
    EXPECT_EQ(failure.observed->kind, trace2d::agent::FieldValueKind::Float);
    EXPECT_FLOAT_EQ(failure.observed->floatValue, 2.0F);
    EXPECT_EQ(failure.frame, 2U);
    EXPECT_EQ(failure.seed, 77U);

    EXPECT_EQ(failure.snapshot.runtime.frame, 2U);
    EXPECT_EQ(failure.snapshot.runtime.seed, 77U);
    EXPECT_EQ(failure.snapshot.inputFrame, 2U);
    ASSERT_TRUE(failure.snapshot.entity.has_value());
    EXPECT_EQ(failure.snapshot.entity->semanticId, "player");
    EXPECT_FLOAT_EQ(failure.snapshot.entity->transform.position.x, 2.0F);
    ASSERT_EQ(failure.snapshot.relevantInput.size(), 1U);
    EXPECT_EQ(failure.snapshot.relevantInput[0].control, InputControl::KeyD);
    EXPECT_TRUE(failure.snapshot.relevantInput[0].state.held);
}

TEST(GameplayScenarioTests, AssertionReusesQueryOneAmbiguitySemantics)
{
    GameplayScenario scenario{};
    ASSERT_TRUE(scenario.LoadSceneToml(AmbiguousSceneText, "ambiguous.trace2d.toml").Succeeded());

    EXPECT_FALSE(scenario.AssertFloatFieldEquals(
        "tag:enemy",
        "Transform2D",
        "position.x",
        0.0F));

    const GameplayScenarioReport& report = scenario.Report();
    ASSERT_EQ(report.failures.size(), 1U);
    EXPECT_EQ(report.failures[0].code, GameplayAssertionFailureCode::AmbiguousMatch);
    EXPECT_EQ(report.failures[0].selector, "tag:enemy");
    EXPECT_FALSE(report.failures[0].observed.has_value());
    EXPECT_FALSE(report.failures[0].snapshot.entity.has_value());
}

TEST(GameplayScenarioTests, ResetRestoresLoadedSceneInputRuntimeAndReportState)
{
    GameplayScenario scenario{};
    ASSERT_TRUE(scenario.LoadSceneToml(GameplaySceneText, "reset.trace2d.toml").Succeeded());

    scenario.SchedulePress(1U, InputControl::KeyD);
    scenario.RunFrames(2U, MovePlayerWhileRightHeld);
    ASSERT_TRUE(scenario.AssertFloatFieldEquals(
        "#player",
        "Transform2D",
        "position.x",
        2.0F));

    scenario.Reset(55U);

    EXPECT_EQ(scenario.Runtime().State().frame, 0U);
    EXPECT_EQ(scenario.Runtime().State().seed, 55U);
    EXPECT_EQ(scenario.Input().CurrentFrame(), 0U);
    EXPECT_FALSE(scenario.Input().Held(InputControl::KeyD));
    EXPECT_TRUE(scenario.Report().inputEvents.empty());
    EXPECT_TRUE(scenario.Report().failures.empty());
    EXPECT_TRUE(scenario.AssertFloatFieldEquals(
        "#player",
        "Transform2D",
        "position.x",
        0.0F));
}

TEST(GameplayScenarioTests, RepeatedFailingScenarioProducesIdenticalReport)
{
    const GameplayScenarioReport first = RunDeterministicFailure();
    const GameplayScenarioReport second = RunDeterministicFailure();

    ASSERT_FALSE(first.Succeeded());
    ASSERT_FALSE(second.Succeeded());
    EXPECT_EQ(first, second);
    ASSERT_EQ(first.failures.size(), 1U);
    EXPECT_EQ(first.failures[0].frame, 2U);
    ASSERT_TRUE(first.failures[0].observed.has_value());
    EXPECT_FLOAT_EQ(first.failures[0].observed->floatValue, 2.0F);
}
} // namespace
