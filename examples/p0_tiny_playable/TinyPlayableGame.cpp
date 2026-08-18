#include "TinyPlayableGame.hpp"

#include <trace2d/input/Input.hpp>
#include <trace2d/ui/Ui.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
trace2d::scene::EntityId RequireEntity(
    trace2d::scene::Scene& scene,
    const std::string& semanticId,
    const std::string& name,
    const float x,
    const float y)
{
    trace2d::scene::EntityDescriptor descriptor{};
    descriptor.semanticId = semanticId;
    descriptor.name = name;
    descriptor.transform.position = {x, y};
    return scene.CreateEntity(std::move(descriptor));
}

void RequireUiSuccess(const trace2d::ui::UiActionResult result, const char* const message)
{
    if (result != trace2d::ui::UiActionResult::Success) throw std::runtime_error{message};
}

void RequireProgressSuccess(const trace2d::ui::UiProgressResult result, const char* const message)
{
    if (result != trace2d::ui::UiProgressResult::Success) throw std::runtime_error{message};
}
} // namespace

void TinyPlayableGame::OnStart(trace2d::application::GameContext& context)
{
    player_ = RequireEntity(context.Scene(), "game.player", "Courier", PlayerStartX, GroundY);
    hazard_ = RequireEntity(context.Scene(), "game.hazard", "Pulse Gate", HazardX, GroundY);
    beacon_ = RequireEntity(context.Scene(), "game.beacon", "Beacon", GoalX, GroundY);

    moveAction_ = context.Actions().AddAxis1DAction(
        "game.move.horizontal",
        trace2d::input::InputControl::KeyA,
        trace2d::input::InputControl::KeyD);
    interactAction_ = context.Actions().AddButtonAction("game.claim-beacon");
    context.Actions().BindButton(interactAction_, trace2d::input::InputControl::Space);
    context.Actions().BindButton(interactAction_, trace2d::input::InputControl::Enter);

    context.Ui().ReserveElements(2U);

    trace2d::ui::UiElement health{};
    health.id = "hud.health";
    health.kind = trace2d::ui::UiElementKind::Panel;
    health.bounds = {.x = 20U, .y = 16U, .width = 180U, .height = 16U};
    health.name = "Health";
    RequireUiSuccess(context.Ui().AddElement(std::move(health)), "P0 could not create hud.health.");
    RequireProgressSuccess(
        context.Ui().ConfigureProgress("hud.health", MaximumHealth, MaximumHealth),
        "P0 could not configure hud.health progress.");

    trace2d::ui::UiElement objective{};
    objective.id = "hud.objective";
    objective.kind = trace2d::ui::UiElementKind::Panel;
    objective.bounds = {.x = 440U, .y = 16U, .width = 180U, .height = 16U};
    objective.name = "Beacon claimed";
    RequireUiSuccess(context.Ui().AddElement(std::move(objective)), "P0 could not create hud.objective.");
    RequireProgressSuccess(
        context.Ui().ConfigureProgress("hud.objective", 0U, 1U),
        "P0 could not configure hud.objective progress.");
}

void TinyPlayableGame::OnFixedUpdate(
    trace2d::application::GameContext& context,
    const trace2d::application::FixedUpdate& update)
{
    trace2d::scene::Entity* const player = context.Scene().TryGet(player_);
    trace2d::scene::Entity* const hazard = context.Scene().TryGet(hazard_);
    const trace2d::scene::Entity* const beacon = context.Scene().TryGet(beacon_);
    const trace2d::ui::UiElement* const objective = context.Ui().Find("hud.objective");
    const trace2d::ui::UiElement* const health = context.Ui().Find("hud.health");
    if (player == nullptr || hazard == nullptr || beacon == nullptr || objective == nullptr || health == nullptr)
        throw std::runtime_error{"P0 canonical scene/UI state is unavailable."};

    const bool objectiveComplete = objective->progress.Active() && objective->progress.Value() == 1U;
    const bool hazardActive = !objectiveComplete && (update.frame % 120U) < 60U;
    hazard->LocalTransform().position.y = hazardActive ? GroundY : HazardSafeY;

    if (objectiveComplete) return;

    const float move = context.Actions().Axis1D(moveAction_);
    player->LocalTransform().position.x = std::clamp(
        player->LocalTransform().position.x + move * MovePerFixedFrame,
        PlayerMinX,
        PlayerMaxX);

    const float hazardDx = std::abs(player->LocalTransform().position.x - hazard->LocalTransform().position.x);
    const float hazardDy = std::abs(player->LocalTransform().position.y - hazard->LocalTransform().position.y);
    if (hazardDx <= HazardCollisionHalfWidth && hazardDy <= HazardCollisionHalfHeight)
    {
        const std::uint32_t nextHealth = health->progress.Value() == 0U ? 0U : health->progress.Value() - 1U;
        RequireProgressSuccess(
            context.Ui().SetProgress("hud.health", nextHealth, MaximumHealth),
            "P0 could not update hud.health after a hazard hit.");
        player->LocalTransform().position.x = PlayerStartX;
        return;
    }

    if (context.Actions().Pressed(interactAction_) &&
        std::abs(player->LocalTransform().position.x - beacon->LocalTransform().position.x) <= GoalInteractionDistance)
    {
        RequireProgressSuccess(
            context.Ui().SetProgress("hud.objective", 1U, 1U),
            "P0 could not publish the completed objective state.");
        hazard->LocalTransform().position.y = HazardSafeY;
    }
}
