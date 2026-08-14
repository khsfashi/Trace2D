#include "ExampleGame.hpp"

#include <trace2d/agent/Inspection.hpp>
#include <trace2d/agent/WorkResult.hpp>
#include <trace2d/agent/WorkSpec.hpp>

#include <chrono>
#include <exception>

int main()
{
    using namespace std::chrono_literals;

    try
    {
        trace2d::agent::WorkSpec spec{};
        spec.id = "e0-external-game";
        spec.intent = "Prove the external Game/Application boundary.";
        spec.acceptance.push_back(trace2d::agent::AcceptanceCriterion{
            .id = "e0.external-game.lifecycle",
            .description = "External game reaches deterministic canonical state and shuts down cleanly.",
            .verification = trace2d::agent::VerificationClass::Deterministic,
        });

        trace2d::agent::WorkResult result{};

        trace2d::application::ApplicationConfig config{};
        config.runtime.fixedTimestep = 10ms;
        config.runtime.seed = 69;
        config.scene.semanticId = "e0.scene";
        config.scene.name = "E0 Scene";

        ExampleGame game{};
        trace2d::application::Application application{game, config};
        application.BindWorkContracts(&spec, &result);
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

        application.Start();
        application.StepFrames(3);

        trace2d::agent::AgentFacade agent{
            &application.Runtime(),
            &application.Scene(),
            &application.Ui()};
        const trace2d::agent::QueryOneResult player = agent.QueryOne("#game.player");
        if (!player.Succeeded() || player.match->transform.position.x != 2.0F)
        {
            return 2;
        }

        const trace2d::application::ApplicationSnapshot snapshot = application.Snapshot();
        if (snapshot.frame != 3U || snapshot.entityCount != 1U || snapshot.uiElementCount != 1U)
        {
            return 3;
        }
        if (!snapshot.workSpecBound || !snapshot.workResultBound || snapshot.presentationBound)
        {
            return 4;
        }
        if (game.ObservedWorkId() != spec.id || result.workId != spec.id || result.revisions.size() != 1U)
        {
            return 5;
        }

        application.Stop();
        if (game.FixedUpdateCount() != 3U)
        {
            return 6;
        }
        if (result.revisions.front().verification.size() != 1U)
        {
            return 7;
        }
        const trace2d::agent::VerificationRecord& verification = result.revisions.front().verification.front();
        if (verification.acceptanceId != spec.acceptance.front().id
            || verification.outcome != trace2d::agent::VerificationOutcome::Passed)
        {
            return 8;
        }

        return 0;
    }
    catch (const std::exception&)
    {
        return 10;
    }
}
