#include <trace2d/ui/UiLayout.hpp>

#include <algorithm>
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
    if (node.localBounds.width == 0U || node.localBounds.height == 0U)
    {
        return UiLayoutResult::InvalidBounds;
    }

    UiResolvedLayoutNode resolved{};
    resolved.id = std::move(node.id);
    resolved.parentId = std::move(node.parentId);
    resolved.localBounds = node.localBounds;
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
                if (!ContainsInCanvas(node.localBounds))
                {
                    return UiLayoutResult::InvalidBounds;
                }
                node.bounds = node.localBounds;
                node.depth = 0U;
            }
            else
            {
                const UiResolvedLayoutNode& parent = nodes_[node.parentIndex];
                if (!ContainsInParent(parent.bounds, node.localBounds))
                {
                    return UiLayoutResult::ChildOutsideParent;
                }

                node.bounds = UiRect{
                    parent.bounds.x + node.localBounds.x,
                    parent.bounds.y + node.localBounds.y,
                    node.localBounds.width,
                    node.localBounds.height,
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
} // namespace trace2d::ui
