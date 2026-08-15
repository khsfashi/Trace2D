#pragma once

#include <trace2d/input/TextInput.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::ui
{
inline constexpr std::uint32_t MaxUiCanvasDimension = 4096U;

struct UiRect final
{
    std::uint32_t x{0};
    std::uint32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};

    [[nodiscard]] bool operator==(const UiRect&) const noexcept = default;
};

enum class UiElementKind : std::uint8_t
{
    Panel,
    Label,
    Button,
    TextInput,
};

struct UiElement final
{
    std::string id{};
    UiElementKind kind{UiElementKind::Panel};
    UiRect bounds{};
    std::string name{};
    std::string text{};
    bool visible{true};
    bool enabled{true};
    std::uint64_t activationCount{0};
};

struct UiTextCompositionState final
{
    std::string_view text{};
    std::int32_t selectionStart{-1};
    std::int32_t selectionLength{-1};
    bool active{false};
};

enum class UiActionResult : std::uint8_t
{
    Success,
    InvalidDocumentSize,
    InvalidId,
    DuplicateId,
    InvalidBounds,
    NotFound,
    NotVisible,
    Disabled,
    NotFocusable,
    NotActivatable,
    NotTextInput,
    NotFocused,
    InvalidTextCompositionRange,
};

[[nodiscard]] std::string_view ToString(UiElementKind kind) noexcept;
[[nodiscard]] std::string_view ToString(UiActionResult result) noexcept;
[[nodiscard]] bool IsFocusable(UiElementKind kind) noexcept;
[[nodiscard]] bool IsActivatable(UiElementKind kind) noexcept;

class UiDocument final
{
public:
    UiDocument() = default;
    UiDocument(std::uint32_t width, std::uint32_t height) noexcept;

    [[nodiscard]] std::uint32_t Width() const noexcept;
    [[nodiscard]] std::uint32_t Height() const noexcept;
    [[nodiscard]] bool HasValidSize() const noexcept;
    [[nodiscard]] std::span<const UiElement> Elements() const noexcept;
    [[nodiscard]] const UiElement* Find(std::string_view id) const noexcept;
    [[nodiscard]] const UiElement* FocusedElement() const noexcept;
    [[nodiscard]] bool IsFocused(std::string_view id) const noexcept;
    [[nodiscard]] UiTextCompositionState TextComposition() const noexcept;

    void ReserveElements(std::size_t count);
    [[nodiscard]] UiActionResult AddElement(UiElement element);
    [[nodiscard]] UiActionResult Focus(std::string_view id) noexcept;
    [[nodiscard]] UiActionResult Activate(std::string_view id) noexcept;

    // Existing semantic/Agent API: replace the complete committed value of one focused textbox.
    [[nodiscard]] UiActionResult InputText(std::string_view id, std::string_view text);

    // Physical host, direct host, and headless/virtual text input all converge here. Committed UTF-8
    // appends to the focused textbox for the I3 baseline; IME composition remains transient and is
    // never copied into the committed textbox value until a Committed event arrives.
    [[nodiscard]] UiActionResult ApplyTextInput(const input::TextInputEvent& event);

    void ClearFocus() noexcept;

private:
    [[nodiscard]] UiElement* FindMutable(std::string_view id) noexcept;
    [[nodiscard]] UiElement* FocusedTextInput() noexcept;
    [[nodiscard]] bool Contains(const UiRect& bounds) const noexcept;
    void ClearTextComposition() noexcept;

    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::vector<UiElement> elements_{};
    std::size_t focusedIndex_{static_cast<std::size_t>(-1)};

    // At most one UI element can own focus, so I3 retains one reusable preedit buffer per document
    // instead of adding a std::string to every Panel/Label/Button/TextInput instance.
    std::string textComposition_{};
    std::int32_t textCompositionSelectionStart_{-1};
    std::int32_t textCompositionSelectionLength_{-1};
};
} // namespace trace2d::ui
