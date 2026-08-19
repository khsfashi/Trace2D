#include <trace2d/scene/TweenSequence2D.hpp>

#include <algorithm>

namespace trace2d::scene
{
namespace
{
[[nodiscard]] bool IsValidTimeDomain(const runtime::TweenTimeDomain2D domain) noexcept
{
    return domain == runtime::TweenTimeDomain2D::Simulation ||
        domain == runtime::TweenTimeDomain2D::Presentation;
}

[[nodiscard]] bool IsTerminal(const TweenSequencePlaybackState2D playback) noexcept
{
    return playback == TweenSequencePlaybackState2D::Completed ||
        playback == TweenSequencePlaybackState2D::Cancelled;
}

[[nodiscard]] TweenSequenceCancellationReason2D MapCancellation(
    const runtime::TweenCancellationReason2D reason) noexcept
{
    switch (reason)
    {
    case runtime::TweenCancellationReason2D::Replaced:
        return TweenSequenceCancellationReason2D::ChildReplaced;
    case runtime::TweenCancellationReason2D::TargetInvalidated:
        return TweenSequenceCancellationReason2D::TargetInvalidated;
    case runtime::TweenCancellationReason2D::PropertyWriteRejected:
        return TweenSequenceCancellationReason2D::PropertyWriteRejected;
    case runtime::TweenCancellationReason2D::Explicit:
    case runtime::TweenCancellationReason2D::None:
        return TweenSequenceCancellationReason2D::BindingFailure;
    }
    return TweenSequenceCancellationReason2D::BindingFailure;
}
} // namespace

TweenSequenceStatus2D TweenSequenceSystem2D::Step(
    const runtime::TweenTimeDomain2D domain,
    const runtime::TweenTime2D delta) noexcept
{
    if (!IsValidTimeDomain(domain))
    {
        return {TweenSequenceError2D::InvalidTimeDomain};
    }
    if (delta.count() < 0)
    {
        return {TweenSequenceError2D::NegativeDelta};
    }

    for (Slot& slot : slots_)
    {
        if (!slot.occupied || slot.domain != domain || IsTerminal(slot.playback))
        {
            continue;
        }
        const TweenSequenceStatus2D refreshed = RefreshChildState(slot);
        if (!refreshed.Succeeded())
        {
            return refreshed;
        }
    }

    runtime::TweenTime2D remaining = delta;
    while (remaining.count() > 0)
    {
        runtime::TweenTime2D segment = remaining;
        bool hasPlayingSequence = false;

        for (const Slot& slot : slots_)
        {
            if (!slot.occupied || slot.domain != domain ||
                slot.playback != TweenSequencePlaybackState2D::Playing)
            {
                continue;
            }
            hasPlayingSequence = true;

            if (slot.nextChildIndex < slot.children.size())
            {
                const runtime::TweenTime2D childOffset = slot.children[slot.nextChildIndex].offset;
                if (childOffset > slot.elapsed)
                {
                    segment = std::min(segment, childOffset - slot.elapsed);
                }
            }
            if (slot.duration > slot.elapsed)
            {
                segment = std::min(segment, slot.duration - slot.elapsed);
            }
        }

        if (!hasPlayingSequence)
        {
            return WrapBinding(bindings_.Step(domain, remaining));
        }

        if (segment.count() <= 0)
        {
            bool progressed = false;
            for (Slot& slot : slots_)
            {
                if (!slot.occupied || slot.domain != domain ||
                    slot.playback != TweenSequencePlaybackState2D::Playing)
                {
                    continue;
                }
                const std::size_t priorChildIndex = slot.nextChildIndex;
                const TweenSequenceStatus2D activation = ActivateDueChildren(slot);
                if (!activation.Succeeded())
                {
                    return activation;
                }
                progressed = progressed || slot.nextChildIndex != priorChildIndex;
                if (slot.playback == TweenSequencePlaybackState2D::Playing &&
                    slot.elapsed >= slot.duration)
                {
                    Complete(slot);
                    progressed = true;
                }
            }
            if (!progressed)
            {
                segment = remaining;
            }
            else
            {
                continue;
            }
        }

        const TweenBindingStatus2D step = bindings_.Step(domain, segment);
        if (!step.Succeeded())
        {
            return WrapBinding(step);
        }

        for (Slot& slot : slots_)
        {
            if (!slot.occupied || slot.domain != domain ||
                slot.playback != TweenSequencePlaybackState2D::Playing)
            {
                continue;
            }
            slot.elapsed += segment;
        }
        remaining -= segment;

        for (Slot& slot : slots_)
        {
            if (!slot.occupied || slot.domain != domain ||
                slot.playback != TweenSequencePlaybackState2D::Playing)
            {
                continue;
            }
            const TweenSequenceStatus2D refreshed = RefreshChildState(slot);
            if (!refreshed.Succeeded())
            {
                return refreshed;
            }
            if (slot.playback != TweenSequencePlaybackState2D::Playing)
            {
                continue;
            }
            const TweenSequenceStatus2D activation = ActivateDueChildren(slot);
            if (!activation.Succeeded())
            {
                return activation;
            }
            if (slot.playback == TweenSequencePlaybackState2D::Playing &&
                slot.elapsed >= slot.duration)
            {
                const TweenSequenceStatus2D finalRefresh = RefreshChildState(slot);
                if (!finalRefresh.Succeeded())
                {
                    return finalRefresh;
                }
                if (slot.playback == TweenSequencePlaybackState2D::Playing)
                {
                    Complete(slot);
                }
            }
        }
    }

    if (delta.count() == 0)
    {
        const TweenBindingStatus2D step = bindings_.Step(domain, delta);
        if (!step.Succeeded())
        {
            return WrapBinding(step);
        }
    }
    return {};
}

TweenSequenceStatus2D TweenSequenceSystem2D::ActivateDueChildren(Slot& slot) noexcept
{
    while (slot.nextChildIndex < slot.children.size() &&
           slot.children[slot.nextChildIndex].offset <= slot.elapsed)
    {
        const std::size_t index = slot.nextChildIndex;
        const TweenSequenceChildDefinition2D& child = slot.children[index];
        runtime::TweenHandle2D handle{};
        const TweenBindingStatus2D create = bindings_.Create(child.binding, child.bindingSpec, handle);
        if (!create.Succeeded())
        {
            (void)CancelActiveChildren(slot);
            slot.playback = TweenSequencePlaybackState2D::Cancelled;
            slot.cancellationReason = TweenSequenceCancellationReason2D::BindingFailure;
            DecrementActive();
            return WrapBinding(create);
        }
        slot.childHandles[index] = handle;
        ++slot.nextChildIndex;
    }
    return {};
}

TweenSequenceStatus2D TweenSequenceSystem2D::RefreshChildState(Slot& slot) noexcept
{
    for (std::size_t index = 0U; index < slot.nextChildIndex; ++index)
    {
        runtime::TweenHandle2D& childHandle = slot.childHandles[index];
        if (!childHandle.Valid())
        {
            continue;
        }
        runtime::TweenState2D childState{};
        const TweenBindingStatus2D inspect = bindings_.Inspect(childHandle, childState);
        if (!inspect.Succeeded())
        {
            (void)CancelActiveChildren(slot);
            if (!IsTerminal(slot.playback))
            {
                slot.playback = TweenSequencePlaybackState2D::Cancelled;
                slot.cancellationReason = TweenSequenceCancellationReason2D::BindingFailure;
                DecrementActive();
            }
            return WrapBinding(inspect);
        }
        if (childState.playback == runtime::TweenPlaybackState2D::Completed)
        {
            childHandle = runtime::TweenHandle2D{};
            continue;
        }
        if (childState.playback == runtime::TweenPlaybackState2D::Cancelled)
        {
            return CancelForChild(slot, childState.cancellationReason);
        }
    }
    return {};
}

TweenSequenceStatus2D TweenSequenceSystem2D::CancelActiveChildren(Slot& slot) noexcept
{
    for (const runtime::TweenHandle2D childHandle : slot.childHandles)
    {
        if (!childHandle.Valid())
        {
            continue;
        }
        runtime::TweenState2D childState{};
        const TweenBindingStatus2D inspect = bindings_.Inspect(childHandle, childState);
        if (!inspect.Succeeded())
        {
            return WrapBinding(inspect);
        }
        if (childState.playback == runtime::TweenPlaybackState2D::Playing ||
            childState.playback == runtime::TweenPlaybackState2D::Paused)
        {
            const TweenBindingStatus2D cancel = bindings_.Cancel(childHandle);
            if (!cancel.Succeeded())
            {
                return WrapBinding(cancel);
            }
        }
    }
    return {};
}

TweenSequenceStatus2D TweenSequenceSystem2D::CancelForChild(
    Slot& slot,
    const runtime::TweenCancellationReason2D reason) noexcept
{
    if (IsTerminal(slot.playback))
    {
        return {};
    }
    const TweenSequenceStatus2D cancellation = CancelActiveChildren(slot);
    if (!cancellation.Succeeded())
    {
        return cancellation;
    }
    slot.playback = TweenSequencePlaybackState2D::Cancelled;
    slot.cancellationReason = MapCancellation(reason);
    DecrementActive();
    return {};
}

TweenSequenceStatus2D TweenSequenceSystem2D::WrapBinding(
    const TweenBindingStatus2D status) const noexcept
{
    if (status.Succeeded())
    {
        return {};
    }
    return {
        TweenSequenceError2D::BindingFailure,
        status.error,
        status.tweenError,
    };
}
} // namespace trace2d::scene
