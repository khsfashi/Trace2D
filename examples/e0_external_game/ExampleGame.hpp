#pragma once

#include <trace2d/application/Application.hpp>
#include <trace2d/scene/SceneText.hpp>

#include <cstdint>
#include <string>

struct Health final
{
    std::int64_t current{100};
    std::int64_t maximum{100};
};

struct ExampleComponentTypes final
{
    trace2d::scene::SceneComponentTypes scene{};
    trace2d::scene::ComponentTypeHandle<Health> health{};
};

[[nodiscard]] ExampleComponentTypes RegisterExampleComponents(trace2d::scene::ComponentRegistry& registry);
[[nodiscard]] trace2d::scene::SceneLoadResult LoadExampleAuthoredScene(
    const trace2d::scene::ComponentRegistry& registry,
    const std::string& path);

class ExampleGame final : public trace2d::application::Game
{
public:
    explicit ExampleGame(ExampleComponentTypes types) noexcept;

    void OnStart(trace2d::application::GameContext& context) override;
    void OnFixedUpdate(
        trace2d::application::GameContext& context,
        const trace2d::application::FixedUpdate& update) override;
    void OnStop(trace2d::application::GameContext& context) override;

    [[nodiscard]] trace2d::scene::EntityId Player() const noexcept;
    [[nodiscard]] std::uint64_t FixedUpdateCount() const noexcept;
    [[nodiscard]] const std::string& ObservedWorkId() const noexcept;

private:
    ExampleComponentTypes types_{};
    trace2d::scene::EntityId player_{};
    std::uint64_t fixedUpdateCount_{0};
    std::string observedWorkId_{};
    std::string observedAcceptanceId_{};
};
