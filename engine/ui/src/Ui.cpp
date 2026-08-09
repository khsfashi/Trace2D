#include <trace2d/ui/Ui.hpp>

#include <utility>

namespace trace2d::ui
{
std::string_view ToString(const UiElementKind kind) noexcept
{
    switch (kind)
    {
    case UiElementKind::Panel:
        return "panel";
    case UiElementKind::Label:
        return "label";
    case UiElementKind::Button:
        return "button";
    case UiElementKind::TextInput:
        return "text_input";
    }

    return "unknown";
}

std::string_view ToString(const UiActionResult result) noexcept
{
    switch (result)
    {
    case UiActionResult::Success:
        return "success";
    case UiActionResult::InvalidDocumentSize:
        return "invalid_document_size";
    case UiActionResult::InvalidId:
        return "invalid_id";
    case UiActionResult::DuplicateId:
        return "duplicate_id";
    case UiActionResult::InvalidBounds:
        return "invalid_bounds";
    case UiActionResult::NotFound:
        return "not_found";
    case UiActionResult::Disabled:
        return "disabled";
    case UiActionResult::NotFocusable:
        return "not_focusable";
    case UiActionResult::NotActivatable:
        return "not_activatable";
    }

    return "unknown";
}

bool IsFocusable(const UiElementKind kind) noexcept
{
    return kind == UiElementKind::Button || kind == UiElementKind::TextInput;
}

bool IsActivatable(const UiElementKind kind) noexcept
{
    return kind == UiElementKind::Button;
}

UiDocument::UiDocument(const std::uint32_t width, const std::uint32_t height) noexcept
    : width_{width},
      height_{height}
{
}

std::uint32_t UiDocument::Width() const noexcept
{
    return width_;
}

std::uint32_t UiDocument::Height() const noexcept
{
    return height_;
}

bool UiDocument::HasValidSize() const noexcept
{
    return width_ > 0U && height_ > 0U &&
           width_ <= MaxUiCanvasDimension && height_ <= MaxUiCanvasDimension;
}

std::span<const UiElement> UiDocument::Elements() const noexcept
{
    return elements_;
}

const UiElement* UiDocument::Find(const std::string_view id) const noexcept
{
    for (const UiElement& element : elements_)
    {
        if (element.id == id)
        {
            return &element;
        }
    }

    return nullptr;
}

UiElement* UiDocument::FindMutable(const std::string_view id) noexcept
{
    for (UiElement& element : elements_)
    {
        if (element.id == id)
        {
            return &element;
        }
    }

    return nullptr;
}

const UiElement* UiDocument::FocusedElement() const noexcept
{
    if (focusedIndex_ >= elements_.size())
    {
        return nullptr;
    }

    return &elements_[focusedIndex_];
}

bool UiDocument::IsFocused(const std::string_view id) const noexcept
{
    const UiElement* focused = FocusedElement();
    return focused != nullptr && focused->id == id;
}

void UiDocument::ReserveElements(const std::size_t count)
{
    elements_.reserve(count);
}

UiActionResult UiDocument::AddElement(UiElement element)
{
    if (!HasValidSize())
    {
        return UiActionResult::InvalidDocumentSize;
    }

    if (element.id.empty())
    {
        return UiActionResult::InvalidId;
    }

    if (Find(element.id) != nullptr)
    {
        return UiActionResult::DuplicateId;
    }

    if (!Contains(element.bounds))
    {
        return UiActionResult::InvalidBounds;
    }

    elements_.push_back(std::move(element));
    return UiActionResult::Success;
}

UiActionResult UiDocument::Focus(const std::string_view id) noexcept
{
    for (std::size_t index = 0U; index < elements_.size(); ++index)
    {
        const UiElement& element = elements_[index];
        if (element.id != id)
        {
            continue;
        }

        if (!element.enabled)
        {
            return UiActionResult::Disabled;
        }

        if (!IsFocusable(element.kind))
        {
            return UiActionResult::NotFocusable;
        }

        focusedIndex_ = index;
        return UiActionResult::Success;
    }

    return UiActionResult::NotFound;
}

UiActionResult UiDocument::Activate(const std::string_view id) noexcept
{
    UiElement* element = FindMutable(id);
    if (element == nullptr)
    {
        return UiActionResult::NotFound;
    }

    if (!element->enabled)
    {
        return UiActionResult::Disabled;
    }

    if (!IsActivatable(element->kind))
    {
        return UiActionResult::NotActivatable;
    }

    ++element->activationCount;
    return UiActionResult::Success;
}

void UiDocument::ClearFocus() noexcept
{
    focusedIndex_ = static_cast<std::size_t>(-1);
}

bool UiDocument::Contains(const UiRect& bounds) const noexcept
{
    if (bounds.width == 0U || bounds.height == 0U)
    {
        return false;
    }

    if (bounds.x >= width_ || bounds.y >= height_)
    {
        return false;
    }

    return bounds.width <= width_ - bounds.x && bounds.height <= height_ - bounds.y;
}
} // namespace trace2d::ui
