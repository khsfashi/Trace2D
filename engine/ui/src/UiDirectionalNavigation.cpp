#include <trace2d/ui/Ui.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

namespace trace2d::ui
{
namespace
{
struct DirectionalScore final
{
    bool outsideCone{true};
    std::uint64_t distanceSquared{0U};
    std::uint64_t secondary{0U};
    std::uint64_t primary{0U};
    std::size_t authoredIndex{0U};
};

[[nodiscard]] std::int64_t Center2X(const UiRect& bounds) noexcept
{
    return static_cast<std::int64_t>(bounds.x) * 2 + static_cast<std::int64_t>(bounds.width);
}

[[nodiscard]] std::int64_t Center2Y(const UiRect& bounds) noexcept
{
    return static_cast<std::int64_t>(bounds.y) * 2 + static_cast<std::int64_t>(bounds.height);
}

[[nodiscard]] std::uint64_t Magnitude(const std::int64_t value) noexcept
{
    return static_cast<std::uint64_t>(value < 0 ? -value : value);
}

[[nodiscard]] bool IsBetter(const DirectionalScore& candidate, const DirectionalScore& best) noexcept
{
    if (candidate.outsideCone != best.outsideCone)
    {
        return !candidate.outsideCone;
    }
    if (candidate.distanceSquared != best.distanceSquared)
    {
        return candidate.distanceSquared < best.distanceSquared;
    }
    if (candidate.secondary != best.secondary)
    {
        return candidate.secondary < best.secondary;
    }
    if (candidate.primary != best.primary)
    {
        return candidate.primary < best.primary;
    }
    return candidate.authoredIndex < best.authoredIndex;
}
} // namespace

UiActionResult UiDocument::FocusDirectional(const UiNavigationDirection direction) noexcept
{
    if (focusedIndex_ >= elements_.size())
    {
        focusedIndex_ = InvalidUiElementIndex;
        return UiActionResult::NotFocused;
    }

    const UiElement& focused = elements_[focusedIndex_];
    if (!IsInteractionAllowed(focusedIndex_) || !focused.visible || !focused.enabled ||
        !IsFocusable(focused.kind))
    {
        ClearFocus();
        return UiActionResult::NotFocused;
    }

    const std::int64_t originX = Center2X(focused.bounds);
    const std::int64_t originY = Center2Y(focused.bounds);

    std::size_t bestIndex = InvalidUiElementIndex;
    DirectionalScore best{};
    best.distanceSquared = std::numeric_limits<std::uint64_t>::max();
    best.secondary = std::numeric_limits<std::uint64_t>::max();
    best.primary = std::numeric_limits<std::uint64_t>::max();
    best.authoredIndex = std::numeric_limits<std::size_t>::max();

    for (std::size_t index = 0U; index < elements_.size(); ++index)
    {
        if (index == focusedIndex_)
        {
            continue;
        }

        const UiElement& candidate = elements_[index];
        if (!IsInteractionAllowed(index) || !candidate.visible || !candidate.enabled ||
            !IsFocusable(candidate.kind))
        {
            continue;
        }

        const std::int64_t deltaX = Center2X(candidate.bounds) - originX;
        const std::int64_t deltaY = Center2Y(candidate.bounds) - originY;

        std::int64_t signedPrimary = 0;
        std::int64_t signedSecondary = 0;
        switch (direction)
        {
        case UiNavigationDirection::Left:
            signedPrimary = -deltaX;
            signedSecondary = deltaY;
            break;
        case UiNavigationDirection::Right:
            signedPrimary = deltaX;
            signedSecondary = deltaY;
            break;
        case UiNavigationDirection::Up:
            signedPrimary = -deltaY;
            signedSecondary = deltaX;
            break;
        case UiNavigationDirection::Down:
            signedPrimary = deltaY;
            signedSecondary = deltaX;
            break;
        }

        if (signedPrimary <= 0)
        {
            continue;
        }

        const std::uint64_t primary = static_cast<std::uint64_t>(signedPrimary);
        const std::uint64_t secondary = Magnitude(signedSecondary);
        const std::uint64_t distanceSquared = primary * primary + secondary * secondary;
        const DirectionalScore score{
            .outsideCone = secondary > primary,
            .distanceSquared = distanceSquared,
            .secondary = secondary,
            .primary = primary,
            .authoredIndex = index,
        };

        if (bestIndex == InvalidUiElementIndex || IsBetter(score, best))
        {
            bestIndex = index;
            best = score;
        }
    }

    if (bestIndex == InvalidUiElementIndex)
    {
        return UiActionResult::NotFocusable;
    }

    return FocusIndex(bestIndex);
}
} // namespace trace2d::ui
