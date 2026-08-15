#include <trace2d/ui/UiLayout.hpp>

#include <algorithm>
#include <limits>
#include <numeric>
#include <utility>

namespace trace2d::ui
{
std::string_view ToString(const UiLayoutResult result) noexcept
{
    switch (result)
    {
    case UiLayoutResult::Success:
        return "success";
    case UiLayoutResult::InvalidCanvasSize:
        return "invalid_canvas_size";
    case UiLayoutResult::InvalidId:
        return "invalid_id";
    case UiLayoutResult::TooManyNodes:
        return "too_many_nodes";
    case UiLayoutResult::InvalidPlacementMode:
        return "invalid_placement_mode";
    case UiLayoutResult::InvalidAnchor:
        return "invalid_anchor";
    case UiLayoutResult::InvalidBounds:
        return "invalid_bounds";
    case UiLayoutResult::DuplicateId:
        return "duplicate_id";
    case UiLayoutResult::UnknownParent:
        return "unknown_parent";
    case UiLayoutResult::SelfParent:
        return "self_parent";
    case UiLayoutResult::HierarchyCycle:
        return "hierarchy_cycle";
    case UiLayoutResult::ChildOutsideParent:
        return "child_outside_parent";
    case UiLayoutResult::AlreadyFinalized:
        return "already_finalized";
    }

    return "unknown";
}

std::string_view ToString(const UiLayoutPlacementMode mode) noexcept
{
    switch (mode)
    {
    case UiLayoutPlacementMode::Absolute:
        return "absolute";
    case UiLayoutPlacementMode::AnchoredFixed:
        return "anchored_fixed";
    }

    return "unknown";
}

UiLayoutTree::UiLayoutTree(const std::uint32_t width, const std::uint32_t height) noexcept
    : width_{width},
      height_{height}
{
}

std::uint32_t UiLayoutTree::Width() const noexcept
{
    return width_;
}

std::uint32_t UiLayoutTree::Height() const noexcept
{
    return height_;
}

bool UiLayoutTree::HasValidSize() const noexcept
{
    return width_ > 0U && height_ > 0U &&
           width_ <= MaxUiCanvasDimension && height_ <= MaxUiCanvasDimension;
}

bool UiLayoutTree::IsFinalized() const noexcept
{
    return finalized_;
}

std::span<const UiResolvedLayoutNode> UiLayoutTree::Nodes() const noexcept
{
    return nodes_;
}

const UiResolvedLayoutNode* UiLayoutTree::Find(const std::string_view id) const noexcept
{
    if (!finalized_ || id.empty())
    {
        return nullptr;
    }

    const std::size_t index = FindPreparedIndex(id);
    return index == InvalidUiLayoutIndex ? nullptr : &nodes_[index];
}

void UiLayoutTree::ReserveNodes(const std::size_t count)
{
    const std::size_t boundedCount = std::min(count, MaxUiLayoutNodes);
    nodes_.reserve(boundedCount);
    lookup_.reserve(boundedCount);
    visitState_.reserve(boundedCount);
    chain_.reserve(boundedCount);
}

UiLayoutResult UiLayoutTree::AddNode(UiLayoutNodeSpec node)
{
    if (finalized_)
    {
        return UiLayoutResult::AlreadyFinalized;
    }
    if (node.id.empty())
    {
        return UiLayoutResult::InvalidId;
    }
    if (nodes_.size() >= MaxUiLayoutNodes)
    {
        return UiLayoutResult::TooManyNodes;
    }

    switch (node.placementMode)
    {
    case UiLayoutPlacementMode::Absolute:
        if (node.localBounds.width == 0U || node.localBounds.height == 0U)
        {
            return UiLayoutResult::InvalidBounds;
        }
        break;
    case UiLayoutPlacementMode::AnchoredFixed:
        if (node.anchored.width == 0U || node.anchored.height == 0U)
        {
            return UiLayoutResult::InvalidBounds;
        }
        if (!IsValidNormalized(node.anchored.anchor) ||
            !IsValidNormalized(node.anchored.pivot))
        {
            return UiLayoutResult::InvalidAnchor;
        }
        break;
    default:
        return UiLayoutResult::InvalidPlacementMode;
    }

    UiResolvedLayoutNode resolved{};
    resolved.id = std::move(node.id);
    resolved.parentId = std::move(node.parentId);
    resolved.localBounds = node.localBounds;
    resolved.placementMode = node.placementMode;
    resolved.anchored = node.anchored;
    nodes_.push_back(std::move(resolved));
    return UiLayoutResult::Success;
}

UiLayoutResult UiLayoutTree::Finalize()
{
    if (finalized_)
    {
        return UiLayoutResult::AlreadyFinalized;
    }
    if (!HasValidSize())
    {
        return UiLayoutResult::InvalidCanvasSize;
    }

    lookup_.resize(nodes_.size());
    std::iota(lookup_.begin(), lookup_.end(), std::size_t{0U});
    std::sort(
        lookup_.begin(),
        lookup_.end(),
        [this](const std::size_t lhs, const std::size_t rhs)
        {
            return nodes_[lhs].id < nodes_[rhs].id;
        });

    for (std::size_t lookupIndex = 1U; lookupIndex < lookup_.size(); ++lookupIndex)
    {
        if (nodes_[lookup_[lookupIndex - 1U]].id == nodes_[lookup_[lookupIndex]].id)
        {
            return UiLayoutResult::DuplicateId;
        }
    }

    for (UiResolvedLayoutNode& node : nodes_)
    {
        node.parentIndex = InvalidUiLayoutIndex;
        node.depth = 0U;
        node.resolvedLocalBounds = {};
        node.bounds = {};

        if (node.parentId.empty())
        {
            continue;
        }
        if (node.parentId == node.id)
        {
            return UiLayoutResult::SelfParent;
        }

        const std::size_t parentIndex = FindPreparedIndex(node.parentId);
        if (parentIndex == InvalidUiLayoutIndex)
        {
            return UiLayoutResult::UnknownParent;
        }
        node.parentIndex = parentIndex;
    }

    visitState_.assign(nodes_.size(), std::uint8_t{0U});
    chain_.clear();

    for (std::size_t start = 0U; start < nodes_.size(); ++start)
    {
        if (visitState_[start] == 2U)
        {
            continue;
        }

        chain_.clear();
        std::size_t current = start;
        while (current != InvalidUiLayoutIndex && visitState_[current] != 2U)
        {
            if (visitState_[current] == 1U)
            {
                return UiLayoutResult::HierarchyCycle;
            }

            visitState_[current] = 1U;
            chain_.push_back(current);
            current = nodes_[current].parentIndex;
        }

        while (!chain_.empty())
        {
            const std::size_t index = chain_.back();
            chain_.pop_back();
            UiResolvedLayoutNode& node = nodes_[index];

            if (node.parentIndex == InvalidUiLayoutIndex)
            {
                UiRect resolvedLocal{};
                if (!ResolveLocalBounds(node, width_, height_, resolvedLocal) ||
                    !ContainsInCanvas(resolvedLocal))
                {
                    return UiLayoutResult::InvalidBounds;
                }

                node.resolvedLocalBounds = resolvedLocal;
                node.bounds = resolvedLocal;
                node.depth = 0U;
            }
            else
            {
                const UiResolvedLayoutNode& parent = nodes_[node.parentIndex];
                UiRect resolvedLocal{};
                if (!ResolveLocalBounds(
                        node,
                        parent.bounds.width,
                        parent.bounds.height,
                        resolvedLocal) ||
                    !ContainsInParent(parent.bounds, resolvedLocal))
                {
                    return UiLayoutResult::ChildOutsideParent;
                }

                node.resolvedLocalBounds = resolvedLocal;
                node.bounds = UiRect{
                    parent.bounds.x + resolvedLocal.x,
                    parent.bounds.y + resolvedLocal.y,
                    resolvedLocal.width,
                    resolvedLocal.height,
                };
                node.depth = parent.depth + 1U;
            }

            visitState_[index] = 2U;
        }
    }

    finalized_ = true;
    return UiLayoutResult::Success;
}

std::size_t UiLayoutTree::FindPreparedIndex(const std::string_view id) const noexcept
{
    const auto iterator = std::lower_bound(
        lookup_.begin(),
        lookup_.end(),
        id,
        [this](const std::size_t index, const std::string_view value)
        {
            return nodes_[index].id < value;
        });

    if (iterator == lookup_.end() || nodes_[*iterator].id != id)
    {
        return InvalidUiLayoutIndex;
    }
    return *iterator;
}

bool UiLayoutTree::ContainsInCanvas(const UiRect& bounds) const noexcept
{
    if (bounds.width == 0U || bounds.height == 0U)
    {
        return false;
    }
    if (bounds.x >= width_ || bounds.y >= height_)
    {
        return false;
    }
    return bounds.width <= width_ - bounds.x &&
           bounds.height <= height_ - bounds.y;
}

bool UiLayoutTree::ContainsInParent(
    const UiRect& parentBounds,
    const UiRect& localBounds) noexcept
{
    if (localBounds.width == 0U || localBounds.height == 0U)
    {
        return false;
    }
    if (localBounds.x >= parentBounds.width || localBounds.y >= parentBounds.height)
    {
        return false;
    }
    return localBounds.width <= parentBounds.width - localBounds.x &&
           localBounds.height <= parentBounds.height - localBounds.y;
}

bool UiLayoutTree::IsValidNormalized(const UiNormalizedPoint point) noexcept
{
    return point.x <= UiNormalizedUnit && point.y <= UiNormalizedUnit;
}

std::uint32_t UiLayoutTree::ResolveNormalized(
    const std::uint32_t extent,
    const std::uint16_t normalized) noexcept
{
    const std::uint64_t scaled =
        static_cast<std::uint64_t>(extent) * static_cast<std::uint64_t>(normalized);
    return static_cast<std::uint32_t>(
        (scaled + static_cast<std::uint64_t>(UiNormalizedUnit / 2U)) /
        static_cast<std::uint64_t>(UiNormalizedUnit));
}

bool UiLayoutTree::ResolveLocalBounds(
    const UiResolvedLayoutNode& node,
    const std::uint32_t referenceWidth,
    const std::uint32_t referenceHeight,
    UiRect& resolved) noexcept
{
    switch (node.placementMode)
    {
    case UiLayoutPlacementMode::Absolute:
        resolved = node.localBounds;
        return true;
    case UiLayoutPlacementMode::AnchoredFixed:
        break;
    default:
        return false;
    }

    const std::uint32_t anchorX = ResolveNormalized(referenceWidth, node.anchored.anchor.x);
    const std::uint32_t anchorY = ResolveNormalized(referenceHeight, node.anchored.anchor.y);
    const std::uint32_t pivotX = ResolveNormalized(node.anchored.width, node.anchored.pivot.x);
    const std::uint32_t pivotY = ResolveNormalized(node.anchored.height, node.anchored.pivot.y);

    const std::int64_t originX =
        static_cast<std::int64_t>(anchorX) + static_cast<std::int64_t>(node.anchored.offsetX) -
        static_cast<std::int64_t>(pivotX);
    const std::int64_t originY =
        static_cast<std::int64_t>(anchorY) + static_cast<std::int64_t>(node.anchored.offsetY) -
        static_cast<std::int64_t>(pivotY);

    constexpr std::int64_t MaxCoordinate =
        static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max());
    if (originX < 0 || originY < 0 || originX > MaxCoordinate || originY > MaxCoordinate)
    {
        return false;
    }

    resolved = UiRect{
        static_cast<std::uint32_t>(originX),
        static_cast<std::uint32_t>(originY),
        node.anchored.width,
        node.anchored.height,
    };
    return true;
}
} // namespace trace2d::ui
