#include "ExampleGame.hpp"

#include <trace2d/agent/Inspection.hpp>
#include <trace2d/agent/WorkResult.hpp>
#include <trace2d/agent/WorkSpec.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <string>
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

int Fail(const int code, const std::string_view message)
{
    std::cerr << "Trace2D E2 external gate failure [" << code << "]: " << message << '\n';
    return code;
}

int FailQuery(const int code, const std::string_view stage, const trace2d::agent::QueryOneResult& query)
{
    std::cerr << "Trace2D E2 external gate query failure [" << code << "] at " << stage;
    if (query.error.has_value())
    {
        std::cerr << ": " << trace2d::agent::ToString(query.error->code);
        if (!query.error->message.empty()) std::cerr << " - " << query.error->message;
    }
    else
    {
        std::cerr << ": query returned no structured error";
    }
    std::cerr << '\n';
    return code;
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
        if (!firstLoad.Succeeded()) return Fail(1, "initial authored scene load failed");
        const trace2d::scene::SceneSaveResult firstSave = trace2d::scene::SaveSceneToml(*firstLoad.scene);
        if (!firstSave.Succeeded()) return Fail(2, "first canonical scene save failed");
        trace2d::scene::SceneLoadResult secondLoad = trace2d::scene::LoadSceneToml(firstSave.text, registry, "canonical-e2.trace2d.toml");
        if (!secondLoad.Succeeded()) return Fail(3, "canonical authored scene reload failed");
        const trace2d::scene::SceneSaveResult secondSave = trace2d::scene::SaveSceneToml(*secondLoad.scene);
        if (!secondSave.Succeeded() || firstSave.text != secondSave.text) return Fail(4, "canonical save-load-save text was not stable");

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

        if (!application.Scene().FindBySemanticId("game.weapon").has_value())
            return Fail(18, "Scene authority lost game.weapon after fixed-step execution");

        trace2d::agent::AgentFacade agent{&application.Runtime(), &application.Scene(), &application.Ui()};
        const auto weaponBeforeGameplayQuery = agent.QueryOne("#game.weapon");
        if (!weaponBeforeGameplayQuery.Succeeded())
            return FailQuery(19, "semantic-id query before game.health query", weaponBeforeGameplayQuery);

        const auto player = agent.QueryOne("type:game.health");
        if (!player.Succeeded() || player.match->semanticId != "game.player") return FailQuery(5, "game.health type query", player);
        const auto* health = FindComponent(*player.match, "game.health");
        if (health == nullptr) return Fail(6, "game.health component was absent from Agent snapshot");
        const auto* current = FindField(*health, "current");
        if (current == nullptr || current->value.kind != trace2d::agent::FieldValueKind::SignedInteger || current->value.signedIntegerValue != 98)
            return Fail(7, "game.health.current did not reflect fixed-step canonical state");

        if (!application.Scene().FindBySemanticId("game.weapon").has_value())
            return Fail(21, "game.health Agent query mutated Scene semantic identity");

        const auto weapon = agent.QueryOne("#game.weapon");
        if (!weapon.Succeeded()) return FailQuery(8, "semantic-id query after game.health query", weapon);
        const auto* hierarchy = FindComponent(*weapon.match, "Hierarchy2D");
        if (hierarchy == nullptr) return Fail(9, "Hierarchy2D component was absent from weapon Agent snapshot");
        const auto* parent = FindField(*hierarchy, "parent");
        if (parent == nullptr || parent->value.kind != trace2d::agent::FieldValueKind::String || parent->value.stringValue != "game.player")
            return Fail(10, "Hierarchy2D.parent did not resolve to game.player");
        const auto* worldX = FindField(*hierarchy, "world.position.x");
        if (worldX == nullptr || worldX->value.kind != trace2d::agent::FieldValueKind::Float || worldX->value.floatValue != 3.0F)
            return Fail(11, "Hierarchy2D world.position.x did not reflect parent motion");

        const auto snapshot = application.Snapshot();
        if (snapshot.frame != 3U || snapshot.entityCount != 2U || snapshot.uiElementCount != 1U)
            return Fail(12, "Application snapshot counts/frame did not match E2 fixture");
        if (!snapshot.workSpecBound || !snapshot.workResultBound || snapshot.presentationBound)
            return Fail(13, "Application work/presentation bindings did not match headless fixture");
        if (game.ObservedWorkId() != spec.id || result.workId != spec.id || result.revisions.size() != 1U)
            return Fail(14, "WorkSpec/WorkResult identity was not preserved");

        application.Stop();
        if (game.FixedUpdateCount() != 3U) return Fail(15, "external Game fixed-update count was not exactly three");
        if (result.revisions.front().verification.size() != 1U) return Fail(16, "external Game did not publish exactly one verification record");
        const auto& verification = result.revisions.front().verification.front();
        if (verification.acceptanceId != spec.acceptance.front().id || verification.outcome != trace2d::agent::VerificationOutcome::Passed)
            return Fail(17, "external Game verification record did not pass the E2 acceptance ID");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Trace2D E2 external gate exception: " << error.what() << '\n';
        return 20;
    }
}
