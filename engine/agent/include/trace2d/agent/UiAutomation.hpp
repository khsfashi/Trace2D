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

struct UiElementSnapshot final
{
    std::string id{};
    UiRole role{UiRole::Panel};
    std::string name{};
    UiRectSnapshot bounds{};
    bool visible{true};
    bool enabled{true};
    bool focused{false};
    std::string text{};
    std::uint64_t activationCount{0};

    [[nodiscard]] bool operator==(const UiElementSnapshot&) const noexcept = default;
};

struct UiTreeSnapshot final
{
    std::uint32_t width{0};
    std::uint32_t height{0};
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
    std::optional<std::string> text{};
    std::optional<std::uint64_t> activationCount{};

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
