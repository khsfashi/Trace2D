#include <trace2d/profile/Profile2D.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace
{
using namespace std::chrono_literals;
using namespace trace2d::profile;

TEST(Profile2DTests, PreparesBoundedStorageAndRegistersScopesOnlyBeforeCapture)
{
    CpuProfiler2D profiler{};
    EXPECT_EQ(profiler.Prepare(2U, 3U, 4U), ProfileResult2D::Success);
    EXPECT_EQ(profiler.Prepare(2U, 3U, 4U), ProfileResult2D::AlreadyPrepared);

    ProfileScopeId2D invalid{};
    EXPECT_EQ(profiler.RegisterScope({}, invalid), ProfileResult2D::InvalidScopeName);
    EXPECT_FALSE(invalid.Valid());

    const std::string oversized(ProfileScopeNameCapacity2D + 1U, 'x');
    EXPECT_EQ(profiler.RegisterScope(oversized, invalid), ProfileResult2D::InvalidScopeName);

    ProfileScopeId2D update{};
    ProfileScopeId2D render{};
    EXPECT_EQ(profiler.RegisterScope("update", update), ProfileResult2D::Success);
    EXPECT_EQ(profiler.RegisterScope("render", render), ProfileResult2D::Success);
    EXPECT_EQ(profiler.ScopeName(update), "update");
    EXPECT_EQ(profiler.ScopeName(render), "render");

    ProfileScopeId2D duplicate{};
    EXPECT_EQ(profiler.RegisterScope("update", duplicate), ProfileResult2D::DuplicateScopeName);

    ProfileScopeId2D overflow{};
    EXPECT_EQ(profiler.RegisterScope("audio", overflow), ProfileResult2D::ScopeCapacityExceeded);

    profiler.SetEnabled(true);
    EXPECT_EQ(profiler.BeginFrame(1U, 0ns), ProfileResult2D::Success);
    EXPECT_EQ(profiler.EndFrame(1ns), ProfileResult2D::Success);

    EXPECT_EQ(profiler.RegisterScope("late", overflow), ProfileResult2D::ScopeRegistryFrozen);

    const auto metrics = profiler.Metrics();
    EXPECT_EQ(metrics.registeredScopeCount, 2U);
    EXPECT_EQ(metrics.retainedScopeCapacity, 2U);
    EXPECT_EQ(metrics.retainedFrameCapacity, 3U);
    EXPECT_EQ(metrics.retainedStackCapacity, 4U);
    EXPECT_EQ(metrics.retainedHistoryTimingSlotCount, 6U);
}

TEST(Profile2DTests, ComputesNestedInclusiveAndExclusiveTimingFromSuppliedTimestamps)
{
    CpuProfiler2D profiler{};
    ASSERT_EQ(profiler.Prepare(3U, 2U, 4U), ProfileResult2D::Success);

    ProfileScopeId2D root{};
    ProfileScopeId2D child{};
    ProfileScopeId2D untouched{};
    ASSERT_EQ(profiler.RegisterScope("root", root), ProfileResult2D::Success);
    ASSERT_EQ(profiler.RegisterScope("child", child), ProfileResult2D::Success);
    ASSERT_EQ(profiler.RegisterScope("untouched", untouched), ProfileResult2D::Success);

    profiler.SetEnabled(true);
    ASSERT_EQ(profiler.BeginFrame(7U, 100ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EnterScope(root, 110ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EnterScope(child, 120ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.ExitScope(child, 150ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.ExitScope(root, 180ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EndFrame(200ns), ProfileResult2D::Success);

    CpuProfileFrameView2D frame{};
    ASSERT_EQ(profiler.InspectFrameFromLatest(0U, frame), ProfileResult2D::Success);
    EXPECT_EQ(frame.frame.frameIndex, 7U);
    EXPECT_EQ(frame.frame.beginTimestamp, 100ns);
    EXPECT_EQ(frame.frame.endTimestamp, 200ns);
    EXPECT_EQ(frame.frame.frameDuration, 100ns);
    ASSERT_EQ(frame.scopeTimings.size(), 3U);

    const auto& rootTiming = frame.scopeTimings[root.value];
    EXPECT_EQ(rootTiming.availability, ProfileMetricAvailability2D::Available);
    EXPECT_EQ(rootTiming.callCount, 1U);
    EXPECT_EQ(rootTiming.inclusiveTime, 70ns);
    EXPECT_EQ(rootTiming.exclusiveTime, 40ns);

    const auto& childTiming = frame.scopeTimings[child.value];
    EXPECT_EQ(childTiming.availability, ProfileMetricAvailability2D::Available);
    EXPECT_EQ(childTiming.callCount, 1U);
    EXPECT_EQ(childTiming.inclusiveTime, 30ns);
    EXPECT_EQ(childTiming.exclusiveTime, 30ns);

    const auto& untouchedTiming = frame.scopeTimings[untouched.value];
    EXPECT_EQ(untouchedTiming.availability, ProfileMetricAvailability2D::NotMeasured);
    EXPECT_EQ(untouchedTiming.callCount, 0U);
    EXPECT_EQ(untouchedTiming.inclusiveTime, 0ns);
    EXPECT_EQ(untouchedTiming.exclusiveTime, 0ns);
}

TEST(Profile2DTests, SupportsRecursiveScopesAndDistinguishesMeasuredZeroFromNotMeasured)
{
    CpuProfiler2D profiler{};
    ASSERT_EQ(profiler.Prepare(2U, 2U, 4U), ProfileResult2D::Success);

    ProfileScopeId2D recursive{};
    ProfileScopeId2D untouched{};
    ASSERT_EQ(profiler.RegisterScope("recursive", recursive), ProfileResult2D::Success);
    ASSERT_EQ(profiler.RegisterScope("untouched", untouched), ProfileResult2D::Success);

    profiler.SetEnabled(true);
    ASSERT_EQ(profiler.BeginFrame(1U, 0ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EnterScope(recursive, 10ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EnterScope(recursive, 20ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.ExitScope(recursive, 30ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.ExitScope(recursive, 50ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EndFrame(60ns), ProfileResult2D::Success);

    CpuProfileFrameView2D first{};
    ASSERT_EQ(profiler.InspectFrameFromLatest(0U, first), ProfileResult2D::Success);
    const auto& recursiveTiming = first.scopeTimings[recursive.value];
    EXPECT_EQ(recursiveTiming.callCount, 2U);
    EXPECT_EQ(recursiveTiming.inclusiveTime, 50ns);
    EXPECT_EQ(recursiveTiming.exclusiveTime, 40ns);

    ASSERT_EQ(profiler.BeginFrame(2U, 100ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EnterScope(recursive, 110ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.ExitScope(recursive, 110ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EndFrame(120ns), ProfileResult2D::Success);

    CpuProfileFrameView2D second{};
    ASSERT_EQ(profiler.InspectFrameFromLatest(0U, second), ProfileResult2D::Success);
    EXPECT_EQ(second.scopeTimings[recursive.value].availability, ProfileMetricAvailability2D::Available);
    EXPECT_EQ(second.scopeTimings[recursive.value].callCount, 1U);
    EXPECT_EQ(second.scopeTimings[recursive.value].inclusiveTime, 0ns);
    EXPECT_EQ(second.scopeTimings[untouched.value].availability, ProfileMetricAvailability2D::NotMeasured);
}

TEST(Profile2DTests, DisabledRecorderPublishesNoFrameAndKeepsUnavailableVocabularyExplicit)
{
    CpuProfiler2D profiler{};
    ASSERT_EQ(profiler.Prepare(1U, 1U, 1U), ProfileResult2D::Success);

    ProfileScopeId2D scope{};
    ASSERT_EQ(profiler.RegisterScope("frame", scope), ProfileResult2D::Success);

    EXPECT_FALSE(profiler.Enabled());
    EXPECT_EQ(profiler.BeginFrame(1U, 0ns), ProfileResult2D::Disabled);

    CpuProfileFrameView2D frame{};
    EXPECT_EQ(profiler.InspectFrameFromLatest(0U, frame), ProfileResult2D::FrameNotAvailable);
    EXPECT_EQ(ToString(ProfileMetricAvailability2D::Available), "available");
    EXPECT_EQ(ToString(ProfileMetricAvailability2D::NotSupported), "not_supported");
    EXPECT_EQ(ToString(ProfileMetricAvailability2D::NotEnabled), "not_enabled");
    EXPECT_EQ(ToString(ProfileMetricAvailability2D::NotMeasured), "not_measured");
}

TEST(Profile2DTests, RejectsCapacityMismatchAndTimestampFailuresWithoutPublishingPartialFrames)
{
    CpuProfiler2D profiler{};
    ASSERT_EQ(profiler.Prepare(2U, 2U, 1U), ProfileResult2D::Success);

    ProfileScopeId2D outer{};
    ProfileScopeId2D inner{};
    ASSERT_EQ(profiler.RegisterScope("outer", outer), ProfileResult2D::Success);
    ASSERT_EQ(profiler.RegisterScope("inner", inner), ProfileResult2D::Success);
    profiler.SetEnabled(true);

    ASSERT_EQ(profiler.BeginFrame(1U, 0ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EnterScope(outer, 1ns), ProfileResult2D::Success);
    EXPECT_EQ(profiler.EnterScope(inner, 2ns), ProfileResult2D::ScopeStackCapacityExceeded);

    CpuProfileFrameView2D frame{};
    EXPECT_EQ(profiler.InspectFrameFromLatest(0U, frame), ProfileResult2D::FrameNotAvailable);

    ASSERT_EQ(profiler.BeginFrame(2U, 10ns), ProfileResult2D::Success);
    EXPECT_EQ(profiler.ExitScope(outer, 11ns), ProfileResult2D::ScopeStackEmpty);
    EXPECT_EQ(profiler.InspectFrameFromLatest(0U, frame), ProfileResult2D::FrameNotAvailable);

    ASSERT_EQ(profiler.BeginFrame(3U, 20ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EnterScope(outer, 21ns), ProfileResult2D::Success);
    EXPECT_EQ(profiler.EndFrame(22ns), ProfileResult2D::UnclosedScopes);
    EXPECT_EQ(profiler.InspectFrameFromLatest(0U, frame), ProfileResult2D::FrameNotAvailable);

    ASSERT_EQ(profiler.BeginFrame(4U, 30ns), ProfileResult2D::Success);
    EXPECT_EQ(profiler.EnterScope(outer, 29ns), ProfileResult2D::NonMonotonicTimestamp);
    EXPECT_EQ(profiler.InspectFrameFromLatest(0U, frame), ProfileResult2D::FrameNotAvailable);

    EXPECT_EQ(profiler.Metrics().invalidFrameCount, 4U);
}

TEST(Profile2DTests, RejectsMismatchedScopeExitAndRecoversOnTheNextFrame)
{
    CpuProfiler2D profiler{};
    ASSERT_EQ(profiler.Prepare(2U, 2U, 2U), ProfileResult2D::Success);

    ProfileScopeId2D outer{};
    ProfileScopeId2D inner{};
    ASSERT_EQ(profiler.RegisterScope("outer", outer), ProfileResult2D::Success);
    ASSERT_EQ(profiler.RegisterScope("inner", inner), ProfileResult2D::Success);
    profiler.SetEnabled(true);

    ASSERT_EQ(profiler.BeginFrame(1U, 0ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EnterScope(outer, 1ns), ProfileResult2D::Success);
    EXPECT_EQ(profiler.ExitScope(inner, 2ns), ProfileResult2D::ScopeMismatch);

    ASSERT_EQ(profiler.BeginFrame(2U, 10ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EnterScope(inner, 11ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.ExitScope(inner, 12ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EndFrame(13ns), ProfileResult2D::Success);

    CpuProfileFrameView2D frame{};
    ASSERT_EQ(profiler.InspectFrameFromLatest(0U, frame), ProfileResult2D::Success);
    EXPECT_EQ(frame.frame.frameIndex, 2U);
    EXPECT_EQ(profiler.Metrics().invalidFrameCount, 1U);
    EXPECT_EQ(profiler.Metrics().committedFrameCount, 1U);
}

TEST(Profile2DTests, ReusesBoundedFrameRingWithoutCapacityGrowth)
{
    CpuProfiler2D profiler{};
    ASSERT_EQ(profiler.Prepare(1U, 2U, 1U), ProfileResult2D::Success);

    ProfileScopeId2D scope{};
    ASSERT_EQ(profiler.RegisterScope("frame", scope), ProfileResult2D::Success);
    profiler.SetEnabled(true);

    const auto retainedBefore = profiler.Metrics();
    for (std::uint64_t frameIndex = 1U; frameIndex <= 3U; ++frameIndex)
    {
        const auto begin = std::chrono::nanoseconds{static_cast<std::int64_t>(frameIndex * 10U)};
        ASSERT_EQ(profiler.BeginFrame(frameIndex, begin), ProfileResult2D::Success);
        ASSERT_EQ(profiler.EnterScope(scope, begin), ProfileResult2D::Success);
        ASSERT_EQ(profiler.ExitScope(scope, begin + 1ns), ProfileResult2D::Success);
        ASSERT_EQ(profiler.EndFrame(begin + 2ns), ProfileResult2D::Success);
    }

    const auto retainedAfter = profiler.Metrics();
    EXPECT_EQ(retainedAfter.retainedScopeCapacity, retainedBefore.retainedScopeCapacity);
    EXPECT_EQ(retainedAfter.retainedFrameCapacity, retainedBefore.retainedFrameCapacity);
    EXPECT_EQ(retainedAfter.retainedStackCapacity, retainedBefore.retainedStackCapacity);
    EXPECT_EQ(retainedAfter.retainedHistoryTimingSlotCount, retainedBefore.retainedHistoryTimingSlotCount);
    EXPECT_EQ(retainedAfter.retainedFrameCount, 2U);
    EXPECT_EQ(retainedAfter.committedFrameCount, 3U);
    EXPECT_EQ(retainedAfter.overwrittenFrameCount, 1U);
    EXPECT_EQ(retainedAfter.scopeEnterCount, 3U);
    EXPECT_EQ(retainedAfter.scopeExitCount, 3U);

    CpuProfileFrameView2D latest{};
    CpuProfileFrameView2D previous{};
    ASSERT_EQ(profiler.InspectFrameFromLatest(0U, latest), ProfileResult2D::Success);
    ASSERT_EQ(profiler.InspectFrameFromLatest(1U, previous), ProfileResult2D::Success);
    EXPECT_EQ(latest.frame.frameIndex, 3U);
    EXPECT_EQ(previous.frame.frameIndex, 2U);

    CpuProfileFrameView2D unavailable{};
    EXPECT_EQ(profiler.InspectFrameFromLatest(2U, unavailable), ProfileResult2D::FrameNotAvailable);
}

TEST(Profile2DTests, ValidatesPreparationCapacityWithoutPartialPreparation)
{
    CpuProfiler2D profiler{};
    EXPECT_EQ(profiler.Prepare(0U, 1U, 1U), ProfileResult2D::InvalidCapacity);
    EXPECT_EQ(profiler.Prepare(1U, 0U, 1U), ProfileResult2D::InvalidCapacity);
    EXPECT_EQ(profiler.Prepare(1U, 1U, 0U), ProfileResult2D::InvalidCapacity);
    EXPECT_EQ(profiler.Prepare(1U, 1U, 1U), ProfileResult2D::Success);
}
} // namespace
