#include <trace2d/scene/TweenSequence2D.hpp>

#include <algorithm>
#include <limits>

namespace trace2d::scene
{
namespace
{
[[nodiscard]] bool IsValidTimeDomain(const runtime::TweenTimeDomain2D domain) noexcept
{
    return domain == runtime::TweenTimeDomain2D::Simulation ||
        domain == runtime::TweenTimeDomain2D::Presentation;
}

[[nodiscard]] bool IsValidStartMode(const TweenStartMode2D mode) noexcept
{
    return mode == TweenStartMode2D::Explicit || mode == TweenStartMode2D::CaptureCurrent;
}

[[nodiscard]] bool IsValidEndMode(const TweenEndMode2D mode) noexcept
{
    return mode == TweenEndMode2D::Absolute || mode == TweenEndMode2D::Relative;
}

[[nodiscard]] bool IsValidConflictPolicy(const TweenConflictPolicy2D policy) noexcept
{
    return policy == TweenConflictPolicy2D::Reject || policy == TweenConflictPolicy2D::Replace;
}

[[nodiscard]] runtime::TweenValue2D ZeroValue(const runtime::TweenValueType2D type) noexcept
{
    switch (type)
    {
    case runtime::TweenValueType2D::Float:
        return runtime::TweenValue2D::Float(0.0F);
    case runtime::TweenValueType2D::Float2:
        return runtime::TweenValue2D::Float2(0.0F, 0.0F);
    case runtime::TweenValueType2D::Color:
        return runtime::TweenValue2D::Color(0.0F, 0.0F, 0.0F, 0.0F);
    }
    return runtime::TweenValue2D{};
}

[[nodiscard]] bool TryAddTime(
    const runtime::TweenTime2D lhs,
    const runtime::TweenTime2D rhs,
    runtime::TweenTime2D& out) noexcept
{
    if (lhs.count() < 0 || rhs.count() < 0)
    {
        return false;
    }
    using Rep = runtime::TweenTime2D::rep;
    const Rep maximum = std::numeric_limits<Rep>::max();
    if (lhs.count() > maximum - rhs.count())
    {
        return false;
    }
    out = runtime::TweenTime2D{lhs.count() + rhs.count()};
    return true;
}

[[nodiscard]] TweenSequenceStatus2D ValidateChildSpec(
    const runtime::TweenTimeDomain2D sequenceDomain,
    const ResolvedTweenBinding2D& binding,
    const TweenBindingSpec2D& spec,
    runtime::TweenTime2D& outSpan) noexcept
{
    if (!IsValidTimeDomain(sequenceDomain) || !IsValidTimeDomain(spec.tween.domain))
    {
        return {TweenSequenceError2D::InvalidTimeDomain};
    }
    if (spec.tween.domain != sequenceDomain)
    {
        return {TweenSequenceError2D::MixedTimeDomain};
    }
    if (!IsValidStartMode(spec.startMode) || !IsValidEndMode(spec.endMode) ||
        !IsValidConflictPolicy(spec.conflictPolicy))
    {
        return {TweenSequenceError2D::InvalidAuthoringMode};
    }
    if (spec.tween.infinite)
    {
        return {TweenSequenceError2D::InfiniteChildUnsupported};
    }
    if (spec.tween.end.type != binding.valueType ||
        (spec.startMode == TweenStartMode2D::Explicit && spec.tween.start.type != binding.valueType))
    {
        return {TweenSequenceError2D::ValueTypeMismatch};
    }

    runtime::TweenSpec2D prepared = spec.tween;
    if (spec.startMode == TweenStartMode2D::CaptureCurrent)
    {
        prepared.start = ZeroValue(binding.valueType);
    }
    const runtime::Tween2DStatus validation = runtime::ValidateTweenSpec2D(prepared);
    if (!validation.Succeeded())
    {
        return {
            TweenSequenceError2D::InvalidTweenSpec,
            TweenBindingError2D::None,
            validation.error,
        };
    }

    using Rep = runtime::TweenTime2D::rep;
    const Rep maximum = std::numeric_limits<Rep>::max();
    const std::uint64_t loopCount = static_cast<std::uint64_t>(spec.tween.repeatCount) + 1U;
    const auto durationCount = spec.tween.duration.count();
    if (durationCount > 0 &&
        loopCount > static_cast<std::uint64_t>(maximum / durationCount))
    {
        return {TweenSequenceError2D::TimeOverflow};
    }
    const Rep activeCount = durationCount * static_cast<Rep>(loopCount);
    if (spec.tween.delay.count() > maximum - activeCount)
    {
        return {TweenSequenceError2D::TimeOverflow};
    }
    outSpan = runtime::TweenTime2D{spec.tween.delay.count() + activeCount};
    return {};
}

[[nodiscard]] bool IntervalsOverlap(
    const runtime::TweenTime2D leftStart,
    const runtime::TweenTime2D leftEnd,
    const runtime::TweenTime2D rightStart,
    const runtime::TweenTime2D rightEnd) noexcept
{
    return leftStart < rightEnd && rightStart < leftEnd;
}
} // namespace

TweenSequenceDefinition2D::TweenSequenceDefinition2D(
    const runtime::TweenTimeDomain2D domain) noexcept
    : domain_{domain}
{
}

TweenSequenceStatus2D TweenSequenceDefinition2D::Append(
    const ResolvedTweenBinding2D& binding,
    const TweenBindingSpec2D& spec)
{
    const runtime::TweenTime2D offset = duration_;
    const TweenSequenceStatus2D status = AddChild(offset, binding, spec);
    if (status.Succeeded())
    {
        joinAnchor_ = offset;
    }
    return status;
}

TweenSequenceStatus2D TweenSequenceDefinition2D::Join(
    const ResolvedTweenBinding2D& binding,
    const TweenBindingSpec2D& spec)
{
    return AddChild(joinAnchor_, binding, spec);
}

TweenSequenceStatus2D TweenSequenceDefinition2D::Insert(
    const runtime::TweenTime2D offset,
    const ResolvedTweenBinding2D& binding,
    const TweenBindingSpec2D& spec)
{
    if (offset.count() < 0)
    {
        return {TweenSequenceError2D::NegativeOffset};
    }
    const TweenSequenceStatus2D status = AddChild(offset, binding, spec);
    if (status.Succeeded())
    {
        joinAnchor_ = offset;
    }
    return status;
}

TweenSequenceStatus2D TweenSequenceDefinition2D::Interval(
    const runtime::TweenTime2D duration) noexcept
{
    if (duration.count() < 0)
    {
        return {TweenSequenceError2D::NegativeInterval};
    }
    runtime::TweenTime2D next{};
    if (!TryAddTime(duration_, duration, next))
    {
        return {TweenSequenceError2D::TimeOverflow};
    }
    duration_ = next;
    joinAnchor_ = duration_;
    return {};
}

TweenSequenceStatus2D TweenSequenceDefinition2D::AddChild(
    const runtime::TweenTime2D offset,
    const ResolvedTweenBinding2D& binding,
    const TweenBindingSpec2D& spec)
{
    if (offset.count() < 0)
    {
        return {TweenSequenceError2D::NegativeOffset};
    }

    runtime::TweenTime2D span{};
    const TweenSequenceStatus2D validation = ValidateChildSpec(domain_, binding, spec, span);
    if (!validation.Succeeded())
    {
        return validation;
    }

    runtime::TweenTime2D end{};
    if (!TryAddTime(offset, span, end))
    {
        return {TweenSequenceError2D::TimeOverflow};
    }

    children_.push_back(TweenSequenceChildDefinition2D{
        offset,
        binding,
        spec,
        nextAuthorOrder_++,
    });
    if (end > duration_)
    {
        duration_ = end;
    }
    return {};
}

TweenSequenceStatus2D TweenSequenceSystem2D::ValidateDefinition(
    const TweenSequenceDefinition2D& definition) const noexcept
{
    if (!IsValidTimeDomain(definition.domain_))
    {
        return {TweenSequenceError2D::InvalidTimeDomain};
    }
    if (definition.duration_.count() <= 0)
    {
        return {TweenSequenceError2D::EmptyDefinition};
    }

    for (std::size_t leftIndex = 0U; leftIndex < definition.children_.size(); ++leftIndex)
    {
        const auto& left = definition.children_[leftIndex];
        runtime::TweenTime2D leftSpan{};
        const TweenSequenceStatus2D leftStatus =
            ValidateChildSpec(definition.domain_, left.binding, left.bindingSpec, leftSpan);
        if (!leftStatus.Succeeded())
        {
            return leftStatus;
        }
        runtime::TweenTime2D leftEnd{};
        if (!TryAddTime(left.offset, leftSpan, leftEnd))
        {
            return {TweenSequenceError2D::TimeOverflow};
        }

        for (std::size_t rightIndex = leftIndex + 1U;
             rightIndex < definition.children_.size();
             ++rightIndex)
        {
            const auto& right = definition.children_[rightIndex];
            if (!(left.binding == right.binding))
            {
                continue;
            }
            runtime::TweenTime2D rightSpan{};
            const TweenSequenceStatus2D rightStatus =
                ValidateChildSpec(definition.domain_, right.binding, right.bindingSpec, rightSpan);
            if (!rightStatus.Succeeded())
            {
                return rightStatus;
            }
            runtime::TweenTime2D rightEnd{};
            if (!TryAddTime(right.offset, rightSpan, rightEnd))
            {
                return {TweenSequenceError2D::TimeOverflow};
            }
            if (IntervalsOverlap(left.offset, leftEnd, right.offset, rightEnd))
            {
                return {TweenSequenceError2D::ChildConflict};
            }
        }
    }
    return {};
}
} // namespace trace2d::scene
