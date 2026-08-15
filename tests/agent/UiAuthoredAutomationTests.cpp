#include <trace2d/agent/Inspection.hpp>
#include <trace2d/ui/UiText.hpp>

#include <gtest/gtest.h>

#include <string_view>

namespace
{
constexpr std::string_view AuthoredUi = R"(format_version = 1

[canvas]
width = 160
height = 96

[[elements]]
id = "root"
kind = "panel"
bounds = [0, 0, 160, 96]
name = "Main Menu"

[[elements]]
id = "start"
kind = "button"
parent = "root"
bounds = [8, 32, 96, 24]
name = "Start Game"
text = "Start Game"

[[elements]]
id = "secondary"
kind = "button"
parent = "root"
bounds = [112, 32, 40, 24]
name = "Secondary"
text = "Other"

[[elements]]
id = "player_name"
kind = "text_input"
parent = "root"
bounds = [8, 64, 120, 24]
name = "Player Name"
text = "Player"
)";

TEST(AgentUiAuthoredAutomationTests, AuthoredTomlRunsAHeadlessSemanticInteractionLoop)
{
    trace2d::ui::UiLoadResult load = trace2d::ui::LoadUiToml(AuthoredUi, "semantic_ui.trace2d.toml");
    ASSERT_TRUE(load.Succeeded());
    ASSERT_TRUE(load.document.has_value());

    trace2d::ui::UiDocument& document = *load.document;
    trace2d::agent::AgentFacade facade{nullptr, nullptr, &document};

    trace2d::agent::UiSelector start{};
    start.role = trace2d::agent::UiRole::Button;
    start.name = "Start Game";

    const trace2d::agent::UiQueryOneResult queriedStart = facade.QueryOneUi(start);
    ASSERT_TRUE(queriedStart.Succeeded());
    EXPECT_EQ(queriedStart.match->id, "start");
    ASSERT_TRUE(queriedStart.match->parentId.has_value());
    EXPECT_EQ(*queriedStart.match->parentId, "root");
    EXPECT_EQ(queriedStart.match->depth, 1U);
    EXPECT_EQ(
        queriedStart.match->localBounds,
        (trace2d::agent::UiRectSnapshot{8U, 32U, 96U, 24U}));
    EXPECT_EQ(
        queriedStart.match->bounds,
        (trace2d::agent::UiRectSnapshot{8U, 32U, 96U, 24U}));

    const trace2d::agent::UiActionResponse activated = facade.ActivateUi(start);
    ASSERT_TRUE(activated.Succeeded());
    EXPECT_EQ(activated.element->activationCount, 1U);
    ASSERT_TRUE(activated.element->parentId.has_value());
    EXPECT_EQ(*activated.element->parentId, "root");

    trace2d::agent::UiSelector playerName{};
    playerName.role = trace2d::agent::UiRole::TextBox;
    playerName.name = "Player Name";

    ASSERT_TRUE(facade.FocusUi(playerName).Succeeded());
    const trace2d::agent::UiActionResponse typed = facade.InputUiText(playerName, "Ada");
    ASSERT_TRUE(typed.Succeeded());
    EXPECT_EQ(typed.element->text, "Ada");
    EXPECT_EQ(typed.element->depth, 1U);

    trace2d::agent::UiExpectedState expected{};
    expected.focused = true;
    expected.text = "Ada";
    EXPECT_TRUE(facade.AssertUi(playerName, expected).Succeeded());
}

TEST(AgentUiAuthoredAutomationTests, SingleSemanticQueryRejectsAmbiguousRoles)
{
    trace2d::ui::UiLoadResult load = trace2d::ui::LoadUiToml(AuthoredUi);
    ASSERT_TRUE(load.Succeeded());
    ASSERT_TRUE(load.document.has_value());

    trace2d::agent::AgentFacade facade{nullptr, nullptr, &*load.document};
    trace2d::agent::UiSelector buttons{};
    buttons.role = trace2d::agent::UiRole::Button;

    const trace2d::agent::UiQueryOneResult result = facade.QueryOneUi(buttons);
    ASSERT_FALSE(result.Succeeded());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->code, trace2d::agent::UiAutomationErrorCode::AmbiguousMatch);
    EXPECT_EQ(trace2d::agent::ToString(result.error->code), "ambiguous_match");
    EXPECT_FALSE(result.error->message.empty());
}
} // namespace
