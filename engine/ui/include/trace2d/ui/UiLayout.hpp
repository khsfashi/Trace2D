#pragma once

#include <trace2d/ui/Ui.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::ui
{
inline constexpr std::size_t MaxUiLayoutNodes = 65536U;
inline constexpr std::size_t InvalidUiLayoutIndex = std::numeric_limits<std::size_t>::max();

// Exact normalized coordinate unit used by anchored UI placement. 0 is the leading edge,
// UiNormalizedUnit / 2 is the center, and UiNormalizedUnit is the trailing edge.
inline constexpr std::uint16_t UiNormalizedUnit = 1024U;

enum class UiLayoutResult : std::uint8_t
{
    Success,
    InvalidCanvasSize,
    InvalidId,
    TooManyNodes,
    InvalidPlacementMode,
    InvalidAnchor,
    InvalidBounds,
    DuplicateId,
    UnknownParent,
    SelfParent,
    HierarchyCycle,
    ChildOutsideParent,
    AlreadyFinalized,
};

enum class UiLayoutPlacementMode : std::uint8_t
{
    Absolute = 0,
    AnchoredFixed,
};

struct UiNormalizedPoint final
{
    std::uint16_t x{0U};
    std::uint16_t y{0U};

    [[nodiscard]] bool operator==(const UiNormalizedPoint&) const noexcept = default;
};

struct UiAnchoredPlacement final
{
    UiNormalizedPoint anchor{};
    UiNormalizedPoint pivot{};
    std::int32_t offsetX{0};
    std::int32_t offsetY{0};
    std::uint32_t width{0U};
    std::uint32_t height{0U};

    [[nodiscard]] bool operator==(const UiAnchoredPlacement&) const noexcept = default;
};

struct UiLayoutNodeSpec final
{
    std::string id{};
    std::string parentId{};
    UiRect localBounds{};
    UiLayoutPlacementMode placementMode{UiLayoutPlacementMode::Absolute};
    UiAnchoredPlacement anchored{};
};

struct UiResolvedLayoutNode final
{
    std::string id{};
    std::string parentId{};
    std::size_t parentIndex{InvalidUiLayoutIndex};
    std::uint32_t depth{0U};

    // Authored absolute-mode rectangle retained for inspection/source compatibility. AnchoredFixed
    // nodes retain their authored UiAnchoredPlacement separately and leave this field unchanged.
    UiRect localBounds{};
    UiLayoutPlacementMode placementMode{UiLayoutPlacementMode::Absolute};
    UiAnchoredPlacement anchored{};

    // Final parent-local rectangle after placement resolution, followed by absolute logical-canvas
    // bounds. For Absolute placement resolvedLocalBounds == localBounds.
    UiRect resolvedLocalBounds{};
    UiRect bounds{};
};

[[nodiscard]] std::string_view ToString(UiLayoutResult result) noexcept;
[[nodiscard]] std::string_view ToString(UiLayoutPlacementMode mode) noexcept;

class UiLayoutTree final
{
public:
    UiLayoutTree() = default;
    UiLayoutTree(std::uint32_t width, std::uint32_t height) noexcept;

    [[nodiscard]] std::uint32_t Width() const noexcept;
    [[nodiscard]] std::uint32_t Height() const noexcept;
    [[nodiscard]] bool HasValidSize() const noexcept;
    [[nodiscard]] bool IsFinalized() const noexcept;
    [[nodiscard]] std::span<const UiResolvedLayoutNode> Nodes() const noexcept;
    [[nodiscard]] const UiResolvedLayoutNode* Find(std::string_view id) const noexcept;

    void ReserveNodes(std::size_t count);
    [[nodiscard]] UiLayoutResult AddNode(UiLayoutNodeSpec node);
    [[nodiscard]] UiLayoutResult Finalize();

private:
    [[nodiscard]] std::size_t FindPreparedIndex(std::string_view id) const noexcept;
    [[nodiscard]] bool ContainsInCanvas(const UiRect& bounds) const noexcept;
    [[nodiscard]] static bool ContainsInParent(
        const UiRect& parentBounds,
        const UiRect& localBounds) noexcept;
    [[nodiscard]] static bool IsValidNormalized(UiNormalizedPoint point) noexcept;
    [[nodiscard]] static std::uint32_t ResolveNormalized(
        std::uint32_t extent,
        std::uint16_t normalized) noexcept;
    [[nodiscard]] static bool ResolveLocalBounds(
        const UiResolvedLayoutNode& node,
        std::uint32_t referenceWidth,
        std::uint32_t referenceHeight,
        UiRect& resolved) noexcept;

    std::uint32_t width_{0U};
    std::uint32_t height_{0U};
    std::vector<UiResolvedLayoutNode> nodes_{};
    std::vector<std::size_t> lookup_{};
    std::vector<std::uint8_t> visitState_{};
    std::vector<std::size_t> chain_{};
    bool finalized_{false};
};
} // namespace trace2d::ui
