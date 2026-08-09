#pragma once

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
    std::string text{};
    bool enabled{true};
    std::uint64_t activationCount{0};
};

enum class UiActionResult : std::uint8_t
{
    Success,
    InvalidDocumentSize,
    InvalidId,
    DuplicateId,
    InvalidBounds,
    NotFound,
    Disabled,
    NotFocusable,
    NotActivatable,
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

    void ReserveElements(std::size_t count);
    [[nodiscard]] UiActionResult AddElement(UiElement element);
    [[nodiscard]] UiActionResult Focus(std::string_view id) noexcept;
    [[nodiscard]] UiActionResult Activate(std::string_view id) noexcept;
    void ClearFocus() noexcept;

private:
    [[nodiscard]] UiElement* FindMutable(std::string_view id) noexcept;
    [[nodiscard]] bool Contains(const UiRect& bounds) const noexcept;

    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::vector<UiElement> elements_{};
    std::size_t focusedIndex_{static_cast<std::size_t>(-1)};
};
} // namespace trace2d::ui
