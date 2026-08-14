#include "ExampleGame.hpp"

#include <trace2d/agent/WorkResult.hpp>
#include <trace2d/agent/WorkSpec.hpp>

#include <stdexcept>
#include <utility>

void ExampleGame::OnStart(trace2d::application::GameContext& context)
{
    trace2d::scene::EntityDescriptor player{};
    player.semanticId = "game.player";
    player.name = "Player";
    player.tags = {"player"};
    player_ = context.Scene().CreateEntity(std::move(player));

    trace2d::ui::UiElement status{};
    status.id = "game.status";
    status.kind = trace2d::ui::UiElementKind::Label;
    status.bounds = trace2d::ui::UiRect{.x = 8, .y = 8, .width = 240, .height = 24};
    status.name = "Status";
    status.text = "running";
    if (context.Ui().AddElement(std::move(status)) != trace2d::ui::UiActionResult::Success)
    {
        throw std::runtime_error{"External E0 game could not create its status UI."};
    }

    if (const trace2d::agent::WorkSpec* const spec = context.WorkSpec(); spec != nullptr)
    {
        observedWorkId_ = spec->id;
        if (!spec->acceptance.empty())
        {
            observedAcceptanceId_ = spec->acceptance.front().id;
        }
    }

    if (trace2d::agent::WorkResult* const result = context.WorkResult(); result != nullptr)
    {
        result->workId = observedWorkId_.empty() ? "e0-external-game" : observedWorkId_;

        trace2d::agent::WorkRevision revision{};
        revision.id = "external-game-start";
        revision.changedPaths = {"examples/e0_external_game"};
        result->revisions.push_back(std::move(revision));
    }
}

void ExampleGame::OnFixedUpdate(
    trace2d::application::GameContext& context,
    const trace2d::application::FixedUpdate&)
{
    trace2d::scene::Entity* const player = context.Scene().TryGet(player_);
    if (player == nullptr)
    {
        throw std::runtime_error{"External E0 game lost its player entity."};
    }

    if (context.Input().Held(trace2d::input::InputControl::KeyD))
    {
        player->Transform().position.x += 1.0F;
    }

    ++fixedUpdateCount_;
}

void ExampleGame::OnStop(trace2d::application::GameContext& context)
{
    const trace2d::scene::Entity* const player = context.Scene().TryGet(player_);
    const trace2d::ui::UiElement* const status = context.Ui().Find("game.status");
    if (player == nullptr || status == nullptr)
    {
        throw std::runtime_error{"External E0 game canonical state disappeared before shutdown."};
    }

    trace2d::agent::WorkResult* const result = context.WorkResult();
    if (result == nullptr || result->revisions.empty())
    {
        return;
    }

    trace2d::agent::VerificationRecord verification{};
    verification.acceptanceId = observedAcceptanceId_.empty()
        ? "e0.external-game.lifecycle"
        : observedAcceptanceId_;
    verification.verification = trace2d::agent::VerificationClass::Deterministic;
    verification.outcome = trace2d::agent::VerificationOutcome::Passed;
    verification.summary = "External game completed with canonical player and UI state intact.";
    verification.evidence = {"scene:#game.player", "ui:game.status"};
    result->revisions.back().verification.push_back(std::move(verification));
}

trace2d::scene::EntityId ExampleGame::Player() const noexcept
{
    return player_;
}

std::uint64_t ExampleGame::FixedUpdateCount() const noexcept
{
    return fixedUpdateCount_;
}

const std::string& ExampleGame::ObservedWorkId() const noexcept
{
    return observedWorkId_;
}
