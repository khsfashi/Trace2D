#include <trace2d/application/Application.hpp>

#include <trace2d/agent/Inspection.hpp>
#include <trace2d/agent/WorkResult.hpp>
#include <trace2d/agent/WorkSpec.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
using namespace std::chrono_literals;

static_assert(std::is_same_v<
    decltype(std::declval<trace2d::application::Application&>().Input()),
    const trace2d::input::InputSystem&>);

class RecordingGame final : public trace2d::application::Game
{
public:
    void OnStart(trace2d::application::GameContext& context) override
    {
        trace2d::scene::EntityDescriptor player{};
        player.semanticId = "game.player";
        player.name = "Player";
        player_ = context.Scene().CreateEntity(std::move(player));

        if (const trace2d::agent::WorkSpec* const spec = context.WorkSpec(); spec != nullptr)
        {
            observedWorkId = spec->id;
        }
        if (trace2d::agent::WorkResult* const result = context.WorkResult(); result != nullptr)
        {
            result->workId = observedWorkId;
        }

        ++startCount;
    }

    void OnFixedUpdate(
        trace2d::application::GameContext& context,
        const trace2d::application::FixedUpdate& update) override
    {
        frames.push_back(update.frame);
        deltas.push_back(update.fixedDelta);

        trace2d::scene::Entity* const player = context.Scene().TryGet(player_);
        if (player == nullptr)
        {
            throw std::logic_error{"Test player is missing."};
        }
        if (context.Input().Held(trace2d::input::InputControl::KeyD))
        {
            player->Transform().position.x += 1.0F;
        }
    }

    void OnStop(trace2d::application::GameContext&) override
    {
        ++stopCount;
    }

    trace2d::scene::EntityId player_{};
    std::vector<std::uint64_t> frames{};
    std::vector<std::chrono::nanoseconds> deltas{};
    std::string observedWorkId{};
    std::uint64_t startCount{0};
    std::uint64_t stopCount{0};
};

trace2d::application::ApplicationConfig MakeConfig()
{
    trace2d::application::ApplicationConfig config{};
    config.runtime.fixedTimestep = 10ms;
    config.runtime.seed = 69;
    config.scene.semanticId = "test.scene";
    config.scene.name = "Test Scene";
    config.uiWidth = 320;
    config.uiHeight = 180;
    return config;
}

struct PresentationProbe final
{
    std::uint64_t calls{0};
    std::uint64_t frame{0};
    std::size_t entityCount{0};
};

void RecordPresentation(
    const trace2d::application::GameContext& context,
    void* const userData)
{
    auto* const probe = static_cast<PresentationProbe*>(userData);
    ++probe->calls;
    probe->frame = context.Runtime().State().frame;
    probe->entityCount = context.Scene().EntityCount();
}

TEST(ApplicationTests, OwnsLifecycleAndOneGameCallbackPerFixedStep)
{
    RecordingGame game{};
    trace2d::application::Application application{game, MakeConfig()};

    EXPECT_EQ(application.Lifecycle(), trace2d::application::ApplicationLifecycle::Created);
    application.Start();
    EXPECT_EQ(game.startCount, 1U);

    application.ScheduleInput(
        1,
        trace2d::input::InputEvent{
            .control = trace2d::input::InputControl::KeyD,
            .type = trace2d::input::InputEventType::Press,
        });
    application.ScheduleInput(
        3,
        trace2d::input::InputEvent{
            .control = trace2d::input::InputControl::KeyD,
            .type = trace2d::input::InputEventType::Release,
        });

    application.StepFrames(3);

    EXPECT_EQ(game.frames, (std::vector<std::uint64_t>{1U, 2U, 3U}));
    EXPECT_EQ(game.deltas, (std::vector<std::chrono::nanoseconds>{10ms, 10ms, 10ms}));
    EXPECT_EQ(application.Runtime().State().frame, 3U);
    EXPECT_EQ(application.Input().CurrentFrame(), 3U);

    const trace2d::scene::Entity* const player = application.Scene().TryGet(game.player_);
    ASSERT_NE(player, nullptr);
    EXPECT_FLOAT_EQ(player->Transform().position.x, 2.0F);

    application.Stop();
    EXPECT_EQ(game.stopCount, 1U);
    EXPECT_EQ(application.Lifecycle(), trace2d::application::ApplicationLifecycle::Stopped);
    application.Stop();
    EXPECT_EQ(game.stopCount, 1U);
}

TEST(ApplicationTests, ElapsedAndExplicitSteppingShareAuthoritativeGameLogic)
{
    RecordingGame explicitGame{};
    RecordingGame elapsedGame{};
    trace2d::application::Application explicitApplication{explicitGame, MakeConfig()};
    trace2d::application::Application elapsedApplication{elapsedGame, MakeConfig()};

    explicitApplication.Start();
    elapsedApplication.Start();

    explicitApplication.StepFrames(3);
    EXPECT_EQ(elapsedApplication.AdvanceElapsed(35ms), 3U);

    EXPECT_EQ(explicitGame.frames, elapsedGame.frames);
    EXPECT_EQ(explicitApplication.Runtime().State().frame, elapsedApplication.Runtime().State().frame);
    EXPECT_EQ(elapsedApplication.Runtime().State().accumulatedWallTime, 5ms);

    explicitApplication.StepFrames();
    EXPECT_EQ(elapsedApplication.AdvanceElapsed(5ms), 1U);
    EXPECT_EQ(explicitGame.frames, elapsedGame.frames);
    EXPECT_EQ(explicitApplication.Runtime().State().simulationTime,
              elapsedApplication.Runtime().State().simulationTime);
}

TEST(ApplicationTests, WorkContractsAndAgentInspectionUseCanonicalApplicationState)
{
    trace2d::agent::WorkSpec spec{};
    spec.id = "work.e0";
    trace2d::agent::WorkResult result{};

    RecordingGame game{};
    trace2d::application::Application application{game, MakeConfig()};
    application.BindWorkContracts(&spec, &result);
    application.Start();
    application.StepFrames();

    const trace2d::application::ApplicationSnapshot snapshot = application.Snapshot();
    EXPECT_TRUE(snapshot.workSpecBound);
    EXPECT_TRUE(snapshot.workResultBound);
    EXPECT_EQ(snapshot.sceneSemanticId, "test.scene");
    EXPECT_EQ(snapshot.entityCount, 1U);
    EXPECT_EQ(game.observedWorkId, spec.id);
    EXPECT_EQ(result.workId, spec.id);

    trace2d::agent::AgentFacade facade{
        &application.Runtime(),
        &application.Scene(),
        &application.Ui()};
    const trace2d::agent::QueryOneResult query = facade.QueryOne("#game.player");
    ASSERT_TRUE(query.Succeeded());
    ASSERT_TRUE(query.match.has_value());
    EXPECT_EQ(query.match->semanticId, "game.player");
    EXPECT_EQ(query.match->transform.position.x, 0.0F);
}

TEST(ApplicationTests, PresentationIsExplicitAndReadsTheSameCanonicalContext)
{
    RecordingGame game{};
    trace2d::application::Application application{game, MakeConfig()};
    application.Start();
    application.StepFrames(2);

    EXPECT_FALSE(application.Present());

    PresentationProbe probe{};
    application.SetPresentationCallback(&RecordPresentation, &probe);
    EXPECT_TRUE(application.Present());
    EXPECT_EQ(probe.calls, 1U);
    EXPECT_EQ(probe.frame, 2U);
    EXPECT_EQ(probe.entityCount, 1U);
    EXPECT_TRUE(application.Snapshot().presentationBound);
}

TEST(ApplicationTests, RejectsSteppingOutsideRunningLifecycle)
{
    RecordingGame game{};
    trace2d::application::Application application{game, MakeConfig()};

    EXPECT_THROW(application.StepFrames(), std::logic_error);
    EXPECT_THROW(static_cast<void>(application.AdvanceElapsed(10ms)), std::logic_error);
    EXPECT_THROW(static_cast<void>(application.Present()), std::logic_error);

    application.Stop();
    EXPECT_EQ(application.Lifecycle(), trace2d::application::ApplicationLifecycle::Stopped);
    EXPECT_THROW(application.Start(), std::logic_error);
}
} // namespace
