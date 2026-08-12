#include <trace2d/runtime/SpriteAnimator2D.hpp>

#include <algorithm>
#include <iterator>
#include <limits>
#include <numeric>
#include <utility>

namespace trace2d::runtime
{
namespace
{
[[nodiscard]] bool IsValidPlaybackState(SpriteAnimationPlaybackState value) noexcept
{
    switch (value)
    {
    case SpriteAnimationPlaybackState::Stopped:
    case SpriteAnimationPlaybackState::Playing:
    case SpriteAnimationPlaybackState::Paused:
        return true;
    }

    return false;
}

[[nodiscard]] bool IsValidLoopMode(SpriteAnimationLoopMode value) noexcept
{
    switch (value)
    {
    case SpriteAnimationLoopMode::Once:
    case SpriteAnimationLoopMode::Loop:
    case SpriteAnimationLoopMode::PingPong:
        return true;
    }

    return false;
}

[[nodiscard]] bool IsValidDirection(SpriteAnimationDirection value) noexcept
{
    switch (value)
    {
    case SpriteAnimationDirection::Forward:
    case SpriteAnimationDirection::Reverse:
        return true;
    }

    return false;
}

[[nodiscard]] SpriteAnimator2DStatus ValidateCompletion(const SpriteAnimator2DState& state) noexcept
{
    if (!state.completed)
    {
        return {};
    }

    if (state.loopMode != SpriteAnimationLoopMode::Once)
    {
        return {SpriteAnimator2DError::InvalidCompletionState, state.frameIndex};
    }

    const SpriteAnimationTime2D expectedTime =
        state.direction == SpriteAnimationDirection::Forward ? state.clip->Duration() : SpriteAnimationTime2D{0};
    if (state.time != expectedTime)
    {
        return {SpriteAnimator2DError::InvalidCompletionState, state.frameIndex};
    }

    return {};
}

[[nodiscard]] SpriteAnimator2DStatus RequireState(const SpriteAnimator2DState& state) noexcept
{
    if (state.clip == nullptr)
    {
        return {SpriteAnimator2DError::NoState, 0};
    }

    return ValidateSpriteAnimator2DState(state);
}

[[nodiscard]] SpriteAnimationTime2D TraversalStart(const SpriteAnimator2DState& state) noexcept
{
    return state.direction == SpriteAnimationDirection::Forward ? SpriteAnimationTime2D{0} : state.clip->Duration();
}

[[nodiscard]] SpriteAnimator2DStatus RefreshFrame(SpriteAnimator2DState& state) noexcept
{
    std::uint32_t frameIndex = 0;
    const SpriteAnimationClipStatus frameStatus = state.clip->ResolveFrameIndex(state.time, frameIndex);
    if (!frameStatus.Succeeded())
    {
        return {SpriteAnimator2DError::TimeOutOfRange, 0};
    }

    state.frameIndex = frameIndex;
    return {};
}

[[nodiscard]] SpriteAnimator2DError ScaleAdvance(
    SpriteAnimationTime2D delta,
    const SpriteAnimator2DState& state,
    std::uint64_t& outWholeNanoseconds,
    std::uint32_t& outRemainder) noexcept
{
    const auto signedCount = delta.count();
    if (signedCount < 0)
    {
        return SpriteAnimator2DError::NegativeDelta;
    }

    const std::uint64_t deltaCount = static_cast<std::uint64_t>(signedCount);
    const std::uint64_t numerator = state.speed.numerator;
    const std::uint64_t denominator = state.speed.denominator;

    if (numerator == 0U)
    {
        outWholeNanoseconds = 0U;
        outRemainder = 0U;
        return SpriteAnimator2DError::None;
    }

    const std::uint64_t whole = deltaCount / denominator;
    const std::uint64_t part = deltaCount % denominator;
    const std::uint64_t maxNanoseconds =
        static_cast<std::uint64_t>(std::numeric_limits<SpriteAnimationTime2D::rep>::max());

    if (whole > maxNanoseconds / numerator)
    {
        return SpriteAnimator2DError::AdvanceOverflow;
    }

    const std::uint64_t base = whole * numerator;
    const std::uint64_t partialNumerator =
        part * numerator + static_cast<std::uint64_t>(state.speedRemainder);
    const std::uint64_t extra = partialNumerator / denominator;
    const std::uint64_t remainder = partialNumerator % denominator;

    if (base > maxNanoseconds - extra)
    {
        return SpriteAnimator2DError::AdvanceOverflow;
    }

    outWholeNanoseconds = base + extra;
    outRemainder = static_cast<std::uint32_t>(remainder);
    return SpriteAnimator2DError::None;
}

class EmissionWriter final
{
public:
    EmissionWriter(std::span<SpriteAnimationEmission2D> output, bool write) noexcept
        : output_(output), write_(write)
    {
    }

    [[nodiscard]] bool Emit(const SpriteAnimationEmission2D& emission) noexcept
    {
        if (count_ >= output_.size())
        {
            return false;
        }

        if (write_)
        {
            output_[count_] = emission;
        }
        ++count_;
        return true;
    }

    [[nodiscard]] std::size_t Count() const noexcept
    {
        return count_;
    }

private:
    std::span<SpriteAnimationEmission2D> output_{};
    std::size_t count_{0};
    bool write_{false};
};

[[nodiscard]] bool EmitAuthored(
    const SpriteAnimationEvent2D& event,
    SpriteAnimationDirection direction,
    EmissionWriter& writer) noexcept
{
    return writer.Emit({
        SpriteAnimationEmissionKind::AuthoredEvent,
        event.eventId,
        event.authoredOrdinal,
        event.offset,
        direction,
    });
}

[[nodiscard]] bool EmitForwardEvents(
    const SpriteAnimationClip2D& clip,
    SpriteAnimationTime2D from,
    SpriteAnimationTime2D to,
    SpriteAnimationDirection direction,
    EmissionWriter& writer) noexcept
{
    const auto events = clip.Events();
    const auto first = std::upper_bound(
        events.begin(),
        events.end(),
        from,
        [](SpriteAnimationTime2D value, const SpriteAnimationEvent2D& event)
        {
            return value < event.offset;
        });
    const auto last = std::upper_bound(
        events.begin(),
        events.end(),
        to,
        [](SpriteAnimationTime2D value, const SpriteAnimationEvent2D& event)
        {
            return value < event.offset;
        });

    for (auto it = first; it != last; ++it)
    {
        if (!EmitAuthored(*it, direction, writer))
        {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool EmitReverseEvents(
    const SpriteAnimationClip2D& clip,
    SpriteAnimationTime2D from,
    SpriteAnimationTime2D to,
    SpriteAnimationDirection direction,
    EmissionWriter& writer) noexcept
{
    const auto events = clip.Events();
    auto rangeBegin = std::lower_bound(
        events.begin(),
        events.end(),
        to,
        [](const SpriteAnimationEvent2D& event, SpriteAnimationTime2D value)
        {
            return event.offset < value;
        });
    auto rangeEnd = std::lower_bound(
        events.begin(),
        events.end(),
        from,
        [](const SpriteAnimationEvent2D& event, SpriteAnimationTime2D value)
        {
            return event.offset < value;
        });

    while (rangeEnd != rangeBegin)
    {
        auto groupEnd = rangeEnd;
        const SpriteAnimationTime2D offset = std::prev(rangeEnd)->offset;
        auto groupBegin = std::lower_bound(
            rangeBegin,
            groupEnd,
            offset,
            [](const SpriteAnimationEvent2D& event, SpriteAnimationTime2D value)
            {
                return event.offset < value;
            });

        for (auto it = groupBegin; it != groupEnd; ++it)
        {
            if (!EmitAuthored(*it, direction, writer))
            {
                return false;
            }
        }

        rangeEnd = groupBegin;
    }

    return true;
}

[[nodiscard]] bool EmitZeroEvents(
    const SpriteAnimationClip2D& clip,
    SpriteAnimationDirection direction,
    EmissionWriter& writer) noexcept
{
    const auto events = clip.Events();
    const auto last = std::upper_bound(
        events.begin(),
        events.end(),
        SpriteAnimationTime2D{0},
        [](SpriteAnimationTime2D value, const SpriteAnimationEvent2D& event)
        {
            return value < event.offset;
        });

    for (auto it = events.begin(); it != last; ++it)
    {
        if (it->offset != SpriteAnimationTime2D{0})
        {
            break;
        }
        if (!EmitAuthored(*it, direction, writer))
        {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool EmitStructural(
    SpriteAnimationEmissionKind kind,
    SpriteAnimationTime2D time,
    SpriteAnimationDirection direction,
    EmissionWriter& writer) noexcept
{
    return writer.Emit({kind, 0U, 0U, time, direction});
}

[[nodiscard]] SpriteAnimator2DError RunTraversal(
    SpriteAnimator2DState& state,
    std::uint64_t remaining,
    EmissionWriter& writer) noexcept
{
    const auto duration = state.clip->Duration();
    const std::uint64_t durationCount = static_cast<std::uint64_t>(duration.count());

    while (remaining > 0U && state.playback == SpriteAnimationPlaybackState::Playing && !state.completed)
    {
        if (state.direction == SpriteAnimationDirection::Forward)
        {
            const std::uint64_t timeCount = static_cast<std::uint64_t>(state.time.count());
            const std::uint64_t distance = durationCount - timeCount;

            if (remaining < distance)
            {
                const auto next = SpriteAnimationTime2D{
                    state.time.count() + static_cast<SpriteAnimationTime2D::rep>(remaining)};
                if (!EmitForwardEvents(*state.clip, state.time, next, state.direction, writer))
                {
                    return SpriteAnimator2DError::OutputCapacityExceeded;
                }
                state.time = next;
                remaining = 0U;
                break;
            }

            if (!EmitForwardEvents(*state.clip, state.time, duration, state.direction, writer))
            {
                return SpriteAnimator2DError::OutputCapacityExceeded;
            }
            remaining -= distance;
            state.time = duration;

            if (state.loopMode == SpriteAnimationLoopMode::Once)
            {
                if (!EmitStructural(SpriteAnimationEmissionKind::Completed, duration, state.direction, writer))
                {
                    return SpriteAnimator2DError::OutputCapacityExceeded;
                }
                state.completed = true;
                state.playback = SpriteAnimationPlaybackState::Paused;
                state.speedRemainder = 0U;
                remaining = 0U;
                break;
            }

            if (state.loopMode == SpriteAnimationLoopMode::Loop)
            {
                if (!EmitStructural(SpriteAnimationEmissionKind::Loop, duration, state.direction, writer))
                {
                    return SpriteAnimator2DError::OutputCapacityExceeded;
                }
                state.time = SpriteAnimationTime2D{0};
                if (!EmitZeroEvents(*state.clip, state.direction, writer))
                {
                    return SpriteAnimator2DError::OutputCapacityExceeded;
                }
                continue;
            }

            if (!EmitStructural(SpriteAnimationEmissionKind::Bounce, duration, state.direction, writer))
            {
                return SpriteAnimator2DError::OutputCapacityExceeded;
            }
            state.direction = SpriteAnimationDirection::Reverse;
            continue;
        }

        const std::uint64_t timeCount = static_cast<std::uint64_t>(state.time.count());
        const std::uint64_t distance = timeCount;

        if (remaining < distance)
        {
            const auto next = SpriteAnimationTime2D{
                state.time.count() - static_cast<SpriteAnimationTime2D::rep>(remaining)};
            if (!EmitReverseEvents(*state.clip, state.time, next, state.direction, writer))
            {
                return SpriteAnimator2DError::OutputCapacityExceeded;
            }
            state.time = next;
            remaining = 0U;
            break;
        }

        if (!EmitReverseEvents(*state.clip, state.time, SpriteAnimationTime2D{0}, state.direction, writer))
        {
            return SpriteAnimator2DError::OutputCapacityExceeded;
        }
        remaining -= distance;
        state.time = SpriteAnimationTime2D{0};

        if (state.loopMode == SpriteAnimationLoopMode::Once)
        {
            if (!EmitStructural(
                    SpriteAnimationEmissionKind::Completed,
                    SpriteAnimationTime2D{0},
                    state.direction,
                    writer))
            {
                return SpriteAnimator2DError::OutputCapacityExceeded;
            }
            state.completed = true;
            state.playback = SpriteAnimationPlaybackState::Paused;
            state.speedRemainder = 0U;
            remaining = 0U;
            break;
        }

        if (state.loopMode == SpriteAnimationLoopMode::Loop)
        {
            if (!EmitStructural(
                    SpriteAnimationEmissionKind::Loop,
                    SpriteAnimationTime2D{0},
                    state.direction,
                    writer))
            {
                return SpriteAnimator2DError::OutputCapacityExceeded;
            }
            state.time = duration;
            continue;
        }

        if (!EmitStructural(
                SpriteAnimationEmissionKind::Bounce,
                SpriteAnimationTime2D{0},
                state.direction,
                writer))
        {
            return SpriteAnimator2DError::OutputCapacityExceeded;
        }
        state.direction = SpriteAnimationDirection::Forward;
    }

    return SpriteAnimator2DError::None;
}

[[nodiscard]] SpriteAnimationAdvanceResult2D AdvanceCandidate(
    SpriteAnimator2DState& candidate,
    SpriteAnimationTime2D delta,
    std::span<SpriteAnimationEmission2D> output,
    bool writeOutput) noexcept
{
    if (delta.count() < 0)
    {
        return {SpriteAnimator2DError::NegativeDelta, 0U};
    }

    if (candidate.playback != SpriteAnimationPlaybackState::Playing || candidate.completed)
    {
        return {};
    }

    std::uint64_t scaledAdvance = 0U;
    std::uint32_t nextRemainder = 0U;
    const SpriteAnimator2DError scaleError = ScaleAdvance(delta, candidate, scaledAdvance, nextRemainder);
    if (scaleError != SpriteAnimator2DError::None)
    {
        return {scaleError, 0U};
    }

    candidate.speedRemainder = nextRemainder;
    if (scaledAdvance == 0U)
    {
        return {};
    }

    EmissionWriter writer{output, writeOutput};
    const SpriteAnimator2DError traversalError = RunTraversal(candidate, scaledAdvance, writer);
    if (traversalError != SpriteAnimator2DError::None)
    {
        return {traversalError, writer.Count()};
    }

    const SpriteAnimator2DStatus frameStatus = RefreshFrame(candidate);
    if (!frameStatus.Succeeded())
    {
        return {frameStatus.error, writer.Count()};
    }

    const SpriteAnimator2DStatus validationStatus = ValidateSpriteAnimator2DState(candidate);
    if (!validationStatus.Succeeded())
    {
        return {validationStatus.error, writer.Count()};
    }

    return {SpriteAnimator2DError::None, writer.Count()};
}
} // namespace

SpriteAnimationClipStatus SpriteAnimationClip2D::Prepare(
    const assets::SpriteAsset* spriteAsset,
    std::uint32_t spriteRegionCount,
    std::span<const SpriteAnimationFrame2D> frames,
    SpriteAnimationClip2D& outClip)
{
    return Prepare(spriteAsset, spriteRegionCount, frames, std::span<const SpriteAnimationEvent2D>{}, outClip);
}

SpriteAnimationClipStatus SpriteAnimationClip2D::Prepare(
    const assets::SpriteAsset* spriteAsset,
    std::uint32_t spriteRegionCount,
    std::span<const SpriteAnimationFrame2D> frames,
    std::span<const SpriteAnimationEvent2D> events,
    SpriteAnimationClip2D& outClip)
{
    if (spriteAsset == nullptr)
    {
        return {SpriteAnimationClipError::NullSpriteAsset, 0, 0};
    }

    if (frames.empty())
    {
        return {SpriteAnimationClipError::EmptyFrames, 0, 0};
    }

    if (frames.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        return {SpriteAnimationClipError::TooManyFrames, 0, 0};
    }

    if (events.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        return {SpriteAnimationClipError::TooManyEvents, 0, 0};
    }

    SpriteAnimationClip2D prepared{};
    prepared.spriteAsset_ = spriteAsset;
    prepared.spriteRegionCount_ = spriteRegionCount;
    prepared.frames_.reserve(frames.size());
    prepared.frameBoundaries_.reserve(frames.size() + 1U);
    prepared.frameBoundaries_.push_back(SpriteAnimationTime2D{0});

    const auto maxNanoseconds = std::numeric_limits<SpriteAnimationTime2D::rep>::max();
    SpriteAnimationTime2D::rep durationCount = 0;

    for (std::size_t index = 0; index < frames.size(); ++index)
    {
        const SpriteAnimationFrame2D& frame = frames[index];
        const auto frameIndex = static_cast<std::uint32_t>(index);

        if (frame.regionIndex >= spriteRegionCount)
        {
            return {SpriteAnimationClipError::RegionIndexOutOfRange, frameIndex, 0};
        }

        const SpriteAnimationTime2D::rep frameDurationCount = frame.duration.count();
        if (frameDurationCount <= 0)
        {
            return {SpriteAnimationClipError::NonPositiveFrameDuration, frameIndex, 0};
        }

        if (durationCount > maxNanoseconds - frameDurationCount)
        {
            return {SpriteAnimationClipError::DurationOverflow, frameIndex, 0};
        }

        durationCount += frameDurationCount;
        prepared.frames_.push_back(frame);
        prepared.frameBoundaries_.push_back(SpriteAnimationTime2D{durationCount});
    }

    prepared.duration_ = SpriteAnimationTime2D{durationCount};
    prepared.events_.reserve(events.size());

    for (std::size_t index = 0; index < events.size(); ++index)
    {
        const SpriteAnimationEvent2D& event = events[index];
        if (event.offset < SpriteAnimationTime2D{0} || event.offset >= prepared.duration_)
        {
            return {
                SpriteAnimationClipError::EventOffsetOutOfRange,
                0,
                static_cast<std::uint32_t>(index),
            };
        }

        prepared.events_.push_back(event);
    }

    std::sort(
        prepared.events_.begin(),
        prepared.events_.end(),
        [](const SpriteAnimationEvent2D& left, const SpriteAnimationEvent2D& right)
        {
            if (left.offset != right.offset)
            {
                return left.offset < right.offset;
            }
            return left.authoredOrdinal < right.authoredOrdinal;
        });

    for (std::size_t index = 1; index < prepared.events_.size(); ++index)
    {
        const SpriteAnimationEvent2D& previous = prepared.events_[index - 1U];
        const SpriteAnimationEvent2D& current = prepared.events_[index];
        if (previous.offset == current.offset && previous.authoredOrdinal == current.authoredOrdinal)
        {
            return {
                SpriteAnimationClipError::DuplicateEventOrdinal,
                0,
                static_cast<std::uint32_t>(index),
            };
        }
    }

    prepared.prepared_ = true;
    outClip = std::move(prepared);
    return {};
}

bool SpriteAnimationClip2D::Prepared() const noexcept
{
    return prepared_;
}

const assets::SpriteAsset* SpriteAnimationClip2D::SpriteAsset() const noexcept
{
    return spriteAsset_;
}

std::uint32_t SpriteAnimationClip2D::SpriteRegionCount() const noexcept
{
    return spriteRegionCount_;
}

std::uint32_t SpriteAnimationClip2D::FrameCount() const noexcept
{
    return static_cast<std::uint32_t>(frames_.size());
}

std::uint32_t SpriteAnimationClip2D::EventCount() const noexcept
{
    return static_cast<std::uint32_t>(events_.size());
}

SpriteAnimationTime2D SpriteAnimationClip2D::Duration() const noexcept
{
    return duration_;
}

std::span<const SpriteAnimationFrame2D> SpriteAnimationClip2D::Frames() const noexcept
{
    return {frames_.data(), frames_.size()};
}

std::span<const SpriteAnimationTime2D> SpriteAnimationClip2D::FrameBoundaries() const noexcept
{
    return {frameBoundaries_.data(), frameBoundaries_.size()};
}

std::span<const SpriteAnimationEvent2D> SpriteAnimationClip2D::Events() const noexcept
{
    return {events_.data(), events_.size()};
}

SpriteAnimationClipStatus SpriteAnimationClip2D::ResolveFrameIndex(
    SpriteAnimationTime2D time,
    std::uint32_t& outFrameIndex) const noexcept
{
    if (!prepared_)
    {
        return {SpriteAnimationClipError::NotPrepared, 0, 0};
    }

    if (time < SpriteAnimationTime2D{0} || time > duration_)
    {
        return {SpriteAnimationClipError::TimeOutOfRange, 0, 0};
    }

    if (time == duration_)
    {
        outFrameIndex = static_cast<std::uint32_t>(frames_.size() - 1U);
        return {};
    }

    const auto upper = std::upper_bound(frameBoundaries_.begin(), frameBoundaries_.end(), time);
    const auto resolvedIndex = static_cast<std::size_t>(upper - frameBoundaries_.begin() - 1);
    outFrameIndex = static_cast<std::uint32_t>(resolvedIndex);
    return {};
}

bool NormalizeSpriteAnimationSpeed(
    SpriteAnimationSpeed2D requested,
    SpriteAnimationSpeed2D& outSpeed) noexcept
{
    if (requested.denominator == 0U)
    {
        return false;
    }

    if (requested.numerator == 0U)
    {
        outSpeed = {0U, 1U};
        return true;
    }

    const std::uint32_t divisor = std::gcd(requested.numerator, requested.denominator);
    outSpeed = {
        requested.numerator / divisor,
        requested.denominator / divisor,
    };
    return true;
}

bool IsCanonicalSpriteAnimationSpeed(SpriteAnimationSpeed2D speed) noexcept
{
    SpriteAnimationSpeed2D normalized{};
    return NormalizeSpriteAnimationSpeed(speed, normalized) && normalized == speed;
}

SpriteAnimator2DStatus MakeSpriteAnimator2DState(
    const SpriteAnimationClip2D& clip,
    SpriteAnimationTime2D time,
    SpriteAnimationPlaybackState playback,
    SpriteAnimationLoopMode loopMode,
    SpriteAnimationDirection direction,
    bool completed,
    SpriteAnimationSpeed2D speed,
    SpriteAnimator2DState& outState) noexcept
{
    if (!clip.Prepared())
    {
        return {SpriteAnimator2DError::UnpreparedClip, 0};
    }

    std::uint32_t frameIndex = 0;
    const SpriteAnimationClipStatus frameStatus = clip.ResolveFrameIndex(time, frameIndex);
    if (!frameStatus.Succeeded())
    {
        return {SpriteAnimator2DError::TimeOutOfRange, 0};
    }

    SpriteAnimationSpeed2D normalizedSpeed{};
    if (!NormalizeSpriteAnimationSpeed(speed, normalizedSpeed))
    {
        return {SpriteAnimator2DError::InvalidSpeed, frameIndex};
    }

    const SpriteAnimator2DState candidate{
        &clip,
        time,
        frameIndex,
        playback,
        loopMode,
        direction,
        completed,
        normalizedSpeed,
        0U,
    };

    const SpriteAnimator2DStatus status = ValidateSpriteAnimator2DState(candidate);
    if (!status.Succeeded())
    {
        return status;
    }

    outState = candidate;
    return {};
}

SpriteAnimator2DStatus ValidateSpriteAnimator2DState(const SpriteAnimator2DState& state) noexcept
{
    if (state.clip == nullptr)
    {
        return {SpriteAnimator2DError::NullClip, 0};
    }

    if (!state.clip->Prepared())
    {
        return {SpriteAnimator2DError::UnpreparedClip, 0};
    }

    if (!IsValidPlaybackState(state.playback))
    {
        return {SpriteAnimator2DError::InvalidPlaybackState, state.frameIndex};
    }

    if (!IsValidLoopMode(state.loopMode))
    {
        return {SpriteAnimator2DError::InvalidLoopMode, state.frameIndex};
    }

    if (!IsValidDirection(state.direction))
    {
        return {SpriteAnimator2DError::InvalidDirection, state.frameIndex};
    }

    if (!IsCanonicalSpriteAnimationSpeed(state.speed))
    {
        return {SpriteAnimator2DError::InvalidSpeed, state.frameIndex};
    }

    if (state.speedRemainder >= state.speed.denominator)
    {
        return {SpriteAnimator2DError::InvalidSpeedRemainder, state.frameIndex};
    }

    if (state.speed.numerator == 0U && state.speedRemainder != 0U)
    {
        return {SpriteAnimator2DError::InvalidSpeedRemainder, state.frameIndex};
    }

    std::uint32_t expectedFrameIndex = 0;
    const SpriteAnimationClipStatus frameStatus = state.clip->ResolveFrameIndex(state.time, expectedFrameIndex);
    if (!frameStatus.Succeeded())
    {
        return {SpriteAnimator2DError::TimeOutOfRange, 0};
    }

    if (state.frameIndex != expectedFrameIndex)
    {
        return {SpriteAnimator2DError::FrameIndexMismatch, expectedFrameIndex};
    }

    return ValidateCompletion(state);
}

SpriteAnimator2DStatus SpriteAnimator2D::RestoreState(const SpriteAnimator2DState& state) noexcept
{
    const SpriteAnimator2DStatus status = ValidateSpriteAnimator2DState(state);
    if (!status.Succeeded())
    {
        return status;
    }

    state_ = state;
    return {};
}

SpriteAnimator2DStatus SpriteAnimator2D::Play() noexcept
{
    const SpriteAnimator2DStatus status = RequireState(state_);
    if (!status.Succeeded())
    {
        return status;
    }

    if (state_.completed)
    {
        return {SpriteAnimator2DError::InvalidPlaybackTransition, state_.frameIndex};
    }

    state_.playback = SpriteAnimationPlaybackState::Playing;
    return {};
}

SpriteAnimator2DStatus SpriteAnimator2D::Pause() noexcept
{
    const SpriteAnimator2DStatus status = RequireState(state_);
    if (!status.Succeeded())
    {
        return status;
    }

    if (state_.playback == SpriteAnimationPlaybackState::Stopped)
    {
        return {SpriteAnimator2DError::InvalidPlaybackTransition, state_.frameIndex};
    }

    state_.playback = SpriteAnimationPlaybackState::Paused;
    return {};
}

SpriteAnimator2DStatus SpriteAnimator2D::Stop() noexcept
{
    const SpriteAnimator2DStatus status = RequireState(state_);
    if (!status.Succeeded())
    {
        return status;
    }

    state_.time = TraversalStart(state_);
    state_.playback = SpriteAnimationPlaybackState::Stopped;
    state_.completed = false;
    state_.speedRemainder = 0U;
    return RefreshFrame(state_);
}

SpriteAnimator2DStatus SpriteAnimator2D::Reset() noexcept
{
    return Stop();
}

SpriteAnimator2DStatus SpriteAnimator2D::Restart() noexcept
{
    const SpriteAnimator2DStatus status = RequireState(state_);
    if (!status.Succeeded())
    {
        return status;
    }

    state_.time = TraversalStart(state_);
    state_.playback = SpriteAnimationPlaybackState::Playing;
    state_.completed = false;
    state_.speedRemainder = 0U;
    return RefreshFrame(state_);
}

SpriteAnimator2DStatus SpriteAnimator2D::Seek(SpriteAnimationTime2D time) noexcept
{
    const SpriteAnimator2DStatus status = RequireState(state_);
    if (!status.Succeeded())
    {
        return status;
    }

    std::uint32_t frameIndex = 0;
    const SpriteAnimationClipStatus frameStatus = state_.clip->ResolveFrameIndex(time, frameIndex);
    if (!frameStatus.Succeeded())
    {
        return {SpriteAnimator2DError::TimeOutOfRange, state_.frameIndex};
    }

    state_.time = time;
    state_.frameIndex = frameIndex;
    state_.completed = false;
    state_.speedRemainder = 0U;
    return {};
}

SpriteAnimator2DStatus SpriteAnimator2D::SetSpeed(SpriteAnimationSpeed2D speed) noexcept
{
    const SpriteAnimator2DStatus status = RequireState(state_);
    if (!status.Succeeded())
    {
        return status;
    }

    SpriteAnimationSpeed2D normalized{};
    if (!NormalizeSpriteAnimationSpeed(speed, normalized))
    {
        return {SpriteAnimator2DError::InvalidSpeed, state_.frameIndex};
    }

    if (normalized == state_.speed)
    {
        return {};
    }

    state_.speed = normalized;
    state_.speedRemainder = 0U;
    return {};
}

SpriteAnimator2DStatus SpriteAnimator2D::SetDirection(SpriteAnimationDirection direction) noexcept
{
    const SpriteAnimator2DStatus status = RequireState(state_);
    if (!status.Succeeded())
    {
        return status;
    }

    if (!IsValidDirection(direction))
    {
        return {SpriteAnimator2DError::InvalidDirection, state_.frameIndex};
    }

    if (direction == state_.direction)
    {
        return {};
    }

    state_.direction = direction;
    state_.completed = false;
    state_.speedRemainder = 0U;
    return {};
}

SpriteAnimationAdvanceResult2D SpriteAnimator2D::Advance(
    SpriteAnimationTime2D delta,
    std::span<SpriteAnimationEmission2D> output) noexcept
{
    const SpriteAnimator2DStatus stateStatus = RequireState(state_);
    if (!stateStatus.Succeeded())
    {
        return {stateStatus.error, 0U};
    }

    SpriteAnimator2DState dryCandidate = state_;
    const SpriteAnimationAdvanceResult2D dryResult = AdvanceCandidate(dryCandidate, delta, output, false);
    if (!dryResult.Succeeded())
    {
        return dryResult;
    }

    SpriteAnimator2DState candidate = state_;
    const SpriteAnimationAdvanceResult2D result = AdvanceCandidate(candidate, delta, output, true);
    if (!result.Succeeded())
    {
        return result;
    }

    state_ = candidate;
    return result;
}

bool SpriteAnimator2D::HasState() const noexcept
{
    return state_.clip != nullptr;
}

const SpriteAnimator2DState& SpriteAnimator2D::State() const noexcept
{
    return state_;
}

const SpriteAnimationFrame2D* SpriteAnimator2D::CurrentFrame() const noexcept
{
    if (!HasState() || state_.frameIndex >= state_.clip->FrameCount())
    {
        return nullptr;
    }

    return &state_.clip->Frames()[state_.frameIndex];
}

bool SpriteAnimator2D::TryGetCurrentRegion(SpriteAnimationRegionSelection2D& outSelection) const noexcept
{
    const SpriteAnimationFrame2D* frame = CurrentFrame();
    if (frame == nullptr)
    {
        return false;
    }

    outSelection = {state_.clip->SpriteAsset(), frame->regionIndex};
    return true;
}

bool SpriteAnimator2D::TryGetCurrentRegionIndex(std::uint32_t& outRegionIndex) const noexcept
{
    SpriteAnimationRegionSelection2D selection{};
    if (!TryGetCurrentRegion(selection))
    {
        return false;
    }

    outRegionIndex = selection.regionIndex;
    return true;
}
} // namespace trace2d::runtime
