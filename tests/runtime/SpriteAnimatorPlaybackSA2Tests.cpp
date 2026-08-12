#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/runtime/SpriteAnimator2D.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <limits>

namespace
{
using namespace std::chrono_literals;
using trace2d::assets::SpriteAsset;
using trace2d::runtime::MakeSpriteAnimator2DState;
using trace2d::runtime::SpriteAnimationClip2D;
using trace2d::runtime::SpriteAnimationClipError;
using trace2d::runtime::SpriteAnimationDirection;
using trace2d::runtime::SpriteAnimationEmission2D;
using trace2d::runtime::SpriteAnimationEmissionKind;
using trace2d::runtime::SpriteAnimationEvent2D;
using trace2d::runtime::SpriteAnimationFrame2D;
using trace2d::runtime::SpriteAnimationLoopMode;
using trace2d::runtime::SpriteAnimationPlaybackState;
using trace2d::runtime::SpriteAnimationSpeed2D;
using trace2d::runtime::SpriteAnimator2D;
using trace2d::runtime::SpriteAnimator2DError;
using trace2d::runtime::SpriteAnimator2DState;

[[nodiscard]] SpriteAsset MakeSpriteAsset(std::size_t regionCount)
{
    SpriteAsset asset{};
    asset.regions.resize(regionCount);
    return asset;
}

[[nodiscard]] SpriteAnimationClip2D PrepareEventClip(const SpriteAsset& asset)
{
    const std::array frames{
        SpriteAnimationFrame2D{0U, 100ns},
        SpriteAnimationFrame2D{1U, 100ns},
        SpriteAnimationFrame2D{2U, 100ns},
    };
    const std::array events{
        SpriteAnimationEvent2D{14U, 250ns, 4U},
        SpriteAnimationEvent2D{13U, 100ns, 3U},
        SpriteAnimationEvent2D{10U, 0ns, 0U},
        SpriteAnimationEvent2D{12U, 100ns, 2U},
        SpriteAnimationEvent2D{11U, 50ns, 1U},
    };

    SpriteAnimationClip2D clip{};
    const auto status = SpriteAnimationClip2D::Prepare(
        &asset,
        static_cast<std::uint32_t>(asset.regions.size()),
        frames,
        events,
        clip);
    EXPECT_TRUE(status.Succeeded());
    return clip;
}

[[nodiscard]] SpriteAnimator2D MakeAnimator(
    SpriteAnimationClip2D& clip,
    std::chrono::nanoseconds time,
    SpriteAnimationLoopMode loopMode,
    SpriteAnimationDirection direction,
    SpriteAnimationSpeed2D speed = {1U, 1U})
{
    SpriteAnimator2DState state{};
    EXPECT_TRUE(MakeSpriteAnimator2DState(
                    clip,
                    time,
                    SpriteAnimationPlaybackState::Playing,
                    loopMode,
                    direction,
                    false,
                    speed,
                    state)
                    .Succeeded());

    SpriteAnimator2D animator{};
    EXPECT_TRUE(animator.RestoreState(state).Succeeded());
    return animator;
}

TEST(SpriteAnimatorPlaybackSA2Tests, PreparesAuthoredEventsInDeterministicTimelineOrder)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareEventClip(asset);

    const auto events = clip.Events();
    ASSERT_EQ(events.size(), 5U);
    EXPECT_EQ(events[0], (SpriteAnimationEvent2D{10U, 0ns, 0U}));
    EXPECT_EQ(events[1], (SpriteAnimationEvent2D{11U, 50ns, 1U}));
    EXPECT_EQ(events[2], (SpriteAnimationEvent2D{12U, 100ns, 2U}));
    EXPECT_EQ(events[3], (SpriteAnimationEvent2D{13U, 100ns, 3U}));
    EXPECT_EQ(events[4], (SpriteAnimationEvent2D{14U, 250ns, 4U}));

    const std::array frames{SpriteAnimationFrame2D{0U, 100ns}};
    const std::array invalidOffset{SpriteAnimationEvent2D{1U, 100ns, 0U}};
    SpriteAnimationClip2D invalidClip{};
    EXPECT_EQ(
        SpriteAnimationClip2D::Prepare(&asset, 3U, frames, invalidOffset, invalidClip).error,
        SpriteAnimationClipError::EventOffsetOutOfRange);

    const std::array duplicateOrdinal{
        SpriteAnimationEvent2D{1U, 10ns, 7U},
        SpriteAnimationEvent2D{2U, 10ns, 7U},
    };
    EXPECT_EQ(
        SpriteAnimationClip2D::Prepare(&asset, 3U, frames, duplicateOrdinal, invalidClip).error,
        SpriteAnimationClipError::DuplicateEventOrdinal);
}

TEST(SpriteAnimatorPlaybackSA2Tests, PreservesExactRationalRemainderAcrossSmallFixedSteps)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareEventClip(asset);
    SpriteAnimator2D animator = MakeAnimator(
        clip,
        0ns,
        SpriteAnimationLoopMode::Once,
        SpriteAnimationDirection::Forward,
        {1U, 2U});
    std::array<SpriteAnimationEmission2D, 8> output{};

    auto result = animator.Advance(1ns, output);
    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(animator.State().time, 0ns);
    EXPECT_EQ(animator.State().speedRemainder, 1U);

    result = animator.Advance(1ns, output);
    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(animator.State().time, 1ns);
    EXPECT_EQ(animator.State().speedRemainder, 0U);

    result = animator.Advance(3ns, output);
    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(animator.State().time, 2ns);
    EXPECT_EQ(animator.State().speedRemainder, 1U);
}

TEST(SpriteAnimatorPlaybackSA2Tests, RejectsScaledAdvanceOverflowWithoutMutatingState)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareEventClip(asset);
    SpriteAnimator2D animator = MakeAnimator(
        clip,
        0ns,
        SpriteAnimationLoopMode::Once,
        SpriteAnimationDirection::Forward,
        {std::numeric_limits<std::uint32_t>::max(), 1U});
    const SpriteAnimator2DState before = animator.State();
    std::array<SpriteAnimationEmission2D, 1> output{};

    const auto result = animator.Advance(
        std::chrono::nanoseconds{std::numeric_limits<std::chrono::nanoseconds::rep>::max()},
        output);

    EXPECT_EQ(result.error, SpriteAnimator2DError::AdvanceOverflow);
    EXPECT_EQ(animator.State(), before);
}

TEST(SpriteAnimatorPlaybackSA2Tests, PauseResumePreservesRemainderWhileSeekStopAndRestartResetIt)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareEventClip(asset);
    SpriteAnimator2D animator = MakeAnimator(
        clip,
        17ns,
        SpriteAnimationLoopMode::Once,
        SpriteAnimationDirection::Forward,
        {1U, 2U});
    std::array<SpriteAnimationEmission2D, 8> output{};

    ASSERT_TRUE(animator.Advance(1ns, output).Succeeded());
    ASSERT_EQ(animator.State().speedRemainder, 1U);

    ASSERT_TRUE(animator.Pause().Succeeded());
    const SpriteAnimator2DState paused = animator.State();
    EXPECT_TRUE(animator.Advance(100ns, output).Succeeded());
    EXPECT_EQ(animator.State(), paused);

    ASSERT_TRUE(animator.Play().Succeeded());
    ASSERT_TRUE(animator.Seek(100ns).Succeeded());
    EXPECT_EQ(animator.State().time, 100ns);
    EXPECT_EQ(animator.State().frameIndex, 1U);
    EXPECT_EQ(animator.State().speedRemainder, 0U);

    ASSERT_TRUE(animator.Stop().Succeeded());
    EXPECT_EQ(animator.State().time, 0ns);
    EXPECT_EQ(animator.State().playback, SpriteAnimationPlaybackState::Stopped);
    EXPECT_EQ(animator.State().speedRemainder, 0U);

    ASSERT_TRUE(animator.Restart().Succeeded());
    EXPECT_EQ(animator.State().time, 0ns);
    EXPECT_EQ(animator.State().playback, SpriteAnimationPlaybackState::Playing);
}

TEST(SpriteAnimatorPlaybackSA2Tests, EmitsForwardAndReverseEventsWithStableEqualTimeOrdinal)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareEventClip(asset);
    std::array<SpriteAnimationEmission2D, 8> output{};

    SpriteAnimator2D forward = MakeAnimator(
        clip,
        40ns,
        SpriteAnimationLoopMode::Once,
        SpriteAnimationDirection::Forward);
    auto result = forward.Advance(70ns, output);
    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.emissionCount, 3U);
    EXPECT_EQ(output[0].eventId, 11U);
    EXPECT_EQ(output[1].eventId, 12U);
    EXPECT_EQ(output[2].eventId, 13U);
    EXPECT_EQ(forward.State().time, 110ns);

    SpriteAnimator2D reverse = MakeAnimator(
        clip,
        120ns,
        SpriteAnimationLoopMode::Once,
        SpriteAnimationDirection::Reverse);
    result = reverse.Advance(80ns, output);
    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.emissionCount, 3U);
    EXPECT_EQ(output[0].eventId, 12U);
    EXPECT_EQ(output[1].eventId, 13U);
    EXPECT_EQ(output[2].eventId, 11U);
    EXPECT_EQ(reverse.State().time, 40ns);
}

TEST(SpriteAnimatorPlaybackSA2Tests, PreservesLoopOrderingIncludingOffsetZeroEvents)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareEventClip(asset);
    std::array<SpriteAnimationEmission2D, 12> output{};

    SpriteAnimator2D forward = MakeAnimator(
        clip,
        240ns,
        SpriteAnimationLoopMode::Loop,
        SpriteAnimationDirection::Forward);
    auto result = forward.Advance(110ns, output);
    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.emissionCount, 4U);
    EXPECT_EQ(output[0].eventId, 14U);
    EXPECT_EQ(output[1].kind, SpriteAnimationEmissionKind::Loop);
    EXPECT_EQ(output[2].eventId, 10U);
    EXPECT_EQ(output[3].eventId, 11U);
    EXPECT_EQ(forward.State().time, 50ns);

    SpriteAnimator2D reverse = MakeAnimator(
        clip,
        60ns,
        SpriteAnimationLoopMode::Loop,
        SpriteAnimationDirection::Reverse);
    result = reverse.Advance(110ns, output);
    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.emissionCount, 4U);
    EXPECT_EQ(output[0].eventId, 11U);
    EXPECT_EQ(output[1].eventId, 10U);
    EXPECT_EQ(output[2].kind, SpriteAnimationEmissionKind::Loop);
    EXPECT_EQ(output[3].eventId, 14U);
    EXPECT_EQ(reverse.State().time, 250ns);
}

TEST(SpriteAnimatorPlaybackSA2Tests, HandlesMultipleWrapsAndPingPongBouncesWithoutEndpointDuplication)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareEventClip(asset);
    std::array<SpriteAnimationEmission2D, 48> output{};

    SpriteAnimator2D looping = MakeAnimator(
        clip,
        250ns,
        SpriteAnimationLoopMode::Loop,
        SpriteAnimationDirection::Forward);
    auto result = looping.Advance(650ns, output);
    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(
        std::count_if(
            output.begin(),
            output.begin() + static_cast<std::ptrdiff_t>(result.emissionCount),
            [](const SpriteAnimationEmission2D& emission)
            {
                return emission.kind == SpriteAnimationEmissionKind::Loop;
            }),
        3);
    EXPECT_EQ(looping.State().time, 0ns);

    SpriteAnimator2D pingPong = MakeAnimator(
        clip,
        250ns,
        SpriteAnimationLoopMode::PingPong,
        SpriteAnimationDirection::Forward);
    result = pingPong.Advance(100ns, output);
    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.emissionCount, 2U);
    EXPECT_EQ(output[0].kind, SpriteAnimationEmissionKind::Bounce);
    EXPECT_EQ(output[0].time, 300ns);
    EXPECT_EQ(output[1].eventId, 14U);
    EXPECT_EQ(pingPong.State().time, 250ns);
    EXPECT_EQ(pingPong.State().direction, SpriteAnimationDirection::Reverse);
}

TEST(SpriteAnimatorPlaybackSA2Tests, CompletesOncePlaybackAtDirectionalEndpoint)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareEventClip(asset);
    std::array<SpriteAnimationEmission2D, 8> output{};

    SpriteAnimator2D forward = MakeAnimator(
        clip,
        250ns,
        SpriteAnimationLoopMode::Once,
        SpriteAnimationDirection::Forward);
    auto result = forward.Advance(100ns, output);
    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.emissionCount, 1U);
    EXPECT_EQ(output[0].kind, SpriteAnimationEmissionKind::Completed);
    EXPECT_EQ(forward.State().time, 300ns);
    EXPECT_EQ(forward.State().frameIndex, 2U);
    EXPECT_TRUE(forward.State().completed);
    EXPECT_EQ(forward.State().playback, SpriteAnimationPlaybackState::Paused);
    EXPECT_EQ(forward.Play().error, SpriteAnimator2DError::InvalidPlaybackTransition);

    SpriteAnimator2D reverse = MakeAnimator(
        clip,
        40ns,
        SpriteAnimationLoopMode::Once,
        SpriteAnimationDirection::Reverse);
    result = reverse.Advance(100ns, output);
    ASSERT_TRUE(result.Succeeded());
    ASSERT_GE(result.emissionCount, 1U);
    EXPECT_EQ(output[result.emissionCount - 1U].kind, SpriteAnimationEmissionKind::Completed);
    EXPECT_EQ(reverse.State().time, 0ns);
    EXPECT_EQ(reverse.State().frameIndex, 0U);
    EXPECT_TRUE(reverse.State().completed);
}

TEST(SpriteAnimatorPlaybackSA2Tests, OutputCapacityExhaustionIsTransactional)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareEventClip(asset);
    SpriteAnimator2D animator = MakeAnimator(
        clip,
        240ns,
        SpriteAnimationLoopMode::Loop,
        SpriteAnimationDirection::Forward);
    const SpriteAnimator2DState before = animator.State();
    std::array<SpriteAnimationEmission2D, 2> output{};

    const auto result = animator.Advance(110ns, output);

    EXPECT_EQ(result.error, SpriteAnimator2DError::OutputCapacityExceeded);
    EXPECT_EQ(animator.State(), before);
}

TEST(SpriteAnimatorPlaybackSA2Tests, ZeroSpeedDoesNotTraverseTimeline)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareEventClip(asset);
    SpriteAnimator2D animator = MakeAnimator(
        clip,
        50ns,
        SpriteAnimationLoopMode::Loop,
        SpriteAnimationDirection::Forward,
        {0U, 7U});
    std::array<SpriteAnimationEmission2D, 8> output{};

    const auto result = animator.Advance(10s, output);

    EXPECT_TRUE(result.Succeeded());
    EXPECT_EQ(result.emissionCount, 0U);
    EXPECT_EQ(animator.State().time, 50ns);
    EXPECT_EQ(animator.State().speed, (SpriteAnimationSpeed2D{0U, 1U}));
    EXPECT_EQ(animator.State().speedRemainder, 0U);
}
} // namespace
