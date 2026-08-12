#include <trace2d/agent/Inspection.hpp>
#include <trace2d/assets/SpriteAssets.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>

namespace
{
using namespace std::chrono_literals;
using trace2d::agent::AgentFacade;
using trace2d::agent::SpriteAnimationAction;
using trace2d::agent::SpriteAnimationActionKind;
using trace2d::agent::SpriteAnimationAssertion;
using trace2d::agent::SpriteAnimationAssertionField;
using trace2d::agent::SpriteAnimationInspectionErrorCode;
using trace2d::agent::SpriteAnimationValue;
using trace2d::agent::SpriteAnimatorBinding;
using trace2d::assets::SpriteAsset;
using trace2d::runtime::MakeSpriteAnimator2DState;
using trace2d::runtime::SpriteAnimationClip2D;
using trace2d::runtime::SpriteAnimationDirection;
using trace2d::runtime::SpriteAnimationEmissionKind;
using trace2d::runtime::SpriteAnimationEvent2D;
using trace2d::runtime::SpriteAnimationFrame2D;
using trace2d::runtime::SpriteAnimationLoopMode;
using trace2d::runtime::SpriteAnimationPlaybackState;
using trace2d::runtime::SpriteAnimationSpeed2D;
using trace2d::runtime::SpriteAnimator2D;
using trace2d::runtime::SpriteAnimator2DState;

[[nodiscard]] SpriteAsset MakeSpriteAsset(const std::size_t regionCount)
{
    SpriteAsset asset{};
    asset.regions.resize(regionCount);
    return asset;
}

[[nodiscard]] SpriteAnimationClip2D PrepareClip(const SpriteAsset& asset)
{
    const std::array frames{
        SpriteAnimationFrame2D{0U, 100ns},
        SpriteAnimationFrame2D{1U, 100ns},
        SpriteAnimationFrame2D{2U, 100ns},
    };
    const std::array events{
        SpriteAnimationEvent2D{11U, 50ns, 0U},
        SpriteAnimationEvent2D{12U, 100ns, 1U},
        SpriteAnimationEvent2D{13U, 100ns, 2U},
        SpriteAnimationEvent2D{14U, 250ns, 3U},
    };

    SpriteAnimationClip2D clip{};
    EXPECT_TRUE(SpriteAnimationClip2D::Prepare(
                    &asset,
                    static_cast<std::uint32_t>(asset.regions.size()),
                    frames,
                    events,
                    clip)
                    .Succeeded());
    return clip;
}

[[nodiscard]] SpriteAnimator2D MakeAnimator(
    SpriteAnimationClip2D& clip,
    const std::chrono::nanoseconds time = 40ns,
    const SpriteAnimationLoopMode loopMode = SpriteAnimationLoopMode::Loop,
    const SpriteAnimationDirection direction = SpriteAnimationDirection::Forward,
    const SpriteAnimationSpeed2D speed = {1U, 1U})
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

TEST(SpriteAnimationInspectionTests, InspectsAuthoritativeAnimatorStateWithoutRenderer)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareClip(asset);
    SpriteAnimator2D animator = MakeAnimator(clip, 100ns, SpriteAnimationLoopMode::PingPong);
    AgentFacade agent{};

    const auto result = agent.InspectSpriteAnimator(SpriteAnimatorBinding{"hero", &animator});

    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.snapshot.has_value());
    EXPECT_EQ(result.snapshot->entitySemanticId, "hero");
    EXPECT_EQ(result.snapshot->clipDurationNanoseconds, 300);
    EXPECT_EQ(result.snapshot->clipFrameCount, 3U);
    EXPECT_EQ(result.snapshot->clipEventCount, 4U);
    EXPECT_EQ(result.snapshot->timeNanoseconds, 100);
    EXPECT_EQ(result.snapshot->frameIndex, 1U);
    EXPECT_EQ(result.snapshot->regionIndex, 1U);
    EXPECT_EQ(result.snapshot->playback, SpriteAnimationPlaybackState::Playing);
    EXPECT_EQ(result.snapshot->loopMode, SpriteAnimationLoopMode::PingPong);
    EXPECT_EQ(result.snapshot->direction, SpriteAnimationDirection::Forward);
    EXPECT_FALSE(result.snapshot->completed);
    EXPECT_EQ(result.snapshot->speedNumerator, 1U);
    EXPECT_EQ(result.snapshot->speedDenominator, 1U);
    EXPECT_EQ(result.snapshot->speedRemainder, 0U);
}

TEST(SpriteAnimationInspectionTests, AdvanceReturnsExactOrderedEmissionEvidence)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareClip(asset);
    SpriteAnimator2D animator = MakeAnimator(clip, 40ns, SpriteAnimationLoopMode::Once);
    AgentFacade agent{};

    const SpriteAnimationAction action{
        .kind = SpriteAnimationActionKind::Advance,
        .time = 70ns,
        .emissionCapacity = 4U,
    };
    const auto result = agent.ActOnSpriteAnimator(SpriteAnimatorBinding{"hero", &animator}, action);

    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.snapshot.has_value());
    ASSERT_EQ(result.emissions.size(), 3U);
    EXPECT_EQ(result.emissions[0].kind, SpriteAnimationEmissionKind::AuthoredEvent);
    EXPECT_EQ(result.emissions[0].eventId, 11U);
    EXPECT_EQ(result.emissions[1].eventId, 12U);
    EXPECT_EQ(result.emissions[2].eventId, 13U);
    EXPECT_EQ(result.snapshot->timeNanoseconds, 110);
    EXPECT_EQ(result.snapshot->frameIndex, 1U);
    EXPECT_EQ(animator.State().time, 110ns);
}

TEST(SpriteAnimationInspectionTests, AdvanceCapacityFailurePreservesAnimatorState)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareClip(asset);
    SpriteAnimator2D animator = MakeAnimator(clip, 40ns, SpriteAnimationLoopMode::Once);
    const SpriteAnimator2DState before = animator.State();
    AgentFacade agent{};

    const SpriteAnimationAction action{
        .kind = SpriteAnimationActionKind::Advance,
        .time = 70ns,
        .emissionCapacity = 1U,
    };
    const auto result = agent.ActOnSpriteAnimator(SpriteAnimatorBinding{"hero", &animator}, action);

    ASSERT_FALSE(result.Succeeded());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->code, SpriteAnimationInspectionErrorCode::OutputCapacityExceeded);
    EXPECT_TRUE(result.emissions.empty());
    EXPECT_EQ(animator.State(), before);
    ASSERT_TRUE(result.snapshot.has_value());
    EXPECT_EQ(result.snapshot->timeNanoseconds, 40);
}

TEST(SpriteAnimationInspectionTests, ActionsDelegateToRuntimeAuthorityAndDoNotReplayEvents)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareClip(asset);
    SpriteAnimator2D animator = MakeAnimator(clip);
    AgentFacade agent{};
    const SpriteAnimatorBinding binding{"hero", &animator};

    auto result = agent.ActOnSpriteAnimator(binding, SpriteAnimationAction{.kind = SpriteAnimationActionKind::Pause});
    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.snapshot->playback, SpriteAnimationPlaybackState::Paused);
    EXPECT_TRUE(result.emissions.empty());

    result = agent.ActOnSpriteAnimator(binding, SpriteAnimationAction{
        .kind = SpriteAnimationActionKind::Seek,
        .time = 250ns,
    });
    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.snapshot->timeNanoseconds, 250);
    EXPECT_EQ(result.snapshot->frameIndex, 2U);
    EXPECT_TRUE(result.emissions.empty());

    result = agent.ActOnSpriteAnimator(binding, SpriteAnimationAction{
        .kind = SpriteAnimationActionKind::SetSpeed,
        .speed = {3U, 2U},
    });
    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.snapshot->speedNumerator, 3U);
    EXPECT_EQ(result.snapshot->speedDenominator, 2U);

    result = agent.ActOnSpriteAnimator(binding, SpriteAnimationAction{
        .kind = SpriteAnimationActionKind::SetDirection,
        .direction = SpriteAnimationDirection::Reverse,
    });
    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.snapshot->direction, SpriteAnimationDirection::Reverse);

    result = agent.ActOnSpriteAnimator(binding, SpriteAnimationAction{.kind = SpriteAnimationActionKind::Restart});
    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(result.snapshot->timeNanoseconds, 300);
    EXPECT_EQ(result.snapshot->playback, SpriteAnimationPlaybackState::Playing);
    EXPECT_TRUE(result.emissions.empty());
}

TEST(SpriteAnimationInspectionTests, ExactAssertionsReturnExpectedObservedAndBoundedContext)
{
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareClip(asset);
    SpriteAnimator2D animator = MakeAnimator(clip, 100ns, SpriteAnimationLoopMode::PingPong);
    AgentFacade agent{};
    const SpriteAnimatorBinding binding{"hero", &animator};

    auto result = agent.AssertSpriteAnimator(binding, SpriteAnimationAssertion{
        .field = SpriteAnimationAssertionField::FrameIndex,
        .expected = SpriteAnimationValue::Unsigned(1U),
    });
    ASSERT_TRUE(result.Succeeded());
    ASSERT_TRUE(result.observed.has_value());
    EXPECT_EQ(*result.observed, SpriteAnimationValue::Unsigned(1U));

    result = agent.AssertSpriteAnimator(binding, SpriteAnimationAssertion{
        .field = SpriteAnimationAssertionField::Playback,
        .expected = SpriteAnimationValue::String("paused"),
    });
    ASSERT_FALSE(result.Succeeded());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->code, SpriteAnimationInspectionErrorCode::StateMismatch);
    ASSERT_TRUE(result.observed.has_value());
    EXPECT_EQ(*result.observed, SpriteAnimationValue::String("playing"));
    EXPECT_EQ(result.context.entitySemanticId, "hero");
    EXPECT_EQ(result.context.timeNanoseconds, 100);
    EXPECT_EQ(result.context.frameIndex, 1U);
    EXPECT_EQ(result.context.regionIndex, 1U);

    result = agent.AssertSpriteAnimator(binding, SpriteAnimationAssertion{
        .field = SpriteAnimationAssertionField::Completed,
        .expected = SpriteAnimationValue::Unsigned(0U),
    });
    ASSERT_FALSE(result.Succeeded());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->code, SpriteAnimationInspectionErrorCode::TypeMismatch);
    ASSERT_TRUE(result.observed.has_value());
    EXPECT_EQ(*result.observed, SpriteAnimationValue::Boolean(false));
}

TEST(SpriteAnimationInspectionTests, ReportsStableBindingFailures)
{
    AgentFacade agent{};
    const auto missing = agent.InspectSpriteAnimator(SpriteAnimatorBinding{"hero", nullptr});
    ASSERT_FALSE(missing.Succeeded());
    ASSERT_TRUE(missing.error.has_value());
    EXPECT_EQ(missing.error->code, SpriteAnimationInspectionErrorCode::AnimatorUnavailable);

    SpriteAnimator2D animator{};
    const auto empty = agent.InspectSpriteAnimator(SpriteAnimatorBinding{"hero", &animator});
    ASSERT_FALSE(empty.Succeeded());
    ASSERT_TRUE(empty.error.has_value());
    EXPECT_EQ(empty.error->code, SpriteAnimationInspectionErrorCode::AnimatorStateUnavailable);
}
} // namespace
