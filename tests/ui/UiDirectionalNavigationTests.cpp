#include <trace2d/input/TextInput.hpp>
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

void ExpectFocus(UiDocument& document, const char* id)
{
    ASSERT_NE(document.FocusedElement(), nullptr);
    EXPECT_EQ(document.FocusedElement()->id, id);
}
} // namespace

TEST(UiDirectionalNavigationTests, MovesInEachCardinalDirectionFromResolvedLogicalCenters)
{
    UiDocument document(300U, 200U);
    AddButton(document, "center", UiRect{140U, 90U, 20U, 20U});
    AddButton(document, "left", UiRect{80U, 90U, 20U, 20U});
    AddButton(document, "right", UiRect{200U, 90U, 20U, 20U});
    AddButton(document, "up", UiRect{140U, 30U, 20U, 20U});
    AddButton(document, "down", UiRect{140U, 150U, 20U, 20U});

    ASSERT_EQ(document.Focus("center"), UiActionResult::Success);
    ASSERT_EQ(document.FocusDirectional(UiNavigationDirection::Left), UiActionResult::Success);
    ExpectFocus(document, "left");

    ASSERT_EQ(document.Focus("center"), UiActionResult::Success);
    ASSERT_EQ(document.FocusDirectional(UiNavigationDirection::Right), UiActionResult::Success);
    ExpectFocus(document, "right");

    ASSERT_EQ(document.Focus("center"), UiActionResult::Success);
    ASSERT_EQ(document.FocusDirectional(UiNavigationDirection::Up), UiActionResult::Success);
    ExpectFocus(document, "up");

    ASSERT_EQ(document.Focus("center"), UiActionResult::Success);
    ASSERT_EQ(document.FocusDirectional(UiNavigationDirection::Down), UiActionResult::Success);
    ExpectFocus(document, "down");
}

TEST(UiDirectionalNavigationTests, PrefersDirectionalConeBeforeSlightlyCloserOffConeCandidate)
{
    UiDocument document(220U, 180U);
    AddButton(document, "origin", UiRect{100U, 100U, 20U, 20U});

    // Doubled center deltas from origin are (+10, +12): closer, but just outside the 45-degree cone.
    AddButton(document, "off_cone", UiRect{105U, 106U, 20U, 20U});
    // Doubled center delta is (+16, 0): slightly farther, but directionally aligned.
    AddButton(document, "aligned", UiRect{108U, 100U, 20U, 20U});

    ASSERT_EQ(document.Focus("origin"), UiActionResult::Success);
    ASSERT_EQ(document.FocusDirectional(UiNavigationDirection::Right), UiActionResult::Success);
    ExpectFocus(document, "aligned");
}

TEST(UiDirectionalNavigationTests, FallsBackDeterministicallyOutsideConeWhenNecessary)
{
    UiDocument document(220U, 180U);
    AddButton(document, "origin", UiRect{100U, 100U, 20U, 20U});
    AddButton(document, "near", UiRect{105U, 106U, 20U, 20U});
    AddButton(document, "far", UiRect{106U, 110U, 20U, 20U});

    ASSERT_EQ(document.Focus("origin"), UiActionResult::Success);
    ASSERT_EQ(document.FocusDirectional(UiNavigationDirection::Right), UiActionResult::Success);
    ExpectFocus(document, "near");
}

TEST(UiDirectionalNavigationTests, SkipsIneligibleCandidatesAndUsesAuthoredOrderAsFinalTieBreak)
{
    UiDocument document(260U, 180U);
    AddButton(document, "origin", UiRect{60U, 80U, 20U, 20U});
    AddButton(document, "hidden", UiRect{75U, 80U, 20U, 20U}, false, true);
    AddButton(document, "disabled", UiRect{80U, 80U, 20U, 20U}, true, false);
    AddElement(document, "panel", UiElementKind::Panel, UiRect{85U, 80U, 20U, 20U});
    AddButton(document, "first_tie", UiRect{100U, 80U, 20U, 20U});
    AddButton(document, "second_tie", UiRect{100U, 80U, 20U, 20U});

    ASSERT_EQ(document.Focus("origin"), UiActionResult::Success);
    ASSERT_EQ(document.FocusDirectional(UiNavigationDirection::Right), UiActionResult::Success);
    ExpectFocus(document, "first_tie");
}

TEST(UiDirectionalNavigationTests, RequiresExistingFocusAndPreservesFocusWhenNoCandidateExists)
{
    UiDocument document(180U, 100U);
    AddButton(document, "center", UiRect{100U, 40U, 20U, 20U});
    AddButton(document, "left", UiRect{40U, 40U, 20U, 20U});

    EXPECT_EQ(document.FocusDirectional(UiNavigationDirection::Right), UiActionResult::NotFocused);
    EXPECT_EQ(document.FocusedElement(), nullptr);

    ASSERT_EQ(document.Focus("center"), UiActionResult::Success);
    EXPECT_EQ(document.FocusDirectional(UiNavigationDirection::Right), UiActionResult::NotFocusable);
    ExpectFocus(document, "center");
}

TEST(UiDirectionalNavigationTests, DirectionalMoveUsesExistingFocusAuthorityAndClearsImeComposition)
{
    UiDocument document(220U, 120U);
    AddElement(document, "chat", UiElementKind::TextInput, UiRect{20U, 40U, 100U, 24U});
    AddButton(document, "send", UiRect{150U, 40U, 50U, 24U});

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

    ASSERT_EQ(document.FocusDirectional(UiNavigationDirection::Right), UiActionResult::Success);
    ExpectFocus(document, "send");
    EXPECT_FALSE(document.TextComposition().active);
}
} // namespace trace2d::ui
