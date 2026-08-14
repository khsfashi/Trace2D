#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/scene/World.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace
{
struct Health final
{
    std::int64_t current{100};
    std::int64_t maximum{100};
};

trace2d::scene::SemanticValue IntValue(const std::int64_t value)
{
    trace2d::scene::SemanticValue result{};
    result.kind = trace2d::scene::SemanticValueKind::SignedInteger;
    result.signedIntegerValue = value;
    return result;
}

trace2d::scene::ComponentTypeHandle<Health> RegisterHealth(trace2d::scene::ComponentRegistry& registry)
{
    trace2d::scene::ComponentRegistration<Health> registration{};
    registration.typeId = "game.health";
    registration.schemaVersion = 1;
    registration.componentClass = trace2d::scene::ComponentClass::Authored;
    registration.parseAuthored = [](const auto& authored, Health& health, std::string& error)
    {
        const auto* current = authored.Find("current");
        const auto* maximum = authored.Find("maximum");
        if (authored.fields.size() != 2U || current == nullptr || maximum == nullptr ||
            current->kind != trace2d::scene::SemanticValueKind::SignedInteger ||
            maximum->kind != trace2d::scene::SemanticValueKind::SignedInteger)
        {
            error = "game.health requires signed integer current and maximum.";
            return false;
        }
        health.current = current->signedIntegerValue;
        health.maximum = maximum->signedIntegerValue;
        return true;
    };
    registration.validate = [](const Health& health, std::string& error)
    {
        if (health.maximum <= 0 || health.current < 0 || health.current > health.maximum)
        {
            error = "game.health requires 0 <= current <= maximum and maximum > 0.";
            return false;
        }
        return true;
    };
    registration.serializeAuthored = [](const Health& health)
    {
        trace2d::scene::ComponentAuthoringObject authored{};
        authored.fields.push_back({"current", IntValue(health.current)});
        authored.fields.push_back({"maximum", IntValue(health.maximum)});
        return authored;
    };
    registration.inspect = [](const Health& health)
    {
        return std::vector<trace2d::scene::ComponentInspectionField>{
            {"current", IntValue(health.current)},
            {"maximum", IntValue(health.maximum)},
        };
    };
    return registry.Register(std::move(registration));
}

struct Fixture final
{
    trace2d::scene::ComponentRegistry components{};
    trace2d::scene::ComponentTypeHandle<Health> health{};
    trace2d::assets::ResourceRegistry resources{"."};
    trace2d::assets::ResourceHandle<trace2d::assets::TextureResource> texture{};
    trace2d::assets::ResourceHandle<trace2d::assets::SceneTemplateResource> sceneTemplate{};

    Fixture()
    {
        (void)trace2d::scene::RegisterSceneComponents(components);
        health = RegisterHealth(components);
        components.Freeze();

        trace2d::assets::TextureResource textureResource{};
        textureResource.width = 1U;
        textureResource.height = 1U;
        textureResource.canonicalRgba8 = {255U, 255U, 255U, 255U};
        const auto textureResult = resources.PublishTexture("textures/enemy.png", std::move(textureResource));
        if (!textureResult.Succeeded()) throw std::runtime_error{"failed to publish fixture texture"};
        texture = textureResult.handle;

        trace2d::assets::SceneTemplateResource templateResource{};
        templateResource.canonicalToml = std::string{TemplateToml};
        const std::array dependencies{texture.Untyped()};
        const auto templateResult = resources.PublishSceneTemplate(
            "templates/enemy.trace2d.toml",
            std::move(templateResource),
            dependencies);
        if (!templateResult.Succeeded()) throw std::runtime_error{"failed to publish fixture scene template"};
        sceneTemplate = templateResult.handle;
    }

    static constexpr std::string_view TemplateToml = R"toml(format_version = 2

[scene]
id = "enemy-template"
name = "Enemy Template"

[[entities]]
id = "body"
name = "Body"
tags = ["enemy"]

[entities.transform]
position = [1, 2]
rotation_radians = 0
scale = [1, 1]

[[entities.components]]
type = "game.health"
version = 1

[entities.components.data]
current = 80
maximum = 100

[[entities.components]]
type = "trace2d.visibility2d"
version = 1

[entities.components.data]
visible = true

[[entities]]
id = "weapon"
name = "Weapon"
parent = "body"

[entities.transform]
position = [3, 0]
rotation_radians = 0
scale = [1, 1]
)toml";
};

trace2d::scene::TemplateComponentOverride HealthOverride(
    const std::string& localEntityId,
    const std::int64_t current,
    const std::int64_t maximum)
{
    trace2d::scene::AuthoredComponentSnapshot component{};
    component.typeId = "game.health";
    component.schemaVersion = 1U;
    component.data.fields.push_back({"current", IntValue(current)});
    component.data.fields.push_back({"maximum", IntValue(maximum)});
    return trace2d::scene::TemplateComponentOverride{localEntityId, std::move(component)};
}

trace2d::scene::TemplateInstantiationRequest Request(
    const Fixture& fixture,
    std::string instanceId,
    const float rootX)
{
    trace2d::scene::TemplateInstantiationRequest request{};
    request.worldId = "main";
    request.templateResource = fixture.sceneTemplate;
    request.instanceId = std::move(instanceId);
    request.rootTransform.position.x = rootX;
    return request;
}
} // namespace

TEST(WorldTests, InstantiatesReusableHierarchyTwiceWithTypedOverrideAndSharedResourceRetention)
{
    Fixture fixture{};
    trace2d::scene::WorldLifecycle worlds{fixture.components, fixture.resources};
    ASSERT_TRUE(worlds.CreateWorld({"main", "Main", 0}).Succeeded());

    auto first = Request(fixture, "enemy-a", 10.0F);
    first.componentOverrides.push_back(HealthOverride("body", 40, 100));
    ASSERT_TRUE(worlds.Instantiate(first).Succeeded());
    ASSERT_TRUE(worlds.Instantiate(Request(fixture, "enemy-b", 20.0F)).Succeeded());

    const auto stats = worlds.Stats();
    EXPECT_EQ(stats.templateCompiles, 1U);
    EXPECT_EQ(stats.templateCacheHits, 1U);
    EXPECT_EQ(stats.compiledTemplateCount, 1U);

    const trace2d::scene::Scene* scene = worlds.TryGetScene("main");
    ASSERT_NE(scene, nullptr);
    EXPECT_EQ(scene->EntityCount(), 6U); // two instance anchors + two authored entities per instance

    const auto firstBody = worlds.FindInstanceEntity("main", "enemy-a", "body");
    const auto secondBody = worlds.FindInstanceEntity("main", "enemy-b", "body");
    ASSERT_TRUE(firstBody.has_value());
    ASSERT_TRUE(secondBody.has_value());
    EXPECT_NE(*firstBody, *secondBody);

    const Health* firstHealth = scene->TryGetComponent(*firstBody, fixture.health);
    const Health* secondHealth = scene->TryGetComponent(*secondBody, fixture.health);
    ASSERT_NE(firstHealth, nullptr);
    ASSERT_NE(secondHealth, nullptr);
    EXPECT_EQ(firstHealth->current, 40);
    EXPECT_EQ(secondHealth->current, 80);

    trace2d::scene::Transform2D firstWorld{};
    ASSERT_TRUE(scene->TryGetWorldTransform(*firstBody, firstWorld));
    EXPECT_FLOAT_EQ(firstWorld.position.x, 11.0F);
    EXPECT_FLOAT_EQ(firstWorld.position.y, 2.0F);

    const auto templateSnapshot = fixture.resources.Inspect(fixture.sceneTemplate.Untyped());
    ASSERT_TRUE(templateSnapshot.has_value());
    EXPECT_EQ(templateSnapshot->callerRetainCount, 2U);
    ASSERT_EQ(templateSnapshot->dependencies.size(), 1U);
    EXPECT_EQ(templateSnapshot->dependencies.front().canonicalReference, "textures/enemy.png");
    EXPECT_EQ(templateSnapshot->memory.cpuRetention, trace2d::assets::CpuRetentionPolicy::Required);
    EXPECT_TRUE(templateSnapshot->memory.cpuPayloadResident);
    EXPECT_GT(templateSnapshot->memory.knownRetainedCpuBytes, Fixture::TemplateToml.size());
    EXPECT_EQ(trace2d::assets::ToString(templateSnapshot->identity.domain), "scene_template");

    const auto textureUnload = fixture.resources.Unload(fixture.texture.Untyped());
    ASSERT_FALSE(textureUnload.Succeeded());
    ASSERT_TRUE(textureUnload.diagnostic.has_value());
    EXPECT_EQ(textureUnload.diagnostic->code, trace2d::assets::ResourceErrorCode::HasDependents);

    ASSERT_TRUE(worlds.Despawn("main", "enemy-a").Succeeded());
    ASSERT_TRUE(worlds.Despawn("main", "enemy-b").Succeeded());
    const auto releasedSnapshot = fixture.resources.Inspect(fixture.sceneTemplate.Untyped());
    ASSERT_TRUE(releasedSnapshot.has_value());
    EXPECT_EQ(releasedSnapshot->callerRetainCount, 0U);
}

TEST(WorldTests, SafePointCommandsAreFifoAndDespawnNeverAliasesAReusedEntitySlot)
{
    Fixture fixture{};
    trace2d::scene::WorldLifecycle worlds{fixture.components, fixture.resources};
    ASSERT_TRUE(worlds.CreateWorld({"main", "Main", 0}).Succeeded());

    const std::uint64_t spawnSequence = worlds.QueueInstantiate(Request(fixture, "enemy", 0.0F));
    EXPECT_EQ(worlds.PendingStructuralCommandCount(), 1U);
    EXPECT_FALSE(worlds.FindInstanceRoot("main", "enemy").has_value());

    const auto& firstCommit = worlds.CommitStructuralChanges();
    ASSERT_TRUE(firstCommit.Succeeded());
    ASSERT_EQ(firstCommit.results.size(), 1U);
    EXPECT_EQ(firstCommit.results[0].sequence, spawnSequence);
    EXPECT_EQ(firstCommit.results[0].kind, trace2d::scene::StructuralCommandKind::Instantiate);

    const auto staleBody = worlds.FindInstanceEntity("main", "enemy", "body");
    ASSERT_TRUE(staleBody.has_value());
    const trace2d::scene::Scene* scene = worlds.TryGetScene("main");
    ASSERT_NE(scene, nullptr);
    ASSERT_TRUE(scene->Contains(*staleBody));

    const std::uint64_t despawnSequence = worlds.QueueDespawn("main", "enemy");
    const std::uint64_t respawnSequence = worlds.QueueInstantiate(Request(fixture, "enemy", 5.0F));
    const auto& secondCommit = worlds.CommitStructuralChanges();
    ASSERT_TRUE(secondCommit.Succeeded());
    ASSERT_EQ(secondCommit.results.size(), 2U);
    EXPECT_EQ(secondCommit.results[0].sequence, despawnSequence);
    EXPECT_EQ(secondCommit.results[0].kind, trace2d::scene::StructuralCommandKind::Despawn);
    EXPECT_EQ(secondCommit.results[1].sequence, respawnSequence);
    EXPECT_EQ(secondCommit.results[1].kind, trace2d::scene::StructuralCommandKind::Instantiate);

    const auto liveBody = worlds.FindInstanceEntity("main", "enemy", "body");
    ASSERT_TRUE(liveBody.has_value());
    EXPECT_FALSE(scene->Contains(*staleBody));
    EXPECT_TRUE(scene->Contains(*liveBody));
    EXPECT_NE(*staleBody, *liveBody);

    const auto stats = worlds.Stats();
    EXPECT_EQ(stats.queuedCommands, 3U);
    EXPECT_EQ(stats.committedCommands, 3U);
    EXPECT_EQ(stats.pendingCommands, 0U);
    EXPECT_GE(stats.retainedCommandCapacity, 16U);
}

TEST(WorldTests, InvalidOverrideRollsBackEntitiesAndTemplateRetainTransactionally)
{
    Fixture fixture{};
    trace2d::scene::WorldLifecycle worlds{fixture.components, fixture.resources};
    ASSERT_TRUE(worlds.CreateWorld({"main", "Main", 0}).Succeeded());
    trace2d::scene::Scene* scene = worlds.TryGetScene("main");
    ASSERT_NE(scene, nullptr);

    auto invalid = Request(fixture, "bad", 0.0F);
    invalid.componentOverrides.push_back(HealthOverride("body", 125, 100));
    const auto result = worlds.Instantiate(invalid);
    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(result.code, trace2d::scene::WorldOperationCode::ComponentFailure);
    EXPECT_EQ(scene->EntityCount(), 0U);
    EXPECT_FALSE(worlds.FindInstanceRoot("main", "bad").has_value());

    const auto templateSnapshot = fixture.resources.Inspect(fixture.sceneTemplate.Untyped());
    ASSERT_TRUE(templateSnapshot.has_value());
    EXPECT_EQ(templateSnapshot->callerRetainCount, 0U);

    const auto conflict = scene->CreateEntity({.semanticId = "main/conflict"});
    const auto conflictResult = worlds.Instantiate(Request(fixture, "conflict", 0.0F));
    ASSERT_FALSE(conflictResult.Succeeded());
    EXPECT_EQ(conflictResult.code, trace2d::scene::WorldOperationCode::SemanticConflict);
    EXPECT_EQ(scene->EntityCount(), 1U);
    EXPECT_TRUE(scene->Contains(conflict));
}

TEST(WorldTests, WorldOrderIsExplicitAndUnloadReloadPreservesSceneIncarnationForStaleSafety)
{
    Fixture fixture{};
    trace2d::scene::WorldLifecycle worlds{fixture.components, fixture.resources};
    ASSERT_TRUE(worlds.CreateWorld({"beta", "Beta", 10}).Succeeded());
    ASSERT_TRUE(worlds.CreateWorld({"alpha", "Alpha", 10}).Succeeded());
    ASSERT_TRUE(worlds.CreateWorld({"early", "Early", -1}).Succeeded());

    ASSERT_EQ(worlds.OrderedWorldCount(), 3U);
    EXPECT_EQ(worlds.OrderedWorldId(0), "early");
    EXPECT_EQ(worlds.OrderedWorldId(1), "alpha");
    EXPECT_EQ(worlds.OrderedWorldId(2), "beta");

    trace2d::scene::TemplateInstantiationRequest request{};
    request.worldId = "alpha";
    request.templateResource = fixture.sceneTemplate;
    request.instanceId = "resident";
    ASSERT_TRUE(worlds.Instantiate(request).Succeeded());
    const auto oldEntity = worlds.FindInstanceEntity("alpha", "resident", "body");
    ASSERT_TRUE(oldEntity.has_value());
    trace2d::scene::Scene* alphaScene = worlds.TryGetScene("alpha");
    ASSERT_NE(alphaScene, nullptr);

    ASSERT_TRUE(worlds.UnloadWorld("alpha").Succeeded());
    EXPECT_EQ(worlds.OrderedWorldCount(), 2U);
    EXPECT_EQ(worlds.OrderedWorldId(0), "early");
    EXPECT_EQ(worlds.OrderedWorldId(1), "beta");
    EXPECT_FALSE(alphaScene->Contains(*oldEntity));

    ASSERT_TRUE(worlds.CreateWorld({"alpha", "Alpha Reloaded", 10}).Succeeded());
    EXPECT_EQ(worlds.TryGetScene("alpha"), alphaScene);
    EXPECT_FALSE(alphaScene->Contains(*oldEntity));
    ASSERT_EQ(worlds.OrderedWorldCount(), 3U);
    EXPECT_EQ(worlds.OrderedWorldId(0), "early");
    EXPECT_EQ(worlds.OrderedWorldId(1), "alpha");
    EXPECT_EQ(worlds.OrderedWorldId(2), "beta");

    request.instanceId = "resident";
    ASSERT_TRUE(worlds.Instantiate(request).Succeeded());
    const auto newEntity = worlds.FindInstanceEntity("alpha", "resident", "body");
    ASSERT_TRUE(newEntity.has_value());
    EXPECT_NE(*oldEntity, *newEntity);
}
