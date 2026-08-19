#include <trace2d/runtime/Tween2D.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <limits>

namespace
{
using namespace std::chrono_literals;
using namespace trace2d::runtime;

[[nodiscard]] TweenSpec2D FloatSpec(
    const TweenTimeDomain2D domain = TweenTimeDomain2D::Presentation,
    const TweenTime2D duration = 100ns)
{
    TweenSpec2D spec{};
    spec.domain = domain;
    spec.duration = duration;
    spec.start = TweenValue2D::Float(0.0F);
    spec.end = TweenValue2D::Float(10.0F);
    return spec;
}

[[nodiscard]] TweenState2D Inspect(const TweenPool2D& pool, const TweenHandle2D handle)
{
    TweenState2D state{};
    EXPECT_TRUE(pool.Inspect(handle, state).Succeeded());
    return state;
}

TEST(Tween2DTests, ValidatesFixedTypedSpecsAndInterpolatesWithoutVariantConversion)
{
    TweenSpec2D spec = FloatSpec();
    EXPECT_TRUE(ValidateTweenSpec2D(spec).Succeeded());

    spec.delay = -1ns;
    EXPECT_EQ(ValidateTweenSpec2D(spec).error, Tween2DError::NegativeDelay);
    spec.delay = 0ns;
    spec.duration = 0ns;
    EXPECT_EQ(ValidateTweenSpec2D(spec).error, Tween2DError::NonPositiveDuration);
    spec.duration = 1ns;
    spec.end = TweenValue2D::Float2(1.0F, 2.0F);
    EXPECT_EQ(ValidateTweenSpec2D(spec).error, Tween2DError::ValueTypeMismatch);

    const TweenValue2D float2 = InterpolateTweenValue2D(
        TweenValue2D::Float2(2.0F, 4.0F),
        TweenValue2D::Float2(6.0F, 12.0F),
        0.25);
    EXPECT_EQ(float2.type, TweenValueType2D::Float2);
    EXPECT_FLOAT_EQ(float2.components[0], 3.0F);
    EXPECT_FLOAT_EQ(float2.components[1], 6.0F);

    const TweenValue2D color = InterpolateTweenValue2D(
        TweenValue2D::Color(0.0F, 0.25F, 0.5F, 1.0F),
        TweenValue2D::Color(1.0F, 0.75F, 1.0F, 0.0F),
        0.5);
    EXPECT_EQ(color.type, TweenValueType2D::Color);
    EXPECT_FLOAT_EQ(color.components[0], 0.5F);
    EXPECT_FLOAT_EQ(color.components[1], 0.5F);
    EXPECT_FLOAT_EQ(color.components[2], 0.75F);
    EXPECT_FLOAT_EQ(color.components[3], 0.5F);
}

TEST(Tween2DTests, FreezesNamedEasingFormulas)
{
    EXPECT_DOUBLE_EQ(EvaluateTweenEasing2D(TweenEasing2D::Linear, 0.25), 0.25);
    EXPECT_DOUBLE_EQ(EvaluateTweenEasing2D(TweenEasing2D::EaseInQuad, 0.25), 0.0625);
    EXPECT_DOUBLE_EQ(EvaluateTweenEasing2D(TweenEasing2D::EaseOutQuad, 0.25), 0.4375);
    EXPECT_DOUBLE_EQ(EvaluateTweenEasing2D(TweenEasing2D::EaseInOutQuad, 0.25), 0.125);
    EXPECT_DOUBLE_EQ(EvaluateTweenEasing2D(TweenEasing2D::EaseInOutQuad, 0.75), 0.875);
    EXPECT_DOUBLE_EQ(EvaluateTweenEasing2D(TweenEasing2D::EaseInCubic, 0.5), 0.125);
    EXPECT_DOUBLE_EQ(EvaluateTweenEasing2D(TweenEasing2D::EaseOutCubic, 0.5), 0.875);
    EXPECT_DOUBLE_EQ(EvaluateTweenEasing2D(TweenEasing2D::EaseInOutCubic, 0.25), 0.0625);
    EXPECT_DOUBLE_EQ(EvaluateTweenEasing2D(TweenEasing2D::EaseInOutCubic, 0.75), 0.9375);
    EXPECT_DOUBLE_EQ(EvaluateTweenEasing2D(TweenEasing2D::Linear, -1.0), 0.0);
    EXPECT_DOUBLE_EQ(EvaluateTweenEasing2D(TweenEasing2D::Linear, 2.0), 1.0);
}

TEST(Tween2DTests, StepsOnlyTheExplicitTimeDomainAndKeepsDelayBoundaryExact)
{
    TweenPool2D pool{};

    TweenSpec2D simulation = FloatSpec(TweenTimeDomain2D::Simulation);
    simulation.delay = 10ns;
    TweenHandle2D simulationHandle{};
    ASSERT_TRUE(pool.Create(simulation, simulationHandle).Succeeded());

    TweenSpec2D presentation = FloatSpec(TweenTimeDomain2D::Presentation);
    TweenHandle2D presentationHandle{};
    ASSERT_TRUE(pool.Create(presentation, presentationHandle).Succeeded());

    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, 50ns).Succeeded());
    TweenState2D presentationState = Inspect(pool, presentationHandle);
    EXPECT_FLOAT_EQ(presentationState.currentValue.components[0], 5.0F);
    EXPECT_EQ(presentationState.loopElapsed, 50ns);

    TweenState2D simulationState = Inspect(pool, simulationHandle);
    EXPECT_EQ(simulationState.delayElapsed, 0ns);
    EXPECT_EQ(simulationState.loopElapsed, 0ns);
    EXPECT_FLOAT_EQ(simulationState.currentValue.components[0], 0.0F);

    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Simulation, 10ns).Succeeded());
    simulationState = Inspect(pool, simulationHandle);
    EXPECT_EQ(simulationState.delayElapsed, 10ns);
    EXPECT_EQ(simulationState.loopElapsed, 0ns);
    EXPECT_FLOAT_EQ(simulationState.currentValue.components[0], 0.0F);

    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Simulation, 25ns).Succeeded());
    simulationState = Inspect(pool, simulationHandle);
    EXPECT_EQ(simulationState.delayElapsed, 10ns);
    EXPECT_EQ(simulationState.loopElapsed, 25ns);
    EXPECT_FLOAT_EQ(simulationState.currentValue.components[0], 2.5F);
}

TEST(Tween2DTests, CompletesAtExactDurationBoundary)
{
    TweenPool2D pool{};
    TweenHandle2D handle{};
    ASSERT_TRUE(pool.Create(FloatSpec(), handle).Succeeded());

    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, 99ns).Succeeded());
    TweenState2D state = Inspect(pool, handle);
    EXPECT_EQ(state.playback, TweenPlaybackState2D::Playing);
    EXPECT_EQ(state.loopElapsed, 99ns);
    EXPECT_FLOAT_EQ(state.currentValue.components[0], 9.9F);

    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, 1ns).Succeeded());
    state = Inspect(pool, handle);
    EXPECT_EQ(state.playback, TweenPlaybackState2D::Completed);
    EXPECT_EQ(state.loopElapsed, 100ns);
    EXPECT_EQ(state.loopIndex, 0U);
    EXPECT_FLOAT_EQ(state.currentValue.components[0], 10.0F);
    EXPECT_EQ(pool.Metrics().activeCount, 0U);
}

TEST(Tween2DTests, RepeatAndYoyoBoundariesRemainContinuousAndPauseIsClockless)
{
    TweenPool2D pool{};
    TweenSpec2D spec = FloatSpec();
    spec.repeatCount = 2U;
    spec.loopMode = TweenLoopMode2D::Yoyo;

    TweenHandle2D handle{};
    ASSERT_TRUE(pool.Create(spec, handle).Succeeded());

    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, 100ns).Succeeded());
    TweenState2D state = Inspect(pool, handle);
    EXPECT_EQ(state.loopIndex, 1U);
    EXPECT_EQ(state.loopElapsed, 0ns);
    EXPECT_TRUE(state.reverse);
    EXPECT_FLOAT_EQ(state.currentValue.components[0], 10.0F);

    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, 25ns).Succeeded());
    state = Inspect(pool, handle);
    EXPECT_FLOAT_EQ(state.currentValue.components[0], 7.5F);

    ASSERT_TRUE(pool.Pause(handle).Succeeded());
    const TweenState2D paused = Inspect(pool, handle);
    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, 50ns).Succeeded());
    EXPECT_EQ(Inspect(pool, handle), paused);

    ASSERT_TRUE(pool.Resume(handle).Succeeded());
    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, 75ns).Succeeded());
    state = Inspect(pool, handle);
    EXPECT_EQ(state.loopIndex, 2U);
    EXPECT_EQ(state.loopElapsed, 0ns);
    EXPECT_FALSE(state.reverse);
    EXPECT_FLOAT_EQ(state.currentValue.components[0], 0.0F);

    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, 100ns).Succeeded());
    state = Inspect(pool, handle);
    EXPECT_EQ(state.playback, TweenPlaybackState2D::Completed);
    EXPECT_EQ(state.loopIndex, 2U);
    EXPECT_FLOAT_EQ(state.currentValue.components[0], 10.0F);

    ASSERT_TRUE(pool.Restart(handle).Succeeded());
    state = Inspect(pool, handle);
    EXPECT_EQ(state.playback, TweenPlaybackState2D::Playing);
    EXPECT_EQ(state.loopIndex, 0U);
    EXPECT_EQ(state.loopElapsed, 0ns);
    EXPECT_FLOAT_EQ(state.currentValue.components[0], 0.0F);
    EXPECT_EQ(pool.Metrics().activeCount, 1U);

    ASSERT_TRUE(pool.Cancel(handle).Succeeded());
    state = Inspect(pool, handle);
    EXPECT_EQ(state.playback, TweenPlaybackState2D::Cancelled);
    EXPECT_EQ(state.cancellationReason, TweenCancellationReason2D::Explicit);
    EXPECT_EQ(pool.Metrics().activeCount, 0U);
}

TEST(Tween2DTests, LargeDeltaCrossesManyLoopsWithoutPerBoundaryIteration)
{
    TweenPool2D pool{};
    TweenSpec2D finite = FloatSpec(TweenTimeDomain2D::Simulation, 10ns);
    finite.repeatCount = 4U;

    TweenHandle2D finiteHandle{};
    ASSERT_TRUE(pool.Create(finite, finiteHandle).Succeeded());
    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Simulation, 37ns).Succeeded());
    TweenState2D state = Inspect(pool, finiteHandle);
    EXPECT_EQ(state.loopIndex, 3U);
    EXPECT_EQ(state.loopElapsed, 7ns);
    EXPECT_FLOAT_EQ(state.currentValue.components[0], 7.0F);

    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Simulation, 13ns).Succeeded());
    state = Inspect(pool, finiteHandle);
    EXPECT_EQ(state.playback, TweenPlaybackState2D::Completed);
    EXPECT_EQ(state.loopIndex, 4U);
    EXPECT_FLOAT_EQ(state.currentValue.components[0], 10.0F);

    TweenSpec2D infinite = FloatSpec(TweenTimeDomain2D::Presentation, 1ns);
    infinite.infinite = true;
    TweenHandle2D infiniteHandle{};
    ASSERT_TRUE(pool.Create(infinite, infiniteHandle).Succeeded());
    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, 1'000'000ns).Succeeded());
    state = Inspect(pool, infiniteHandle);
    EXPECT_EQ(state.loopIndex, 1'000'000U);
    EXPECT_EQ(state.loopElapsed, 0ns);
    EXPECT_EQ(state.playback, TweenPlaybackState2D::Playing);
}

TEST(Tween2DTests, InvalidDeltaAndLoopCounterOverflowDoNotMutateTheAffectedTween)
{
    TweenPool2D pool{};
    TweenSpec2D spec = FloatSpec(TweenTimeDomain2D::Presentation, 1ns);
    spec.infinite = true;

    TweenHandle2D handle{};
    ASSERT_TRUE(pool.Create(spec, handle).Succeeded());
    const TweenState2D initial = Inspect(pool, handle);
    EXPECT_EQ(pool.Step(TweenTimeDomain2D::Presentation, -1ns).error, Tween2DError::NegativeDelta);
    EXPECT_EQ(Inspect(pool, handle), initial);

    constexpr TweenTime2D::rep MaxTicks = std::numeric_limits<TweenTime2D::rep>::max();
    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, TweenTime2D{MaxTicks}).Succeeded());
    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, TweenTime2D{MaxTicks}).Succeeded());
    const TweenState2D beforeOverflow = Inspect(pool, handle);
    EXPECT_EQ(beforeOverflow.loopIndex, std::numeric_limits<std::uint64_t>::max() - 1U);

    EXPECT_EQ(
        pool.Step(TweenTimeDomain2D::Presentation, 2ns).error,
        Tween2DError::LoopCounterOverflow);
    EXPECT_EQ(Inspect(pool, handle), beforeOverflow);
}

TEST(Tween2DTests, RetainedPoolReusesSlotsAndRejectsStaleGenerations)
{
    TweenPool2D pool{};
    pool.Reserve(2U);

    TweenHandle2D first{};
    TweenHandle2D second{};
    ASSERT_TRUE(pool.Create(FloatSpec(), first).Succeeded());
    ASSERT_TRUE(pool.Create(FloatSpec(), second).Succeeded());
    const TweenPoolMetrics2D filled = pool.Metrics();
    EXPECT_EQ(filled.activeCount, 2U);
    EXPECT_EQ(filled.retainedSlotCount, 2U);
    EXPECT_EQ(filled.retainedCapacity, 2U);
    EXPECT_EQ(filled.highWaterActiveCount, 2U);

    ASSERT_TRUE(pool.Cancel(first).Succeeded());
    TweenHandle2D recycled{};
    ASSERT_TRUE(pool.Create(FloatSpec(), recycled).Succeeded());
    EXPECT_EQ(recycled.index, first.index);
    EXPECT_NE(recycled.generation, first.generation);

    TweenState2D staleState{};
    EXPECT_EQ(pool.Inspect(first, staleState).error, Tween2DError::InvalidHandle);
    EXPECT_EQ(pool.Pause(first).error, Tween2DError::InvalidHandle);

    const TweenPoolMetrics2D reused = pool.Metrics();
    EXPECT_EQ(reused.activeCount, 2U);
    EXPECT_EQ(reused.retainedSlotCount, 2U);
    EXPECT_EQ(reused.retainedCapacity, 2U);
    EXPECT_EQ(reused.highWaterActiveCount, 2U);
    EXPECT_EQ(reused.createdCount, 3U);
    EXPECT_EQ(reused.reusedSlotCount, 1U);

    const std::uint64_t retainedCapacity = reused.retainedCapacity;
    for (std::uint32_t step = 0U; step < 50U; ++step)
    {
        ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, 1ns).Succeeded());
    }
    EXPECT_EQ(pool.Metrics().retainedCapacity, retainedCapacity);
}

TEST(Tween2DTests, InvalidPlaybackTransitionsFailClosed)
{
    TweenPool2D pool{};
    TweenHandle2D handle{};
    ASSERT_TRUE(pool.Create(FloatSpec(), handle).Succeeded());

    EXPECT_EQ(pool.Resume(handle).error, Tween2DError::InvalidPlaybackTransition);
    ASSERT_TRUE(pool.Pause(handle).Succeeded());
    EXPECT_EQ(pool.Pause(handle).error, Tween2DError::InvalidPlaybackTransition);
    ASSERT_TRUE(pool.Resume(handle).Succeeded());
    ASSERT_TRUE(pool.Cancel(handle).Succeeded());
    EXPECT_EQ(pool.Cancel(handle).error, Tween2DError::InvalidPlaybackTransition);
    EXPECT_EQ(pool.Resume(handle).error, Tween2DError::InvalidPlaybackTransition);
}
} // namespace
