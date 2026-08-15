#include <trace2d/ui/Ui.hpp>

namespace trace2d::ui
{
UiActionResult UiDocument::FocusNext() noexcept
{
    return MoveFocus(true);
}

UiActionResult UiDocument::FocusPrevious() noexcept
{
    return MoveFocus(false);
}

UiActionResult UiDocument::ActivateFocused() noexcept
{
    if (focusedIndex_ >= elements_.size())
    {
        return UiActionResult::NotFocused;
    }

    return ActivateIndex(focusedIndex_);
}

UiActionResult UiDocument::MoveFocus(const bool forward) noexcept
{
    const std::size_t count = elements_.size();
    if (count == 0U)
    {
        return UiActionResult::NotFocusable;
    }

    if (focusedIndex_ < count)
    {
        const UiElement& focused = elements_[focusedIndex_];
        if (!focused.visible || !focused.enabled || !IsFocusable(focused.kind))
        {
            ClearFocus();
        }
    }
    else
    {
        focusedIndex_ = InvalidUiElementIndex;
    }

    std::size_t index = 0U;
    if (focusedIndex_ < count)
    {
        if (forward)
        {
            index = focusedIndex_ + 1U;
            if (index == count)
            {
                index = 0U;
            }
        }
        else
        {
            index = focusedIndex_ == 0U ? count - 1U : focusedIndex_ - 1U;
        }
    }
    else
    {
        index = forward ? 0U : count - 1U;
    }

    for (std::size_t scanned = 0U; scanned < count; ++scanned)
    {
        const UiElement& candidate = elements_[index];
        if (candidate.visible && candidate.enabled && IsFocusable(candidate.kind))
        {
            return FocusIndex(index);
        }

        if (forward)
        {
            ++index;
            if (index == count)
            {
                index = 0U;
            }
        }
        else
        {
            index = index == 0U ? count - 1U : index - 1U;
        }
    }

    return UiActionResult::NotFocusable;
}
} // namespace trace2d::ui
