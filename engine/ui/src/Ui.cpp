#include <trace2d/ui/Ui.hpp>

#include <cmath>
#include <limits>
#include <utility>

namespace trace2d::ui
{
namespace
{
[[nodiscard]] bool EqualsCommittedPlusComposition(
    const std::string_view candidate,
    const std::string_view committed,
    const std::string_view composition) noexcept
{
    if (committed.size() > std::numeric_limits<std::size_t>::max() - composition.size() ||
        candidate.size() != committed.size() + composition.size())
    {
        return false;
    }
    return candidate.substr(0U, committed.size()) == committed &&
           candidate.substr(committed.size()) == composition;
}
} // namespace

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
    case UiActionResult::NotVisible:
        return "not_visible";
    case UiActionResult::Disabled:
        return "disabled";
    case UiActionResult::NotFocusable:
        return "not_focusable";
    case UiActionResult::NotActivatable:
        return "not_activatable";
    case UiActionResult::NotTextInput:
        return "not_text_input";
    case UiActionResult::NotFocused:
        return "not_focused";
    case UiActionResult::OutsideModalScope:
        return "outside_modal_scope";
    case UiActionResult::InvalidTextCompositionRange:
        return "invalid_text_composition_range";
    }

    return "unknown";
}

std::string_view ToString(const UiPointerRouteStatus status) noexcept
{
    switch (status)
    {
    case UiPointerRouteStatus::Success:
        return "success";
    case UiPointerRouteStatus::InvalidPosition:
        return "invalid_position";
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

bool IsPointerInteractive(const UiElementKind kind) noexcept
{
    return kind == UiElementKind::Button || kind == UiElementKind::TextInput;
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

const UiElement* UiDocument::HoveredElement() const noexcept
{
    if (hoveredIndex_ >= elements_.size())
    {
        return nullptr;
    }

    return &elements_[hoveredIndex_];
}

const UiElement* UiDocument::CapturedElement() const noexcept
{
    if (pointerCaptureIndex_ >= elements_.size())
    {
        return nullptr;
    }

    return &elements_[pointerCaptureIndex_];
}

UiElement* UiDocument::FocusedTextInput() noexcept
{
    if (focusedIndex_ >= elements_.size())
    {
        return nullptr;
    }

    UiElement& element = elements_[focusedIndex_];
    return element.kind == UiElementKind::TextInput ? &element : nullptr;
}

bool UiDocument::IsFocused(const std::string_view id) const noexcept
{
    const UiElement* focused = FocusedElement();
    return focused != nullptr && focused->id == id;
}

UiTextCompositionState UiDocument::TextComposition() const noexcept
{
    return UiTextCompositionState{
        .text = textComposition_,
        .selectionStart = textCompositionSelectionStart_,
        .selectionLength = textCompositionSelectionLength_,
        .active = !textComposition_.empty(),
    };
}

void UiDocument::ReserveElements(const std::size_t count)
{
    elements_.reserve(count);
    modalScopeMembership_.reserve(count);
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

    // Direct callers from the original flat UI contract do not need to populate U2 hierarchy
    // metadata. For a root, the resolved parent-local rectangle is identical to absolute bounds.
    if (element.localBounds.width == 0U || element.localBounds.height == 0U)
    {
        element.localBounds = element.bounds;
    }

    // Runtime interaction state is never accepted as authored input.
    element.hovered = false;
    element.pointerPressed = false;

    element.textSourceIdentity = nextTextSourceIdentity_;
    ++nextTextSourceIdentity_;
    if (nextTextSourceIdentity_ == 0U)
    {
        nextTextSourceIdentity_ = 1U;
    }
    element.displayTextRevision = 1U;
    element.textLayout = {};

    elements_.push_back(std::move(element));
    return UiActionResult::Success;
}

UiActionResult UiDocument::Focus(const std::string_view id) noexcept
{
    for (std::size_t index = 0U; index < elements_.size(); ++index)
    {
        if (elements_[index].id == id)
        {
            return FocusIndex(index);
        }
    }

    return UiActionResult::NotFound;
}

UiActionResult UiDocument::FocusIndex(const std::size_t index) noexcept
{
    if (index >= elements_.size())
    {
        return UiActionResult::NotFound;
    }

    if (!IsInteractionAllowed(index))
    {
        return UiActionResult::OutsideModalScope;
    }

    const UiElement& element = elements_[index];
    if (!element.visible)
    {
        return UiActionResult::NotVisible;
    }

    if (!element.enabled)
    {
        return UiActionResult::Disabled;
    }

    if (!IsFocusable(element.kind))
    {
        return UiActionResult::NotFocusable;
    }

    if (focusedIndex_ != index)
    {
        ClearTextComposition();
        focusedIndex_ = index;
    }
    return UiActionResult::Success;
}

UiActionResult UiDocument::Activate(const std::string_view id) noexcept
{
    for (std::size_t index = 0U; index < elements_.size(); ++index)
    {
        if (elements_[index].id == id)
        {
            return ActivateIndex(index);
        }
    }

    return UiActionResult::NotFound;
}

UiActionResult UiDocument::ActivateIndex(const std::size_t index) noexcept
{
    if (index >= elements_.size())
    {
        return UiActionResult::NotFound;
    }

    if (!IsInteractionAllowed(index))
    {
        return UiActionResult::OutsideModalScope;
    }

    UiElement& element = elements_[index];
    if (!element.visible)
    {
        return UiActionResult::NotVisible;
    }

    if (!element.enabled)
    {
        return UiActionResult::Disabled;
    }

    if (!IsActivatable(element.kind))
    {
        return UiActionResult::NotActivatable;
    }

    ++element.activationCount;
    return UiActionResult::Success;
}

UiPointerRouteResult UiDocument::ApplyPointer(
    const input::PointerState& pointer,
    const input::InputControlState primaryButton) noexcept
{
    UiPointerRouteResult result{};
    result.consumed = HasModalScope();
    if (!std::isfinite(pointer.x) || !std::isfinite(pointer.y))
    {
        result.status = UiPointerRouteStatus::InvalidPosition;
        result.hoveredIndex = hoveredIndex_;
        result.capturedIndex = pointerCaptureIndex_;
        return result;
    }

    // Capture is direct-index runtime state. If game code invalidates the target between pointer
    // samples, cancel deterministically before any release/activation processing.
    if (pointerCaptureIndex_ < elements_.size())
    {
        const UiElement& captured = elements_[pointerCaptureIndex_];
        if (!IsInteractionAllowed(pointerCaptureIndex_) || !captured.visible || !captured.enabled ||
            !IsPointerInteractive(captured.kind))
        {
            ClearPointerCapture();
        }
    }
    else
    {
        pointerCaptureIndex_ = InvalidUiElementIndex;
    }

    const std::size_t hitIndex = HitTestTopmost(pointer.x, pointer.y);
    if (hoveredIndex_ != hitIndex)
    {
        if (hoveredIndex_ < elements_.size())
        {
            elements_[hoveredIndex_].hovered = false;
        }
        hoveredIndex_ = hitIndex;
        if (hoveredIndex_ < elements_.size())
        {
            elements_[hoveredIndex_].hovered = true;
        }
    }
    result.hoveredIndex = hoveredIndex_;

    // The aggregate InputControlState can report pressed+released in the same fixed frame. Process
    // press before release so a quick click still has one deterministic capture/activation path.
    if (primaryButton.pressed && pointerCaptureIndex_ == InvalidUiElementIndex &&
        hitIndex < elements_.size())
    {
        result.consumed = true;
        UiElement& target = elements_[hitIndex];
        if (target.enabled && FocusIndex(hitIndex) == UiActionResult::Success)
        {
            pointerCaptureIndex_ = hitIndex;
            target.pointerPressed = true;
        }
    }

    if (pointerCaptureIndex_ < elements_.size())
    {
        result.consumed = true;
        const std::size_t capturedIndex = pointerCaptureIndex_;
        UiElement& captured = elements_[capturedIndex];

        if (primaryButton.released)
        {
            const bool releaseInside = ContainsPoint(captured.bounds, pointer.x, pointer.y);
            if (releaseInside && IsActivatable(captured.kind) &&
                ActivateIndex(capturedIndex) == UiActionResult::Success)
            {
                result.activated = true;
            }
            ClearPointerCapture();
        }
        else if (primaryButton.held || primaryButton.pressed)
        {
            captured.pointerPressed = true;
        }
        else
        {
            // A caller may skip the exact release-transition sample. Never leave stale capture held
            // after the canonical primary button is observed up.
            ClearPointerCapture();
        }
    }

    result.capturedIndex = pointerCaptureIndex_;
    return result;
}

UiActionResult UiDocument::InputText(const std::string_view id, const std::string_view text)
{
    UiElement* element = FindMutable(id);
    if (element == nullptr)
    {
        return UiActionResult::NotFound;
    }

    const std::size_t index = static_cast<std::size_t>(element - elements_.data());
    if (!IsInteractionAllowed(index))
    {
        return UiActionResult::OutsideModalScope;
    }

    if (!element->visible)
    {
        return UiActionResult::NotVisible;
    }

    if (!element->enabled)
    {
        return UiActionResult::Disabled;
    }

    if (element->kind != UiElementKind::TextInput)
    {
        return UiActionResult::NotTextInput;
    }

    if (!IsFocused(id))
    {
        return UiActionResult::NotFocused;
    }

    const bool hadComposition = !textComposition_.empty();
    const bool displayChanged = hadComposition
        ? !EqualsCommittedPlusComposition(text, element->text, textComposition_)
        : element->text != text;

    if (element->text != text)
    {
        element->text.assign(text);
    }
    if (displayChanged)
    {
        TouchDisplayText(*element);
    }
    else if (hadComposition)
    {
        element->textLayout = {};
    }
    ClearTextComposition(false);
    return UiActionResult::Success;
}

UiActionResult UiDocument::ApplyTextInput(const input::TextInputEvent& event)
{
    const UiElement* const focused = FocusedElement();
    if (focused == nullptr)
    {
        return UiActionResult::NotFocused;
    }
    if (!focused->visible)
    {
        return UiActionResult::NotVisible;
    }
    if (!focused->enabled)
    {
        return UiActionResult::Disabled;
    }

    UiElement* const textInput = FocusedTextInput();
    if (textInput == nullptr)
    {
        return UiActionResult::NotTextInput;
    }

    switch (event.type)
    {
    case input::TextInputEventType::Committed:
        {
            const bool hadComposition = !textComposition_.empty();
            const bool displayChanged = hadComposition
                ? event.text != textComposition_
                : !event.text.empty();
            if (!event.text.empty())
            {
                textInput->text.append(event.text);
            }
            if (displayChanged)
            {
                TouchDisplayText(*textInput);
            }
            else if (hadComposition)
            {
                textInput->textLayout = {};
            }
            ClearTextComposition(false);
        }
        return UiActionResult::Success;

    case input::TextInputEventType::Composition:
        if (event.selectionStart < -1 || event.selectionLength < -1)
        {
            return UiActionResult::InvalidTextCompositionRange;
        }

        if (event.text.empty())
        {
            ClearTextComposition();
            return UiActionResult::Success;
        }

        {
            const bool textChanged = textComposition_ != event.text;
            textComposition_.assign(event.text);
            textCompositionSelectionStart_ = event.selectionStart;
            textCompositionSelectionLength_ = event.selectionLength;
            if (textChanged)
            {
                TouchDisplayText(*textInput);
            }
        }
        return UiActionResult::Success;
    }

    return UiActionResult::NotTextInput;
}

void UiDocument::ClearFocus() noexcept
{
    ClearTextComposition();
    focusedIndex_ = InvalidUiElementIndex;
}

void UiDocument::ClearPointerState() noexcept
{
    if (hoveredIndex_ < elements_.size())
    {
        elements_[hoveredIndex_].hovered = false;
    }
    hoveredIndex_ = InvalidUiElementIndex;
    ClearPointerCapture();
}

void UiDocument::ClearPointerCapture() noexcept
{
    if (pointerCaptureIndex_ < elements_.size())
    {
        elements_[pointerCaptureIndex_].pointerPressed = false;
    }
    pointerCaptureIndex_ = InvalidUiElementIndex;
}

void UiDocument::TouchDisplayText(UiElement& element) noexcept
{
    if (element.displayTextRevision == std::numeric_limits<std::uint64_t>::max())
    {
        element.displayTextRevision = 1U;
    }
    else
    {
        ++element.displayTextRevision;
    }
    element.textLayout = {};
}

void UiDocument::PublishTextLayoutEvidence(
    const std::string_view id,
    const UiTextLayoutEvidence& evidence) noexcept
{
    UiElement* const element = FindMutable(id);
    if (element == nullptr || !evidence.valid ||
        evidence.sourceRevision != element->displayTextRevision)
    {
        if (element != nullptr)
        {
            element->textLayout = {};
        }
        return;
    }
    element->textLayout = evidence;
}

void UiDocument::ClearTextComposition(const bool touchDisplay) noexcept
{
    if (touchDisplay && !textComposition_.empty())
    {
        UiElement* const textInput = FocusedTextInput();
        if (textInput != nullptr)
        {
            TouchDisplayText(*textInput);
        }
    }
    textComposition_.clear();
    textCompositionSelectionStart_ = -1;
    textCompositionSelectionLength_ = -1;
}

std::size_t UiDocument::HitTestTopmost(const float x, const float y) const noexcept
{
    for (std::size_t reverseIndex = elements_.size(); reverseIndex > 0U; --reverseIndex)
    {
        const std::size_t index = reverseIndex - 1U;
        const UiElement& element = elements_[index];
        if (!IsInteractionAllowed(index) || !element.visible || !IsPointerInteractive(element.kind))
        {
            continue;
        }
        if (ContainsPoint(element.bounds, x, y))
        {
            return index;
        }
    }
    return InvalidUiElementIndex;
}

bool UiDocument::ContainsPoint(const UiRect& bounds, const float x, const float y) noexcept
{
    if (!std::isfinite(x) || !std::isfinite(y) || bounds.width == 0U || bounds.height == 0U)
    {
        return false;
    }

    const double pointX = static_cast<double>(x);
    const double pointY = static_cast<double>(y);
    const double left = static_cast<double>(bounds.x);
    const double top = static_cast<double>(bounds.y);
    const double right = left + static_cast<double>(bounds.width);
    const double bottom = top + static_cast<double>(bounds.height);
    return pointX >= left && pointX < right && pointY >= top && pointY < bottom;
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
