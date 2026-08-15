#include <trace2d/ui/UiLayout.hpp>

#include <algorithm>
#include <limits>
#include <numeric>
#include <utility>

namespace trace2d::ui
{
namespace
{
[[nodiscard]] bool HasInsets(const UiInsets& insets) noexcept
{
    return insets.left != 0U || insets.top != 0U || insets.right != 0U || insets.bottom != 0U;
}
} // namespace

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
    case UiLayoutResult::InvalidContainerLayout:
        return "invalid_container_layout";
    case UiLayoutResult::StackParentRequired:
        return "stack_parent_required";
    case UiLayoutResult::StackOverflow:
        return "stack_overflow";
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
    case UiLayoutPlacementMode::StackFixed:
        return "stack_fixed";
    }

    return "unknown";
}

std::string_view ToString(const UiContainerLayoutMode mode) noexcept
{
    switch (mode)
    {
    case UiContainerLayoutMode::None:
        return "none";
    case UiContainerLayoutMode::HorizontalStack:
        return "horizontal_stack";
    case UiContainerLayoutMode::VerticalStack:
        return "vertical_stack";
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
    chain_.reserve(boundedCount + 1U);
    childOffsets_.reserve(boundedCount + 1U);
    childIndices_.reserve(boundedCount);
    resolveQueue_.reserve(boundedCount);
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

    switch (node.containerLayout)
    {
    case UiContainerLayoutMode::None:
        if (HasInsets(node.padding) || node.spacing != 0U)
        {
            return UiLayoutResult::InvalidContainerLayout;
        }
        break;
    case UiContainerLayoutMode::HorizontalStack:
    case UiContainerLayoutMode::VerticalStack:
        break;
    default:
        return UiLayoutResult::InvalidContainerLayout;
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
    case UiLayoutPlacementMode::StackFixed:
        if (node.stackFixed.width == 0U || node.stackFixed.height == 0U)
        {
            return UiLayoutResult::InvalidBounds;
        }
        if (node.parentId.empty())
        {
            return UiLayoutResult::StackParentRequired;
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
    resolved.stackFixed = node.stackFixed;
    resolved.containerLayout = node.containerLayout;
    resolved.padding = node.padding;
    resolved.spacing = node.spacing;
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

    // Resolve hierarchy depth and reject cycles before any geometry is published. This walk is
    // independent of source order and only resolves semantic parent indices once.
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
            node.depth = node.parentIndex == InvalidUiLayoutIndex
                ? 0U
                : nodes_[node.parentIndex].depth + 1U;
            visitState_[index] = 2U;
        }
    }

    // Build one contiguous authored-order child index. This prevents stack traversal from
    // degrading into an O(N^2) scan while keeping child-before-parent source documents valid.
    childOffsets_.assign(nodes_.size() + 1U, 0U);
    for (const UiResolvedLayoutNode& node : nodes_)
    {
        if (node.parentIndex != InvalidUiLayoutIndex)
        {
            ++childOffsets_[node.parentIndex + 1U];
        }
    }
    std::partial_sum(childOffsets_.begin(), childOffsets_.end(), childOffsets_.begin());

    childIndices_.resize(childOffsets_.back());
    chain_ = childOffsets_;
    for (std::size_t index = 0U; index < nodes_.size(); ++index)
    {
        const std::size_t parentIndex = nodes_[index].parentIndex;
        if (parentIndex != InvalidUiLayoutIndex)
        {
            childIndices_[chain_[parentIndex]++] = index;
        }
    }

    // Resolve roots first, then breadth-first resolve each parent's direct children. A stack
    // parent's flow cursor is therefore consumed exactly once in authored child order, while a
    // nested stack is visited only after its own final rectangle is known.
    resolveQueue_.clear();
    for (std::size_t index = 0U; index < nodes_.size(); ++index)
    {
        UiResolvedLayoutNode& node = nodes_[index];
        if (node.parentIndex != InvalidUiLayoutIndex)
        {
            continue;
        }
        if (node.placementMode == UiLayoutPlacementMode::StackFixed)
        {
            return UiLayoutResult::StackParentRequired;
        }

        UiRect resolvedLocal{};
        if (!ResolveLocalBounds(node, width_, height_, resolvedLocal) ||
            !ContainsInCanvas(resolvedLocal))
        {
            return UiLayoutResult::InvalidBounds;
        }

        node.resolvedLocalBounds = resolvedLocal;
        node.bounds = resolvedLocal;
        resolveQueue_.push_back(index);
    }

    for (std::size_t queueIndex = 0U; queueIndex < resolveQueue_.size(); ++queueIndex)
    {
        const std::size_t parentIndex = resolveQueue_[queueIndex];
        const UiResolvedLayoutNode& parent = nodes_[parentIndex];

        if (parent.containerLayout != UiContainerLayoutMode::None)
        {
            const std::uint64_t horizontalPadding =
                static_cast<std::uint64_t>(parent.padding.left) + parent.padding.right;
            const std::uint64_t verticalPadding =
                static_cast<std::uint64_t>(parent.padding.top) + parent.padding.bottom;
            if (horizontalPadding > parent.bounds.width || verticalPadding > parent.bounds.height)
            {
                return UiLayoutResult::StackOverflow;
            }
        }

        std::uint64_t stackCursor = 0U;
        bool hasPreviousStackItem = false;
        const std::size_t childBegin = childOffsets_[parentIndex];
        const std::size_t childEnd = childOffsets_[parentIndex + 1U];
        for (std::size_t childOffset = childBegin; childOffset < childEnd; ++childOffset)
        {
            const std::size_t childIndex = childIndices_[childOffset];
            UiResolvedLayoutNode& child = nodes_[childIndex];
            UiRect resolvedLocal{};

            if (child.placementMode == UiLayoutPlacementMode::StackFixed)
            {
                if (parent.containerLayout == UiContainerLayoutMode::None)
                {
                    return UiLayoutResult::StackParentRequired;
                }
                if (!ResolveStackFixedBounds(
                        parent,
                        child,
                        stackCursor,
                        hasPreviousStackItem,
                        resolvedLocal))
                {
                    return UiLayoutResult::StackOverflow;
                }
            }
            else if (!ResolveLocalBounds(
                         child,
                         parent.bounds.width,
                         parent.bounds.height,
                         resolvedLocal))
            {
                // U1 established that a valid anchored child whose signed placement escapes its
                // parent is a containment failure, not an invalid authored rectangle. Keep that
                // diagnostic contract while roots still report InvalidBounds.
                return child.placementMode == UiLayoutPlacementMode::AnchoredFixed
                    ? UiLayoutResult::ChildOutsideParent
                    : UiLayoutResult::InvalidBounds;
            }

            if (!ContainsInParent(parent.bounds, resolvedLocal))
            {
                return child.placementMode == UiLayoutPlacementMode::StackFixed
                    ? UiLayoutResult::StackOverflow
                    : UiLayoutResult::ChildOutsideParent;
            }

            child.resolvedLocalBounds = resolvedLocal;
            child.bounds = UiRect{
                parent.bounds.x + resolvedLocal.x,
                parent.bounds.y + resolvedLocal.y,
                resolvedLocal.width,
                resolvedLocal.height,
            };
            resolveQueue_.push_back(childIndex);
        }
    }

    if (resolveQueue_.size() != nodes_.size())
    {
        return UiLayoutResult::HierarchyCycle;
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
    case UiLayoutPlacementMode::StackFixed:
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

bool UiLayoutTree::ResolveStackFixedBounds(
    const UiResolvedLayoutNode& parent,
    const UiResolvedLayoutNode& child,
    std::uint64_t& cursor,
    bool& hasPreviousStackItem,
    UiRect& resolved) noexcept
{
    if (parent.containerLayout == UiContainerLayoutMode::None ||
        child.placementMode != UiLayoutPlacementMode::StackFixed)
    {
        return false;
    }

    const std::uint64_t parentWidth = parent.bounds.width;
    const std::uint64_t parentHeight = parent.bounds.height;
    const std::uint64_t paddingLeft = parent.padding.left;
    const std::uint64_t paddingTop = parent.padding.top;
    const std::uint64_t paddingRight = parent.padding.right;
    const std::uint64_t paddingBottom = parent.padding.bottom;
    if (paddingLeft + paddingRight > parentWidth || paddingTop + paddingBottom > parentHeight)
    {
        return false;
    }

    const std::uint64_t contentRight = parentWidth - paddingRight;
    const std::uint64_t contentBottom = parentHeight - paddingBottom;
    if (!hasPreviousStackItem)
    {
        cursor = parent.containerLayout == UiContainerLayoutMode::HorizontalStack
            ? paddingLeft
            : paddingTop;
    }
    else
    {
        cursor += parent.spacing;
    }

    const std::uint64_t marginLeft = child.stackFixed.margin.left;
    const std::uint64_t marginTop = child.stackFixed.margin.top;
    const std::uint64_t marginRight = child.stackFixed.margin.right;
    const std::uint64_t marginBottom = child.stackFixed.margin.bottom;
    const std::uint64_t width = child.stackFixed.width;
    const std::uint64_t height = child.stackFixed.height;

    std::uint64_t x = 0U;
    std::uint64_t y = 0U;
    std::uint64_t occupiedEnd = 0U;
    if (parent.containerLayout == UiContainerLayoutMode::HorizontalStack)
    {
        x = cursor + marginLeft;
        y = paddingTop + marginTop;
        const std::uint64_t endX = x + width + marginRight;
        const std::uint64_t endY = y + height + marginBottom;
        if (endX > contentRight || endY > contentBottom)
        {
            return false;
        }
        occupiedEnd = endX;
    }
    else
    {
        x = paddingLeft + marginLeft;
        y = cursor + marginTop;
        const std::uint64_t endX = x + width + marginRight;
        const std::uint64_t endY = y + height + marginBottom;
        if (endX > contentRight || endY > contentBottom)
        {
            return false;
        }
        occupiedEnd = endY;
    }

    if (x > std::numeric_limits<std::uint32_t>::max() ||
        y > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    resolved = UiRect{
        static_cast<std::uint32_t>(x),
        static_cast<std::uint32_t>(y),
        child.stackFixed.width,
        child.stackFixed.height,
    };
    cursor = occupiedEnd;
    hasPreviousStackItem = true;
    return true;
}
} // namespace trace2d::ui
