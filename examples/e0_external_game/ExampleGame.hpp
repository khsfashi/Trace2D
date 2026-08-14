#pragma once

#include <trace2d/application/Application.hpp>

#include <cstdint>
#include <string>

class ExampleGame final : public trace2d::application::Game
{
public:
    void OnStart(trace2d::application::GameContext& context) override;
    void OnFixedUpdate(
        trace2d::application::GameContext& context,
        const trace2d::application::FixedUpdate& update) override;
    void OnStop(trace2d::application::GameContext& context) override;

    [[nodiscard]] trace2d::scene::EntityId Player() const noexcept;
    [[nodiscard]] std::uint64_t FixedUpdateCount() const noexcept;
    [[nodiscard]] const std::string& ObservedWorkId() const noexcept;

private:
    trace2d::scene::EntityId player_{};
    std::uint64_t fixedUpdateCount_{0};
    std::string observedWorkId_{};
    std::string observedAcceptanceId_{};
};
