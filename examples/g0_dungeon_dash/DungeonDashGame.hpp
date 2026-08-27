#pragma once

#include <trace2d/application/Application.hpp>
#include <trace2d/input/ActionMap.hpp>
#include <trace2d/scene/Scene.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

class DungeonDashGame final : public trace2d::application::Game
{
public:
    enum class State : std::uint8_t
    {
        Running = 0,
        Won,
        Lost,
    };

    static constexpr float CanvasWidth = 768.0F;
    static constexpr float CanvasHeight = 432.0F;
    static constexpr float WorldHalfWidth = 8.0F;
    static constexpr float WorldHalfHeight = 4.5F;
    static constexpr float PlayerSpeed = 3.8F;
    static constexpr float HunterBaseSpeed = 1.25F;
    static constexpr float HunterSpeedPerRelic = 0.16F;
    static constexpr float PlayerRadius = 0.38F;
    static constexpr float HunterRadius = 0.42F;
    static constexpr float RelicRadius = 0.34F;
    static constexpr std::size_t RelicCount = 5U;

    void OnStart(trace2d::application::GameContext& context) override;
    void OnFixedUpdate(
        trace2d::application::GameContext& context,
        const trace2d::application::FixedUpdate& update) override;

    [[nodiscard]] State CurrentState() const noexcept;
    [[nodiscard]] std::size_t CollectedRelicCount() const noexcept;
    [[nodiscard]] bool RelicCollected(std::size_t index) const noexcept;
    [[nodiscard]] trace2d::scene::EntityId Player() const noexcept;
    [[nodiscard]] trace2d::scene::EntityId Hunter() const noexcept;
    [[nodiscard]] trace2d::scene::EntityId Relic(std::size_t index) const noexcept;

private:
    void ResetRound(trace2d::application::GameContext& context);

    trace2d::scene::EntityId player_{};
    trace2d::scene::EntityId hunter_{};
    std::array<trace2d::scene::EntityId, RelicCount> relics_{};
    std::array<bool, RelicCount> collected_{};

    trace2d::input::Axis1DActionId horizontalAction_{};
    trace2d::input::Axis1DActionId verticalAction_{};
    trace2d::input::ButtonActionId restartAction_{};

    State state_{State::Running};
    std::size_t collectedCount_{0U};
};
