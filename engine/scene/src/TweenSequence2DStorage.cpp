#include <trace2d/scene/TweenSequence2D.hpp>

#include <algorithm>

namespace trace2d::scene
{
TweenSequenceMetrics2D TweenSequenceSystem2D::Metrics() const noexcept
{
    std::uint64_t retainedChildren = 0U;
    std::uint64_t retainedChildCapacity = 0U;
    for (const Slot& slot : slots_)
    {
        retainedChildren += static_cast<std::uint64_t>(slot.children.size());
        retainedChildCapacity += static_cast<std::uint64_t>(slot.children.capacity());
    }
    return TweenSequenceMetrics2D{
        activeSequenceCount_,
        static_cast<std::uint64_t>(slots_.size()),
        static_cast<std::uint64_t>(slots_.capacity()),
        retainedChildren,
        retainedChildCapacity,
        highWaterActiveSequenceCount_,
        createdSequenceCount_,
        reusedSequenceSlotCount_,
    };
}

TweenSequenceSystem2D::Slot* TweenSequenceSystem2D::ResolveMutable(
    const TweenSequenceHandle2D handle) noexcept
{
    if (!handle.Valid() || static_cast<std::size_t>(handle.index) >= slots_.size())
    {
        return nullptr;
    }
    Slot& slot = slots_[handle.index];
    if (!slot.occupied || slot.generation != handle.generation)
    {
        return nullptr;
    }
    return &slot;
}

const TweenSequenceSystem2D::Slot* TweenSequenceSystem2D::Resolve(
    const TweenSequenceHandle2D handle) const noexcept
{
    if (!handle.Valid() || static_cast<std::size_t>(handle.index) >= slots_.size())
    {
        return nullptr;
    }
    const Slot& slot = slots_[handle.index];
    if (!slot.occupied || slot.generation != handle.generation)
    {
        return nullptr;
    }
    return &slot;
}

void TweenSequenceSystem2D::Complete(Slot& slot) noexcept
{
    if (slot.playback != TweenSequencePlaybackState2D::Playing)
    {
        return;
    }
    slot.elapsed = slot.duration;
    slot.playback = TweenSequencePlaybackState2D::Completed;
    slot.cancellationReason = TweenSequenceCancellationReason2D::None;
    DecrementActive();
}

void TweenSequenceSystem2D::IncrementActive() noexcept
{
    ++activeSequenceCount_;
    highWaterActiveSequenceCount_ = std::max(highWaterActiveSequenceCount_, activeSequenceCount_);
}

void TweenSequenceSystem2D::DecrementActive() noexcept
{
    if (activeSequenceCount_ > 0U)
    {
        --activeSequenceCount_;
    }
}
} // namespace trace2d::scene
