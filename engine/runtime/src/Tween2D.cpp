#include <trace2d/runtime/Tween2D.hpp>

#include <algorithm>
#include <limits>

namespace trace2d::runtime
{
namespace
{
[[nodiscard]] bool IsValidTimeDomain(const TweenTimeDomain2D domain) noexcept
{
    return domain == TweenTimeDomain2D::Simulation ||
        domain == TweenTimeDomain2D::Presentation;
}

[[nodiscard]] bool IsValidValueType(const TweenValueType2D type) noexcept
{
    return type == TweenValueType2D::Float || type == TweenValueType2D::Float2 ||
        type == TweenValueType2D::Color;
}

[[nodiscard]] bool IsValidEasing(const TweenEasing2D easing) noexcept
{
    return easing >= TweenEasing2D::Linear && easing <= TweenEasing2D::EaseInOutCubic;
}

[[nodiscard]] bool IsValidLoopMode(const TweenLoopMode2D mode) noexcept
{
    return mode == TweenLoopMode2D::Restart || mode == TweenLoopMode2D::Yoyo;
}

[[nodiscard]] bool IsValidCancellationReason(const TweenCancellationReason2D reason) noexcept
{
    return reason == TweenCancellationReason2D::Explicit ||
        reason == TweenCancellationReason2D::Replaced ||
        reason == TweenCancellationReason2D::TargetInvalidated ||
        reason == TweenCancellationReason2D::PropertyWriteRejected;
}

[[nodiscard]] std::uint64_t UnsignedCount(const TweenTime2D value) noexcept
{
    return static_cast<std::uint64_t>(value.count());
}

[[nodiscard]] bool IsTerminal(const TweenPlaybackState2D state) noexcept
{
    return state == TweenPlaybackState2D::Completed || state == TweenPlaybackState2D::Cancelled;
}

[[nodiscard]] bool IsReverseLoop(const TweenSpec2D& spec, const std::uint64_t loopIndex) noexcept
{
    return spec.loopMode == TweenLoopMode2D::Yoyo && (loopIndex & 1U) != 0U;
}

[[nodiscard]] TweenValue2D SampleValue(
    const TweenSpec2D& spec,
    const std::uint64_t loopIndex,
    const TweenTime2D loopElapsed) noexcept
{
    const double progress = static_cast<double>(loopElapsed.count()) /
        static_cast<double>(spec.duration.count());
    const double eased = EvaluateTweenEasing2D(spec.easing, progress);
    if (IsReverseLoop(spec, loopIndex))
    {
        return InterpolateTweenValue2D(spec.end, spec.start, eased);
    }
    return InterpolateTweenValue2D(spec.start, spec.end, eased);
}

[[nodiscard]] TweenValue2D CompletedValue(const TweenSpec2D& spec) noexcept
{
    const std::uint64_t finalLoop = static_cast<std::uint64_t>(spec.repeatCount);
    return IsReverseLoop(spec, finalLoop) ? spec.start : spec.end;
}
} // namespace

TweenValue2D TweenValue2D::Float(const float value) noexcept
{
    return TweenValue2D{TweenValueType2D::Float, {value, 0.0F, 0.0F, 0.0F}};
}

TweenValue2D TweenValue2D::Float2(const float x, const float y) noexcept
{
    return TweenValue2D{TweenValueType2D::Float2, {x, y, 0.0F, 0.0F}};
}

TweenValue2D TweenValue2D::Color(
    const float r,
    const float g,
    const float b,
    const float a) noexcept
{
    return TweenValue2D{TweenValueType2D::Color, {r, g, b, a}};
}

Tween2DStatus ValidateTweenSpec2D(const TweenSpec2D& spec) noexcept
{
    if (!IsValidTimeDomain(spec.domain))
    {
        return {Tween2DError::InvalidTimeDomain};
    }
    if (!IsValidValueType(spec.start.type) || !IsValidValueType(spec.end.type))
    {
        return {Tween2DError::InvalidValueType};
    }
    if (spec.start.type != spec.end.type)
    {
        return {Tween2DError::ValueTypeMismatch};
    }
    if (!IsValidEasing(spec.easing))
    {
        return {Tween2DError::InvalidEasing};
    }
    if (!IsValidLoopMode(spec.loopMode))
    {
        return {Tween2DError::InvalidLoopMode};
    }
    if (spec.delay.count() < 0)
    {
        return {Tween2DError::NegativeDelay};
    }
    if (spec.duration.count() <= 0)
    {
        return {Tween2DError::NonPositiveDuration};
    }
    return {};
}

double EvaluateTweenEasing2D(const TweenEasing2D easing, const double progress) noexcept
{
    const double t = std::clamp(progress, 0.0, 1.0);
    switch (easing)
    {
    case TweenEasing2D::Linear:
        return t;
    case TweenEasing2D::EaseInQuad:
        return t * t;
    case TweenEasing2D::EaseOutQuad:
    {
        const double oneMinusT = 1.0 - t;
        return 1.0 - (oneMinusT * oneMinusT);
    }
    case TweenEasing2D::EaseInOutQuad:
        if (t < 0.5)
        {
            return 2.0 * t * t;
        }
        else
        {
            const double x = -2.0 * t + 2.0;
            return 1.0 - ((x * x) / 2.0);
        }
    case TweenEasing2D::EaseInCubic:
        return t * t * t;
    case TweenEasing2D::EaseOutCubic:
    {
        const double oneMinusT = 1.0 - t;
        return 1.0 - (oneMinusT * oneMinusT * oneMinusT);
    }
    case TweenEasing2D::EaseInOutCubic:
        if (t < 0.5)
        {
            return 4.0 * t * t * t;
        }
        else
        {
            const double x = -2.0 * t + 2.0;
            return 1.0 - ((x * x * x) / 2.0);
        }
    }
    return t;
}

TweenValue2D InterpolateTweenValue2D(
    const TweenValue2D& start,
    const TweenValue2D& end,
    const double easedProgress) noexcept
{
    TweenValue2D result = start;
    if (start.type != end.type || !IsValidValueType(start.type))
    {
        return result;
    }

    const double t = std::clamp(easedProgress, 0.0, 1.0);
    std::size_t componentCount = 1U;
    if (start.type == TweenValueType2D::Float2)
    {
        componentCount = 2U;
    }
    else if (start.type == TweenValueType2D::Color)
    {
        componentCount = 4U;
    }

    for (std::size_t index = 0U; index < componentCount; ++index)
    {
        const double from = static_cast<double>(start.components[index]);
        const double to = static_cast<double>(end.components[index]);
        result.components[index] = static_cast<float>(from + ((to - from) * t));
    }
    return result;
}

void TweenPool2D::Reserve(const std::size_t capacity)
{
    slots_.reserve(capacity);
}

Tween2DStatus TweenPool2D::Create(const TweenSpec2D& spec, TweenHandle2D& outHandle)
{
    const Tween2DStatus validation = ValidateTweenSpec2D(spec);
    if (!validation.Succeeded())
    {
        return validation;
    }

    for (std::size_t index = 0U; index < slots_.size(); ++index)
    {
        Slot& slot = slots_[index];
        if (!slot.occupied || !IsTerminal(slot.state.playback) ||
            slot.generation == std::numeric_limits<std::uint64_t>::max())
        {
            continue;
        }

        ++slot.generation;
        slot.spec = spec;
        slot.occupied = true;
        ResetSlotState(slot);
        ++createdCount_;
        ++reusedSlotCount_;
        IncrementActive();
        outHandle = TweenHandle2D{static_cast<std::uint32_t>(index), slot.generation};
        return {};
    }

    if (slots_.size() >= static_cast<std::size_t>(TweenHandle2D::InvalidIndex))
    {
        return {Tween2DError::SlotCapacityExceeded};
    }

    Slot slot{};
    slot.spec = spec;
    slot.generation = 1U;
    slot.occupied = true;
    ResetSlotState(slot);
    slots_.push_back(slot);
    ++createdCount_;
    IncrementActive();
    outHandle = TweenHandle2D{
        static_cast<std::uint32_t>(slots_.size() - 1U),
        slot.generation,
    };
    return {};
}

Tween2DStatus TweenPool2D::Inspect(
    const TweenHandle2D handle,
    TweenState2D& outState) const noexcept
{
    const Slot* const slot = Resolve(handle);
    if (slot == nullptr)
    {
        return {Tween2DError::InvalidHandle};
    }
    outState = slot->state;
    return {};
}

Tween2DStatus TweenPool2D::Pause(const TweenHandle2D handle) noexcept
{
    Slot* const slot = ResolveMutable(handle);
    if (slot == nullptr)
    {
        return {Tween2DError::InvalidHandle};
    }
    if (slot->state.playback != TweenPlaybackState2D::Playing)
    {
        return {Tween2DError::InvalidPlaybackTransition};
    }
    slot->state.playback = TweenPlaybackState2D::Paused;
    return {};
}

Tween2DStatus TweenPool2D::Resume(const TweenHandle2D handle) noexcept
{
    Slot* const slot = ResolveMutable(handle);
    if (slot == nullptr)
    {
        return {Tween2DError::InvalidHandle};
    }
    if (slot->state.playback != TweenPlaybackState2D::Paused)
    {
        return {Tween2DError::InvalidPlaybackTransition};
    }
    slot->state.playback = TweenPlaybackState2D::Playing;
    return {};
}

Tween2DStatus TweenPool2D::Restart(const TweenHandle2D handle) noexcept
{
    Slot* const slot = ResolveMutable(handle);
    if (slot == nullptr)
    {
        return {Tween2DError::InvalidHandle};
    }
    const bool wasTerminal = IsTerminal(slot->state.playback);
    ResetSlotState(*slot);
    if (wasTerminal)
    {
        IncrementActive();
    }
    return {};
}

Tween2DStatus TweenPool2D::Rebase(
    const TweenHandle2D handle,
    const TweenValue2D start,
    const TweenValue2D end) noexcept
{
    Slot* const slot = ResolveMutable(handle);
    if (slot == nullptr)
    {
        return {Tween2DError::InvalidHandle};
    }
    if (IsTerminal(slot->state.playback) || slot->state.loopIndex != 0U ||
        slot->state.loopElapsed.count() != 0)
    {
        return {Tween2DError::InvalidPlaybackTransition};
    }
    if (!IsValidValueType(start.type) || !IsValidValueType(end.type))
    {
        return {Tween2DError::InvalidValueType};
    }
    if (start.type != end.type || start.type != slot->spec.start.type)
    {
        return {Tween2DError::ValueTypeMismatch};
    }

    slot->spec.start = start;
    slot->spec.end = end;
    slot->state.currentValue = start;
    return {};
}

Tween2DStatus TweenPool2D::Cancel(
    const TweenHandle2D handle,
    const TweenCancellationReason2D reason) noexcept
{
    Slot* const slot = ResolveMutable(handle);
    if (slot == nullptr)
    {
        return {Tween2DError::InvalidHandle};
    }
    if (!IsValidCancellationReason(reason))
    {
        return {Tween2DError::InvalidCancellationReason};
    }
    if (slot->state.playback == TweenPlaybackState2D::Cancelled)
    {
        return {Tween2DError::InvalidPlaybackTransition};
    }
    const bool wasCompleted = slot->state.playback == TweenPlaybackState2D::Completed;
    if (wasCompleted &&
        reason != TweenCancellationReason2D::TargetInvalidated &&
        reason != TweenCancellationReason2D::PropertyWriteRejected)
    {
        return {Tween2DError::InvalidPlaybackTransition};
    }
    slot->state.playback = TweenPlaybackState2D::Cancelled;
    slot->state.cancellationReason = reason;
    if (!wasCompleted)
    {
        DecrementActive();
    }
    return {};
}

Tween2DStatus TweenPool2D::Step(
    const TweenTimeDomain2D domain,
    const TweenTime2D delta) noexcept
{
    if (!IsValidTimeDomain(domain))
    {
        return {Tween2DError::InvalidTimeDomain};
    }
    if (delta.count() < 0)
    {
        return {Tween2DError::NegativeDelta};
    }

    for (Slot& slot : slots_)
    {
        if (!slot.occupied || slot.state.playback != TweenPlaybackState2D::Playing ||
            slot.spec.domain != domain)
        {
            continue;
        }

        const Tween2DStatus status = AdvanceSlot(slot, delta);
        if (!status.Succeeded())
        {
            return status;
        }
    }
    return {};
}

TweenPoolMetrics2D TweenPool2D::Metrics() const noexcept
{
    return TweenPoolMetrics2D{
        activeCount_,
        static_cast<std::uint64_t>(slots_.size()),
        static_cast<std::uint64_t>(slots_.capacity()),
        highWaterActiveCount_,
        createdCount_,
        reusedSlotCount_,
    };
}

TweenPool2D::Slot* TweenPool2D::ResolveMutable(const TweenHandle2D handle) noexcept
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

const TweenPool2D::Slot* TweenPool2D::Resolve(const TweenHandle2D handle) const noexcept
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

Tween2DStatus TweenPool2D::AdvanceSlot(Slot& slot, const TweenTime2D delta) noexcept
{
    TweenState2D next = slot.state;
    std::uint64_t deltaCount = UnsignedCount(delta);

    const std::uint64_t delayCount = UnsignedCount(slot.spec.delay);
    const std::uint64_t delayElapsed = UnsignedCount(next.delayElapsed);
    if (delayElapsed < delayCount)
    {
        const std::uint64_t delayRemaining = delayCount - delayElapsed;
        if (deltaCount <= delayRemaining)
        {
            next.delayElapsed = TweenTime2D{
                static_cast<TweenTime2D::rep>(delayElapsed + deltaCount)};
            slot.state = next;
            return {};
        }

        next.delayElapsed = slot.spec.delay;
        deltaCount -= delayRemaining;
    }

    if (deltaCount == 0U)
    {
        slot.state = next;
        return {};
    }

    const std::uint64_t durationCount = UnsignedCount(slot.spec.duration);
    const std::uint64_t loopElapsed = UnsignedCount(next.loopElapsed);
    const std::uint64_t activeTotal = loopElapsed + deltaCount;
    const std::uint64_t crossedBoundaries = activeTotal / durationCount;
    const std::uint64_t remainder = activeTotal % durationCount;

    if (!slot.spec.infinite)
    {
        const std::uint64_t finalLoopIndex = static_cast<std::uint64_t>(slot.spec.repeatCount);
        const std::uint64_t loopsRemaining = finalLoopIndex - next.loopIndex + 1U;
        if (crossedBoundaries >= loopsRemaining)
        {
            next.loopIndex = finalLoopIndex;
            next.reverse = IsReverseLoop(slot.spec, finalLoopIndex);
            next.loopElapsed = slot.spec.duration;
            next.currentValue = CompletedValue(slot.spec);
            next.playback = TweenPlaybackState2D::Completed;
            next.cancellationReason = TweenCancellationReason2D::None;
            slot.state = next;
            DecrementActive();
            return {};
        }
    }

    if (crossedBoundaries > std::numeric_limits<std::uint64_t>::max() - next.loopIndex)
    {
        return {Tween2DError::LoopCounterOverflow};
    }

    next.loopIndex += crossedBoundaries;
    next.loopElapsed = TweenTime2D{static_cast<TweenTime2D::rep>(remainder)};
    next.reverse = IsReverseLoop(slot.spec, next.loopIndex);
    next.currentValue = SampleValue(slot.spec, next.loopIndex, next.loopElapsed);
    slot.state = next;
    return {};
}

void TweenPool2D::ResetSlotState(Slot& slot) noexcept
{
    slot.state = TweenState2D{};
    slot.state.domain = slot.spec.domain;
    slot.state.playback = TweenPlaybackState2D::Playing;
    slot.state.cancellationReason = TweenCancellationReason2D::None;
    slot.state.delayElapsed = TweenTime2D{0};
    slot.state.loopElapsed = TweenTime2D{0};
    slot.state.loopIndex = 0U;
    slot.state.reverse = false;
    slot.state.currentValue = slot.spec.start;
}

void TweenPool2D::IncrementActive() noexcept
{
    ++activeCount_;
    highWaterActiveCount_ = std::max(highWaterActiveCount_, activeCount_);
}

void TweenPool2D::DecrementActive() noexcept
{
    if (activeCount_ > 0U)
    {
        --activeCount_;
    }
}
} // namespace trace2d::runtime
