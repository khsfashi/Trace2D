#include <trace2d/runtime/Tween2D.hpp>

#include <gtest/gtest.h>

#include <chrono>

namespace
{
using namespace std::chrono_literals;
using namespace trace2d::runtime;

TEST(Tween2DControlTests, RebaseFreezesCaptureAtActivationAndCancellationReasonIsTyped)
{
    TweenPool2D pool{};
    TweenSpec2D spec{};
    spec.domain = TweenTimeDomain2D::Presentation;
    spec.delay = 10ns;
    spec.duration = 10ns;
    spec.start = TweenValue2D::Float(0.0F);
    spec.end = TweenValue2D::Float(10.0F);

    TweenHandle2D handle{};
    ASSERT_TRUE(pool.Create(spec, handle).Succeeded());
    ASSERT_TRUE(pool.Rebase(handle, TweenValue2D::Float(5.0F), TweenValue2D::Float(15.0F)).Succeeded());

    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, 10ns).Succeeded());
    TweenState2D state{};
    ASSERT_TRUE(pool.Inspect(handle, state).Succeeded());
    EXPECT_FLOAT_EQ(state.currentValue.components[0], 5.0F);
    EXPECT_EQ(state.loopElapsed, 0ns);

    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, 1ns).Succeeded());
    ASSERT_TRUE(pool.Inspect(handle, state).Succeeded());
    EXPECT_FLOAT_EQ(state.currentValue.components[0], 6.0F);
    EXPECT_EQ(
        pool.Rebase(handle, TweenValue2D::Float(7.0F), TweenValue2D::Float(17.0F)).error,
        Tween2DError::InvalidPlaybackTransition);

    ASSERT_TRUE(pool.Cancel(handle, TweenCancellationReason2D::TargetInvalidated).Succeeded());
    ASSERT_TRUE(pool.Inspect(handle, state).Succeeded());
    EXPECT_EQ(state.playback, TweenPlaybackState2D::Cancelled);
    EXPECT_EQ(state.cancellationReason, TweenCancellationReason2D::TargetInvalidated);
}

TEST(Tween2DControlTests, CompletedBoundSampleCanFailClosedWithoutDoubleDecrement)
{
    TweenPool2D pool{};
    TweenSpec2D spec{};
    spec.duration = 1ns;
    spec.start = TweenValue2D::Float(0.0F);
    spec.end = TweenValue2D::Float(1.0F);

    TweenHandle2D handle{};
    ASSERT_TRUE(pool.Create(spec, handle).Succeeded());
    ASSERT_TRUE(pool.Step(TweenTimeDomain2D::Presentation, 1ns).Succeeded());
    EXPECT_EQ(pool.Metrics().activeCount, 0U);

    ASSERT_TRUE(pool.Cancel(handle, TweenCancellationReason2D::PropertyWriteRejected).Succeeded());
    TweenState2D state{};
    ASSERT_TRUE(pool.Inspect(handle, state).Succeeded());
    EXPECT_EQ(state.playback, TweenPlaybackState2D::Cancelled);
    EXPECT_EQ(state.cancellationReason, TweenCancellationReason2D::PropertyWriteRejected);
    EXPECT_EQ(pool.Metrics().activeCount, 0U);
    EXPECT_EQ(pool.Cancel(handle).error, Tween2DError::InvalidPlaybackTransition);
}

TEST(Tween2DControlTests, CancellationRejectsNoneReason)
{
    TweenPool2D pool{};
    TweenSpec2D spec{};
    spec.duration = 1ns;
    spec.start = TweenValue2D::Float(0.0F);
    spec.end = TweenValue2D::Float(1.0F);

    TweenHandle2D handle{};
    ASSERT_TRUE(pool.Create(spec, handle).Succeeded());
    EXPECT_EQ(
        pool.Cancel(handle, TweenCancellationReason2D::None).error,
        Tween2DError::InvalidCancellationReason);
}
} // namespace
