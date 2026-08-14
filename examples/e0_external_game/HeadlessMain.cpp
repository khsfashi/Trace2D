#include "ExampleGame.hpp"

#include <trace2d/agent/Inspection.hpp>
#include <trace2d/agent/WorkResult.hpp>
#include <trace2d/agent/WorkSpec.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <string_view>
#include <utility>

namespace
{
const trace2d::agent::ComponentSnapshot* FindComponent(
    const trace2d::agent::EntitySnapshot& entity,
    const std::string_view type)
{
    const auto iterator = std::find_if(entity.components.begin(), entity.components.end(), [type](const auto& component)
    {
        return std::string_view{component.type} == type;
    });
    return iterator == entity.components.end() ? nullptr : &*iterator;
}

const trace2d::agent::ComponentFieldSnapshot* FindField(
    const trace2d::agent::ComponentSnapshot& component,
    const std::string_view name)
{
    const auto iterator = std::find_if(component.fields.begin(), component.fields.end(), [name](const auto& field)
    {
        return std::string_view{field.name} == name;
    });
    return iterator == component.fields.end() ? nullptr : &*iterator;
}
} // namespace

int main()
{
    using namespace std::chrono_literals;
    try
    {
        trace2d::scene::ComponentRegistry registry{};
        const ExampleComponentTypes componentTypes = RegisterExampleComponents(registry);
        registry.Freeze();

        trace2d::scene::SceneLoadResult firstLoad = LoadExampleAuthoredScene(registry, "content/scenes/main.trace2d.toml");
        if (!firstLoad.Succeeded()) return 1;
        const trace2d::scene::SceneSaveResult firstSave = trace2d::scene::SaveSceneToml(*firstLoad.scene);
        if (!firstSave.Succeeded()) return 2;
        trace2d::scene::SceneLoadResult secondLoad = trace2d::scene::LoadSceneToml(firstSave.text, registry, "canonical-e2.trace2d.toml");
        if (!secondLoad.Succeeded()) return 3;
        const trace2d::scene::SceneSaveResult secondSave = trace2d::scene::SaveSceneToml(*secondLoad.scene);
        if (!secondSave.Succeeded() || firstSave.text != secondSave.text) return 4;

        trace2d::agent::WorkSpec spec{};
        spec.id = "e2-external-game";
        spec.intent = "Prove deterministic authored hierarchy and external typed component composition.";
        spec.acceptance.push_back(trace2d::agent::AcceptanceCriterion{
            .id = "e2.external-game.composition",
            .description = "External authored hierarchy, engine components, and game.health remain typed, deterministic, and Agent-visible.",
            .verification = trace2d::agent::VerificationClass::Deterministic,
        });
        trace2d::agent::WorkResult result{};

        trace2d::application::ApplicationConfig config{};
        config.runtime.fixedTimestep = 10ms;
        config.runtime.seed = 71;
        config.scene.semanticId = "e2.placeholder";
        config.scene.name = "E2 Placeholder";

        ExampleGame game{componentTypes};
        trace2d::application::Application application{game, config};
        application.Scene() = std::move(*secondLoad.scene);
        application.BindWorkContracts(&spec, &result);
        application.ScheduleInput(1, {.control = trace2d::input::InputControl::KeyD, .type = trace2d::input::InputEventType::Press});
        application.ScheduleInput(3, {.control = trace2d::input::InputControl::KeyD, .type = trace2d::input::InputEventType::Release});
        application.Start();
        application.StepFrames(3);

        trace2d::agent::AgentFacade agent{&application.Runtime(), &application.Scene(), &application.Ui()};
        const auto player = agent.QueryOne("type:game.health");
        if (!player.Succeeded() || player.match->semanticId != "game.player") return 5;
        const auto* health = FindComponent(*player.match, "game.health");
        if (health == nullptr) return 6;
        const auto* current = FindField(*health, "current");
        if (current == nullptr || current->value.kind != trace2d::agent::FieldValueKind::SignedInteger || current->value.signedIntegerValue != 98) return 7;

        const auto weapon = agent.QueryOne("#game.weapon");
        if (!weapon.Succeeded() || !weapon.match->parentSemanticId.has_value() || *weapon.match->parentSemanticId != "game.player") return 8;
        if (weapon.match->worldTransform.position.x != 3.0F) return 9;

        const auto snapshot = application.Snapshot();
        if (snapshot.frame != 3U || snapshot.entityCount != 2U || snapshot.uiElementCount != 1U) return 10;
        if (!snapshot.workSpecBound || !snapshot.workResultBound || snapshot.presentationBound) return 11;
        if (game.ObservedWorkId() != spec.id || result.workId != spec.id || result.revisions.size() != 1U) return 12;

        application.Stop();
        if (game.FixedUpdateCount() != 3U) return 13;
        if (result.revisions.front().verification.size() != 1U) return 14;
        const auto& verification = result.revisions.front().verification.front();
        if (verification.acceptanceId != spec.acceptance.front().id || verification.outcome != trace2d::agent::VerificationOutcome::Passed) return 15;
        return 0;
    }
    catch (const std::exception&)
    {
        return 20;
    }
}
