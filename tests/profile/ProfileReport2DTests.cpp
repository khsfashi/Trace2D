#include <trace2d/profile/ProfileReport2D.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <string>

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
        .sourceRevision = "perf3-test",
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
    const auto report = nlohmann::json::parse(jsonText);

    EXPECT_EQ(report.at("schema").get<std::string>(), std::string{ProfileReportSchema2D});
    EXPECT_EQ(report.at("context").at("cpu_identity").at("availability"), "not_measured");
    EXPECT_TRUE(report.at("context").at("cpu_identity").at("value").is_null());
    EXPECT_EQ(report.at("structural").at("schema").get<std::string>(), std::string{StructuralProfileSchema2D});
    ASSERT_EQ(report.at("structural").at("metrics").size(), 1U);
    EXPECT_EQ(report.at("structural").at("metrics").at(0).at("name"), "renderer.draw_calls");

    const auto& frames = report.at("cpu").at("frames");
    ASSERT_EQ(frames.size(), 2U);
    EXPECT_EQ(frames.at(0).at("frame_index"), 2U);
    EXPECT_EQ(frames.at(1).at("frame_index"), 3U);
    EXPECT_EQ(report.at("cpu").at("availability"), "available");
    EXPECT_EQ(report.at("cpu").at("storage").at("overwritten_frame_count"), 1U);

    const auto& scopes = report.at("cpu").at("scopes");
    ASSERT_EQ(scopes.size(), 3U);
    EXPECT_EQ(scopes.at(root.value).at("availability"), "available");
    EXPECT_EQ(scopes.at(root.value).at("measured_frame_count"), 2U);
    EXPECT_EQ(scopes.at(root.value).at("call_count"), 2U);
    EXPECT_EQ(scopes.at(root.value).at("inclusive_total_ns"), 70U);
    EXPECT_EQ(scopes.at(root.value).at("exclusive_total_ns"), 40U);
    EXPECT_EQ(scopes.at(root.value).at("min_inclusive_ns"), 0U);
    EXPECT_EQ(scopes.at(root.value).at("max_inclusive_ns"), 70U);

    EXPECT_EQ(scopes.at(child.value).at("availability"), "available");
    EXPECT_EQ(scopes.at(child.value).at("measured_frame_count"), 1U);
    EXPECT_EQ(scopes.at(child.value).at("call_count"), 1U);
    EXPECT_EQ(scopes.at(child.value).at("inclusive_total_ns"), 30U);
    EXPECT_EQ(scopes.at(child.value).at("exclusive_total_ns"), 30U);
    EXPECT_EQ(scopes.at(child.value).at("min_inclusive_ns"), 30U);
    EXPECT_EQ(scopes.at(child.value).at("max_inclusive_ns"), 30U);

    EXPECT_EQ(scopes.at(untouched.value).at("availability"), "not_measured");
    EXPECT_EQ(scopes.at(untouched.value).at("measured_frame_count"), 0U);
    EXPECT_EQ(scopes.at(untouched.value).at("inclusive_total_ns"), 0U);

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
    const auto disabled = nlohmann::json::parse(disabledJson);
    EXPECT_EQ(disabled.at("cpu").at("availability"), "not_enabled");
    EXPECT_EQ(disabled.at("cpu").at("scopes").at(0).at("availability"), "not_enabled");

    profiler.SetEnabled(true);
    std::string unmeasuredJson{};
    ASSERT_EQ(BuildProfileReportJson(structural, profiler, TestContext(), unmeasuredJson), ProfileReportResult2D::Success);
    const auto unmeasured = nlohmann::json::parse(unmeasuredJson);
    EXPECT_EQ(unmeasured.at("cpu").at("availability"), "not_measured");
    EXPECT_EQ(unmeasured.at("cpu").at("scopes").at(0).at("availability"), "not_measured");
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
