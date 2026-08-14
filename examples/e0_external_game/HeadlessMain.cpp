#include "ExampleGame.hpp"

#include <trace2d/agent/Inspection.hpp>
#include <trace2d/agent/WorkResult.hpp>
#include <trace2d/agent/WorkSpec.hpp>
#include <trace2d/assets/ResourceRegistry.hpp>

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

trace2d::scene::SemanticValue IntValue(const std::int64_t value)
{
    trace2d::scene::SemanticValue result{};
    result.kind = trace2d::scene::SemanticValueKind::SignedInteger;
    result.signedIntegerValue = value;
    return result;
}

trace2d::scene::TemplateComponentOverride HealthOverride(const std::int64_t current)
{
    trace2d::scene::AuthoredComponentSnapshot component{};
    component.typeId = "game.health";
    component.schemaVersion = 1U;
    component.data.fields.push_back({"current", IntValue(current)});
    component.data.fields.push_back({"maximum", IntValue(100)});
    return trace2d::scene::TemplateComponentOverride{"body", std::move(component)};
}

trace2d::scene::TemplateInstantiationRequest EnemyRequest(
    const trace2d::assets::ResourceHandle<trace2d::assets::SceneTemplateResource> sceneTemplate,
    std::string instanceId,
    const float rootX)
{
    trace2d::scene::TemplateInstantiationRequest request{};
    request.worldId = "arena";
    request.templateResource = sceneTemplate;
    request.instanceId = std::move(instanceId);
    request.rootTransform.position.x = rootX;
    return request;
}

int Fail(const int code, const std::string_view message)
{
    std::cerr << "Trace2D external gate failure [" << code << "]: " << message << '\n';
    return code;
}

int FailQuery(const int code, const std::string_view stage, const trace2d::agent::QueryOneResult& query)
{
    std::cerr << "Trace2D external gate query failure [" << code << "] at " << stage;
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

        trace2d::assets::ResourceRegistry resources{"."};
        trace2d::assets::TextureResource sharedTexture{};
        sharedTexture.width = 1U;
        sharedTexture.height = 1U;
        sharedTexture.canonicalRgba8 = {255U, 255U, 255U, 255U};
        const auto publishedTexture = resources.PublishTexture("runtime/w0-shared.rgba8", std::move(sharedTexture));
        if (!publishedTexture.Succeeded()) return Fail(22, "shared W0 texture resource publication failed");
        const auto sceneTemplate = LoadExampleSceneTemplate(
            resources,
            publishedTexture.handle,
            "content/templates/enemy.trace2d.toml");

        trace2d::agent::WorkSpec spec{};
        spec.id = "e2-w0-external-game";
        spec.intent = "Prove deterministic authored composition plus W0 template/world lifecycle.";
        spec.acceptance.push_back(trace2d::agent::AcceptanceCriterion{
            .id = "e2.external-game.composition",
            .description = "External authored hierarchy, engine components, and game.health remain typed, deterministic, and Agent-visible.",
            .verification = trace2d::agent::VerificationClass::Deterministic,
        });
        trace2d::agent::WorkResult result{};

        trace2d::application::ApplicationConfig config{};
        config.runtime.fixedTimestep = 10ms;
        config.runtime.seed = 87;
        config.scene.semanticId = "w0.placeholder";
        config.scene.name = "W0 Placeholder";

        ExampleGame game{componentTypes, sceneTemplate};
        trace2d::application::Application application{game, config};
        application.Scene() = std::move(*secondLoad.scene);

        trace2d::scene::WorldLifecycle worlds{registry, resources};
        if (!worlds.AttachWorld({"main", "Main", 0}, application.Scene()).Succeeded())
            return Fail(23, "Application canonical Scene could not attach as W0 main world");
        if (!worlds.CreateWorld({"arena", "Arena", 10}).Succeeded())
            return Fail(24, "W0 additive arena world creation failed");
        trace2d::scene::Scene* const arenaBeforeUnload = worlds.TryGetScene("arena");
        if (arenaBeforeUnload == nullptr) return Fail(25, "W0 arena world did not expose canonical Scene state");

        auto firstEnemy = EnemyRequest(sceneTemplate, "enemy-a", 10.0F);
        firstEnemy.componentOverrides.push_back(HealthOverride(40));
        if (!worlds.Instantiate(firstEnemy).Succeeded()) return Fail(26, "first reusable template instance failed");
        if (!worlds.Instantiate(EnemyRequest(sceneTemplate, "enemy-b", 20.0F)).Succeeded())
            return Fail(27, "second reusable template instance failed");

        const auto duplicate = worlds.Instantiate(firstEnemy);
        if (duplicate.code != trace2d::scene::WorldOperationCode::DuplicateInstance)
            return Fail(28, "duplicate W0 instance ID was not rejected structurally");
        auto staleTemplate = sceneTemplate;
        ++staleTemplate.generation;
        const auto staleReference = worlds.Instantiate(EnemyRequest(staleTemplate, "invalid-reference", 0.0F));
        if (staleReference.code != trace2d::scene::WorldOperationCode::TemplateStale)
            return Fail(29, "stale W0 template reference was not rejected structurally");

        const auto firstBody = worlds.FindInstanceEntity("arena", "enemy-a", "body");
        const auto firstWeapon = worlds.FindInstanceEntity("arena", "enemy-a", "weapon");
        if (!firstBody.has_value() || !firstWeapon.has_value())
            return Fail(30, "external W0 template instance did not publish stable local entities");
        const Health* const overriddenHealth = arenaBeforeUnload->TryGetComponent(*firstBody, componentTypes.health);
        if (overriddenHealth == nullptr || overriddenHealth->current != 40)
            return Fail(31, "external W0 typed authored override did not survive schema validation");
        const trace2d::scene::Entity* const weaponEntity = arenaBeforeUnload->TryGet(*firstWeapon);
        if (weaponEntity == nullptr || weaponEntity->Parent() != firstBody)
            return Fail(32, "external W0 hierarchy order/parent resolution was not deterministic");
        trace2d::scene::Transform2D weaponWorld{};
        if (!arenaBeforeUnload->TryGetWorldTransform(*firstWeapon, weaponWorld) || weaponWorld.position.x != 13.0F)
            return Fail(33, "external W0 root/local transform composition was incorrect");

        const auto instances = worlds.InspectInstances("arena");
        if (instances.size() != 2U || instances[0].instanceId != "enemy-a" || instances[1].instanceId != "enemy-b")
            return Fail(34, "external W0 instance inspection order/identity was not stable");
        const auto templateSnapshot = resources.Inspect(sceneTemplate.Untyped());
        if (!templateSnapshot.has_value() || templateSnapshot->callerRetainCount != 2U || templateSnapshot->dependencies.size() != 1U)
            return Fail(35, "external W0 shared resource ownership did not use the #86 registry");

        const trace2d::scene::EntityId staleBodyAcrossWorldReload = *firstBody;
        application.BindWorldLifecycle(&worlds);
        application.BindWorkContracts(&spec, &result);
        application.ScheduleInput(1, {.control = trace2d::input::InputControl::KeyD, .type = trace2d::input::InputEventType::Press});
        application.ScheduleInput(3, {.control = trace2d::input::InputControl::KeyD, .type = trace2d::input::InputEventType::Release});
        application.Start();
        application.StepFrames(3);

        if (!game.SpawnWasDeferredToSafePoint() || !game.DespawnWasDeferredToSafePoint() || !game.RuntimeDespawnInvalidatedHandle())
            return Fail(36, "external Game did not observe deterministic post-callback W0 structural safe points");
        const auto& finalStructuralCommit = worlds.LastCommitReport();
        if (!finalStructuralCommit.Succeeded() || finalStructuralCommit.results.size() != 1U ||
            finalStructuralCommit.results.front().kind != trace2d::scene::StructuralCommandKind::UnloadWorld)
            return Fail(37, "frame-3 structural commit did not deterministically unload the arena world");
        if (worlds.TryGetScene("arena") != nullptr || arenaBeforeUnload->Contains(staleBodyAcrossWorldReload))
            return Fail(38, "world unload did not invalidate the pre-unload entity handle");
        const auto releasedTemplate = resources.Inspect(sceneTemplate.Untyped());
        if (!releasedTemplate.has_value() || releasedTemplate->callerRetainCount != 0U)
            return Fail(39, "world unload did not release per-instance scene-template ownership");

        if (!worlds.CreateWorld({"arena", "Arena Reloaded", 10}).Succeeded())
            return Fail(40, "headless world reload failed");
        if (worlds.TryGetScene("arena") != arenaBeforeUnload || arenaBeforeUnload->Contains(staleBodyAcrossWorldReload))
            return Fail(41, "world reload did not preserve Scene incarnation generation safety");
        if (worlds.OrderedWorldCount() != 2U || worlds.OrderedWorldId(0) != "main" || worlds.OrderedWorldId(1) != "arena")
            return Fail(42, "headless additive world ordering changed after reload");
        if (!worlds.Instantiate(EnemyRequest(sceneTemplate, "enemy-a", 10.0F)).Succeeded())
            return Fail(43, "template instance could not be recreated after world reload");
        const auto reloadedBody = worlds.FindInstanceEntity("arena", "enemy-a", "body");
        if (!reloadedBody.has_value() || *reloadedBody == staleBodyAcrossWorldReload || arenaBeforeUnload->Contains(staleBodyAcrossWorldReload))
            return Fail(44, "stale entity handle aliased the reloaded template instance");

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
            return Fail(12, "Application snapshot counts/frame did not match external fixture");
        if (!snapshot.workSpecBound || !snapshot.workResultBound || snapshot.presentationBound || !snapshot.worldLifecycleBound)
            return Fail(13, "Application work/presentation/world bindings did not match headless fixture");
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
        std::cerr << "Trace2D external gate exception: " << error.what() << '\n';
        return 45;
    }
}
