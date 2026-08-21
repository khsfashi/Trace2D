#include <trace2d/profile/ProfileReport2D.hpp>

#include <chrono>
#include <string>

int main()
{
    using namespace std::chrono_literals;
    using namespace trace2d::profile;

    StructuralProfileSnapshot2D structural{};
    if (structural.Prepare(1U) != StructuralProfileResult2D::Success)
    {
        return 1;
    }
    if (structural.AddMetric(
            "external.consumer.frames",
            "1",
            StructuralProfileMetricKind2D::Counter,
            ProfileMetricAvailability2D::Available,
            1U) != StructuralProfileResult2D::Success)
    {
        return 2;
    }

    CpuProfiler2D profiler{};
    if (profiler.Prepare(1U, 1U, 1U) != ProfileResult2D::Success)
    {
        return 3;
    }
    ProfileScopeId2D scope{};
    if (profiler.RegisterScope("external.consumer", scope) != ProfileResult2D::Success)
    {
        return 4;
    }
    profiler.SetEnabled(true);
    if (profiler.BeginFrame(1U, 0ns) != ProfileResult2D::Success ||
        profiler.EnterScope(scope, 1ns) != ProfileResult2D::Success ||
        profiler.ExitScope(scope, 2ns) != ProfileResult2D::Success ||
        profiler.EndFrame(3ns) != ProfileResult2D::Success)
    {
        return 5;
    }

    const ProfileReportContext2D context{
        .engineVersion = "external-sdk",
        .sourceRevision = "installed-package",
        .workload = "e0-profile-contract",
        .buildConfiguration = "consumer",
        .operatingSystem = "consumer-os",
        .architecture = "consumer-arch",
        .compiler = "consumer-compiler",
        .cpuIdentityAvailability = ProfileMetricAvailability2D::NotMeasured,
        .cpuIdentity = {},
        .rendererBackend = "headless",
        .timingSource = "consumer-test-clock",
        .fixedTimestepNanoseconds = 16'666'667U,
        .warmupFrameCount = 0U,
        .requestedSampleFrameCount = 1U,
    };

    std::string report{};
    if (BuildProfileReportJson(structural, profiler, context, report) != ProfileReportResult2D::Success)
    {
        return 6;
    }
    if (report.find(ProfileReportSchema2D) == std::string::npos ||
        report.find("external.consumer.frames") == std::string::npos ||
        report.find("external.consumer") == std::string::npos)
    {
        return 7;
    }

    return 0;
}
