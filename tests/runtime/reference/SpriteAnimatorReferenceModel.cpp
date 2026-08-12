#include "SpriteAnimatorReferenceModel.hpp"

#include <stdexcept>

namespace trace2d::testing
{
namespace
{
using namespace std::chrono_literals;
using runtime::SpriteAnimationDirection;
using runtime::SpriteAnimationEmission2D;
using runtime::SpriteAnimationEmissionKind;
using runtime::SpriteAnimationLoopMode;
using runtime::SpriteAnimationPlaybackState;
using runtime::SpriteAnimationTime2D;
} // namespace

SpriteAnimatorReferenceModel::SpriteAnimatorReferenceModel(runtime::SpriteAnimator2DState state) noexcept
    : state_(state)
{
}

const runtime::SpriteAnimator2DState& SpriteAnimatorReferenceModel::State() const noexcept
{
    return state_;
}

std::vector<SpriteAnimationEmission2D> SpriteAnimatorReferenceModel::Advance(SpriteAnimationTime2D delta)
{
    if (delta.count() < 0)
    {
        throw std::invalid_argument{"reference animator does not accept negative delta"};
    }

    std::vector<SpriteAnimationEmission2D> transcript{};
    if (state_.playback != SpriteAnimationPlaybackState::Playing || state_.completed)
    {
        return transcript;
    }

    if (state_.speed.numerator == 0U)
    {
        state_.speedRemainder = 0U;
        return transcript;
    }

    const auto inputNanoseconds = static_cast<std::uint64_t>(delta.count());
    for (std::uint64_t input = 0; input < inputNanoseconds; ++input)
    {
        if (state_.playback != SpriteAnimationPlaybackState::Playing || state_.completed)
        {
            break;
        }

        const std::uint64_t accumulated =
            static_cast<std::uint64_t>(state_.speedRemainder) + state_.speed.numerator;
        const std::uint64_t timelineSteps = accumulated / state_.speed.denominator;
        state_.speedRemainder = static_cast<std::uint32_t>(accumulated % state_.speed.denominator);

        for (std::uint64_t step = 0; step < timelineSteps; ++step)
        {
            if (state_.playback != SpriteAnimationPlaybackState::Playing || state_.completed)
            {
                break;
            }
            AdvanceOneTimelineNanosecond(transcript);
        }
    }

    RefreshFrameIndex();
    return transcript;
}

bool SpriteAnimatorReferenceModel::PrepareTimelineStep(std::vector<SpriteAnimationEmission2D>& transcript)
{
    while (state_.playback == SpriteAnimationPlaybackState::Playing && !state_.completed)
    {
        if (state_.direction == SpriteAnimationDirection::Forward && state_.time == state_.clip->Duration())
        {
            HandleForwardEndpoint(transcript);
            continue;
        }

        if (state_.direction == SpriteAnimationDirection::Reverse && state_.time == SpriteAnimationTime2D{0})
        {
            HandleReverseEndpoint(transcript);
            continue;
        }

        return true;
    }

    return false;
}

void SpriteAnimatorReferenceModel::AdvanceOneTimelineNanosecond(
    std::vector<SpriteAnimationEmission2D>& transcript)
{
    if (!PrepareTimelineStep(transcript))
    {
        return;
    }

    if (state_.direction == SpriteAnimationDirection::Forward)
    {
        state_.time += 1ns;
        EmitEventsAt(state_.time, state_.direction, transcript);
        if (state_.time == state_.clip->Duration())
        {
            HandleForwardEndpoint(transcript);
        }
        return;
    }

    state_.time -= 1ns;
    EmitEventsAt(state_.time, state_.direction, transcript);
    if (state_.time == SpriteAnimationTime2D{0})
    {
        HandleReverseEndpoint(transcript);
    }
}

void SpriteAnimatorReferenceModel::HandleForwardEndpoint(std::vector<SpriteAnimationEmission2D>& transcript)
{
    const SpriteAnimationTime2D duration = state_.clip->Duration();

    if (state_.loopMode == SpriteAnimationLoopMode::Once)
    {
        EmitStructural(SpriteAnimationEmissionKind::Completed, duration, state_.direction, transcript);
        state_.completed = true;
        state_.playback = SpriteAnimationPlaybackState::Paused;
        state_.speedRemainder = 0U;
        return;
    }

    if (state_.loopMode == SpriteAnimationLoopMode::Loop)
    {
        EmitStructural(SpriteAnimationEmissionKind::Loop, duration, state_.direction, transcript);
        state_.time = SpriteAnimationTime2D{0};
        EmitEventsAt(state_.time, state_.direction, transcript);
        return;
    }

    EmitStructural(SpriteAnimationEmissionKind::Bounce, duration, state_.direction, transcript);
    state_.direction = SpriteAnimationDirection::Reverse;
}

void SpriteAnimatorReferenceModel::HandleReverseEndpoint(std::vector<SpriteAnimationEmission2D>& transcript)
{
    constexpr SpriteAnimationTime2D zero{0};

    if (state_.loopMode == SpriteAnimationLoopMode::Once)
    {
        EmitStructural(SpriteAnimationEmissionKind::Completed, zero, state_.direction, transcript);
        state_.completed = true;
        state_.playback = SpriteAnimationPlaybackState::Paused;
        state_.speedRemainder = 0U;
        return;
    }

    if (state_.loopMode == SpriteAnimationLoopMode::Loop)
    {
        EmitStructural(SpriteAnimationEmissionKind::Loop, zero, state_.direction, transcript);
        state_.time = state_.clip->Duration();
        return;
    }

    EmitStructural(SpriteAnimationEmissionKind::Bounce, zero, state_.direction, transcript);
    state_.direction = SpriteAnimationDirection::Forward;
}

void SpriteAnimatorReferenceModel::EmitEventsAt(
    SpriteAnimationTime2D time,
    SpriteAnimationDirection direction,
    std::vector<SpriteAnimationEmission2D>& transcript) const
{
    for (const runtime::SpriteAnimationEvent2D& event : state_.clip->Events())
    {
        if (event.offset != time)
        {
            continue;
        }

        transcript.push_back({
            SpriteAnimationEmissionKind::AuthoredEvent,
            event.eventId,
            event.authoredOrdinal,
            event.offset,
            direction,
        });
    }
}

void SpriteAnimatorReferenceModel::EmitStructural(
    SpriteAnimationEmissionKind kind,
    SpriteAnimationTime2D time,
    SpriteAnimationDirection direction,
    std::vector<SpriteAnimationEmission2D>& transcript) const
{
    transcript.push_back({kind, 0U, 0U, time, direction});
}

void SpriteAnimatorReferenceModel::RefreshFrameIndex() noexcept
{
    if (state_.time == state_.clip->Duration())
    {
        state_.frameIndex = state_.clip->FrameCount() - 1U;
        return;
    }

    SpriteAnimationTime2D boundary{0};
    const auto frames = state_.clip->Frames();
    for (std::size_t index = 0; index < frames.size(); ++index)
    {
        boundary += frames[index].duration;
        if (state_.time < boundary)
        {
            state_.frameIndex = static_cast<std::uint32_t>(index);
            return;
        }
    }
}
} // namespace trace2d::testing
