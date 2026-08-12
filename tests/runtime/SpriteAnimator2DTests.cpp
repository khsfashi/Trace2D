#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/runtime/SpriteAnimator2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <limits>

namespace
{
using namespace std::chrono_literals;
using trace2d::assets::SpriteAsset;
using trace2d::runtime::SpriteAnimationClip2D;
using trace2d::runtime::SpriteAnimationClipError;
using trace2d::runtime::SpriteAnimationDirection;
using trace2d::runtime::SpriteAnimationFrame2D;
using trace2d::runtime::SpriteAnimationLoopMode;
using trace2d::runtime::SpriteAnimationPlaybackState;
using trace2d::runtime::SpriteAnimationRegionSelection2D;
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

[[nodiscard]] SpriteAnimationClip2D PrepareVariableClip(const SpriteAsset& asset)
{
    const std::array frames{
        SpriteAnimationFrame2D{2U, 100ms},
        SpriteAnimationFrame2D{4U, 50ms},
        SpriteAnimationFrame2D{1U, 150ms},
    };

    SpriteAnimationClip2D clip{};
    const auto status = SpriteAnimationClip2D::Prepare(
        &asset,
        static_cast<std::uint32_t>(asset.regions.size()),
        frames,
        clip);
    EXPECT_TRUE(status.Succeeded());
    return clip;
}

TEST(SpriteAnimator2DTests, PreparesVariableDurationsAndCachesExactBoundaries)
{
    const SpriteAsset asset = MakeSpriteAsset(5U);
    SpriteAnimationClip2D clip = PrepareVariableClip(asset);

    ASSERT_TRUE(clip.Prepared());
    EXPECT_EQ(clip.SpriteAsset(), &asset);
    EXPECT_EQ(clip.SpriteRegionCount(), 5U);
    EXPECT_EQ(clip.FrameCount(), 3U);
    EXPECT_EQ(clip.Duration(), 300ms);

    const auto boundaries = clip.FrameBoundaries();
    ASSERT_EQ(boundaries.size(), 4U);
    EXPECT_EQ(boundaries[0], 0ns);
    EXPECT_EQ(boundaries[1], 100ms);
    EXPECT_EQ(boundaries[2], 150ms);
    EXPECT_EQ(boundaries[3], 300ms);

    const auto frames = clip.Frames();
    ASSERT_EQ(frames.size(), 3U);
    EXPECT_EQ(frames[0].regionIndex, 2U);
    EXPECT_EQ(frames[1].regionIndex, 4U);
    EXPECT_EQ(frames[2].regionIndex, 1U);
}

TEST(SpriteAnimator2DTests, ResolvesHalfOpenBoundariesAndTerminalFrameExactly)
{
    const SpriteAsset asset = MakeSpriteAsset(5U);
    SpriteAnimationClip2D clip = PrepareVariableClip(asset);
    std::uint32_t frameIndex = 99U;

    EXPECT_TRUE(clip.ResolveFrameIndex(0ns, frameIndex).Succeeded());
    EXPECT_EQ(frameIndex, 0U);

    EXPECT_TRUE(clip.ResolveFrameIndex(99ms, frameIndex).Succeeded());
    EXPECT_EQ(frameIndex, 0U);

    EXPECT_TRUE(clip.ResolveFrameIndex(100ms, frameIndex).Succeeded());
    EXPECT_EQ(frameIndex, 1U);

    EXPECT_TRUE(clip.ResolveFrameIndex(150ms, frameIndex).Succeeded());
    EXPECT_EQ(frameIndex, 2U);

    EXPECT_TRUE(clip.ResolveFrameIndex(299ms, frameIndex).Succeeded());
    EXPECT_EQ(frameIndex, 2U);

    EXPECT_TRUE(clip.ResolveFrameIndex(300ms, frameIndex).Succeeded());
    EXPECT_EQ(frameIndex, 2U);

    EXPECT_EQ(clip.ResolveFrameIndex(-1ns, frameIndex).error, SpriteAnimationClipError::TimeOutOfRange);
    EXPECT_EQ(clip.ResolveFrameIndex(300ms + 1ns, frameIndex).error, SpriteAnimationClipError::TimeOutOfRange);
}

TEST(SpriteAnimator2DTests, RejectsInvalidClipInputsWithoutReplacingPreparedOutput)
{
    const SpriteAsset asset = MakeSpriteAsset(5U);
    SpriteAnimationClip2D clip = PrepareVariableClip(asset);
    const auto originalDuration = clip.Duration();
    const auto* originalAsset = clip.SpriteAsset();

    const std::array<SpriteAnimationFrame2D, 0> emptyFrames{};
    EXPECT_EQ(
        SpriteAnimationClip2D::Prepare(&asset, 5U, emptyFrames, clip).error,
        SpriteAnimationClipError::EmptyFrames);
    EXPECT_EQ(clip.Duration(), originalDuration);
    EXPECT_EQ(clip.SpriteAsset(), originalAsset);

    const std::array zeroDurationFrames{
        SpriteAnimationFrame2D{0U, 0ns},
    };
    EXPECT_EQ(
        SpriteAnimationClip2D::Prepare(&asset, 5U, zeroDurationFrames, clip).error,
        SpriteAnimationClipError::NonPositiveFrameDuration);
    EXPECT_EQ(clip.Duration(), originalDuration);

    const std::array invalidRegionFrames{
        SpriteAnimationFrame2D{3U, 1ns},
    };
    EXPECT_EQ(
        SpriteAnimationClip2D::Prepare(&asset, 3U, invalidRegionFrames, clip).error,
        SpriteAnimationClipError::RegionIndexOutOfRange);
    EXPECT_EQ(clip.Duration(), originalDuration);

    const auto maxDuration = std::chrono::nanoseconds{std::numeric_limits<std::chrono::nanoseconds::rep>::max()};
    const std::array overflowFrames{
        SpriteAnimationFrame2D{0U, maxDuration},
        SpriteAnimationFrame2D{0U, 1ns},
    };
    EXPECT_EQ(
        SpriteAnimationClip2D::Prepare(&asset, 1U, overflowFrames, clip).error,
        SpriteAnimationClipError::DurationOverflow);
    EXPECT_EQ(clip.Duration(), originalDuration);

    const std::array validSingleFrame{
        SpriteAnimationFrame2D{0U, 1ns},
    };
    EXPECT_EQ(
        SpriteAnimationClip2D::Prepare(nullptr, 1U, validSingleFrame, clip).error,
        SpriteAnimationClipError::NullSpriteAsset);
    EXPECT_EQ(clip.Duration(), originalDuration);
    EXPECT_EQ(clip.SpriteAsset(), originalAsset);
}

TEST(SpriteAnimator2DTests, NormalizesExactSpeedWithoutFloatingPointAuthority)
{
    SpriteAnimationSpeed2D speed{};

    EXPECT_TRUE(trace2d::runtime::NormalizeSpriteAnimationSpeed({2U, 4U}, speed));
    EXPECT_EQ(speed, (SpriteAnimationSpeed2D{1U, 2U}));
    EXPECT_TRUE(trace2d::runtime::IsCanonicalSpriteAnimationSpeed(speed));

    EXPECT_TRUE(trace2d::runtime::NormalizeSpriteAnimationSpeed({0U, 25U}, speed));
    EXPECT_EQ(speed, (SpriteAnimationSpeed2D{0U, 1U}));

    EXPECT_FALSE(trace2d::runtime::NormalizeSpriteAnimationSpeed({1U, 0U}, speed));
    EXPECT_FALSE(trace2d::runtime::IsCanonicalSpriteAnimationSpeed({2U, 4U}));
}

TEST(SpriteAnimator2DTests, BuildsTypedAuthoritativeStateAndResolvesCurrentRegion)
{
    const SpriteAsset asset = MakeSpriteAsset(5U);
    SpriteAnimationClip2D clip = PrepareVariableClip(asset);
    SpriteAnimator2DState state{};

    const auto status = trace2d::runtime::MakeSpriteAnimator2DState(
        clip,
        100ms,
        SpriteAnimationPlaybackState::Playing,
        SpriteAnimationLoopMode::Loop,
        SpriteAnimationDirection::Forward,
        false,
        {2U, 4U},
        state);

    ASSERT_TRUE(status.Succeeded());
    EXPECT_EQ(state.clip, &clip);
    EXPECT_EQ(state.time, 100ms);
    EXPECT_EQ(state.frameIndex, 1U);
    EXPECT_EQ(state.playback, SpriteAnimationPlaybackState::Playing);
    EXPECT_EQ(state.loopMode, SpriteAnimationLoopMode::Loop);
    EXPECT_EQ(state.direction, SpriteAnimationDirection::Forward);
    EXPECT_FALSE(state.completed);
    EXPECT_EQ(state.speed, (SpriteAnimationSpeed2D{1U, 2U}));

    SpriteAnimator2D animator{};
    ASSERT_TRUE(animator.RestoreState(state).Succeeded());
    ASSERT_TRUE(animator.HasState());
    ASSERT_NE(animator.CurrentFrame(), nullptr);
    EXPECT_EQ(animator.CurrentFrame()->duration, 50ms);

    SpriteAnimationRegionSelection2D selection{};
    ASSERT_TRUE(animator.TryGetCurrentRegion(selection));
    EXPECT_EQ(selection.asset, &asset);
    EXPECT_EQ(selection.regionIndex, 4U);

    std::uint32_t regionIndex = 0;
    ASSERT_TRUE(animator.TryGetCurrentRegionIndex(regionIndex));
    EXPECT_EQ(regionIndex, 4U);
}

TEST(SpriteAnimator2DTests, RejectsInconsistentFrameAndPreservesPriorState)
{
    const SpriteAsset asset = MakeSpriteAsset(5U);
    SpriteAnimationClip2D clip = PrepareVariableClip(asset);
    SpriteAnimator2DState validState{};
    ASSERT_TRUE(trace2d::runtime::MakeSpriteAnimator2DState(
                    clip,
                    150ms,
                    SpriteAnimationPlaybackState::Paused,
                    SpriteAnimationLoopMode::Once,
                    SpriteAnimationDirection::Forward,
                    false,
                    {1U, 1U},
                    validState)
                    .Succeeded());

    SpriteAnimator2D animator{};
    ASSERT_TRUE(animator.RestoreState(validState).Succeeded());

    SpriteAnimator2DState invalidState = validState;
    invalidState.frameIndex = 1U;
    const auto invalidStatus = animator.RestoreState(invalidState);

    EXPECT_EQ(invalidStatus.error, SpriteAnimator2DError::FrameIndexMismatch);
    EXPECT_EQ(invalidStatus.expectedFrameIndex, 2U);
    EXPECT_EQ(animator.State(), validState);
}

TEST(SpriteAnimator2DTests, ValidatesCompletionAsExplicitNonLoopEndpointState)
{
    const SpriteAsset asset = MakeSpriteAsset(5U);
    SpriteAnimationClip2D clip = PrepareVariableClip(asset);
    SpriteAnimator2DState state{};

    EXPECT_TRUE(trace2d::runtime::MakeSpriteAnimator2DState(
                    clip,
                    clip.Duration(),
                    SpriteAnimationPlaybackState::Paused,
                    SpriteAnimationLoopMode::Once,
                    SpriteAnimationDirection::Forward,
                    true,
                    {1U, 1U},
                    state)
                    .Succeeded());
    EXPECT_EQ(state.frameIndex, 2U);

    EXPECT_EQ(
        trace2d::runtime::MakeSpriteAnimator2DState(
            clip,
            150ms,
            SpriteAnimationPlaybackState::Paused,
            SpriteAnimationLoopMode::Once,
            SpriteAnimationDirection::Forward,
            true,
            {1U, 1U},
            state)
            .error,
        SpriteAnimator2DError::InvalidCompletionState);

    EXPECT_EQ(
        trace2d::runtime::MakeSpriteAnimator2DState(
            clip,
            clip.Duration(),
            SpriteAnimationPlaybackState::Paused,
            SpriteAnimationLoopMode::Loop,
            SpriteAnimationDirection::Forward,
            true,
            {1U, 1U},
            state)
            .error,
        SpriteAnimator2DError::InvalidCompletionState);

    EXPECT_TRUE(trace2d::runtime::MakeSpriteAnimator2DState(
                    clip,
                    0ns,
                    SpriteAnimationPlaybackState::Stopped,
                    SpriteAnimationLoopMode::Once,
                    SpriteAnimationDirection::Reverse,
                    true,
                    {1U, 1U},
                    state)
                    .Succeeded());
    EXPECT_EQ(state.frameIndex, 0U);
}

TEST(SpriteAnimator2DTests, RejectsInvalidTypedStateValuesAndUnpreparedClip)
{
    SpriteAnimationClip2D clip{};
    SpriteAnimator2DState state{};

    EXPECT_EQ(
        trace2d::runtime::MakeSpriteAnimator2DState(
            clip,
            0ns,
            SpriteAnimationPlaybackState::Stopped,
            SpriteAnimationLoopMode::Once,
            SpriteAnimationDirection::Forward,
            false,
            {1U, 1U},
            state)
            .error,
        SpriteAnimator2DError::UnpreparedClip);

    const SpriteAsset asset = MakeSpriteAsset(5U);
    SpriteAnimationClip2D preparedClip = PrepareVariableClip(asset);
    ASSERT_TRUE(trace2d::runtime::MakeSpriteAnimator2DState(
                    preparedClip,
                    0ns,
                    SpriteAnimationPlaybackState::Stopped,
                    SpriteAnimationLoopMode::Once,
                    SpriteAnimationDirection::Forward,
                    false,
                    {1U, 1U},
                    state)
                    .Succeeded());

    state.playback = static_cast<SpriteAnimationPlaybackState>(255U);
    EXPECT_EQ(trace2d::runtime::ValidateSpriteAnimator2DState(state).error, SpriteAnimator2DError::InvalidPlaybackState);

    state.playback = SpriteAnimationPlaybackState::Stopped;
    state.speed = {1U, 0U};
    EXPECT_EQ(trace2d::runtime::ValidateSpriteAnimator2DState(state).error, SpriteAnimator2DError::InvalidSpeed);
}
} // namespace
