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

enum class UiLayoutResult : std::uint8_t
{
    Success,
    InvalidCanvasSize,
    InvalidId,
    TooManyNodes,
    InvalidBounds,
    DuplicateId,
    UnknownParent,
    SelfParent,
    HierarchyCycle,
    ChildOutsideParent,
    AlreadyFinalized,
};

struct UiLayoutNodeSpec final
{
    std::string id{};
    std::string parentId{};
    UiRect localBounds{};
};

struct UiResolvedLayoutNode final
{
    std::string id{};
    std::string parentId{};
    std::size_t parentIndex{InvalidUiLayoutIndex};
    std::uint32_t depth{0U};
    UiRect localBounds{};
    UiRect bounds{};
};

[[nodiscard]] std::string_view ToString(UiLayoutResult result) noexcept;

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

    std::uint32_t width_{0U};
    std::uint32_t height_{0U};
    std::vector<UiResolvedLayoutNode> nodes_{};
    std::vector<std::size_t> lookup_{};
    std::vector<std::uint8_t> visitState_{};
    std::vector<std::size_t> chain_{};
    bool finalized_{false};
};
} // namespace trace2d::ui
