#pragma once

#include <cstdint>
#include <string>

namespace trace2d::input
{
enum class TextInputEventType : std::uint8_t
{
    Committed,
    Composition,
};

// Variable-payload text input intentionally lives beside, not inside, InputEvent. Button/axis
// gameplay state therefore retains its fixed-size allocation-free hot-path representation while
// real UTF-8 commit/composition events may own bounded text storage at the explicit text-entry
// boundary.
struct TextInputEvent final
{
    TextInputEventType type{TextInputEventType::Committed};
    std::string text{};

    // IME preedit selection/cursor metadata. SDL3 uses -1 when the platform did not provide the
    // value; committed-text events leave both fields at -1. These are UTF-8 character indices,
    // not byte offsets, and I3 preserves them without implementing caret/editing semantics.
    std::int32_t selectionStart{-1};
    std::int32_t selectionLength{-1};

    [[nodiscard]] bool operator==(const TextInputEvent&) const noexcept = default;
};
} // namespace trace2d::input
