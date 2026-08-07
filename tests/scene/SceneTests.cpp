#include <trace2d/scene/Scene.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
trace2d::scene::EntityDescriptor MakeEntity(
    std::string semanticId,
    std::string name,
    std::vector<std::string> tags = {})
{
    trace2d::scene::EntityDescriptor descriptor{};
    descriptor.semanticId = std::move(semanticId);
    descriptor.name = std::move(name);
    descriptor.tags = std::move(tags);
    return descriptor;
}

TEST(SceneTests, CreatesQueriesAndOwnsEntityState)
{
    trace2d::scene::Scene scene{};
    trace2d::scene::EntityDescriptor descriptor =
        MakeEntity("player", "Player", {"controllable", "hero", "hero"});
    descriptor.transform.position = {10.0F, -4.0F};
    descriptor.transform.rotationRadians = 0.5F;
    descriptor.transform.scale = {2.0F, 3.0F};

    const trace2d::scene::EntityId id = scene.CreateEntity(std::move(descriptor));

    EXPECT_TRUE(id.IsValid());
    EXPECT_TRUE(scene.Contains(id));
    EXPECT_EQ(scene.EntityCount(), 1U);
    EXPECT_EQ(scene.FindBySemanticId("player"), id);

    const trace2d::scene::Entity* entity = scene.TryGet(id);
    ASSERT_NE(entity, nullptr);
    EXPECT_EQ(entity->SemanticId(), "player");
    EXPECT_EQ(entity->Name(), "Player");
    EXPECT_TRUE(entity->HasTag("controllable"));
    EXPECT_TRUE(entity->HasTag("hero"));
    EXPECT_FALSE(entity->HasTag("enemy"));
    EXPECT_EQ(entity->Tags(), (std::vector<std::string>{"controllable", "hero"}));
    EXPECT_EQ(entity->Transform().position, (trace2d::scene::Vector2{10.0F, -4.0F}));
    EXPECT_FLOAT_EQ(entity->Transform().rotationRadians, 0.5F);
    EXPECT_EQ(entity->Transform().scale, (trace2d::scene::Vector2{2.0F, 3.0F}));
}

TEST(SceneTests, MutableLookupUpdatesTransformWithoutChangingIdentity)
{
    trace2d::scene::Scene scene{};
    const trace2d::scene::EntityId id = scene.CreateEntity(MakeEntity("player", "Player"));

    trace2d::scene::Entity* entity = scene.TryGet(id);
    ASSERT_NE(entity, nullptr);
    entity->Transform().position = {3.0F, 7.0F};

    const trace2d::scene::Entity* observed = scene.TryGet(id);
    ASSERT_NE(observed, nullptr);
    EXPECT_EQ(observed->Transform().position, (trace2d::scene::Vector2{3.0F, 7.0F}));
    EXPECT_EQ(scene.FindBySemanticId("player"), id);
}

TEST(SceneTests, DestroyedHandleStaysStaleAfterSlotReuse)
{
    trace2d::scene::Scene scene{};
    const trace2d::scene::EntityId first = scene.CreateEntity(MakeEntity("first", "First"));

    EXPECT_TRUE(scene.DestroyEntity(first));
    EXPECT_FALSE(scene.Contains(first));
    EXPECT_EQ(scene.TryGet(first), nullptr);
    EXPECT_FALSE(scene.FindBySemanticId("first").has_value());
    EXPECT_EQ(scene.EntityCount(), 0U);

    const trace2d::scene::EntityId replacement = scene.CreateEntity(MakeEntity("replacement", "Replacement"));

    EXPECT_EQ(replacement.index, first.index);
    EXPECT_NE(replacement.generation, first.generation);
    EXPECT_TRUE(scene.Contains(replacement));
    EXPECT_FALSE(scene.Contains(first));
    EXPECT_FALSE(scene.DestroyEntity(first));
}

TEST(SceneTests, RejectsDuplicateNonEmptySemanticIds)
{
    trace2d::scene::Scene scene{};
    (void)scene.CreateEntity(MakeEntity("player", "First Player"));

    EXPECT_THROW(
        (void)scene.CreateEntity(MakeEntity("player", "Duplicate Player")),
        std::invalid_argument);
    EXPECT_EQ(scene.EntityCount(), 1U);
}

TEST(SceneTests, AllowsRuntimeEntitiesWithoutSemanticIds)
{
    trace2d::scene::Scene scene{};

    const trace2d::scene::EntityId first = scene.CreateEntity(MakeEntity("", "Projectile A"));
    const trace2d::scene::EntityId second = scene.CreateEntity(MakeEntity("", "Projectile B"));

    EXPECT_NE(first, second);
    EXPECT_EQ(scene.EntityCount(), 2U);
    EXPECT_FALSE(scene.FindBySemanticId("").has_value());
}

TEST(SceneTests, ObservableIterationUsesAscendingSlotIndex)
{
    trace2d::scene::Scene scene{};
    const trace2d::scene::EntityId player = scene.CreateEntity(MakeEntity("player", "Player"));
    const trace2d::scene::EntityId temporary = scene.CreateEntity(MakeEntity("temporary", "Temporary"));
    const trace2d::scene::EntityId boss = scene.CreateEntity(MakeEntity("boss", "Boss"));

    ASSERT_TRUE(scene.DestroyEntity(temporary));
    const trace2d::scene::EntityId enemy = scene.CreateEntity(MakeEntity("enemy", "Enemy"));
    ASSERT_EQ(enemy.index, temporary.index);

    std::vector<std::uint32_t> indices{};
    std::vector<std::string_view> semanticIds{};
    scene.ForEachEntity(
        [&](const trace2d::scene::EntityId id, const trace2d::scene::Entity& entity)
        {
            indices.push_back(id.index);
            semanticIds.push_back(entity.SemanticId());
        });

    EXPECT_EQ(indices, (std::vector<std::uint32_t>{player.index, enemy.index, boss.index}));
    EXPECT_EQ(semanticIds, (std::vector<std::string_view>{"player", "enemy", "boss"}));
}
} // namespace
