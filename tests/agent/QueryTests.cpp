#include <trace2d/agent/Inspection.hpp>

#include <trace2d/scene/Scene.hpp>

#include <gtest/gtest.h>

#include <string>
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

TEST(AgentQueryTests, SelectsAuthoredIdExactly)
{
    trace2d::scene::Scene scene{{.semanticId = "arena", .name = "Arena"}};
    static_cast<void>(scene.CreateEntity(MakeEntity("player", "Player", {"hero"})));
    static_cast<void>(scene.CreateEntity(MakeEntity("boss", "Boss", {"enemy"})));

    const trace2d::agent::AgentFacade facade{nullptr, &scene};
    const trace2d::agent::QueryResult result = facade.Query("#player");

    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.selector.has_value());
    EXPECT_EQ(result.selector->kind, trace2d::agent::SelectorKind::SemanticId);
    EXPECT_EQ(result.selector->value, "player");
    ASSERT_EQ(result.matches.size(), 1U);
    EXPECT_EQ(result.matches[0].semanticId, "player");
    EXPECT_EQ(result.matches[0].name, "Player");
}

TEST(AgentQueryTests, QueryOneResolvesCanonicalDottedSemanticId)
{
    trace2d::scene::Scene scene{{.semanticId = "arena", .name = "Arena"}};
    const auto player = scene.CreateEntity(MakeEntity("game.player", "Player"));
    const auto weapon = scene.CreateEntity(MakeEntity("game.weapon", "Weapon"));
    ASSERT_EQ(scene.SetParent(weapon, player), trace2d::scene::HierarchyResult::Success);

    const trace2d::agent::AgentFacade facade{nullptr, &scene};
    const trace2d::agent::QueryOneResult result = facade.QueryOne("#game.weapon");

    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.match.has_value());
    EXPECT_EQ(result.match->semanticId, "game.weapon");
    ASSERT_TRUE(result.match->parentSemanticId.has_value());
    EXPECT_EQ(*result.match->parentSemanticId, "game.player");
}

TEST(AgentQueryTests, SelectsNameAndComponentTypeExactly)
{
    trace2d::scene::Scene scene{{.semanticId = "arena", .name = "Arena"}};
    static_cast<void>(scene.CreateEntity(MakeEntity("player", "Player")));
    static_cast<void>(scene.CreateEntity(MakeEntity("boss", "Boss")));

    const trace2d::agent::AgentFacade facade{nullptr, &scene};

    const trace2d::agent::QueryResult byName = facade.Query("name:Boss");
    ASSERT_TRUE(byName.Succeeded());
    ASSERT_EQ(byName.matches.size(), 1U);
    EXPECT_EQ(byName.matches[0].semanticId, "boss");

    const trace2d::agent::QueryResult byType = facade.Query("type:Transform2D");
    ASSERT_TRUE(byType.Succeeded());
    ASSERT_EQ(byType.matches.size(), 2U);
    EXPECT_EQ(byType.matches[0].semanticId, "player");
    EXPECT_EQ(byType.matches[1].semanticId, "boss");
}

TEST(AgentQueryTests, TagQueryUsesDeterministicSceneOrder)
{
    trace2d::scene::Scene scene{{.semanticId = "arena", .name = "Arena"}};
    const trace2d::scene::EntityId first =
        scene.CreateEntity(MakeEntity("enemy_a", "Enemy A", {"enemy"}));
    const trace2d::scene::EntityId temporary =
        scene.CreateEntity(MakeEntity("temporary", "Temporary"));
    const trace2d::scene::EntityId third =
        scene.CreateEntity(MakeEntity("enemy_c", "Enemy C", {"enemy"}));

    ASSERT_TRUE(scene.DestroyEntity(temporary));
    const trace2d::scene::EntityId second =
        scene.CreateEntity(MakeEntity("enemy_b", "Enemy B", {"enemy"}));
    ASSERT_EQ(second.index, temporary.index);

    const trace2d::agent::AgentFacade facade{nullptr, &scene};
    const trace2d::agent::QueryResult firstQuery = facade.Query("tag:enemy");
    const trace2d::agent::QueryResult secondQuery = facade.Query("tag:enemy");

    ASSERT_TRUE(firstQuery.Succeeded());
    ASSERT_TRUE(secondQuery.Succeeded());
    EXPECT_EQ(firstQuery.matches, secondQuery.matches);
    ASSERT_EQ(firstQuery.matches.size(), 3U);
    EXPECT_EQ(firstQuery.matches[0].handle.index, first.index);
    EXPECT_EQ(firstQuery.matches[0].semanticId, "enemy_a");
    EXPECT_EQ(firstQuery.matches[1].handle.index, second.index);
    EXPECT_EQ(firstQuery.matches[1].semanticId, "enemy_b");
    EXPECT_EQ(firstQuery.matches[2].handle.index, third.index);
    EXPECT_EQ(firstQuery.matches[2].semanticId, "enemy_c");
}

TEST(AgentQueryTests, MultiQueryTreatsNoMatchAsSuccessfulEmptyResult)
{
    trace2d::scene::Scene scene{{.semanticId = "arena", .name = "Arena"}};
    static_cast<void>(scene.CreateEntity(MakeEntity("player", "Player", {"hero"})));

    const trace2d::agent::AgentFacade facade{nullptr, &scene};
    const trace2d::agent::QueryResult result = facade.Query("tag:enemy");

    ASSERT_TRUE(result.Succeeded());
    EXPECT_TRUE(result.matches.empty());
    EXPECT_FALSE(result.error.has_value());
}

TEST(AgentQueryTests, SingleQueryReportsNoMatchAndAmbiguity)
{
    trace2d::scene::Scene scene{{.semanticId = "arena", .name = "Arena"}};
    static_cast<void>(scene.CreateEntity(MakeEntity("enemy_a", "Enemy A", {"enemy"})));
    static_cast<void>(scene.CreateEntity(MakeEntity("enemy_b", "Enemy B", {"enemy"})));

    const trace2d::agent::AgentFacade facade{nullptr, &scene};

    const trace2d::agent::QueryOneResult noMatch = facade.QueryOne("#player");
    ASSERT_FALSE(noMatch.Succeeded());
    ASSERT_TRUE(noMatch.error.has_value());
    EXPECT_EQ(noMatch.error->code, trace2d::agent::QueryErrorCode::NoMatch);
    EXPECT_EQ(trace2d::agent::ToString(noMatch.error->code), "no_match");

    const trace2d::agent::QueryOneResult ambiguous = facade.QueryOne("tag:enemy");
    ASSERT_FALSE(ambiguous.Succeeded());
    ASSERT_TRUE(ambiguous.error.has_value());
    EXPECT_EQ(ambiguous.error->code, trace2d::agent::QueryErrorCode::AmbiguousMatch);
    EXPECT_EQ(trace2d::agent::ToString(ambiguous.error->code), "ambiguous_match");
    EXPECT_FALSE(ambiguous.error->message.empty());
}

TEST(AgentQueryTests, InvalidSelectorSyntaxReturnsStructuredDiagnostic)
{
    trace2d::scene::Scene scene{{.semanticId = "arena", .name = "Arena"}};
    const trace2d::agent::AgentFacade facade{nullptr, &scene};

    const trace2d::agent::QueryResult unsupported = facade.Query("enemy");
    ASSERT_FALSE(unsupported.Succeeded());
    ASSERT_TRUE(unsupported.error.has_value());
    EXPECT_EQ(unsupported.error->code, trace2d::agent::QueryErrorCode::InvalidSelector);
    EXPECT_EQ(trace2d::agent::ToString(unsupported.error->code), "invalid_selector");
    EXPECT_FALSE(unsupported.error->message.empty());

    const trace2d::agent::QueryResult emptyValue = facade.Query("tag:");
    ASSERT_FALSE(emptyValue.Succeeded());
    ASSERT_TRUE(emptyValue.error.has_value());
    EXPECT_EQ(emptyValue.error->code, trace2d::agent::QueryErrorCode::InvalidSelector);
}

TEST(AgentQueryTests, QueryRequiresAnActiveScene)
{
    const trace2d::agent::AgentFacade facade{nullptr, nullptr};
    const trace2d::agent::QueryResult result = facade.Query("#player");

    ASSERT_FALSE(result.Succeeded());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->code, trace2d::agent::QueryErrorCode::SceneUnavailable);
    EXPECT_EQ(trace2d::agent::ToString(result.error->code), "scene_unavailable");
}
} // namespace
