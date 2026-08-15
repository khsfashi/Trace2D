#include <trace2d/ui/Ui.hpp>

#include <cstddef>

namespace trace2d::ui
{
const UiElement* UiDocument::ModalScopeElement() const noexcept
{
    if (!HasModalScope())
    {
        return nullptr;
    }
    return &elements_[modalScopeIndex_];
}

bool UiDocument::HasModalScope() const noexcept
{
    return modalScopeIndex_ < elements_.size();
}

UiActionResult UiDocument::SetModalScope(const std::string_view id)
{
    std::size_t scopeIndex = InvalidUiElementIndex;
    for (std::size_t index = 0U; index < elements_.size(); ++index)
    {
        if (elements_[index].id == id)
        {
            scopeIndex = index;
            break;
        }
    }

    if (scopeIndex == InvalidUiElementIndex)
    {
        return UiActionResult::NotFound;
    }

    const UiElement& scope = elements_[scopeIndex];
    if (!scope.visible)
    {
        return UiActionResult::NotVisible;
    }
    if (!scope.enabled)
    {
        return UiActionResult::Disabled;
    }

    // Scope changes are explicit state-management work. The byte membership array is retained and
    // reused; steady pointer/navigation/activation only reads it by direct element index.
    modalScopeMembership_.assign(elements_.size(), 0U);
    for (std::size_t index = 0U; index < elements_.size(); ++index)
    {
        if (IsDescendantOrSelf(index, scopeIndex))
        {
            modalScopeMembership_[index] = 1U;
        }
    }
    modalScopeIndex_ = scopeIndex;

    if (focusedIndex_ < elements_.size() && !IsInteractionAllowed(focusedIndex_))
    {
        ClearFocus();
    }

    if (hoveredIndex_ < elements_.size() && !IsInteractionAllowed(hoveredIndex_))
    {
        elements_[hoveredIndex_].hovered = false;
        hoveredIndex_ = InvalidUiElementIndex;
    }

    if (pointerCaptureIndex_ < elements_.size() && !IsInteractionAllowed(pointerCaptureIndex_))
    {
        ClearPointerCapture();
    }

    return UiActionResult::Success;
}

void UiDocument::ClearModalScope() noexcept
{
    modalScopeIndex_ = InvalidUiElementIndex;
}

bool UiDocument::IsInteractionAllowed(const std::size_t index) const noexcept
{
    if (!HasModalScope())
    {
        return index < elements_.size();
    }
    return index < modalScopeMembership_.size() && modalScopeMembership_[index] != 0U;
}

bool UiDocument::IsDescendantOrSelf(
    const std::size_t index,
    const std::size_t ancestorIndex) const noexcept
{
    if (index >= elements_.size() || ancestorIndex >= elements_.size())
    {
        return false;
    }

    std::size_t current = index;
    for (std::size_t hops = 0U; hops < elements_.size(); ++hops)
    {
        if (current == ancestorIndex)
        {
            return true;
        }

        const std::size_t parent = elements_[current].parentIndex;
        if (parent == InvalidUiElementIndex || parent >= elements_.size())
        {
            return false;
        }
        current = parent;
    }

    // U0/U2 reject hierarchy cycles. The bounded guard keeps direct test/setup misuse from turning a
    // modal transition into an unbounded walk without adding hierarchy work to steady interaction.
    return false;
}
} // namespace trace2d::ui
