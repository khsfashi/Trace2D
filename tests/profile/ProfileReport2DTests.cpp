#include <trace2d/profile/ProfileReport2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <limits>
#include <string>
#include <string_view>

namespace
{
using namespace std::chrono_literals;
using namespace trace2d::profile;

StructuralProfileSnapshot2D PreparedStructuralSnapshot()
{
    StructuralProfileSnapshot2D snapshot{};
    if (snapshot.Prepare(2U) != StructuralProfileResult2D::Success)
    {
        return snapshot;
    }
    static_cast<void>(snapshot.AddMetric(
        "renderer.draw_calls",
        "1",
        StructuralProfileMetricKind2D::Counter,
        ProfileMetricAvailability2D::Available,
        7U));
    return snapshot;
}

ProfileReportContext2D TestContext()
{
    return ProfileReportContext2D{
        .engineVersion = "0.1.0",
        .sourceRevision = "perf4-test",
        .workload = "profile-report-regression",
        .buildConfiguration = "Debug",
        .operatingSystem = "test-os",
        .architecture = "test-arch",
        .compiler = "test-compiler",
        .cpuIdentityAvailability = ProfileMetricAvailability2D::NotMeasured,
        .cpuIdentity = {},
        .rendererBackend = "headless",
        .timingSource = "deterministic-test-clock",
        .fixedTimestepNanoseconds = 16'666'667U,
        .warmupFrameCount = 2U,
        .requestedSampleFrameCount = 3U,
    };
}

void ExpectContains(const std::string& text, const std::string_view expected)
{
    EXPECT_NE(text.find(expected), std::string::npos) << "missing JSON fragment: " << expected;
}

TEST(ProfileReport2DTests, ComposesWrappedCpuHistoryChronologicallyWithExactAggregates)
{
    auto structural = PreparedStructuralSnapshot();
    ASSERT_TRUE(structural.Prepared());

    CpuProfiler2D profiler{};
    ASSERT_EQ(profiler.Prepare(3U, 2U, 4U), ProfileResult2D::Success);
    ProfileScopeId2D root{};
    ProfileScopeId2D child{};
    ProfileScopeId2D untouched{};
    ASSERT_EQ(profiler.RegisterScope("root", root), ProfileResult2D::Success);
    ASSERT_EQ(profiler.RegisterScope("child", child), ProfileResult2D::Success);
    ASSERT_EQ(profiler.RegisterScope("untouched", untouched), ProfileResult2D::Success);
    profiler.SetEnabled(true);

    ASSERT_EQ(profiler.BeginFrame(1U, 0ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EnterScope(root, 0ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.ExitScope(root, 1ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EndFrame(2ns), ProfileResult2D::Success);

    ASSERT_EQ(profiler.BeginFrame(2U, 100ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EnterScope(root, 110ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EnterScope(child, 120ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.ExitScope(child, 150ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.ExitScope(root, 180ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EndFrame(200ns), ProfileResult2D::Success);

    ASSERT_EQ(profiler.BeginFrame(3U, 300ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EnterScope(root, 310ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.ExitScope(root, 310ns), ProfileResult2D::Success);
    ASSERT_EQ(profiler.EndFrame(320ns), ProfileResult2D::Success);

    const auto profilerMetricsBefore = profiler.Metrics();
    const auto structuralMetricsBefore = structural.StorageMetrics();

    std::string jsonText = "previous";
    ASSERT_EQ(BuildProfileReportJson(structural, profiler, TestContext(), jsonText), ProfileReportResult2D::Success);

    ExpectContains(jsonText, "\"schema\":\"trace2d.profile.report.v1\"");
    ExpectContains(jsonText, "\"cpu_identity\":{\"availability\":\"not_measured\",\"value\":null}");
    ExpectContains(jsonText, "\"structural\":{\"schema\":\"trace2d.profile.structural.v1\"");
    ExpectContains(jsonText, "\"name\":\"renderer.draw_calls\"");
    ExpectContains(jsonText, "\"cpu\":{\"availability\":\"available\"");
    ExpectContains(jsonText, "\"overwritten_frame_count\":1");
    ExpectContains(jsonText, "\"gpu\":{\"availability\":\"not_supported\",\"backend\":\"headless\"");

    const auto frame2Position = jsonText.find("\"frame_index\":2");
    const auto frame3Position = jsonText.find("\"frame_index\":3");
    ASSERT_NE(frame2Position, std::string::npos);
    ASSERT_NE(frame3Position, std::string::npos);
    EXPECT_LT(frame2Position, frame3Position);
    EXPECT_EQ(jsonText.find("\"frame_index\":1"), std::string::npos);

    ExpectContains(
        jsonText,
        "{\"name\":\"root\",\"availability\":\"available\",\"measured_frame_count\":2,"
        "\"call_count\":2,\"inclusive_total_ns\":70,\"exclusive_total_ns\":40,"
        "\"min_inclusive_ns\":0,\"max_inclusive_ns\":70}");
    ExpectContains(
        jsonText,
        "{\"name\":\"child\",\"availability\":\"available\",\"measured_frame_count\":1,"
        "\"call_count\":1,\"inclusive_total_ns\":30,\"exclusive_total_ns\":30,"
        "\"min_inclusive_ns\":30,\"max_inclusive_ns\":30}");
    ExpectContains(
        jsonText,
        "{\"name\":\"untouched\",\"availability\":\"not_measured\",\"measured_frame_count\":0,"
        "\"call_count\":0,\"inclusive_total_ns\":0,\"exclusive_total_ns\":0,"
        "\"min_inclusive_ns\":0,\"max_inclusive_ns\":0}");

    EXPECT_EQ(profiler.Metrics(), profilerMetricsBefore);
    EXPECT_EQ(structural.StorageMetrics(), structuralMetricsBefore);
}

TEST(ProfileReport2DTests, DistinguishesDisabledAndEnabledButUnmeasuredCpuCapture)
{
    auto structural = PreparedStructuralSnapshot();
    ASSERT_TRUE(structural.Prepared());

    CpuProfiler2D profiler{};
    ASSERT_EQ(profiler.Prepare(1U, 1U, 1U), ProfileResult2D::Success);
    ProfileScopeId2D frameScope{};
    ASSERT_EQ(profiler.RegisterScope("frame", frameScope), ProfileResult2D::Success);

    std::string disabledJson{};
    ASSERT_EQ(BuildProfileReportJson(structural, profiler, TestContext(), disabledJson), ProfileReportResult2D::Success);
    ExpectContains(disabledJson, "\"cpu\":{\"availability\":\"not_enabled\"");
    ExpectContains(
        disabledJson,
        "{\"name\":\"frame\",\"availability\":\"not_enabled\",\"measured_frame_count\":0");

    profiler.SetEnabled(true);
    std::string unmeasuredJson{};
    ASSERT_EQ(BuildProfileReportJson(structural, profiler, TestContext(), unmeasuredJson), ProfileReportResult2D::Success);
    ExpectContains(unmeasuredJson, "\"cpu\":{\"availability\":\"not_measured\"");
    ExpectContains(
        unmeasuredJson,
        "{\"name\":\"frame\",\"availability\":\"not_measured\",\"measured_frame_count\":0");
}

TEST(ProfileReport2DTests, GpuEvidencePreservesMeasuredZeroAndContextIdentity)
{
    auto structural = PreparedStructuralSnapshot();
    ASSERT_TRUE(structural.Prepared());

    CpuProfiler2D profiler{};
    ASSERT_EQ(profiler.Prepare(1U, 1U, 1U), ProfileResult2D::Success);

    constexpr std::array<GpuProfileFrameTiming2D, 2> frames{
        GpuProfileFrameTiming2D{42U, 0U},
        GpuProfileFrameTiming2D{43U, 5U},
    };
    const GpuProfileEvidence2D gpu{
        .availability = ProfileMetricAvailability2D::Available,
        .deviceIdentityAvailability = ProfileMetricAvailability2D::Available,
        .deviceIdentity = "test-gpu",
        .driverIdentityAvailability = ProfileMetricAvailability2D::Available,
        .driverIdentity = "test-driver",
        .timingSource = "public-test-timestamps",
        .frames = frames,
    };

    std::string jsonText{};
    ASSERT_EQ(
        BuildProfileReportJson(structural, profiler, TestContext(), gpu, jsonText),
        ProfileReportResult2D::Success);

    ExpectContains(jsonText, "\"gpu\":{\"availability\":\"available\",\"backend\":\"headless\"");
    ExpectContains(jsonText, "\"device_identity\":{\"availability\":\"available\",\"value\":\"test-gpu\"}");
    ExpectContains(jsonText, "\"driver_identity\":{\"availability\":\"available\",\"value\":\"test-driver\"}");
    ExpectContains(jsonText, "\"timing_source\":\"public-test-timestamps\"");
    ExpectContains(jsonText, "{\"frame_index\":42,\"duration_ns\":0}");
    ExpectContains(
        jsonText,
        "\"aggregate\":{\"availability\":\"available\",\"measured_frame_count\":2,"
        "\"total_ns\":5,\"min_ns\":0,\"max_ns\":5}");
}

TEST(ProfileReport2DTests, RejectsGpuFramesWhenTimingIsUnavailable)
{
    auto structural = PreparedStructuralSnapshot();
    CpuProfiler2D profiler{};
    ASSERT_EQ(profiler.Prepare(1U, 1U, 1U), ProfileResult2D::Success);

    constexpr std::array<GpuProfileFrameTiming2D, 1> frames{
        GpuProfileFrameTiming2D{1U, 10U},
    };
    const GpuProfileEvidence2D gpu{
        .availability = ProfileMetricAvailability2D::NotSupported,
        .frames = frames,
    };

    std::string output = "retained-previous-report";
    EXPECT_EQ(
        BuildProfileReportJson(structural, profiler, TestContext(), gpu, output),
        ProfileReportResult2D::InvalidGpuEvidence);
    EXPECT_EQ(output, "retained-previous-report");
    EXPECT_EQ(ToString(ProfileReportResult2D::InvalidGpuEvidence), "invalid_gpu_evidence");
}

TEST(ProfileReport2DTests, GpuAggregationOverflowLeavesPreviousOutputUntouched)
{
    auto structural = PreparedStructuralSnapshot();
    CpuProfiler2D profiler{};
    ASSERT_EQ(profiler.Prepare(1U, 1U, 1U), ProfileResult2D::Success);

    constexpr std::array<GpuProfileFrameTiming2D, 2> frames{
        GpuProfileFrameTiming2D{1U, std::numeric_limits<std::uint64_t>::max()},
        GpuProfileFrameTiming2D{2U, 1U},
    };
    const GpuProfileEvidence2D gpu{
        .availability = ProfileMetricAvailability2D::Available,
        .timingSource = "overflow-test",
        .frames = frames,
    };

    std::string output = "retained-previous-report";
    EXPECT_EQ(
        BuildProfileReportJson(structural, profiler, TestContext(), gpu, output),
        ProfileReportResult2D::AggregationOverflow);
    EXPECT_EQ(output, "retained-previous-report");
}

TEST(ProfileReport2DTests, FailureLeavesPreviousOutputUntouched)
{
    StructuralProfileSnapshot2D unprepared{};
    CpuProfiler2D profiler{};
    ASSERT_EQ(profiler.Prepare(1U, 1U, 1U), ProfileResult2D::Success);

    std::string output = "retained-previous-report";
    EXPECT_EQ(
        BuildProfileReportJson(unprepared, profiler, TestContext(), output),
        ProfileReportResult2D::StructuralSnapshotNotPrepared);
    EXPECT_EQ(output, "retained-previous-report");
    EXPECT_EQ(ToString(ProfileReportResult2D::AggregationOverflow), "aggregation_overflow");
}
} // namespace
