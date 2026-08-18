#include "TinyPlayableGame.hpp"

#include <trace2d/application/Application.hpp>
#include <trace2d/input/Input.hpp>
#include <trace2d/ui/Ui.hpp>

#include <chrono>
#include <cmath>
#include <exception>
#include <iostream>
#include <string_view>

namespace
{
int Fail(const int code, const std::string_view message)
{
    std::cerr << "Trace2D P0 product-proof failure [" << code << "]: " << message << '\n';
    return code;
}

bool Near(const float left, const float right) noexcept
{
    return std::abs(left - right) < 0.01F;
}

trace2d::application::ApplicationConfig MakeConfig()
{
    trace2d::application::ApplicationConfig config{};
    config.runtime.fixedTimestep = std::chrono::nanoseconds{16'666'667};
    config.runtime.seed = 315U;
    config.scene.semanticId = "p0.tiny-playable";
    config.scene.name = "P0 Tiny Playable";
    config.uiWidth = static_cast<std::uint32_t>(TinyPlayableGame::CanvasWidth);
    config.uiHeight = static_cast<std::uint32_t>(TinyPlayableGame::CanvasHeight);
    return config;
}

const trace2d::ui::UiElement* RequireProgress(
    const trace2d::application::Application& application,
    const std::string_view id)
{
    const trace2d::ui::UiElement* const element = application.Ui().Find(id);
    return element != nullptr && element->progress.Active() ? element : nullptr;
}
} // namespace

int main()
{
    try
    {
        // Scenario 1: deliberately cross the active pulse gate. The canonical UI state must record
        // one hit and the canonical Scene transform must record the reset to the start position.
        {
            TinyPlayableGame game{};
            trace2d::application::Application application{game, MakeConfig()};
            application.ScheduleInput(1U, {
                .control = trace2d::input::InputControl::KeyD,
                .type = trace2d::input::InputEventType::Press,
            });
            application.ScheduleInput(27U, {
                .control = trace2d::input::InputControl::KeyD,
                .type = trace2d::input::InputEventType::Release,
            });
            application.Start();
            application.StepFrames(35U);

            const auto playerId = application.Scene().FindBySemanticId("game.player");
            const auto hazardId = application.Scene().FindBySemanticId("game.hazard");
            const trace2d::ui::UiElement* const health = RequireProgress(application, "hud.health");
            const trace2d::ui::UiElement* const objective = RequireProgress(application, "hud.objective");
            if (!playerId.has_value() || !hazardId.has_value() || health == nullptr || objective == nullptr)
                return Fail(1, "collision scenario did not expose canonical Scene/UI state");

            const trace2d::scene::Entity* const player = application.Scene().TryGet(*playerId);
            const trace2d::scene::Entity* const hazard = application.Scene().TryGet(*hazardId);
            if (player == nullptr || hazard == nullptr) return Fail(2, "collision scenario entity handle was stale");
            if (health->progress.Value() != 2U || health->progress.Maximum() != TinyPlayableGame::MaximumHealth)
                return Fail(3, "active hazard did not decrement engine-owned health exactly once");
            if (!Near(player->LocalTransform().position.x, TinyPlayableGame::PlayerStartX))
                return Fail(4, "active hazard did not reset the canonical player transform");
            if (!Near(hazard->LocalTransform().position.y, TinyPlayableGame::GroundY))
                return Fail(5, "hazard semantic transform did not report the active state");
            if (objective->progress.Value() != 0U) return Fail(6, "collision scenario completed the objective unexpectedly");
            if (!application.Actions().FindAxis1DAction("game.move.horizontal").has_value() ||
                !application.Actions().FindButtonAction("game.claim-beacon").has_value())
                return Fail(7, "semantic gameplay actions were not discoverable after finalization");
            application.Stop();
        }

        // Scenario 2: wait for the safe pulse window, cross to the beacon, and claim it. This proves
        // a complete playable path without screenshot inference: Scene position, health and objective
        // are all asserted from ordinary engine-owned semantic state.
        {
            TinyPlayableGame game{};
            trace2d::application::Application application{game, MakeConfig()};
            application.ScheduleInput(60U, {
                .control = trace2d::input::InputControl::KeyD,
                .type = trace2d::input::InputEventType::Press,
            });
            application.ScheduleInput(120U, {
                .control = trace2d::input::InputControl::KeyD,
                .type = trace2d::input::InputEventType::Release,
            });
            application.ScheduleInput(121U, {
                .control = trace2d::input::InputControl::Space,
                .type = trace2d::input::InputEventType::Press,
            });
            application.ScheduleInput(122U, {
                .control = trace2d::input::InputControl::Space,
                .type = trace2d::input::InputEventType::Release,
            });
            application.Start();
            application.StepFrames(122U);

            const auto playerId = application.Scene().FindBySemanticId("game.player");
            const auto hazardId = application.Scene().FindBySemanticId("game.hazard");
            const auto beaconId = application.Scene().FindBySemanticId("game.beacon");
            const trace2d::ui::UiElement* const health = RequireProgress(application, "hud.health");
            const trace2d::ui::UiElement* const objective = RequireProgress(application, "hud.objective");
            if (!playerId.has_value() || !hazardId.has_value() || !beaconId.has_value() ||
                health == nullptr || objective == nullptr)
                return Fail(8, "success scenario did not expose canonical Scene/UI state");

            const trace2d::scene::Entity* const player = application.Scene().TryGet(*playerId);
            const trace2d::scene::Entity* const hazard = application.Scene().TryGet(*hazardId);
            const trace2d::scene::Entity* const beacon = application.Scene().TryGet(*beaconId);
            if (player == nullptr || hazard == nullptr || beacon == nullptr)
                return Fail(9, "success scenario entity handle was stale");
            if (!Near(player->LocalTransform().position.x, TinyPlayableGame::GoalX) ||
                !Near(beacon->LocalTransform().position.x, TinyPlayableGame::GoalX))
                return Fail(10, "safe-window input did not place the player at the beacon");
            if (health->progress.Value() != TinyPlayableGame::MaximumHealth)
                return Fail(11, "successful path lost health despite crossing only the safe window");
            if (objective->progress.Value() != 1U || objective->progress.Maximum() != 1U)
                return Fail(12, "beacon interaction did not publish the completed objective state");
            if (!Near(hazard->LocalTransform().position.y, TinyPlayableGame::HazardSafeY))
                return Fail(13, "completed objective did not leave the hazard in its safe semantic state");
            application.Stop();
        }

        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Trace2D P0 product-proof exception: " << error.what() << '\n';
        return 20;
    }
}
