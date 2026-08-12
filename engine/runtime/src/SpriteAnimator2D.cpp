#include <trace2d/runtime/SpriteAnimator2D.hpp>

#include <algorithm>
#include <limits>
#include <numeric>

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
} // namespace

SpriteAnimationClipStatus SpriteAnimationClip2D::Prepare(
    std::span<const SpriteAnimationFrame2D> frames,
    std::uint32_t spriteRegionCount,
    SpriteAnimationClip2D& outClip)
{
    if (frames.empty())
    {
        return {SpriteAnimationClipError::EmptyFrames, 0};
    }

    if (frames.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        return {SpriteAnimationClipError::TooManyFrames, 0};
    }

    SpriteAnimationClip2D prepared{};
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
            return {SpriteAnimationClipError::RegionIndexOutOfRange, frameIndex};
        }

        const SpriteAnimationTime2D::rep frameDurationCount = frame.duration.count();
        if (frameDurationCount <= 0)
        {
            return {SpriteAnimationClipError::NonPositiveFrameDuration, frameIndex};
        }

        if (durationCount > maxNanoseconds - frameDurationCount)
        {
            return {SpriteAnimationClipError::DurationOverflow, frameIndex};
        }

        durationCount += frameDurationCount;
        prepared.frames_.push_back(frame);
        prepared.frameBoundaries_.push_back(SpriteAnimationTime2D{durationCount});
    }

    prepared.duration_ = SpriteAnimationTime2D{durationCount};
    prepared.prepared_ = true;
    outClip = std::move(prepared);
    return {};
}

bool SpriteAnimationClip2D::Prepared() const noexcept
{
    return prepared_;
}

std::uint32_t SpriteAnimationClip2D::FrameCount() const noexcept
{
    return static_cast<std::uint32_t>(frames_.size());
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

SpriteAnimationClipStatus SpriteAnimationClip2D::ResolveFrameIndex(
    SpriteAnimationTime2D time,
    std::uint32_t& outFrameIndex) const noexcept
{
    if (!prepared_)
    {
        return {SpriteAnimationClipError::NotPrepared, 0};
    }

    if (time < SpriteAnimationTime2D{0} || time > duration_)
    {
        return {SpriteAnimationClipError::TimeOutOfRange, 0};
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

bool SpriteAnimator2D::TryGetCurrentRegionIndex(std::uint32_t& outRegionIndex) const noexcept
{
    const SpriteAnimationFrame2D* frame = CurrentFrame();
    if (frame == nullptr)
    {
        return false;
    }

    outRegionIndex = frame->regionIndex;
    return true;
}
} // namespace trace2d::runtime
