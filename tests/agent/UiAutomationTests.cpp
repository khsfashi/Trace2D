#include <trace2d/agent/Inspection.hpp>
#include <trace2d/ui/Ui.hpp>

#include <gtest/gtest.h>

#include <string>

namespace
{
[[nodiscard]] trace2d::ui::UiElement MakeElement(
    std::string id,
    const trace2d::ui::UiElementKind kind,
    trace2d::ui::UiRect bounds,
    std::string name,
    std::string text = {})
{
    trace2d::ui::UiElement element{};
    element.id = std::move(id);
    element.kind = kind;
    element.bounds = bounds;
    element.name = std::move(name);
    element.text = std::move(text);
    return element;
}

[[nodiscard]] trace2d::ui::UiDocument MakeMainMenu()
{
    trace2d::ui::UiDocument document{160U, 96U};
    EXPECT_EQ(
        document.AddElement(MakeElement(
            "root",
            trace2d::ui::UiElementKind::Panel,
            {0U, 0U, 160U, 96U},
            "Main Menu")),
        trace2d::ui::UiActionResult::Success);
    EXPECT_EQ(
        document.AddElement(MakeElement(
            "start",
            trace2d::ui::UiElementKind::Button,
            {8U, 32U, 96U, 24U},
            "Start Game",
            "Start Game")),
        trace2d::ui::UiActionResult::Success);
    EXPECT_EQ(
        document.AddElement(MakeElement(
            "player_name",
            trace2d::ui::UiElementKind::TextInput,
            {8U, 64U, 120U, 24U},
            "Player Name",
            "Player")),
        trace2d::ui::UiActionResult::Success);
    return document;
}

TEST(AgentUiAutomationTests, HeadlessSemanticFlowUsesRoleAndNameWithoutCoordinates)
{
    trace2d::ui::UiDocument document = MakeMainMenu();
    trace2d::agent::AgentFacade facade{nullptr, nullptr, &document};

    const trace2d::agent::UiTreeResult tree = facade.InspectUi();
    ASSERT_TRUE(tree.Succeeded());
    ASSERT_TRUE(tree.tree.has_value());
    EXPECT_EQ(tree.tree->width, 160U);
    EXPECT_EQ(tree.tree->height, 96U);
    ASSERT_EQ(tree.tree->elements.size(), 3U);
    EXPECT_EQ(tree.tree->elements[0].id, "root");
    EXPECT_EQ(tree.tree->elements[1].id, "start");
    EXPECT_EQ(tree.tree->elements[2].id, "player_name");

    trace2d::agent::UiSelector startSelector{};
    startSelector.role = trace2d::agent::UiRole::Button;
    startSelector.name = "Start Game";

    const trace2d::agent::UiQueryOneResult start = facade.QueryOneUi(startSelector);
    ASSERT_TRUE(start.Succeeded());
    ASSERT_TRUE(start.match.has_value());
    EXPECT_EQ(start.match->id, "start");
    EXPECT_TRUE(start.match->visible);
    EXPECT_TRUE(start.match->enabled);
    EXPECT_FALSE(start.match->focused);

    const trace2d::agent::UiActionResponse firstActivation = facade.ActivateUi(startSelector);
    ASSERT_TRUE(firstActivation.Succeeded());
    EXPECT_EQ(firstActivation.element->activationCount, 1U);
    const trace2d::agent::UiActionResponse secondActivation = facade.ActivateUi(startSelector);
    ASSERT_TRUE(secondActivation.Succeeded());
    EXPECT_EQ(secondActivation.element->activationCount, 2U);

    trace2d::agent::UiSelector textSelector{};
    textSelector.role = trace2d::agent::UiRole::TextBox;
    textSelector.name = "Player Name";

    const trace2d::agent::UiActionResponse focus = facade.FocusUi(textSelector);
    ASSERT_TRUE(focus.Succeeded());
    EXPECT_TRUE(focus.element->focused);

    const trace2d::agent::UiActionResponse input = facade.InputUiText(textSelector, "Ada");
    ASSERT_TRUE(input.Succeeded());
    EXPECT_EQ(input.element->name, "Player Name");
    EXPECT_EQ(input.element->text, "Ada");
    EXPECT_TRUE(input.element->focused);

    trace2d::agent::UiExpectedState textExpected{};
    textExpected.visible = true;
    textExpected.enabled = true;
    textExpected.focused = true;
    textExpected.text = "Ada";
    const trace2d::agent::UiAssertionResult textAssertion = facade.AssertUi(textSelector, textExpected);
    ASSERT_TRUE(textAssertion.Succeeded());

    trace2d::agent::UiExpectedState buttonExpected{};
    buttonExpected.activationCount = 2U;
    const trace2d::agent::UiAssertionResult buttonAssertion = facade.AssertUi(startSelector, buttonExpected);
    EXPECT_TRUE(buttonAssertion.Succeeded());
}

TEST(AgentUiAutomationTests, MultiQueryPreservesAuthoredOrder)
{
    trace2d::ui::UiDocument document{128U, 64U};
    ASSERT_EQ(
        document.AddElement(MakeElement(
            "first",
            trace2d::ui::UiElementKind::Button,
            {0U, 0U, 48U, 20U},
            "Action",
            "First")),
        trace2d::ui::UiActionResult::Success);
    ASSERT_EQ(
        document.AddElement(MakeElement(
            "second",
            trace2d::ui::UiElementKind::Button,
            {0U, 24U, 48U, 20U},
            "Action",
            "Second")),
        trace2d::ui::UiActionResult::Success);

    trace2d::agent::AgentFacade facade{nullptr, nullptr, &document};
    trace2d::agent::UiSelector selector{};
    selector.role = trace2d::agent::UiRole::Button;

    const trace2d::agent::UiQueryResult first = facade.QueryUi(selector);
    const trace2d::agent::UiQueryResult second = facade.QueryUi(selector);
    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(second.Succeeded());
    EXPECT_EQ(first.matches, second.matches);
    ASSERT_EQ(first.matches.size(), 2U);
    EXPECT_EQ(first.matches[0].id, "first");
    EXPECT_EQ(first.matches[1].id, "second");
}

TEST(AgentUiAutomationTests, ReportsDeterministicSemanticFailures)
{
    trace2d::ui::UiDocument document = MakeMainMenu();
    trace2d::agent::AgentFacade facade{nullptr, nullptr, &document};

    trace2d::agent::UiSelector emptySelector{};
    const trace2d::agent::UiQueryResult invalid = facade.QueryUi(emptySelector);
    ASSERT_FALSE(invalid.Succeeded());
    ASSERT_TRUE(invalid.error.has_value());
    EXPECT_EQ(invalid.error->code, trace2d::agent::UiAutomationErrorCode::InvalidSelector);

    trace2d::agent::UiSelector textSelector{};
    textSelector.role = trace2d::agent::UiRole::TextBox;
    textSelector.name = "Player Name";
    const trace2d::agent::UiActionResponse inputBeforeFocus = facade.InputUiText(textSelector, "Ada");
    ASSERT_FALSE(inputBeforeFocus.Succeeded());
    ASSERT_TRUE(inputBeforeFocus.error.has_value());
    EXPECT_EQ(inputBeforeFocus.error->code, trace2d::agent::UiAutomationErrorCode::NotFocused);

    trace2d::agent::UiSelector startSelector{};
    startSelector.id = "start";
    const trace2d::agent::UiActionResponse textIntoButton = facade.InputUiText(startSelector, "Ada");
    ASSERT_FALSE(textIntoButton.Succeeded());
    ASSERT_TRUE(textIntoButton.error.has_value());
    EXPECT_EQ(textIntoButton.error->code, trace2d::agent::UiAutomationErrorCode::NotTextInput);

    trace2d::agent::UiExpectedState mismatch{};
    mismatch.activationCount = 99U;
    const trace2d::agent::UiAssertionResult assertion = facade.AssertUi(startSelector, mismatch);
    ASSERT_FALSE(assertion.Succeeded());
    ASSERT_TRUE(assertion.error.has_value());
    EXPECT_EQ(assertion.error->code, trace2d::agent::UiAutomationErrorCode::StateMismatch);
    EXPECT_FALSE(assertion.error->message.empty());

    trace2d::agent::AgentFacade unbound{};
    const trace2d::agent::UiTreeResult unavailable = unbound.InspectUi();
    ASSERT_FALSE(unavailable.Succeeded());
    ASSERT_TRUE(unavailable.error.has_value());
    EXPECT_EQ(unavailable.error->code, trace2d::agent::UiAutomationErrorCode::UiUnavailable);
    EXPECT_EQ(trace2d::agent::ToString(unavailable.error->code), "ui_unavailable");
}

TEST(AgentUiAutomationTests, HiddenAndDisabledControlsRejectActions)
{
    trace2d::ui::UiDocument document{64U, 64U};

    trace2d::ui::UiElement hidden = MakeElement(
        "hidden",
        trace2d::ui::UiElementKind::Button,
        {0U, 0U, 24U, 16U},
        "Hidden",
        "Hidden");
    hidden.visible = false;
    ASSERT_EQ(document.AddElement(std::move(hidden)), trace2d::ui::UiActionResult::Success);

    trace2d::ui::UiElement disabled = MakeElement(
        "disabled",
        trace2d::ui::UiElementKind::Button,
        {0U, 20U, 24U, 16U},
        "Disabled",
        "Disabled");
    disabled.enabled = false;
    ASSERT_EQ(document.AddElement(std::move(disabled)), trace2d::ui::UiActionResult::Success);

    trace2d::agent::AgentFacade facade{nullptr, nullptr, &document};

    trace2d::agent::UiSelector hiddenSelector{};
    hiddenSelector.id = "hidden";
    const trace2d::agent::UiActionResponse hiddenAction = facade.ActivateUi(hiddenSelector);
    ASSERT_FALSE(hiddenAction.Succeeded());
    ASSERT_TRUE(hiddenAction.error.has_value());
    EXPECT_EQ(hiddenAction.error->code, trace2d::agent::UiAutomationErrorCode::NotVisible);

    trace2d::agent::UiSelector disabledSelector{};
    disabledSelector.id = "disabled";
    const trace2d::agent::UiActionResponse disabledAction = facade.ActivateUi(disabledSelector);
    ASSERT_FALSE(disabledAction.Succeeded());
    ASSERT_TRUE(disabledAction.error.has_value());
    EXPECT_EQ(disabledAction.error->code, trace2d::agent::UiAutomationErrorCode::Disabled);
}
} // namespace
