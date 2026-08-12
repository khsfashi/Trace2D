#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/runtime/SpriteAnimator2D.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace
{
using namespace std::chrono_literals;
using trace2d::assets::SpriteAsset;
using trace2d::runtime::MakeSpriteAnimator2DState;
using trace2d::runtime::SpriteAnimationClip2D;
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

struct Fixture final
{
    SpriteAsset asset{};
    SpriteAnimationClip2D clip{};
    SpriteAnimator2D animator{};
};

struct SemanticState final
{
    std::int64_t timeNs{0};
    std::uint32_t frameIndex{0};
    SpriteAnimationPlaybackState playback{SpriteAnimationPlaybackState::Stopped};
    SpriteAnimationLoopMode loopMode{SpriteAnimationLoopMode::Once};
    SpriteAnimationDirection direction{SpriteAnimationDirection::Forward};
    bool completed{false};
    SpriteAnimationSpeed2D speed{};
    std::uint32_t speedRemainder{0};

    [[nodiscard]] bool operator==(const SemanticState&) const noexcept = default;
};

[[nodiscard]] SemanticState Observe(const SpriteAnimator2D& animator)
{
    const auto& state = animator.State();
    return {
        state.time.count(),
        state.frameIndex,
        state.playback,
        state.loopMode,
        state.direction,
        state.completed,
        state.speed,
        state.speedRemainder,
    };
}

void PrepareFixture(
    Fixture& fixture,
    const std::span<const SpriteAnimationFrame2D> frames,
    const std::span<const SpriteAnimationEvent2D> events,
    const std::chrono::nanoseconds startTime,
    const SpriteAnimationLoopMode loopMode,
    const SpriteAnimationDirection direction,
    const SpriteAnimationSpeed2D speed)
{
    fixture.asset.regions.resize(frames.size());
    ASSERT_TRUE(SpriteAnimationClip2D::Prepare(
                    &fixture.asset,
                    static_cast<std::uint32_t>(fixture.asset.regions.size()),
                    frames,
                    events,
                    fixture.clip)
                    .Succeeded());

    SpriteAnimator2DState state{};
    ASSERT_TRUE(MakeSpriteAnimator2DState(
                    fixture.clip,
                    startTime,
                    SpriteAnimationPlaybackState::Playing,
                    loopMode,
                    direction,
                    false,
                    speed,
                    state)
                    .Succeeded());
    ASSERT_TRUE(fixture.animator.RestoreState(state).Succeeded());
}

[[nodiscard]] std::vector<SpriteAnimationEmission2D> AdvanceAndCollect(
    SpriteAnimator2D& animator,
    const std::span<const std::chrono::nanoseconds> deltas)
{
    std::vector<SpriteAnimationEmission2D> transcript{};
    std::array<SpriteAnimationEmission2D, 256> output{};

    for (const auto delta : deltas)
    {
        const auto result = animator.Advance(delta, output);
        EXPECT_TRUE(result.Succeeded());
        transcript.insert(
            transcript.end(),
            output.begin(),
            output.begin() + static_cast<std::ptrdiff_t>(result.emissionCount));
    }
    return transcript;
}

TEST(SpriteAnimationConformanceSA4Tests, ReplaysLongRunningBoundaryHeavySequenceExactly)
{
    const std::array frames{
        SpriteAnimationFrame2D{0U, 80ms},
        SpriteAnimationFrame2D{1U, 120ms},
        SpriteAnimationFrame2D{2U, 100ms},
    };
    const std::array events{
        SpriteAnimationEvent2D{10U, 0ms, 0U},
        SpriteAnimationEvent2D{11U, 50ms, 1U},
        SpriteAnimationEvent2D{12U, 200ms, 2U},
        SpriteAnimationEvent2D{13U, 200ms, 3U},
        SpriteAnimationEvent2D{14U, 299ms, 4U},
    };

    Fixture first{};
    Fixture second{};
    PrepareFixture(first, frames, events, 0ns, SpriteAnimationLoopMode::Loop, SpriteAnimationDirection::Forward, {2U, 3U});
    PrepareFixture(second, frames, events, 0ns, SpriteAnimationLoopMode::Loop, SpriteAnimationDirection::Forward, {2U, 3U});

    std::array<SpriteAnimationEmission2D, 32> firstOutput{};
    std::array<SpriteAnimationEmission2D, 32> secondOutput{};
    for (std::uint32_t step = 0; step < 6'000U; ++step)
    {
        const auto firstResult = first.animator.Advance(16'666'667ns, firstOutput);
        const auto secondResult = second.animator.Advance(16'666'667ns, secondOutput);
        ASSERT_TRUE(firstResult.Succeeded());
        ASSERT_TRUE(secondResult.Succeeded());
        ASSERT_EQ(firstResult.emissionCount, secondResult.emissionCount);
        for (std::size_t index = 0; index < firstResult.emissionCount; ++index)
        {
            EXPECT_EQ(firstOutput[index], secondOutput[index]);
        }
        EXPECT_EQ(Observe(first.animator), Observe(second.animator));
    }
}

TEST(SpriteAnimationConformanceSA4Tests, SplitAndAggregateAdvancesHaveEquivalentStateAndTranscript)
{
    const std::array frames{
        SpriteAnimationFrame2D{0U, 100ms},
        SpriteAnimationFrame2D{1U, 100ms},
        SpriteAnimationFrame2D{2U, 100ms},
    };
    const std::array events{
        SpriteAnimationEvent2D{20U, 0ms, 0U},
        SpriteAnimationEvent2D{21U, 50ms, 1U},
        SpriteAnimationEvent2D{22U, 100ms, 2U},
        SpriteAnimationEvent2D{23U, 100ms, 3U},
        SpriteAnimationEvent2D{24U, 250ms, 4U},
    };

    Fixture aggregate{};
    Fixture split{};
    PrepareFixture(aggregate, frames, events, 240ms, SpriteAnimationLoopMode::Loop, SpriteAnimationDirection::Forward, {3U, 2U});
    PrepareFixture(split, frames, events, 240ms, SpriteAnimationLoopMode::Loop, SpriteAnimationDirection::Forward, {3U, 2U});

    const std::array<std::chrono::nanoseconds, 1> aggregateDelta{425ms};
    const std::array<std::chrono::nanoseconds, 3> splitDeltas{100ms, 200ms, 125ms};
    const auto aggregateTranscript = AdvanceAndCollect(aggregate.animator, aggregateDelta);
    const auto splitTranscript = AdvanceAndCollect(split.animator, splitDeltas);

    EXPECT_EQ(splitTranscript, aggregateTranscript);
    EXPECT_EQ(Observe(split.animator), Observe(aggregate.animator));
}

TEST(SpriteAnimationConformanceSA4Tests, LongRunRationalSpeedMatchesExactQuotientAndRemainder)
{
    const std::array frames{SpriteAnimationFrame2D{0U, 10s}};
    const std::array<SpriteAnimationEvent2D, 0> events{};
    Fixture fixture{};
    PrepareFixture(fixture, frames, events, 0ns, SpriteAnimationLoopMode::Once, SpriteAnimationDirection::Forward, {7U, 13U});

    std::array<SpriteAnimationEmission2D, 1> output{};
    constexpr std::uint64_t steps = 100'000U;
    for (std::uint64_t step = 0; step < steps; ++step)
    {
        const auto result = fixture.animator.Advance(1ns, output);
        ASSERT_TRUE(result.Succeeded());
        ASSERT_EQ(result.emissionCount, 0U);
    }

    constexpr std::uint64_t scaled = steps * 7U;
    EXPECT_EQ(fixture.animator.State().time, std::chrono::nanoseconds{scaled / 13U});
    EXPECT_EQ(fixture.animator.State().speedRemainder, scaled % 13U);
    EXPECT_EQ(fixture.animator.State().frameIndex, 0U);
    EXPECT_FALSE(fixture.animator.State().completed);
}

TEST(SpriteAnimationConformanceSA4Tests, LargePingPongAdvanceReplaysBounceAndEventOrderExactly)
{
    const std::array frames{
        SpriteAnimationFrame2D{0U, 50ms},
        SpriteAnimationFrame2D{1U, 50ms},
        SpriteAnimationFrame2D{2U, 50ms},
        SpriteAnimationFrame2D{3U, 50ms},
    };
    const std::array events{
        SpriteAnimationEvent2D{30U, 25ms, 0U},
        SpriteAnimationEvent2D{31U, 75ms, 1U},
        SpriteAnimationEvent2D{32U, 125ms, 2U},
        SpriteAnimationEvent2D{33U, 175ms, 3U},
    };

    Fixture first{};
    Fixture second{};
    PrepareFixture(first, frames, events, 150ms, SpriteAnimationLoopMode::PingPong, SpriteAnimationDirection::Forward, {1U, 1U});
    PrepareFixture(second, frames, events, 150ms, SpriteAnimationLoopMode::PingPong, SpriteAnimationDirection::Forward, {1U, 1U});

    const std::array<std::chrono::nanoseconds, 1> delta{1'250ms};
    const auto firstTranscript = AdvanceAndCollect(first.animator, delta);
    const auto secondTranscript = AdvanceAndCollect(second.animator, delta);

    EXPECT_EQ(firstTranscript, secondTranscript);
    EXPECT_EQ(Observe(first.animator), Observe(second.animator));
    EXPECT_GE(
        std::count_if(
            firstTranscript.begin(),
            firstTranscript.end(),
            [](const SpriteAnimationEmission2D& emission)
            {
                return emission.kind == SpriteAnimationEmissionKind::Bounce;
            }),
        6);
}

TEST(SpriteAnimationConformanceSA4Tests, CapacityFailureRemainsTransactionalUnderMultiWrapStress)
{
    const std::array frames{
        SpriteAnimationFrame2D{0U, 100ms},
        SpriteAnimationFrame2D{1U, 100ms},
        SpriteAnimationFrame2D{2U, 100ms},
    };
    const std::array events{
        SpriteAnimationEvent2D{40U, 0ms, 0U},
        SpriteAnimationEvent2D{41U, 50ms, 1U},
        SpriteAnimationEvent2D{42U, 150ms, 2U},
        SpriteAnimationEvent2D{43U, 250ms, 3U},
    };

    Fixture fixture{};
    PrepareFixture(fixture, frames, events, 250ms, SpriteAnimationLoopMode::Loop, SpriteAnimationDirection::Reverse, {1U, 1U});
    const SemanticState before = Observe(fixture.animator);
    std::array<SpriteAnimationEmission2D, 1> output{};

    const auto result = fixture.animator.Advance(1'250ms, output);

    EXPECT_EQ(result.error, SpriteAnimator2DError::OutputCapacityExceeded);
    EXPECT_EQ(Observe(fixture.animator), before);
}

TEST(SpriteAnimationConformanceSA4Tests, RestartAndSeekProduceRepeatableFutureTranscriptWithoutHistoricalReplay)
{
    const std::array frames{
        SpriteAnimationFrame2D{0U, 100ms},
        SpriteAnimationFrame2D{1U, 100ms},
        SpriteAnimationFrame2D{2U, 100ms},
    };
    const std::array events{
        SpriteAnimationEvent2D{50U, 0ms, 0U},
        SpriteAnimationEvent2D{51U, 50ms, 1U},
        SpriteAnimationEvent2D{52U, 100ms, 2U},
        SpriteAnimationEvent2D{53U, 250ms, 3U},
    };

    Fixture fixture{};
    PrepareFixture(fixture, frames, events, 0ns, SpriteAnimationLoopMode::Loop, SpriteAnimationDirection::Forward, {1U, 1U});

    ASSERT_TRUE(fixture.animator.Seek(240ms).Succeeded());
    const std::array<std::chrono::nanoseconds, 1> delta{110ms};
    const auto first = AdvanceAndCollect(fixture.animator, delta);

    ASSERT_TRUE(fixture.animator.Restart().Succeeded());
    ASSERT_TRUE(fixture.animator.Seek(240ms).Succeeded());
    const auto second = AdvanceAndCollect(fixture.animator, delta);

    EXPECT_EQ(second, first);
}
} // namespace
