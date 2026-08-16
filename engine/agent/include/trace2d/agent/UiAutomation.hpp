#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::agent
{
enum class UiRole : std::uint8_t
{
    Panel,
    Label,
    Button,
    TextBox,
    ProgressBar,
};

[[nodiscard]] std::string_view ToString(UiRole role) noexcept;

struct UiRectSnapshot final
{
    std::uint32_t x{0};
    std::uint32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};

    [[nodiscard]] bool operator==(const UiRectSnapshot&) const noexcept = default;
};

struct UiPresentationRectSnapshot final
{
    std::int32_t x{0};
    std::int32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};

    [[nodiscard]] bool operator==(const UiPresentationRectSnapshot&) const noexcept = default;
};

struct UiTextCompositionSnapshot final
{
    std::string text{};
    std::int32_t selectionStart{-1};
    std::int32_t selectionLength{-1};

    [[nodiscard]] bool operator==(const UiTextCompositionSnapshot&) const noexcept = default;
};

struct UiTextLayoutSnapshot final
{
    std::uint64_t sourceRevision{0U};
    bool includesComposition{false};
    std::uint64_t glyphCount{0U};
    std::uint64_t lineCount{0U};
    std::int64_t contentWidth26_6{0};
    std::int64_t contentHeight26_6{0};
    std::int64_t layoutWidth26_6{0};
    std::int64_t layoutHeight26_6{0};

    [[nodiscard]] bool operator==(const UiTextLayoutSnapshot&) const noexcept = default;
};

struct UiElementSnapshot final
{
    std::string id{};
    UiRole role{UiRole::Panel};
    std::string name{};
    std::optional<std::string> parentId{};
    std::uint32_t depth{0U};
    UiRectSnapshot localBounds{};
    UiRectSnapshot bounds{};
    UiPresentationRectSnapshot presentationBounds{};
    bool scrollViewport{false};
    std::optional<std::string> scrollOwnerId{};
    std::uint32_t scrollContentWidth{0U};
    std::uint32_t scrollContentHeight{0U};
    std::uint32_t scrollOffsetX{0U};
    std::uint32_t scrollOffsetY{0U};
    std::uint64_t scrollRevision{0U};
    std::uint64_t scrollTranslationUpdates{0U};
    std::uint32_t progressValue{0U};
    std::uint32_t progressMaximum{0U};
    std::uint64_t progressRevision{0U};
    bool clipChildren{false};
    bool clipActive{false};
    UiRectSnapshot clipBounds{};
    bool visible{true};
    bool enabled{true};
    bool focused{false};
    bool hovered{false};
    bool pointerPressed{false};
    bool pointerCaptured{false};
    std::string text{};
    std::uint64_t activationCount{0};
    std::optional<UiTextCompositionSnapshot> composition{};
    std::optional<UiTextLayoutSnapshot> textLayout{};

    [[nodiscard]] bool operator==(const UiElementSnapshot&) const noexcept = default;
};

struct UiTreeSnapshot final
{
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::optional<std::string> modalScopeId{};
    std::vector<UiElementSnapshot> elements{};

    [[nodiscard]] bool operator==(const UiTreeSnapshot&) const noexcept = default;
};

struct UiSelector final
{
    std::optional<std::string> id{};
    std::optional<UiRole> role{};
    std::optional<std::string> name{};

    [[nodiscard]] bool operator==(const UiSelector&) const noexcept = default;
};

enum class UiAutomationErrorCode : std::uint8_t
{
    UiUnavailable,
    InvalidSelector,
    NoMatch,
    AmbiguousMatch,
    NotVisible,
    Disabled,
    NotFocusable,
    NotActivatable,
    NotTextInput,
    NotFocused,
    OutsideModalScope,
    StateMismatch,
    ActionRejected,
};

[[nodiscard]] std::string_view ToString(UiAutomationErrorCode code) noexcept;

struct UiAutomationError final
{
    UiAutomationErrorCode code{UiAutomationErrorCode::UiUnavailable};
    std::string message{};

    [[nodiscard]] bool operator==(const UiAutomationError&) const noexcept = default;
};

struct UiTreeResult final
{
    std::optional<UiTreeSnapshot> tree{};
    std::optional<UiAutomationError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return tree.has_value() && !error.has_value();
    }
};

struct UiQueryResult final
{
    std::optional<UiSelector> selector{};
    std::vector<UiElementSnapshot> matches{};
    std::optional<UiAutomationError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return selector.has_value() && !error.has_value();
    }
};

struct UiQueryOneResult final
{
    std::optional<UiSelector> selector{};
    std::optional<UiElementSnapshot> match{};
    std::optional<UiAutomationError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return selector.has_value() && match.has_value() && !error.has_value();
    }
};

struct UiActionResponse final
{
    std::optional<UiSelector> selector{};
    std::optional<UiElementSnapshot> element{};
    std::optional<UiAutomationError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return selector.has_value() && element.has_value() && !error.has_value();
    }
};

struct UiExpectedState final
{
    std::optional<bool> visible{};
    std::optional<bool> enabled{};
    std::optional<bool> focused{};
    std::optional<bool> hovered{};
    std::optional<bool> pointerPressed{};
    std::optional<bool> pointerCaptured{};
    std::optional<std::string> text{};
    std::optional<std::uint64_t> activationCount{};
    std::optional<std::uint32_t> progressValue{};
    std::optional<std::uint32_t> progressMaximum{};
    std::optional<std::uint64_t> progressRevision{};

    [[nodiscard]] bool operator==(const UiExpectedState&) const noexcept = default;
};

struct UiAssertionResult final
{
    std::optional<UiSelector> selector{};
    std::optional<UiElementSnapshot> observed{};
    std::optional<UiAutomationError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return selector.has_value() && observed.has_value() && !error.has_value();
    }
};
} // namespace trace2d::agent
