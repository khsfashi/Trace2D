#include <trace2d/render/SpriteOrderMask2D.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace trace2d::render
{
namespace
{
struct SortingGroupValidationState final
{
    bool seen{false};
    std::int32_t layer{0};
    std::int32_t order{0};
    std::uint64_t stableOrder{0U};
};

[[nodiscard]] bool IsUngroupedMetadataCanonical(const SpriteSortingGroup2D& group) noexcept
{
    return group.layer == 0 && group.order == 0 && group.stableOrder == 0U;
}

[[nodiscard]] bool IsMaskValid(const SpriteMask2D& mask) noexcept
{
    switch (mask.mode)
    {
    case SpriteMaskMode::None:
        return mask.id == NoSpriteMaskId;
    case SpriteMaskMode::Write:
    case SpriteMaskMode::TestInside:
    case SpriteMaskMode::TestOutside:
        return mask.id >= MinSpriteMaskId && mask.id <= MaxSpriteMaskId;
    }
    return false;
}

[[nodiscard]] std::int32_t TopLayer(const SpriteOrder2D& order) noexcept
{
    return order.group.id == NoSpriteSortingGroupId ? order.layer : order.group.layer;
}

[[nodiscard]] std::int32_t TopOrder(const SpriteOrder2D& order) noexcept
{
    return order.group.id == NoSpriteSortingGroupId ? order.order : order.group.order;
}

[[nodiscard]] std::uint64_t TopStableOrder(const SpriteOrder2D& order) noexcept
{
    return order.group.id == NoSpriteSortingGroupId ? order.stableOrder : order.group.stableOrder;
}

[[nodiscard]] bool IsGrouped(const SpriteOrder2D& order) noexcept
{
    return order.group.id != NoSpriteSortingGroupId;
}
} // namespace

bool SpriteOrderMaskLess(
    const SpriteOrderMaskEntry2D& left,
    const SpriteOrderMaskEntry2D& right) noexcept
{
    const SpriteOrder2D& leftOrder = left.order;
    const SpriteOrder2D& rightOrder = right.order;

    const std::int32_t leftLayer = TopLayer(leftOrder);
    const std::int32_t rightLayer = TopLayer(rightOrder);
    if (leftLayer != rightLayer)
    {
        return leftLayer < rightLayer;
    }

    const std::int32_t leftTopOrder = TopOrder(leftOrder);
    const std::int32_t rightTopOrder = TopOrder(rightOrder);
    if (leftTopOrder != rightTopOrder)
    {
        return leftTopOrder < rightTopOrder;
    }

    const std::uint64_t leftTopStable = TopStableOrder(leftOrder);
    const std::uint64_t rightTopStable = TopStableOrder(rightOrder);
    if (leftTopStable != rightTopStable)
    {
        return leftTopStable < rightTopStable;
    }

    const bool leftGrouped = IsGrouped(leftOrder);
    const bool rightGrouped = IsGrouped(rightOrder);
    if (leftGrouped != rightGrouped)
    {
        // Exact top-level collisions remain deterministic and keep one group atomic. Ungrouped
        // units sort first; this is semantic tie policy, never resource-based reordering.
        return !leftGrouped;
    }

    if (leftGrouped)
    {
        if (leftOrder.group.id != rightOrder.group.id)
        {
            return leftOrder.group.id < rightOrder.group.id;
        }
        if (leftOrder.layer != rightOrder.layer)
        {
            return leftOrder.layer < rightOrder.layer;
        }
        if (leftOrder.order != rightOrder.order)
        {
            return leftOrder.order < rightOrder.order;
        }
        if (leftOrder.stableOrder != rightOrder.stableOrder)
        {
            return leftOrder.stableOrder < rightOrder.stableOrder;
        }
    }

    return left.sourceIndex < right.sourceIndex;
}

SpriteOrderMaskStatus ResolveSpriteOrderMask2D(
    const std::span<SpriteOrderMaskEntry2D> entries) noexcept
{
    std::array<SortingGroupValidationState, 256U> groups{};

    for (std::size_t index = 0U; index < entries.size(); ++index)
    {
        const SpriteOrderMaskEntry2D& entry = entries[index];
        if (index > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) ||
            entry.sourceIndex != static_cast<std::uint32_t>(index))
        {
            return SpriteOrderMaskStatus{
                SpriteOrderMaskError::InvalidSourceIndex,
                entry.sourceIndex,
                entry.order.group.id,
                entry.mask.id,
            };
        }
        if (entry.order.stableOrder == InvalidSpriteStableOrder)
        {
            return SpriteOrderMaskStatus{
                SpriteOrderMaskError::InvalidStableOrder,
                entry.sourceIndex,
                entry.order.group.id,
                entry.mask.id,
            };
        }

        const SpriteSortingGroup2D& group = entry.order.group;
        if (group.id == NoSpriteSortingGroupId)
        {
            if (!IsUngroupedMetadataCanonical(group))
            {
                return SpriteOrderMaskStatus{
                    SpriteOrderMaskError::InvalidSortingGroup,
                    entry.sourceIndex,
                    group.id,
                    entry.mask.id,
                };
            }
        }
        else
        {
            if (group.stableOrder == InvalidSpriteStableOrder)
            {
                return SpriteOrderMaskStatus{
                    SpriteOrderMaskError::InvalidStableOrder,
                    entry.sourceIndex,
                    group.id,
                    entry.mask.id,
                };
            }

            SortingGroupValidationState& state = groups[group.id];
            if (!state.seen)
            {
                state = SortingGroupValidationState{
                    true,
                    group.layer,
                    group.order,
                    group.stableOrder,
                };
            }
            else if (state.layer != group.layer || state.order != group.order ||
                     state.stableOrder != group.stableOrder)
            {
                return SpriteOrderMaskStatus{
                    SpriteOrderMaskError::InconsistentSortingGroup,
                    entry.sourceIndex,
                    group.id,
                    entry.mask.id,
                };
            }
        }

        if (!IsMaskValid(entry.mask))
        {
            return SpriteOrderMaskStatus{
                SpriteOrderMaskError::InvalidMask,
                entry.sourceIndex,
                group.id,
                entry.mask.id,
            };
        }
    }

    std::sort(entries.begin(), entries.end(), SpriteOrderMaskLess);

    std::array<bool, 256U> closedMasks{};
    std::uint8_t activeMask = NoSpriteMaskId;
    bool activeMaskHasTester = false;

    for (const SpriteOrderMaskEntry2D& entry : entries)
    {
        switch (entry.mask.mode)
        {
        case SpriteMaskMode::None:
            break;
        case SpriteMaskMode::Write:
            if (activeMask == entry.mask.id)
            {
                if (activeMaskHasTester)
                {
                    return SpriteOrderMaskStatus{
                        SpriteOrderMaskError::MaskWriterAfterTester,
                        entry.sourceIndex,
                        entry.order.group.id,
                        entry.mask.id,
                    };
                }
                break;
            }

            if (closedMasks[entry.mask.id])
            {
                return SpriteOrderMaskStatus{
                    SpriteOrderMaskError::MaskPhaseReentry,
                    entry.sourceIndex,
                    entry.order.group.id,
                    entry.mask.id,
                };
            }
            if (activeMask != NoSpriteMaskId)
            {
                closedMasks[activeMask] = true;
            }
            activeMask = entry.mask.id;
            activeMaskHasTester = false;
            break;
        case SpriteMaskMode::TestInside:
        case SpriteMaskMode::TestOutside:
            if (activeMask != entry.mask.id)
            {
                return SpriteOrderMaskStatus{
                    SpriteOrderMaskError::MaskTesterWithoutWriter,
                    entry.sourceIndex,
                    entry.order.group.id,
                    entry.mask.id,
                };
            }
            activeMaskHasTester = true;
            break;
        }
    }

    return SpriteOrderMaskStatus{};
}
} // namespace trace2d::render
