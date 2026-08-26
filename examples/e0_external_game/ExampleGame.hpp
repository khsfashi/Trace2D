#pragma once

#include <trace2d/Trace2D.hpp>
#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/scene/SceneText.hpp>

#include <cstdint>
#include <optional>
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
[[nodiscard]] trace2d::assets::ResourceHandle<trace2d::assets::SceneTemplateResource> LoadExampleSceneTemplate(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::assets::ResourceHandle<trace2d::assets::TextureResource> sharedTexture,
    const std::string& path);

class ExampleGame final : public trace2d::application::Game
{
public:
    explicit ExampleGame(
        ExampleComponentTypes types,
        std::optional<trace2d::assets::ResourceHandle<trace2d::assets::SceneTemplateResource>> enemyTemplate = std::nullopt) noexcept;

    void OnStart(trace2d::application::GameContext& context) override;
    void OnFixedUpdate(
        trace2d::application::GameContext& context,
        const trace2d::application::FixedUpdate& update) override;
    void OnStop(trace2d::application::GameContext& context) override;

    [[nodiscard]] trace2d::scene::EntityId Player() const noexcept;
    [[nodiscard]] std::uint64_t FixedUpdateCount() const noexcept;
    [[nodiscard]] const std::string& ObservedWorkId() const noexcept;
    [[nodiscard]] bool SpawnWasDeferredToSafePoint() const noexcept;
    [[nodiscard]] bool DespawnWasDeferredToSafePoint() const noexcept;
    [[nodiscard]] bool RuntimeDespawnInvalidatedHandle() const noexcept;

private:
    ExampleComponentTypes types_{};
    std::optional<trace2d::assets::ResourceHandle<trace2d::assets::SceneTemplateResource>> enemyTemplate_{};
    trace2d::scene::EntityId player_{};
    std::optional<trace2d::scene::EntityId> runtimeEnemyBody_{};
    std::uint64_t fixedUpdateCount_{0};
    std::string observedWorkId_{};
    std::string observedAcceptanceId_{};
    bool spawnWasDeferred_{false};
    bool despawnWasDeferred_{false};
    bool runtimeDespawnInvalidatedHandle_{false};
};