#include <trace2d/scene/TweenSequence2D.hpp>

#include <algorithm>
#include <limits>

namespace trace2d::scene
{
namespace
{
[[nodiscard]] bool IsTerminal(const TweenSequencePlaybackState2D playback) noexcept
{
    return playback == TweenSequencePlaybackState2D::Completed ||
        playback == TweenSequencePlaybackState2D::Cancelled;
}
} // namespace

TweenSequenceSystem2D::TweenSequenceSystem2D(TweenBindingSystem2D& bindings) noexcept
    : bindings_{bindings}
{
}

void TweenSequenceSystem2D::Reserve(
    const std::size_t sequenceCapacity,
    const std::size_t childCapacityPerSequence)
{
    slots_.reserve(sequenceCapacity);
    reservedChildCapacityPerSequence_ = childCapacityPerSequence;
}

TweenSequenceStatus2D TweenSequenceSystem2D::Create(
    const TweenSequenceDefinition2D& definition,
    TweenSequenceHandle2D& outHandle)
{
    const TweenSequenceStatus2D validation = ValidateDefinition(definition);
    if (!validation.Succeeded())
    {
        return validation;
    }

    std::size_t slotIndex = slots_.size();
    bool reused = false;
    for (std::size_t index = 0U; index < slots_.size(); ++index)
    {
        Slot& candidate = slots_[index];
        if (!candidate.occupied || !IsTerminal(candidate.playback) ||
            candidate.generation == std::numeric_limits<std::uint64_t>::max())
        {
            continue;
        }
        slotIndex = index;
        reused = true;
        break;
    }

    if (slotIndex >= static_cast<std::size_t>(TweenSequenceHandle2D::InvalidIndex))
    {
        return {TweenSequenceError2D::SlotCapacityExceeded};
    }

    if (!reused)
    {
        slots_.push_back(Slot{});
    }
    Slot& slot = slots_[slotIndex];
    if (reused)
    {
        ++slot.generation;
        ++reusedSequenceSlotCount_;
    }
    else
    {
        slot.generation = 1U;
    }

    slot.domain = definition.domain_;
    slot.duration = definition.duration_;
    slot.elapsed = runtime::TweenTime2D{0};
    slot.playback = TweenSequencePlaybackState2D::Playing;
    slot.cancellationReason = TweenSequenceCancellationReason2D::None;
    slot.nextChildIndex = 0U;
    slot.occupied = true;

    slot.children.clear();
    if (slot.children.capacity() < definition.children_.size())
    {
        slot.children.reserve(std::max(definition.children_.size(), reservedChildCapacityPerSequence_));
    }
    slot.children.insert(slot.children.end(), definition.children_.begin(), definition.children_.end());
    std::sort(
        slot.children.begin(),
        slot.children.end(),
        [](const TweenSequenceChildDefinition2D& left, const TweenSequenceChildDefinition2D& right)
        {
            if (left.offset != right.offset)
            {
                return left.offset < right.offset;
            }
            return left.authorOrder < right.authorOrder;
        });

    slot.childHandles.clear();
    if (slot.childHandles.capacity() < slot.children.size())
    {
        slot.childHandles.reserve(std::max(slot.children.size(), reservedChildCapacityPerSequence_));
    }
    slot.childHandles.resize(slot.children.size());

    ++createdSequenceCount_;
    IncrementActive();
    outHandle = TweenSequenceHandle2D{static_cast<std::uint32_t>(slotIndex), slot.generation};

    const TweenSequenceStatus2D activation = ActivateDueChildren(slot);
    if (!activation.Succeeded())
    {
        return activation;
    }
    return {};
}

TweenSequenceStatus2D TweenSequenceSystem2D::Inspect(
    const TweenSequenceHandle2D handle,
    TweenSequenceState2D& outState) const noexcept
{
    const Slot* const slot = Resolve(handle);
    if (slot == nullptr)
    {
        return {TweenSequenceError2D::InvalidHandle};
    }

    std::uint64_t activeChildren = 0U;
    std::uint64_t completedChildren = 0U;
    for (std::size_t index = 0U; index < slot->nextChildIndex; ++index)
    {
        const runtime::TweenHandle2D childHandle = slot->childHandles[index];
        if (!childHandle.Valid())
        {
            ++completedChildren;
            continue;
        }
        runtime::TweenState2D childState{};
        if (!bindings_.Inspect(childHandle, childState).Succeeded())
        {
            continue;
        }
        if (childState.playback == runtime::TweenPlaybackState2D::Playing ||
            childState.playback == runtime::TweenPlaybackState2D::Paused)
        {
            ++activeChildren;
        }
        else if (childState.playback == runtime::TweenPlaybackState2D::Completed)
        {
            ++completedChildren;
        }
    }

    outState = TweenSequenceState2D{
        slot->domain,
        slot->playback,
        slot->cancellationReason,
        slot->elapsed,
        slot->duration,
        activeChildren,
        completedChildren,
        static_cast<std::uint64_t>(slot->children.size()),
    };
    return {};
}

TweenSequenceStatus2D TweenSequenceSystem2D::Pause(const TweenSequenceHandle2D handle) noexcept
{
    Slot* const slot = ResolveMutable(handle);
    if (slot == nullptr)
    {
        return {TweenSequenceError2D::InvalidHandle};
    }
    if (slot->playback != TweenSequencePlaybackState2D::Playing)
    {
        return {TweenSequenceError2D::InvalidPlaybackTransition};
    }

    const TweenSequenceStatus2D refreshed = RefreshChildState(*slot);
    if (!refreshed.Succeeded())
    {
        return refreshed;
    }
    if (slot->playback != TweenSequencePlaybackState2D::Playing)
    {
        return {TweenSequenceError2D::InvalidPlaybackTransition};
    }

    for (const runtime::TweenHandle2D childHandle : slot->childHandles)
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
        if (childState.playback == runtime::TweenPlaybackState2D::Playing)
        {
            const TweenBindingStatus2D pause = bindings_.Pause(childHandle);
            if (!pause.Succeeded())
            {
                return WrapBinding(pause);
            }
        }
    }
    slot->playback = TweenSequencePlaybackState2D::Paused;
    return {};
}

TweenSequenceStatus2D TweenSequenceSystem2D::Resume(const TweenSequenceHandle2D handle) noexcept
{
    Slot* const slot = ResolveMutable(handle);
    if (slot == nullptr)
    {
        return {TweenSequenceError2D::InvalidHandle};
    }
    if (slot->playback != TweenSequencePlaybackState2D::Paused)
    {
        return {TweenSequenceError2D::InvalidPlaybackTransition};
    }

    const TweenSequenceStatus2D refreshed = RefreshChildState(*slot);
    if (!refreshed.Succeeded())
    {
        return refreshed;
    }
    if (slot->playback != TweenSequencePlaybackState2D::Paused)
    {
        return {TweenSequenceError2D::InvalidPlaybackTransition};
    }

    for (const runtime::TweenHandle2D childHandle : slot->childHandles)
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
        if (childState.playback == runtime::TweenPlaybackState2D::Paused)
        {
            const TweenBindingStatus2D resume = bindings_.Resume(childHandle);
            if (!resume.Succeeded())
            {
                return WrapBinding(resume);
            }
        }
    }
    slot->playback = TweenSequencePlaybackState2D::Playing;
    return {};
}

TweenSequenceStatus2D TweenSequenceSystem2D::Restart(const TweenSequenceHandle2D handle) noexcept
{
    Slot* const slot = ResolveMutable(handle);
    if (slot == nullptr)
    {
        return {TweenSequenceError2D::InvalidHandle};
    }

    const bool wasTerminal = IsTerminal(slot->playback);
    const TweenSequenceStatus2D cancellation = CancelActiveChildren(*slot);
    if (!cancellation.Succeeded())
    {
        return cancellation;
    }

    std::fill(slot->childHandles.begin(), slot->childHandles.end(), runtime::TweenHandle2D{});
    slot->elapsed = runtime::TweenTime2D{0};
    slot->playback = TweenSequencePlaybackState2D::Playing;
    slot->cancellationReason = TweenSequenceCancellationReason2D::None;
    slot->nextChildIndex = 0U;
    if (wasTerminal)
    {
        IncrementActive();
    }
    return ActivateDueChildren(*slot);
}

TweenSequenceStatus2D TweenSequenceSystem2D::Cancel(const TweenSequenceHandle2D handle) noexcept
{
    Slot* const slot = ResolveMutable(handle);
    if (slot == nullptr)
    {
        return {TweenSequenceError2D::InvalidHandle};
    }
    if (IsTerminal(slot->playback))
    {
        return {TweenSequenceError2D::InvalidPlaybackTransition};
    }

    const TweenSequenceStatus2D cancellation = CancelActiveChildren(*slot);
    if (!cancellation.Succeeded())
    {
        return cancellation;
    }
    slot->playback = TweenSequencePlaybackState2D::Cancelled;
    slot->cancellationReason = TweenSequenceCancellationReason2D::Explicit;
    DecrementActive();
    return {};
}
} // namespace trace2d::scene
