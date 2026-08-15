#include <trace2d/input/TextInput.hpp>
#include <trace2d/ui/Ui.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string_view>
#include <utility>

namespace trace2d::ui
{
namespace
{
[[nodiscard]] input::PointerState Pointer(const float x, const float y) noexcept
{
    return input::PointerState{.x = x, .y = y};
}

[[nodiscard]] input::InputControlState PressedButton() noexcept
{
    return input::InputControlState{.held = true, .pressed = true};
}

[[nodiscard]] input::InputControlState ClickedInOneFrame() noexcept
{
    return input::InputControlState{.pressed = true, .released = true};
}

std::size_t AddElement(
    UiDocument& document,
    const std::string_view id,
    const UiElementKind kind,
    const UiRect bounds,
    const std::size_t parentIndex = InvalidUiElementIndex,
    const std::string_view parentId = {})
{
    const std::size_t index = document.Elements().size();
    UiElement element{};
    element.id = id;
    element.kind = kind;
    element.parentId = parentId;
    element.parentIndex = parentIndex;
    element.bounds = bounds;
    element.name = id;
    EXPECT_EQ(document.AddElement(std::move(element)), UiActionResult::Success);
    return index;
}
} // namespace

TEST(UiModalScopeTests, ActivatingScopeClearsOutsideFocusCompositionHoverAndCapture)
{
    UiDocument document(320U, 180U);
    AddElement(document, "outside_text", UiElementKind::TextInput, UiRect{8U, 8U, 120U, 24U});
    const std::size_t modalIndex =
        AddElement(document, "modal", UiElementKind::Panel, UiRect{80U, 40U, 160U, 100U});
    AddElement(
        document,
        "inside",
        UiElementKind::Button,
        UiRect{96U, 64U, 80U, 24U},
        modalIndex,
        "modal");

    const UiPointerRouteResult pressed =
        document.ApplyPointer(Pointer(20.0F, 16.0F), PressedButton());
    ASSERT_TRUE(pressed.Succeeded());
    ASSERT_NE(document.CapturedElement(), nullptr);
    ASSERT_TRUE(document.IsFocused("outside_text"));

    ASSERT_EQ(
        document.ApplyTextInput(input::TextInputEvent{
            .type = input::TextInputEventType::Composition,
            .text = "preedit",
            .selectionStart = 0,
            .selectionLength = 1,
        }),
        UiActionResult::Success);
    ASSERT_TRUE(document.TextComposition().active);
    ASSERT_NE(document.HoveredElement(), nullptr);

    ASSERT_EQ(document.SetModalScope("modal"), UiActionResult::Success);
    EXPECT_TRUE(document.HasModalScope());
    ASSERT_NE(document.ModalScopeElement(), nullptr);
    EXPECT_EQ(document.ModalScopeElement()->id, "modal");
    EXPECT_EQ(document.FocusedElement(), nullptr);
    EXPECT_FALSE(document.TextComposition().active);
    EXPECT_EQ(document.HoveredElement(), nullptr);
    EXPECT_EQ(document.CapturedElement(), nullptr);
    EXPECT_FALSE(document.Find("outside_text")->hovered);
    EXPECT_FALSE(document.Find("outside_text")->pointerPressed);
}

TEST(UiModalScopeTests, AuthoredAndDirectionalNavigationStayInsideModalSubtree)
{
    UiDocument document(320U, 180U);
    AddElement(document, "background_left", UiElementKind::Button, UiRect{8U, 72U, 40U, 24U});
    const std::size_t modalIndex =
        AddElement(document, "modal", UiElementKind::Panel, UiRect{64U, 40U, 192U, 100U});
    AddElement(
        document,
        "modal_left",
        UiElementKind::Button,
        UiRect{80U, 72U, 48U, 24U},
        modalIndex,
        "modal");
    AddElement(
        document,
        "modal_right",
        UiElementKind::Button,
        UiRect{176U, 72U, 48U, 24U},
        modalIndex,
        "modal");
    AddElement(document, "background_right", UiElementKind::Button, UiRect{232U, 72U, 40U, 24U});

    ASSERT_EQ(document.SetModalScope("modal"), UiActionResult::Success);
    ASSERT_EQ(document.FocusNext(), UiActionResult::Success);
    ASSERT_TRUE(document.IsFocused("modal_left"));
    ASSERT_EQ(document.FocusNext(), UiActionResult::Success);
    ASSERT_TRUE(document.IsFocused("modal_right"));
    ASSERT_EQ(document.FocusNext(), UiActionResult::Success);
    EXPECT_TRUE(document.IsFocused("modal_left"));
    ASSERT_EQ(document.FocusPrevious(), UiActionResult::Success);
    EXPECT_TRUE(document.IsFocused("modal_right"));

    ASSERT_EQ(document.Focus("modal_left"), UiActionResult::Success);
    ASSERT_EQ(document.FocusDirectional(UiNavigationDirection::Right), UiActionResult::Success);
    EXPECT_TRUE(document.IsFocused("modal_right"));

    EXPECT_EQ(document.Focus("background_left"), UiActionResult::OutsideModalScope);
    EXPECT_TRUE(document.IsFocused("modal_right"));
}

TEST(UiModalScopeTests, SemanticActionsOutsideScopeAreRejectedWithoutMutatingState)
{
    UiDocument document(320U, 180U);
    AddElement(document, "background", UiElementKind::Button, UiRect{8U, 8U, 64U, 24U});
    AddElement(document, "background_text", UiElementKind::TextInput, UiRect{8U, 40U, 96U, 24U});
    const std::size_t modalIndex =
        AddElement(document, "modal", UiElementKind::Panel, UiRect{96U, 40U, 160U, 100U});
    AddElement(
        document,
        "inside",
        UiElementKind::Button,
        UiRect{112U, 64U, 64U, 24U},
        modalIndex,
        "modal");

    ASSERT_EQ(document.SetModalScope("modal"), UiActionResult::Success);
    EXPECT_EQ(document.Focus("background"), UiActionResult::OutsideModalScope);
    EXPECT_EQ(document.Activate("background"), UiActionResult::OutsideModalScope);
    EXPECT_EQ(document.InputText("background_text", "blocked"), UiActionResult::OutsideModalScope);
    EXPECT_EQ(document.Find("background")->activationCount, 0U);
    EXPECT_TRUE(document.Find("background_text")->text.empty());

    ASSERT_EQ(document.Activate("inside"), UiActionResult::Success);
    EXPECT_EQ(document.Find("inside")->activationCount, 1U);
}

TEST(UiModalScopeTests, PointerOutsideModalIsConsumedAndCannotActivateBackground)
{
    UiDocument document(320U, 180U);
    AddElement(document, "background", UiElementKind::Button, UiRect{8U, 8U, 64U, 24U});
    const std::size_t modalIndex =
        AddElement(document, "modal", UiElementKind::Panel, UiRect{96U, 40U, 160U, 100U});
    AddElement(
        document,
        "inside",
        UiElementKind::Button,
        UiRect{112U, 64U, 64U, 24U},
        modalIndex,
        "modal");

    ASSERT_EQ(document.SetModalScope("modal"), UiActionResult::Success);

    const UiPointerRouteResult blocked =
        document.ApplyPointer(Pointer(20.0F, 16.0F), ClickedInOneFrame());
    EXPECT_TRUE(blocked.Succeeded());
    EXPECT_TRUE(blocked.consumed);
    EXPECT_FALSE(blocked.activated);
    EXPECT_EQ(document.HoveredElement(), nullptr);
    EXPECT_EQ(document.Find("background")->activationCount, 0U);

    const UiPointerRouteResult inside =
        document.ApplyPointer(Pointer(120.0F, 72.0F), ClickedInOneFrame());
    EXPECT_TRUE(inside.consumed);
    EXPECT_TRUE(inside.activated);
    EXPECT_EQ(document.Find("inside")->activationCount, 1U);

    document.ClearModalScope();
    EXPECT_FALSE(document.HasModalScope());
    const UiPointerRouteResult restored =
        document.ApplyPointer(Pointer(20.0F, 16.0F), ClickedInOneFrame());
    EXPECT_TRUE(restored.activated);
    EXPECT_EQ(document.Find("background")->activationCount, 1U);
}

TEST(UiModalScopeTests, NestedDescendantsParticipateAndInvalidScopeChangeKeepsCurrentModal)
{
    UiDocument document(320U, 180U);
    const std::size_t modalIndex =
        AddElement(document, "modal", UiElementKind::Panel, UiRect{64U, 32U, 192U, 120U});
    const std::size_t nestedIndex = AddElement(
        document,
        "nested",
        UiElementKind::Panel,
        UiRect{80U, 48U, 128U, 80U},
        modalIndex,
        "modal");
    AddElement(
        document,
        "deep_button",
        UiElementKind::Button,
        UiRect{96U, 64U, 64U, 24U},
        nestedIndex,
        "nested");
    AddElement(document, "hidden_modal", UiElementKind::Panel, UiRect{8U, 8U, 32U, 32U});
    UiElement* hidden = const_cast<UiElement*>(document.Find("hidden_modal"));
    ASSERT_NE(hidden, nullptr);
    hidden->visible = false;

    ASSERT_EQ(document.SetModalScope("modal"), UiActionResult::Success);
    ASSERT_EQ(document.FocusNext(), UiActionResult::Success);
    EXPECT_TRUE(document.IsFocused("deep_button"));

    EXPECT_EQ(document.SetModalScope("hidden_modal"), UiActionResult::NotVisible);
    ASSERT_NE(document.ModalScopeElement(), nullptr);
    EXPECT_EQ(document.ModalScopeElement()->id, "modal");
    EXPECT_TRUE(document.IsFocused("deep_button"));
}
} // namespace trace2d::ui
