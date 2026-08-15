#include <trace2d/ui/Ui.hpp>

#include <algorithm>
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

[[nodiscard]] UiRect IntersectRects(const UiRect& lhs, const UiRect& rhs) noexcept
{
    const std::uint32_t left = std::max(lhs.x, rhs.x);
    const std::uint32_t top = std::max(lhs.y, rhs.y);
    const std::uint32_t right = std::min(lhs.x + lhs.width, rhs.x + rhs.width);
    const std::uint32_t bottom = std::min(lhs.y + lhs.height, rhs.y + rhs.height);
    if (right <= left || bottom <= top)
    {
        return UiRect{left, top, 0U, 0U};
    }
    return UiRect{left, top, right - left, bottom - top};
}

[[nodiscard]] std::uint64_t SaturatingAdd(
    const std::uint64_t lhs,
    const std::uint64_t rhs) noexcept
{
    return rhs > std::numeric_limits<std::uint64_t>::max() - lhs
        ? std::numeric_limits<std::uint64_t>::max()
        : lhs + rhs;
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
    case UiActionResult::NotScrollViewport:
        return "not_scroll_viewport";
    case UiActionResult::InvalidScrollContent:
        return "invalid_scroll_content";
    case UiActionResult::UnsupportedScrollHierarchy:
        return "unsupported_scroll_hierarchy";
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

    if (element.clipActive)
    {
        const bool hasClipArea = element.clipBounds.width > 0U && element.clipBounds.height > 0U;
        if (hasClipArea && !Contains(element.clipBounds))
        {
            return UiActionResult::InvalidBounds;
        }
    }
    else
    {
        element.clipBounds = {};
    }

    // Direct callers from the original flat UI contract do not need to populate U2 hierarchy
    // metadata. For a root, the resolved parent-local rectangle is identical to absolute bounds.
    if (element.localBounds.width == 0U || element.localBounds.height == 0U)
    {
        element.localBounds = element.bounds;
    }

    // Runtime interaction / U9 scroll state is never accepted as authored input.
    element.presentationBounds = UiPresentationRect{
        .x = static_cast<std::int32_t>(element.bounds.x),
        .y = static_cast<std::int32_t>(element.bounds.y),
        .width = element.bounds.width,
        .height = element.bounds.height,
    };
    element.scrollOwnerIndex = InvalidUiElementIndex;
    element.scroll = {};
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

UiActionResult UiDocument::ConfigureScrollViewport(
    const std::string_view id,
    const std::uint32_t contentWidth,
    const std::uint32_t contentHeight) noexcept
{
    std::size_t viewportIndex = InvalidUiElementIndex;
    for (std::size_t index = 0U; index < elements_.size(); ++index)
    {
        if (elements_[index].id == id)
        {
            viewportIndex = index;
            break;
        }
    }
    if (viewportIndex == InvalidUiElementIndex)
    {
        return UiActionResult::NotFound;
    }

    UiElement& viewport = elements_[viewportIndex];
    if (viewport.kind != UiElementKind::Panel || viewport.scroll.viewport)
    {
        return UiActionResult::InvalidScrollContent;
    }
    if (viewport.scrollOwnerIndex != InvalidUiElementIndex)
    {
        return UiActionResult::UnsupportedScrollHierarchy;
    }
    if (contentWidth < viewport.bounds.width || contentHeight < viewport.bounds.height ||
        contentWidth > MaxUiCanvasDimension || contentHeight > MaxUiCanvasDimension)
    {
        return UiActionResult::InvalidScrollContent;
    }

    // Validate the complete subtree first so failed configuration publishes no partial ownership.
    for (std::size_t index = 0U; index < elements_.size(); ++index)
    {
        if (index == viewportIndex || !IsDescendantOrSelf(index, viewportIndex))
        {
            continue;
        }

        const UiElement& descendant = elements_[index];
        if (descendant.scroll.viewport || descendant.scrollOwnerIndex != InvalidUiElementIndex ||
            descendant.clipChildren)
        {
            // Nested scroll ownership and movable nested clipping need cumulative transformed clip
            // state. U9 deliberately rejects both instead of adding per-event hierarchy discovery.
            return UiActionResult::UnsupportedScrollHierarchy;
        }

        if (descendant.bounds.x < viewport.bounds.x || descendant.bounds.y < viewport.bounds.y)
        {
            return UiActionResult::InvalidScrollContent;
        }

        const std::uint32_t localX = descendant.bounds.x - viewport.bounds.x;
        const std::uint32_t localY = descendant.bounds.y - viewport.bounds.y;
        if (localX >= contentWidth || localY >= contentHeight ||
            descendant.bounds.width > contentWidth - localX ||
            descendant.bounds.height > contentHeight - localY)
        {
            return UiActionResult::InvalidScrollContent;
        }
    }

    viewport.clipChildren = true;
    viewport.scroll.viewport = true;
    viewport.scroll.contentWidth = contentWidth;
    viewport.scroll.contentHeight = contentHeight;

    for (std::size_t index = 0U; index < elements_.size(); ++index)
    {
        if (index == viewportIndex || !IsDescendantOrSelf(index, viewportIndex))
        {
            continue;
        }

        UiElement& descendant = elements_[index];
        descendant.scrollOwnerIndex = viewportIndex;
        descendant.clipBounds = descendant.clipActive
            ? IntersectRects(descendant.clipBounds, viewport.bounds)
            : viewport.bounds;
        descendant.clipActive = true;
    }

    return UiActionResult::Success;
}

UiActionResult UiDocument::ScrollTo(
    const std::string_view id,
    const std::uint32_t offsetX,
    const std::uint32_t offsetY) noexcept
{
    for (std::size_t index = 0U; index < elements_.size(); ++index)
    {
        if (elements_[index].id == id)
        {
            return SetScrollOffsetIndex(index, offsetX, offsetY);
        }
    }
    return UiActionResult::NotFound;
}

UiActionResult UiDocument::ScrollBy(
    const std::string_view id,
    const std::int32_t deltaX,
    const std::int32_t deltaY) noexcept
{
    for (std::size_t index = 0U; index < elements_.size(); ++index)
    {
        if (elements_[index].id == id)
        {
            return ScrollIndex(index, deltaX, deltaY);
        }
    }
    return UiActionResult::NotFound;
}

UiActionResult UiDocument::ScrollIndex(
    const std::size_t index,
    const std::int64_t deltaX,
    const std::int64_t deltaY) noexcept
{
    if (index >= elements_.size() || !elements_[index].scroll.viewport)
    {
        return UiActionResult::NotScrollViewport;
    }

    const UiElement& viewport = elements_[index];
    const std::uint32_t maxX = viewport.scroll.contentWidth - viewport.bounds.width;
    const std::uint32_t maxY = viewport.scroll.contentHeight - viewport.bounds.height;
    const std::int64_t targetX = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(viewport.scroll.offsetX) + deltaX,
        0,
        static_cast<std::int64_t>(maxX));
    const std::int64_t targetY = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(viewport.scroll.offsetY) + deltaY,
        0,
        static_cast<std::int64_t>(maxY));
    return SetScrollOffsetIndex(
        index,
        static_cast<std::uint32_t>(targetX),
        static_cast<std::uint32_t>(targetY));
}

UiActionResult UiDocument::SetScrollOffsetIndex(
    const std::size_t index,
    const std::uint32_t offsetX,
    const std::uint32_t offsetY) noexcept
{
    if (index >= elements_.size() || !elements_[index].scroll.viewport)
    {
        return UiActionResult::NotScrollViewport;
    }

    UiElement& viewport = elements_[index];
    const std::uint32_t maxX = viewport.scroll.contentWidth - viewport.bounds.width;
    const std::uint32_t maxY = viewport.scroll.contentHeight - viewport.bounds.height;
    const std::uint32_t clampedX = std::min(offsetX, maxX);
    const std::uint32_t clampedY = std::min(offsetY, maxY);
    if (clampedX == viewport.scroll.offsetX && clampedY == viewport.scroll.offsetY)
    {
        return UiActionResult::Success;
    }

    viewport.scroll.offsetX = clampedX;
    viewport.scroll.offsetY = clampedY;
    viewport.scroll.revision = viewport.scroll.revision == std::numeric_limits<std::uint64_t>::max()
        ? 1U
        : viewport.scroll.revision + 1U;

    std::uint64_t updated = 0U;
    bool clearHovered = false;
    for (std::size_t elementIndex = 0U; elementIndex < elements_.size(); ++elementIndex)
    {
        UiElement& element = elements_[elementIndex];
        if (element.scrollOwnerIndex != index)
        {
            continue;
        }

        element.presentationBounds = UiPresentationRect{
            .x = static_cast<std::int32_t>(element.bounds.x) - static_cast<std::int32_t>(clampedX),
            .y = static_cast<std::int32_t>(element.bounds.y) - static_cast<std::int32_t>(clampedY),
            .width = element.bounds.width,
            .height = element.bounds.height,
        };
        ++updated;
        if (hoveredIndex_ == elementIndex)
        {
            element.hovered = false;
            clearHovered = true;
        }
    }
    if (clearHovered)
    {
        hoveredIndex_ = InvalidUiElementIndex;
    }
    viewport.scroll.translationUpdates = SaturatingAdd(viewport.scroll.translationUpdates, updated);
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

    // Wheel routing is edge-triggered work. Ordinary pointer motion never pays this second scan.
    if ((pointer.wheelX != 0.0F || pointer.wheelY != 0.0F) &&
        std::isfinite(pointer.wheelX) && std::isfinite(pointer.wheelY))
    {
        const std::size_t scrollIndex = HitTestTopmostScrollViewport(pointer.x, pointer.y);
        if (scrollIndex < elements_.size())
        {
            const std::int64_t deltaX = pointer.wheelX > 0.0F
                ? -static_cast<std::int64_t>(UiScrollWheelStep)
                : (pointer.wheelX < 0.0F ? static_cast<std::int64_t>(UiScrollWheelStep) : 0);
            const std::int64_t deltaY = pointer.wheelY > 0.0F
                ? -static_cast<std::int64_t>(UiScrollWheelStep)
                : (pointer.wheelY < 0.0F ? static_cast<std::int64_t>(UiScrollWheelStep) : 0);
            (void)ScrollIndex(scrollIndex, deltaX, deltaY);
            result.consumed = true;
        }
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
            const bool releaseInside = IsPointInsideElement(capturedIndex, pointer.x, pointer.y);
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
        if (IsPointInsideElement(index, x, y))
        {
            return index;
        }
    }
    return InvalidUiElementIndex;
}

std::size_t UiDocument::HitTestTopmostScrollViewport(const float x, const float y) const noexcept
{
    for (std::size_t reverseIndex = elements_.size(); reverseIndex > 0U; --reverseIndex)
    {
        const std::size_t index = reverseIndex - 1U;
        const UiElement& element = elements_[index];
        if (!element.scroll.viewport || !element.visible || !element.enabled ||
            !IsInteractionAllowed(index))
        {
            continue;
        }
        if (IsPointInsideElement(index, x, y))
        {
            return index;
        }
    }
    return InvalidUiElementIndex;
}

bool UiDocument::IsPointInsideElement(
    const std::size_t index,
    const float x,
    const float y) const noexcept
{
    if (index >= elements_.size())
    {
        return false;
    }

    const UiElement& element = elements_[index];
    return ContainsPoint(element.presentationBounds, x, y) &&
           (!element.clipActive || ContainsPoint(element.clipBounds, x, y));
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

bool UiDocument::ContainsPoint(
    const UiPresentationRect& bounds,
    const float x,
    const float y) noexcept
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
