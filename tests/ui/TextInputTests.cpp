#include <trace2d/input/TextInput.hpp>
#include <trace2d/ui/Ui.hpp>

#include <gtest/gtest.h>

#include <string>

namespace
{
using trace2d::input::TextInputEvent;
using trace2d::input::TextInputEventType;
using trace2d::ui::UiActionResult;
using trace2d::ui::UiDocument;
using trace2d::ui::UiElement;
using trace2d::ui::UiElementKind;
using trace2d::ui::UiRect;

UiDocument MakeTextDocument()
{
    UiDocument document{320U, 180U};

    UiElement text{};
    text.id = "player_name";
    text.kind = UiElementKind::TextInput;
    text.bounds = UiRect{8U, 8U, 160U, 24U};
    text.text = "Player";
    EXPECT_EQ(document.AddElement(std::move(text)), UiActionResult::Success);

    UiElement button{};
    button.id = "confirm";
    button.kind = UiElementKind::Button;
    button.bounds = UiRect{8U, 40U, 80U, 24U};
    EXPECT_EQ(document.AddElement(std::move(button)), UiActionResult::Success);

    return document;
}

TEST(TextInputTests, CompositionRemainsTransientUntilUtf8Commit)
{
    UiDocument document = MakeTextDocument();
    ASSERT_EQ(document.Focus("player_name"), UiActionResult::Success);

    const std::string koreanPreedit{"\xED\x95\x9C"}; // 한
    const std::string committedKorean{"\xED\x95\x9C\xEA\xB8\x80"}; // 한글
    const std::string cjk{"\xE6\xBC\xA2"}; // 漢

    EXPECT_EQ(
        document.ApplyTextInput(TextInputEvent{
            .type = TextInputEventType::Composition,
            .text = koreanPreedit,
            .selectionStart = 0,
            .selectionLength = 1,
        }),
        UiActionResult::Success);

    const auto* element = document.Find("player_name");
    ASSERT_NE(element, nullptr);
    EXPECT_EQ(element->text, "Player");

    const auto composition = document.TextComposition();
    EXPECT_TRUE(composition.active);
    EXPECT_EQ(composition.text, koreanPreedit);
    EXPECT_EQ(composition.selectionStart, 0);
    EXPECT_EQ(composition.selectionLength, 1);

    EXPECT_EQ(
        document.ApplyTextInput(TextInputEvent{
            .type = TextInputEventType::Committed,
            .text = committedKorean,
        }),
        UiActionResult::Success);
    EXPECT_EQ(document.Find("player_name")->text, std::string{"Player"} + committedKorean);
    EXPECT_FALSE(document.TextComposition().active);

    EXPECT_EQ(
        document.ApplyTextInput(TextInputEvent{
            .type = TextInputEventType::Committed,
            .text = cjk,
        }),
        UiActionResult::Success);
    EXPECT_EQ(document.Find("player_name")->text, std::string{"Player"} + committedKorean + cjk);
}

TEST(TextInputTests, FocusChangesAndSemanticReplacementClearCompositionDeterministically)
{
    UiDocument document = MakeTextDocument();
    ASSERT_EQ(document.Focus("player_name"), UiActionResult::Success);

    const TextInputEvent composition{
        .type = TextInputEventType::Composition,
        .text = "preedit",
        .selectionStart = 2,
        .selectionLength = 3,
    };
    ASSERT_EQ(document.ApplyTextInput(composition), UiActionResult::Success);
    ASSERT_TRUE(document.TextComposition().active);

    // Refocusing the same field does not spuriously reset an active IME composition.
    EXPECT_EQ(document.Focus("player_name"), UiActionResult::Success);
    EXPECT_TRUE(document.TextComposition().active);

    EXPECT_EQ(document.InputText("player_name", "Agent replacement"), UiActionResult::Success);
    EXPECT_EQ(document.Find("player_name")->text, "Agent replacement");
    EXPECT_FALSE(document.TextComposition().active);

    ASSERT_EQ(document.ApplyTextInput(composition), UiActionResult::Success);
    ASSERT_TRUE(document.TextComposition().active);
    EXPECT_EQ(document.Focus("confirm"), UiActionResult::Success);
    EXPECT_FALSE(document.TextComposition().active);

    EXPECT_EQ(
        document.ApplyTextInput(TextInputEvent{
            .type = TextInputEventType::Committed,
            .text = "ignored",
        }),
        UiActionResult::NotTextInput);

    document.ClearFocus();
    EXPECT_EQ(
        document.ApplyTextInput(TextInputEvent{
            .type = TextInputEventType::Committed,
            .text = "ignored",
        }),
        UiActionResult::NotFocused);
}

TEST(TextInputTests, InvalidCompositionMetadataIsRejectedWithoutMutatingState)
{
    UiDocument document = MakeTextDocument();
    ASSERT_EQ(document.Focus("player_name"), UiActionResult::Success);

    ASSERT_EQ(
        document.ApplyTextInput(TextInputEvent{
            .type = TextInputEventType::Composition,
            .text = "valid",
            .selectionStart = -1,
            .selectionLength = -1,
        }),
        UiActionResult::Success);

    EXPECT_EQ(
        document.ApplyTextInput(TextInputEvent{
            .type = TextInputEventType::Composition,
            .text = "invalid",
            .selectionStart = -2,
            .selectionLength = 0,
        }),
        UiActionResult::InvalidTextCompositionRange);
    EXPECT_EQ(document.TextComposition().text, "valid");
    EXPECT_EQ(trace2d::ui::ToString(UiActionResult::InvalidTextCompositionRange), "invalid_text_composition_range");
}
} // namespace
