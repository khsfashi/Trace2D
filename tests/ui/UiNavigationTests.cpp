#include <trace2d/agent/Inspection.hpp>
#include <trace2d/input/ActionMap.hpp>
#include <trace2d/input/Input.hpp>
#include <trace2d/ui/Ui.hpp>

#include <gtest/gtest.h>

namespace trace2d::ui
{
namespace
{
void AddElement(
    UiDocument& document,
    const char* id,
    const UiElementKind kind,
    const UiRect bounds,
    const bool visible = true,
    const bool enabled = true)
{
    ASSERT_EQ(
        document.AddElement(UiElement{
            .id = id,
            .kind = kind,
            .bounds = bounds,
            .name = id,
            .visible = visible,
            .enabled = enabled,
        }),
        UiActionResult::Success);
}

void AddButton(
    UiDocument& document,
    const char* id,
    const UiRect bounds,
    const bool visible = true,
    const bool enabled = true)
{
    AddElement(document, id, UiElementKind::Button, bounds, visible, enabled);
}

void AddTextInput(UiDocument& document, const char* id, const UiRect bounds)
{
    AddElement(document, id, UiElementKind::TextInput, bounds);
}
} // namespace

TEST(UiNavigationTests, ForwardAndBackwardTraversalWrapsAndSkipsIneligibleElements)
{
    UiDocument document(240U, 140U);
    AddElement(document, "panel", UiElementKind::Panel, UiRect{0U, 0U, 220U, 120U});
    AddButton(document, "first", UiRect{10U, 10U, 60U, 20U});
    AddElement(document, "label", UiElementKind::Label, UiRect{10U, 34U, 60U, 20U});
    AddTextInput(document, "edit", UiRect{10U, 58U, 100U, 20U});
    AddButton(document, "disabled", UiRect{10U, 82U, 60U, 20U}, true, false);
    AddButton(document, "hidden", UiRect{80U, 82U, 60U, 20U}, false, true);
    AddButton(document, "last", UiRect{150U, 82U, 60U, 20U});

    EXPECT_EQ(document.FocusedElement(), nullptr);

    ASSERT_EQ(document.FocusNext(), UiActionResult::Success);
    ASSERT_NE(document.FocusedElement(), nullptr);
    EXPECT_EQ(document.FocusedElement()->id, "first");

    ASSERT_EQ(document.FocusNext(), UiActionResult::Success);
    EXPECT_EQ(document.FocusedElement()->id, "edit");

    ASSERT_EQ(document.FocusNext(), UiActionResult::Success);
    EXPECT_EQ(document.FocusedElement()->id, "last");

    ASSERT_EQ(document.FocusNext(), UiActionResult::Success);
    EXPECT_EQ(document.FocusedElement()->id, "first");

    ASSERT_EQ(document.FocusPrevious(), UiActionResult::Success);
    EXPECT_EQ(document.FocusedElement()->id, "last");

    document.ClearFocus();
    ASSERT_EQ(document.FocusPrevious(), UiActionResult::Success);
    EXPECT_EQ(document.FocusedElement()->id, "last");
}

TEST(UiNavigationTests, NoEligibleTargetReturnsNotFocusableWithoutCreatingFocus)
{
    UiDocument document(160U, 96U);
    AddElement(document, "panel", UiElementKind::Panel, UiRect{0U, 0U, 80U, 40U});
    AddElement(document, "label", UiElementKind::Label, UiRect{0U, 40U, 80U, 20U});
    AddButton(document, "disabled", UiRect{80U, 0U, 60U, 20U}, true, false);
    AddButton(document, "hidden", UiRect{80U, 24U, 60U, 20U}, false, true);

    EXPECT_EQ(document.FocusNext(), UiActionResult::NotFocusable);
    EXPECT_EQ(document.FocusPrevious(), UiActionResult::NotFocusable);
    EXPECT_EQ(document.FocusedElement(), nullptr);
}

TEST(UiNavigationTests, FocusChangeClearsCompositionButWrappingToSameTextInputPreservesIt)
{
    UiDocument document(180U, 96U);
    AddTextInput(document, "chat", UiRect{10U, 10U, 100U, 24U});
    AddButton(document, "send", UiRect{10U, 40U, 60U, 24U});

    ASSERT_EQ(document.Focus("chat"), UiActionResult::Success);
    ASSERT_EQ(
        document.ApplyTextInput(input::TextInputEvent{
            .type = input::TextInputEventType::Composition,
            .text = "preedit",
            .selectionStart = 2,
            .selectionLength = 1,
        }),
        UiActionResult::Success);
    ASSERT_TRUE(document.TextComposition().active);

    ASSERT_EQ(document.FocusNext(), UiActionResult::Success);
    ASSERT_NE(document.FocusedElement(), nullptr);
    EXPECT_EQ(document.FocusedElement()->id, "send");
    EXPECT_FALSE(document.TextComposition().active);

    UiDocument single(180U, 96U);
    AddTextInput(single, "only", UiRect{10U, 10U, 100U, 24U});
    ASSERT_EQ(single.Focus("only"), UiActionResult::Success);
    ASSERT_EQ(
        single.ApplyTextInput(input::TextInputEvent{
            .type = input::TextInputEventType::Composition,
            .text = "keep",
            .selectionStart = 1,
            .selectionLength = 0,
        }),
        UiActionResult::Success);

    ASSERT_EQ(single.FocusNext(), UiActionResult::Success);
    ASSERT_NE(single.FocusedElement(), nullptr);
    EXPECT_EQ(single.FocusedElement()->id, "only");
    EXPECT_TRUE(single.TextComposition().active);
    EXPECT_EQ(single.TextComposition().text, "keep");

    ASSERT_EQ(single.FocusPrevious(), UiActionResult::Success);
    EXPECT_TRUE(single.TextComposition().active);
}

TEST(UiNavigationTests, FocusedActivationSharesButtonCounterAuthorityAndRejectsTextInput)
{
    UiDocument document(180U, 96U);
    AddButton(document, "play", UiRect{10U, 10U, 60U, 24U});
    AddTextInput(document, "name", UiRect{10U, 40U, 100U, 24U});

    EXPECT_EQ(document.ActivateFocused(), UiActionResult::NotFocused);

    ASSERT_EQ(document.FocusNext(), UiActionResult::Success);
    ASSERT_EQ(document.ActivateFocused(), UiActionResult::Success);
    ASSERT_NE(document.Find("play"), nullptr);
    EXPECT_EQ(document.Find("play")->activationCount, 1U);

    agent::AgentFacade facade(nullptr, nullptr, &document);
    const agent::UiActionResponse semantic = facade.ActivateUi(agent::UiSelector{.id = "play"});
    ASSERT_TRUE(semantic.Succeeded());
    EXPECT_EQ(document.Find("play")->activationCount, 2U);

    ASSERT_EQ(document.FocusNext(), UiActionResult::Success);
    ASSERT_NE(document.FocusedElement(), nullptr);
    EXPECT_EQ(document.FocusedElement()->id, "name");
    EXPECT_EQ(document.ActivateFocused(), UiActionResult::NotActivatable);
    EXPECT_EQ(document.Find("play")->activationCount, 2U);
}

TEST(UiNavigationTests, ResolvedActionMapEdgesDriveKeyboardAndGamepadNavigationWithoutUiSelectors)
{
    UiDocument document(180U, 96U);
    AddButton(document, "one", UiRect{10U, 10U, 60U, 24U});
    AddButton(document, "two", UiRect{10U, 40U, 60U, 24U});

    input::InputSystem inputSystem{};
    input::VirtualInputSource source{inputSystem};
    input::ActionMap actions{};

    const input::ButtonActionId next = actions.AddButtonAction("ui.next");
    actions.BindButton(next, input::InputControl::ArrowDown);
    actions.BindButton(next, input::InputControl::GamepadDpadDown);

    const input::ButtonActionId accept = actions.AddButtonAction("ui.accept");
    actions.BindButton(accept, input::InputControl::Enter);
    actions.BindButton(accept, input::InputControl::GamepadSouth);
    actions.Finalize();

    source.Press(input::InputControl::ArrowDown);
    actions.Resolve(inputSystem);
    ASSERT_TRUE(actions.Pressed(next));
    ASSERT_EQ(document.FocusNext(), UiActionResult::Success);
    ASSERT_NE(document.FocusedElement(), nullptr);
    EXPECT_EQ(document.FocusedElement()->id, "one");

    inputSystem.AdvanceToFrame(1U);
    source.Release(input::InputControl::ArrowDown);
    actions.Resolve(inputSystem);

    constexpr input::InputDeviceId Gamepad = 7U;
    source.ConnectGamepad(Gamepad);
    inputSystem.AdvanceToFrame(2U);
    source.PressGamepad(Gamepad, input::InputControl::GamepadDpadDown);
    actions.Resolve(inputSystem);
    ASSERT_TRUE(actions.Pressed(next));
    ASSERT_EQ(document.FocusNext(), UiActionResult::Success);
    EXPECT_EQ(document.FocusedElement()->id, "two");

    inputSystem.AdvanceToFrame(3U);
    source.ReleaseGamepad(Gamepad, input::InputControl::GamepadDpadDown);
    actions.Resolve(inputSystem);
    inputSystem.AdvanceToFrame(4U);
    source.PressGamepad(Gamepad, input::InputControl::GamepadSouth);
    actions.Resolve(inputSystem);
    ASSERT_TRUE(actions.Pressed(accept));
    ASSERT_EQ(document.ActivateFocused(), UiActionResult::Success);
    EXPECT_EQ(document.Find("two")->activationCount, 1U);
}
} // namespace trace2d::ui
