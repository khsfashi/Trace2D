#pragma once

#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/input/Input.hpp>
#include <trace2d/input/TextInput.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::ui
{
inline constexpr std::uint32_t MaxUiCanvasDimension = 4096U;
inline constexpr std::size_t InvalidUiElementIndex = std::numeric_limits<std::size_t>::max();
inline constexpr std::uint32_t UiScrollWheelStep = 24U;

struct UiRect final
{
    std::uint32_t x{0};
    std::uint32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};

    [[nodiscard]] bool operator==(const UiRect&) const noexcept = default;
};

// Presentation-space coordinates may become negative when logical scroll content is translated
// above/left of a clipped viewport. Logical UiRect layout truth remains unsigned and unchanged.
struct UiPresentationRect final
{
    std::int32_t x{0};
    std::int32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};

    [[nodiscard]] bool operator==(const UiPresentationRect&) const noexcept = default;
};

enum class UiElementKind : std::uint8_t
{
    Panel,
    Label,
    Button,
    TextInput,
};

struct UiTextLayoutEvidence final
{
    bool valid{false};
    std::uint64_t sourceRevision{0U};
    bool includesComposition{false};
    std::size_t glyphCount{0U};
    std::size_t lineCount{0U};
    std::int64_t contentWidth26_6{0};
    std::int64_t contentHeight26_6{0};
    std::int64_t layoutWidth26_6{0};
    std::int64_t layoutHeight26_6{0};

    [[nodiscard]] bool operator==(const UiTextLayoutEvidence&) const noexcept = default;
};

struct UiScrollState final
{
    bool viewport{false};
    std::uint32_t contentWidth{0U};
    std::uint32_t contentHeight{0U};
    std::uint32_t offsetX{0U};
    std::uint32_t offsetY{0U};
    std::uint64_t revision{0U};
    std::uint64_t translationUpdates{0U};

    [[nodiscard]] bool operator==(const UiScrollState&) const noexcept = default;
};

class UiProgressState final
{
public:
    [[nodiscard]] bool Active() const noexcept
    {
        return active_;
    }

    [[nodiscard]] std::uint32_t Value() const noexcept
    {
        return value_;
    }

    [[nodiscard]] std::uint32_t Maximum() const noexcept
    {
        return maximum_;
    }

    [[nodiscard]] std::uint64_t Revision() const noexcept
    {
        return revision_;
    }

    [[nodiscard]] bool operator==(const UiProgressState&) const noexcept = default;

private:
    bool active_{false};
    std::uint32_t value_{0U};
    std::uint32_t maximum_{0U};
    std::uint64_t revision_{0U};

    friend class UiDocument;
};

class UiImageState final
{
public:
    [[nodiscard]] bool Active() const noexcept
    {
        return active_;
    }

    [[nodiscard]] assets::ResourceHandle<assets::TextureResource> Texture() const noexcept
    {
        return texture_;
    }

    [[nodiscard]] std::uint64_t Revision() const noexcept
    {
        return revision_;
    }

    [[nodiscard]] bool operator==(const UiImageState&) const noexcept = default;

private:
    bool active_{false};
    assets::ResourceHandle<assets::TextureResource> texture_{};
    std::uint64_t revision_{0U};

    friend class UiDocument;
};

struct UiElement final
{
    std::string id{};
    UiElementKind kind{UiElementKind::Panel};

    // U2 retains the resolved hierarchy compiled at authored-load/setup time. parentIndex indexes
    // UiDocument::Elements() and remains InvalidUiElementIndex for roots. localBounds is the
    // resolved parent-local rectangle; bounds is immutable logical/content-space layout truth.
    std::string parentId{};
    std::size_t parentIndex{InvalidUiElementIndex};
    std::uint32_t depth{0U};
    UiRect localBounds{};
    UiRect bounds{};

    // U9 keeps translated presentation geometry separate from logical layout. Ordinary elements
    // initialize this from bounds. Scrolled descendants retain one direct owner index so pointer /
    // raster work never walks hierarchy or resolves semantic strings after setup.
    UiPresentationRect presentationBounds{};
    std::size_t scrollOwnerIndex{InvalidUiElementIndex};
    UiScrollState scroll{};

    // U12 keeps practical Progress state on the same retained element rather than in a side cache.
    // The state can only be activated/mutated through UiDocument, preserving maximum/value
    // invariants while raster and Agent inspection consume direct retained values.
    UiProgressState progress{};

    // U13 keeps only #86 generation-safe texture identity on the retained element. Canonical RGBA8
    // bytes stay ResourceRegistry-owned and renderer residency stays renderer-owned; UI never keeps
    // a second decoded texture/cache/backend handle.
    UiImageState image{};

    // U8 authored clipping is owned by the hierarchy, not by renderer-specific state.
    // clipChildren clips descendants to this element's resolved logical bounds. clipActive /
    // clipBounds are setup-time resolved ancestor evidence consumed directly by pointer/raster
    // paths so ordinary interaction never walks parent chains.
    bool clipChildren{false};
    bool clipActive{false};
    UiRect clipBounds{};

    std::string name{};
    std::string text{};
    bool visible{true};
    bool enabled{true};

    // U4 pointer state is runtime semantic state, not authored layout state. At most one element is
    // hovered and at most one pointer-interactive element is pointerPressed/captured at a time.
    bool hovered{false};
    bool pointerPressed{false};
    std::uint64_t activationCount{0};

    std::uint64_t textSourceIdentity{0U};
    std::uint64_t displayTextRevision{0U};
    UiTextLayoutEvidence textLayout{};
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
    OutsideModalScope,
    InvalidTextCompositionRange,
    NotScrollViewport,
    InvalidScrollContent,
    UnsupportedScrollHierarchy,
};

enum class UiProgressResult : std::uint8_t
{
    Success,
    NotFound,
    InvalidTarget,
    NotProgress,
    InvalidRange,
};

enum class UiImageResult : std::uint8_t
{
    Success,
    NotFound,
    InvalidTarget,
    NotImage,
    InvalidTexture,
};

enum class UiNavigationDirection : std::uint8_t
{
    Left,
    Right,
    Up,
    Down,
};

enum class UiPointerRouteStatus : std::uint8_t
{
    Success,
    InvalidPosition,
};

struct UiPointerRouteResult final
{
    UiPointerRouteStatus status{UiPointerRouteStatus::Success};
    std::size_t hoveredIndex{InvalidUiElementIndex};
    std::size_t capturedIndex{InvalidUiElementIndex};
    bool consumed{false};
    bool activated{false};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return status == UiPointerRouteStatus::Success;
    }
};

[[nodiscard]] std::string_view ToString(UiElementKind kind) noexcept;
[[nodiscard]] std::string_view ToString(UiActionResult result) noexcept;
[[nodiscard]] std::string_view ToString(UiProgressResult result) noexcept;
[[nodiscard]] std::string_view ToString(UiImageResult result) noexcept;
[[nodiscard]] std::string_view ToString(UiPointerRouteStatus status) noexcept;
[[nodiscard]] bool IsFocusable(UiElementKind kind) noexcept;
[[nodiscard]] bool IsActivatable(UiElementKind kind) noexcept;
[[nodiscard]] bool IsPointerInteractive(UiElementKind kind) noexcept;

class UiTextLayoutCache;

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
    [[nodiscard]] const UiElement* HoveredElement() const noexcept;
    [[nodiscard]] const UiElement* CapturedElement() const noexcept;
    [[nodiscard]] const UiElement* ModalScopeElement() const noexcept;
    [[nodiscard]] bool HasModalScope() const noexcept;
    [[nodiscard]] bool IsFocused(std::string_view id) const noexcept;
    [[nodiscard]] UiTextCompositionState TextComposition() const noexcept;

    void ReserveElements(std::size_t count);
    [[nodiscard]] UiActionResult AddElement(UiElement element);
    [[nodiscard]] UiActionResult Focus(std::string_view id) noexcept;
    [[nodiscard]] UiActionResult Activate(std::string_view id) noexcept;

    // U12 specializes an existing non-scroll Panel as a deterministic Progress visual. Mutation is
    // explicit state work; unchanged value/maximum pairs do not advance revision or trigger layout.
    [[nodiscard]] UiProgressResult ConfigureProgress(
        std::string_view id,
        std::uint32_t value,
        std::uint32_t maximum) noexcept;
    [[nodiscard]] UiProgressResult SetProgress(
        std::string_view id,
        std::uint32_t value,
        std::uint32_t maximum) noexcept;

    // U13 specializes an existing non-scroll Panel as an Image while retaining only a live #86
    // TextureResource handle. Registry resolution is explicit mutation/setup work; unchanged handle
    // assignment is a no-op and canonical bytes remain registry-owned.
    [[nodiscard]] UiImageResult ConfigureImage(
        std::string_view id,
        assets::ResourceHandle<assets::TextureResource> texture,
        const assets::ResourceRegistry& resources) noexcept;
    [[nodiscard]] UiImageResult SetImage(
        std::string_view id,
        assets::ResourceHandle<assets::TextureResource> texture,
        const assets::ResourceRegistry& resources) noexcept;

    // U9 configures an existing Panel as a clipped scroll viewport after hierarchy/layout setup.
    // Configuration may discover descendants once. Offset changes then update only retained signed
    // presentation bounds in one allocation-free contiguous pass; logical bounds never move.
    [[nodiscard]] UiActionResult ConfigureScrollViewport(
        std::string_view id,
        std::uint32_t contentWidth,
        std::uint32_t contentHeight) noexcept;
    [[nodiscard]] UiActionResult ScrollTo(
        std::string_view id,
        std::uint32_t offsetX,
        std::uint32_t offsetY) noexcept;
    [[nodiscard]] UiActionResult ScrollBy(
        std::string_view id,
        std::int32_t deltaX,
        std::int32_t deltaY) noexcept;

    // U7 installs at most one active modal root. Membership is prepared from the already-resolved
    // hierarchy only when the scope changes; steady interaction performs direct-index membership
    // checks and never rediscovers parent strings or allocates per event.
    [[nodiscard]] UiActionResult SetModalScope(std::string_view id);
    void ClearModalScope() noexcept;

    // U5 focus traversal consumes the already-resolved authored element order. Hosts map resolved
    // semantic Input Actions (keyboard/gamepad/etc.) to these protocol-independent operations; UI
    // does not own physical key bindings. Traversal scans only on an explicit move edge and never
    // resolves semantic IDs.
    [[nodiscard]] UiActionResult FocusNext() noexcept;
    [[nodiscard]] UiActionResult FocusPrevious() noexcept;
    [[nodiscard]] UiActionResult ActivateFocused() noexcept;

    // U6 directional focus uses the already-resolved logical rectangles and performs one O(N)
    // integer geometry scan only on an explicit navigation edge. Physical key/gamepad policy stays
    // in #72 Input Actions; successful movement converges on the same FocusIndex() authority.
    [[nodiscard]] UiActionResult FocusDirectional(UiNavigationDirection direction) noexcept;

    // Pointer coordinates are logical UI-canvas coordinates. Physical presentation coordinates must
    // first pass the #88 viewport gate/conversion owned by Trace2D::Render. The normal route performs
    // no semantic-id lookup: overlap hit testing is a reverse contiguous scan and capture/focus/
    // activation are direct-index mutations. U9 wheel routing scans scroll viewports only when wheel
    // input is present and mutates the same retained presentation authority used by hit testing.
    [[nodiscard]] UiPointerRouteResult ApplyPointer(
        const input::PointerState& pointer,
        input::InputControlState primaryButton) noexcept;

    [[nodiscard]] UiActionResult InputText(std::string_view id, std::string_view text);
    [[nodiscard]] UiActionResult ApplyTextInput(const input::TextInputEvent& event);

    void ClearFocus() noexcept;
    void ClearPointerState() noexcept;

private:
    [[nodiscard]] UiElement* FindMutable(std::string_view id) noexcept;
    [[nodiscard]] UiElement* FocusedTextInput() noexcept;
    [[nodiscard]] UiActionResult FocusIndex(std::size_t index) noexcept;
    [[nodiscard]] UiActionResult ActivateIndex(std::size_t index) noexcept;
    [[nodiscard]] UiActionResult MoveFocus(bool forward) noexcept;
    [[nodiscard]] UiActionResult ScrollIndex(
        std::size_t index,
        std::int64_t deltaX,
        std::int64_t deltaY) noexcept;
    [[nodiscard]] UiActionResult SetScrollOffsetIndex(
        std::size_t index,
        std::uint32_t offsetX,
        std::uint32_t offsetY) noexcept;
    [[nodiscard]] std::size_t HitTestTopmost(float x, float y) const noexcept;
    [[nodiscard]] std::size_t HitTestTopmostScrollViewport(float x, float y) const noexcept;
    [[nodiscard]] bool IsPointInsideElement(std::size_t index, float x, float y) const noexcept;
    [[nodiscard]] bool IsInteractionAllowed(std::size_t index) const noexcept;
    [[nodiscard]] bool IsDescendantOrSelf(std::size_t index, std::size_t ancestorIndex) const noexcept;
    [[nodiscard]] static bool ContainsPoint(const UiRect& bounds, float x, float y) noexcept;
    [[nodiscard]] static bool ContainsPoint(const UiPresentationRect& bounds, float x, float y) noexcept;
    [[nodiscard]] bool Contains(const UiRect& bounds) const noexcept;
    void ClearPointerCapture() noexcept;
    void TouchDisplayText(UiElement& element) noexcept;
    void PublishTextLayoutEvidence(std::string_view id, const UiTextLayoutEvidence& evidence) noexcept;
    void ClearTextComposition(bool touchDisplay = true) noexcept;

    std::uint32_t width_{0};
    std::uint32_t height_{0};
    std::vector<UiElement> elements_{};
    std::size_t focusedIndex_{InvalidUiElementIndex};
    std::size_t hoveredIndex_{InvalidUiElementIndex};
    std::size_t pointerCaptureIndex_{InvalidUiElementIndex};
    std::size_t modalScopeIndex_{InvalidUiElementIndex};
    std::vector<std::uint8_t> modalScopeMembership_{};
    std::uint64_t nextTextSourceIdentity_{1U};

    std::string textComposition_{};
    std::int32_t textCompositionSelectionStart_{-1};
    std::int32_t textCompositionSelectionLength_{-1};

    friend class UiTextLayoutCache;
};
} // namespace trace2d::ui
