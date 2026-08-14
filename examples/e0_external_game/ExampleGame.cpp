#include "ExampleGame.hpp"

#include <trace2d/agent/WorkResult.hpp>
#include <trace2d/agent/WorkSpec.hpp>

#include <array>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
trace2d::scene::SemanticValue IntValue(const std::int64_t value)
{
    trace2d::scene::SemanticValue result{};
    result.kind = trace2d::scene::SemanticValueKind::SignedInteger;
    result.signedIntegerValue = value;
    return result;
}

bool ParseHealth(
    const trace2d::scene::ComponentAuthoringObject& authored,
    Health& health,
    std::string& error)
{
    if (authored.fields.size() != 2U)
    {
        error = "game.health expects exactly current and maximum.";
        return false;
    }
    const auto* current = authored.Find("current");
    const auto* maximum = authored.Find("maximum");
    if (current == nullptr || maximum == nullptr ||
        current->kind != trace2d::scene::SemanticValueKind::SignedInteger ||
        maximum->kind != trace2d::scene::SemanticValueKind::SignedInteger)
    {
        error = "game.health current/maximum must be signed integers.";
        return false;
    }
    health.current = current->signedIntegerValue;
    health.maximum = maximum->signedIntegerValue;
    return true;
}

bool ValidateHealth(const Health& health, std::string& error)
{
    if (health.maximum <= 0 || health.current < 0 || health.current > health.maximum)
    {
        error = "game.health requires 0 <= current <= maximum and maximum > 0.";
        return false;
    }
    return true;
}

trace2d::scene::ComponentAuthoringObject SerializeHealth(const Health& health)
{
    trace2d::scene::ComponentAuthoringObject authored{};
    authored.fields.push_back({"current", IntValue(health.current)});
    authored.fields.push_back({"maximum", IntValue(health.maximum)});
    return authored;
}

std::vector<trace2d::scene::ComponentInspectionField> InspectHealth(const Health& health)
{
    return {
        {"current", IntValue(health.current)},
        {"maximum", IntValue(health.maximum)},
    };
}
} // namespace

ExampleComponentTypes RegisterExampleComponents(trace2d::scene::ComponentRegistry& registry)
{
    ExampleComponentTypes types{};
    types.scene = trace2d::scene::RegisterSceneComponents(registry);

    trace2d::scene::ComponentRegistration<Health> health{};
    health.typeId = "game.health";
    health.schemaVersion = 1;
    health.componentClass = trace2d::scene::ComponentClass::Authored;
    health.parseAuthored = ParseHealth;
    health.validate = ValidateHealth;
    health.serializeAuthored = SerializeHealth;
    health.inspect = InspectHealth;
    types.health = registry.Register(std::move(health));
    return types;
}

trace2d::scene::SceneLoadResult LoadExampleAuthoredScene(
    const trace2d::scene::ComponentRegistry& registry,
    const std::string& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        trace2d::scene::SceneLoadResult result{};
        result.diagnostics.push_back(trace2d::scene::SceneTextDiagnostic{
            .path = "$",
            .message = "Could not open authored scene file '" + path + "'.",
        });
        return result;
    }
    const std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    return trace2d::scene::LoadSceneToml(text, registry, path);
}

trace2d::assets::ResourceHandle<trace2d::assets::SceneTemplateResource> LoadExampleSceneTemplate(
    trace2d::assets::ResourceRegistry& resources,
    const trace2d::assets::ResourceHandle<trace2d::assets::TextureResource> sharedTexture,
    const std::string& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input) throw std::runtime_error{"Could not open scene template file '" + path + "'."};

    trace2d::assets::SceneTemplateResource sceneTemplate{};
    sceneTemplate.canonicalToml.assign(
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{});
    const std::array dependencies{sharedTexture.Untyped()};
    const auto published = resources.PublishSceneTemplate(path, std::move(sceneTemplate), dependencies);
    if (!published.Succeeded())
    {
        std::string message = "Could not publish scene template resource '" + path + "'.";
        if (published.diagnostic.has_value() && !published.diagnostic->message.empty())
        {
            message.append(" ");
            message.append(published.diagnostic->message);
        }
        throw std::runtime_error{message};
    }
    return published.handle;
}

ExampleGame::ExampleGame(
    ExampleComponentTypes types,
    std::optional<trace2d::assets::ResourceHandle<trace2d::assets::SceneTemplateResource>> enemyTemplate) noexcept
    : types_{std::move(types)}
    , enemyTemplate_{enemyTemplate}
{
}

void ExampleGame::OnStart(trace2d::application::GameContext& context)
{
    const std::optional<trace2d::scene::EntityId> player = context.Scene().FindBySemanticId("game.player");
    const std::optional<trace2d::scene::EntityId> weapon = context.Scene().FindBySemanticId("game.weapon");
    if (!player.has_value() || !weapon.has_value())
        throw std::runtime_error{"External E2 authored scene is missing game.player or game.weapon."};
    player_ = *player;

    const trace2d::scene::Entity* weaponEntity = context.Scene().TryGet(*weapon);
    if (weaponEntity == nullptr || weaponEntity->Parent() != player_)
        throw std::runtime_error{"External E2 authored hierarchy is not authoritative."};

    const Health* health = context.Scene().TryGetComponent(player_, types_.health);
    const trace2d::scene::Visibility2D* visibility = context.Scene().TryGetComponent(player_, types_.scene.visibility);
    if (health == nullptr || visibility == nullptr || !visibility->visible)
        throw std::runtime_error{"External E2 typed authored components were not loaded."};

    trace2d::ui::UiElement status{};
    status.id = "game.status";
    status.kind = trace2d::ui::UiElementKind::Label;
    status.bounds = trace2d::ui::UiRect{.x = 8, .y = 8, .width = 240, .height = 24};
    status.name = "Status";
    status.text = "running";
    if (context.Ui().AddElement(std::move(status)) != trace2d::ui::UiActionResult::Success)
        throw std::runtime_error{"External E2 game could not create its status UI."};

    if (const trace2d::agent::WorkSpec* const spec = context.WorkSpec(); spec != nullptr)
    {
        observedWorkId_ = spec->id;
        if (!spec->acceptance.empty()) observedAcceptanceId_ = spec->acceptance.front().id;
    }
    if (trace2d::agent::WorkResult* const result = context.WorkResult(); result != nullptr)
    {
        result->workId = observedWorkId_.empty() ? "e2-external-game" : observedWorkId_;
        trace2d::agent::WorkRevision revision{};
        revision.id = "external-game-e2-start";
        revision.changedPaths = {"examples/e0_external_game", "engine/scene"};
        result->revisions.push_back(std::move(revision));
    }
}

void ExampleGame::OnFixedUpdate(
    trace2d::application::GameContext& context,
    const trace2d::application::FixedUpdate& update)
{
    trace2d::scene::Entity* const player = context.Scene().TryGet(player_);
    Health* const health = context.Scene().TryGetComponent(player_, types_.health);
    if (player == nullptr || health == nullptr)
        throw std::runtime_error{"External E2 game lost generation-safe canonical state."};

    if (context.Input().Held(trace2d::input::InputControl::KeyD))
    {
        player->LocalTransform().position.x += 1.0F;
        if (health->current > 0) --health->current;
    }

    trace2d::scene::WorldLifecycle* const worlds = context.Worlds();
    if (worlds != nullptr && enemyTemplate_.has_value())
    {
        if (update.frame == 1U)
        {
            trace2d::scene::TemplateInstantiationRequest request{};
            request.worldId = "arena";
            request.templateResource = *enemyTemplate_;
            request.instanceId = "runtime-enemy";
            request.rootTransform.position.x = 30.0F;
            static_cast<void>(worlds->QueueInstantiate(std::move(request)));
            spawnWasDeferred_ = !worlds->FindInstanceRoot("arena", "runtime-enemy").has_value();
        }
        else if (update.frame == 2U)
        {
            runtimeEnemyBody_ = worlds->FindInstanceEntity("arena", "runtime-enemy", "body");
            if (!runtimeEnemyBody_.has_value())
                throw std::runtime_error{"W0 safe point did not publish the queued runtime enemy before frame 2."};
            static_cast<void>(worlds->QueueDespawn("arena", "runtime-enemy"));
            despawnWasDeferred_ = worlds->FindInstanceRoot("arena", "runtime-enemy").has_value();
        }
        else if (update.frame == 3U)
        {
            const trace2d::scene::Scene* const arena = worlds->TryGetScene("arena");
            runtimeDespawnInvalidatedHandle_ = runtimeEnemyBody_.has_value() && arena != nullptr &&
                !arena->Contains(*runtimeEnemyBody_) &&
                !worlds->FindInstanceRoot("arena", "runtime-enemy").has_value();
            if (!runtimeDespawnInvalidatedHandle_)
                throw std::runtime_error{"W0 safe-point despawn did not invalidate the captured entity handle before frame 3."};
            static_cast<void>(worlds->QueueUnloadWorld("arena"));
        }
    }

    ++fixedUpdateCount_;
}

void ExampleGame::OnStop(trace2d::application::GameContext& context)
{
    const trace2d::scene::Entity* const player = context.Scene().TryGet(player_);
    const Health* const health = context.Scene().TryGetComponent(player_, types_.health);
    const trace2d::ui::UiElement* const status = context.Ui().Find("game.status");
    if (player == nullptr || health == nullptr || status == nullptr)
        throw std::runtime_error{"External E2 canonical state disappeared before shutdown."};

    trace2d::agent::WorkResult* const result = context.WorkResult();
    if (result == nullptr || result->revisions.empty()) return;

    trace2d::agent::VerificationRecord verification{};
    verification.acceptanceId = observedAcceptanceId_.empty() ? "e2.external-game.composition" : observedAcceptanceId_;
    verification.verification = trace2d::agent::VerificationClass::Deterministic;
    verification.outcome = trace2d::agent::VerificationOutcome::Passed;
    verification.summary = "External authored hierarchy and typed gameplay component remained canonical and Agent-visible.";
    verification.evidence = {"scene:#game.player", "scene:#game.weapon", "scene:type:game.health", "ui:game.status"};
    result->revisions.back().verification.push_back(std::move(verification));
}

trace2d::scene::EntityId ExampleGame::Player() const noexcept { return player_; }
std::uint64_t ExampleGame::FixedUpdateCount() const noexcept { return fixedUpdateCount_; }
const std::string& ExampleGame::ObservedWorkId() const noexcept { return observedWorkId_; }
bool ExampleGame::SpawnWasDeferredToSafePoint() const noexcept { return spawnWasDeferred_; }
bool ExampleGame::DespawnWasDeferredToSafePoint() const noexcept { return despawnWasDeferred_; }
bool ExampleGame::RuntimeDespawnInvalidatedHandle() const noexcept { return runtimeDespawnInvalidatedHandle_; }
