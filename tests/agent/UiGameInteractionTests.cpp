#include <trace2d/agent/Inspection.hpp>
#include <trace2d/scene/Scene.hpp>
#include <trace2d/ui/UiText.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>

namespace
{
constexpr std::string_view GameUi = R"(format_version = 1

[canvas]
width = 96
height = 48

[[elements]]
id = "start"
kind = "button"
bounds = [8, 8, 80, 24]
name = "Start Game"
text = "Start Game"
)";

void ConsumeStartActivations(
    trace2d::ui::UiDocument& document,
    trace2d::scene::Scene& scene,
    const trace2d::scene::EntityId playerId,
    std::uint64_t& consumedActivationCount)
{
    const trace2d::ui::UiElement* start = document.Find("start");
    ASSERT_NE(start, nullptr);

    trace2d::scene::Entity* player = scene.TryGet(playerId);
    ASSERT_NE(player, nullptr);

    while (consumedActivationCount < start->activationCount)
    {
        ++consumedActivationCount;
        player->Transform().position.x += 10.0F;
    }
}

TEST(AgentUiGameInteractionTests, SemanticActivationDrivesGameStateWithoutCoordinateTargeting)
{
    trace2d::ui::UiLoadResult load = trace2d::ui::LoadUiToml(GameUi, "game_ui.trace2d.toml");
    ASSERT_TRUE(load.Succeeded());
    ASSERT_TRUE(load.document.has_value());

    trace2d::scene::Scene scene{{.semanticId = "game", .name = "Game"}};
    trace2d::scene::EntityDescriptor playerDescriptor{};
    playerDescriptor.semanticId = "player";
    playerDescriptor.name = "Player";
    const trace2d::scene::EntityId playerId = scene.CreateEntity(std::move(playerDescriptor));

    trace2d::ui::UiDocument& document = *load.document;
    trace2d::agent::AgentFacade facade{nullptr, &scene, &document};

    trace2d::agent::UiSelector startSelector{};
    startSelector.role = trace2d::agent::UiRole::Button;
    startSelector.name = "Start Game";

    std::uint64_t consumedActivationCount = 0U;

    ASSERT_TRUE(facade.ActivateUi(startSelector).Succeeded());
    ConsumeStartActivations(document, scene, playerId, consumedActivationCount);

    const trace2d::agent::QueryOneResult firstState = facade.QueryOne("#player");
    ASSERT_TRUE(firstState.Succeeded());
    ASSERT_TRUE(firstState.match.has_value());
    EXPECT_FLOAT_EQ(firstState.match->transform.position.x, 10.0F);

    ConsumeStartActivations(document, scene, playerId, consumedActivationCount);
    const trace2d::agent::QueryOneResult unchangedState = facade.QueryOne("#player");
    ASSERT_TRUE(unchangedState.Succeeded());
    EXPECT_FLOAT_EQ(unchangedState.match->transform.position.x, 10.0F);

    ASSERT_TRUE(facade.ActivateUi(startSelector).Succeeded());
    ConsumeStartActivations(document, scene, playerId, consumedActivationCount);

    const trace2d::agent::QueryOneResult secondState = facade.QueryOne("#player");
    ASSERT_TRUE(secondState.Succeeded());
    EXPECT_FLOAT_EQ(secondState.match->transform.position.x, 20.0F);
    EXPECT_EQ(consumedActivationCount, 2U);
}
} // namespace
