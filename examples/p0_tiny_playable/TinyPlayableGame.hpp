#pragma once

#include <trace2d/application/Application.hpp>
#include <trace2d/input/ActionMap.hpp>
#include <trace2d/scene/Scene.hpp>

#include <cstdint>

class TinyPlayableGame final : public trace2d::application::Game
{
public:
    static constexpr float CanvasWidth = 640.0F;
    static constexpr float CanvasHeight = 360.0F;
    static constexpr float GroundY = 180.0F;
    static constexpr float PlayerStartX = 80.0F;
    static constexpr float GoalX = 560.0F;
    static constexpr float HazardX = 320.0F;
    static constexpr float HazardSafeY = 72.0F;
    static constexpr float MovePerFixedFrame = 8.0F;
    static constexpr float PlayerMinX = 48.0F;
    static constexpr float PlayerMaxX = 592.0F;
    static constexpr float HazardCollisionHalfWidth = 34.0F;
    static constexpr float HazardCollisionHalfHeight = 54.0F;
    static constexpr float GoalInteractionDistance = 32.0F;
    static constexpr std::uint32_t MaximumHealth = 3U;

    void OnStart(trace2d::application::GameContext& context) override;
    void OnFixedUpdate(
        trace2d::application::GameContext& context,
        const trace2d::application::FixedUpdate& update) override;

private:
    trace2d::scene::EntityId player_{};
    trace2d::scene::EntityId hazard_{};
    trace2d::scene::EntityId beacon_{};
    trace2d::input::Axis1DActionId moveAction_{};
    trace2d::input::ButtonActionId interactAction_{};
};
