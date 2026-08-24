#include <trace2d/profile/ProfileReport2D.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace trace2d::profile
{
namespace
{
struct CpuScopeAggregate2D final
{
    ProfileMetricAvailability2D availability{ProfileMetricAvailability2D::NotMeasured};
    std::uint64_t measuredFrameCount{0U};
    std::uint64_t callCount{0U};
    std::uint64_t inclusiveNanoseconds{0U};
    std::uint64_t exclusiveNanoseconds{0U};
    std::uint64_t minInclusiveNanoseconds{0U};
    std::uint64_t maxInclusiveNanoseconds{0U};
    bool measured{false};
};

[[nodiscard]] bool AddChecked(
    const std::uint64_t lhs,
    const std::uint64_t rhs,
    std::uint64_t& out) noexcept
{
    if (lhs > std::numeric_limits<std::uint64_t>::max() - rhs)
    {
        return false;
    }

    out = lhs + rhs;
    return true;
}

[[nodiscard]] bool DurationNanoseconds(
    const ProfileTimestamp2D duration,
    std::uint64_t& out) noexcept
{
    if (duration.count() < 0)
    {
        return false;
    }

    out = static_cast<std::uint64_t>(duration.count());
    return true;
}

[[nodiscard]] ProfileMetricAvailability2D CpuCaptureAvailability(
    const CpuProfiler2D& profiler,
    const CpuProfilerMetrics2D& metrics) noexcept
{
    if (metrics.retainedFrameCount > 0U)
    {
        return ProfileMetricAvailability2D::Available;
    }

    return profiler.Enabled()
        ? ProfileMetricAvailability2D::NotMeasured
        : ProfileMetricAvailability2D::NotEnabled;
}

[[nodiscard]] nlohmann::ordered_json IdentityJson(
    const ProfileMetricAvailability2D availability,
    const std::string_view value)
{
    return nlohmann::ordered_json{
        {"availability", std::string{ToString(availability)}},
        {"value", availability == ProfileMetricAvailability2D::Available
            ? nlohmann::ordered_json(std::string{value})
            : nlohmann::ordered_json(nullptr)},
    };
}

[[nodiscard]] nlohmann::ordered_json UnsupportedGpuJson(const ProfileReportContext2D& context)
{
    return nlohmann::ordered_json{
        {"availability", std::string{ToString(ProfileMetricAvailability2D::NotSupported)}},
        {"backend", std::string{context.rendererBackend}},
        {"device_identity", IdentityJson(ProfileMetricAvailability2D::NotMeasured, {})},
        {"driver_identity", IdentityJson(ProfileMetricAvailability2D::NotMeasured, {})},
        {"timing_source", nullptr},
        {"frames", nlohmann::ordered_json::array()},
        {"aggregate", nlohmann::ordered_json{
            {"availability", std::string{ToString(ProfileMetricAvailability2D::NotSupported)}},
            {"measured_frame_count", 0U},
            {"total_ns", 0U},
            {"min_ns", 0U},
            {"max_ns", 0U},
        }},
    };
}
} // namespace

std::string_view ToString(const ProfileReportResult2D result) noexcept
{
    switch (result)
    {
    case ProfileReportResult2D::Success:
        return "success";
    case ProfileReportResult2D::StructuralSnapshotNotPrepared:
        return "structural_snapshot_not_prepared";
    case ProfileReportResult2D::CpuFrameUnavailable:
        return "cpu_frame_unavailable";
    case ProfileReportResult2D::AggregationOverflow:
        return "aggregation_overflow";
    case ProfileReportResult2D::InvalidGpuEvidence:
        return "invalid_gpu_evidence";
    }

    return "unknown";
}

ProfileReportResult2D BuildProfileReportJson(
    const StructuralProfileSnapshot2D& structuralSnapshot,
    const CpuProfiler2D& cpuProfiler,
    const ProfileReportContext2D& context,
    std::string& outJson)
{
    if (!structuralSnapshot.Prepared())
    {
        return ProfileReportResult2D::StructuralSnapshotNotPrepared;
    }

    const CpuProfilerMetrics2D cpuMetrics = cpuProfiler.Metrics();
    const ProfileMetricAvailability2D cpuAvailability = CpuCaptureAvailability(cpuProfiler, cpuMetrics);
    std::vector<CpuScopeAggregate2D> aggregates(cpuMetrics.registeredScopeCount);
    nlohmann::ordered_json frames = nlohmann::ordered_json::array();

    for (std::size_t chronologicalIndex = 0U;
         chronologicalIndex < cpuMetrics.retainedFrameCount;
         ++chronologicalIndex)
    {
        const std::size_t offsetFromLatest = cpuMetrics.retainedFrameCount - chronologicalIndex - 1U;
        CpuProfileFrameView2D frame{};
        if (cpuProfiler.InspectFrameFromLatest(offsetFromLatest, frame) != ProfileResult2D::Success)
        {
            return ProfileReportResult2D::CpuFrameUnavailable;
        }

        std::uint64_t beginNanoseconds = 0U;
        std::uint64_t endNanoseconds = 0U;
        std::uint64_t durationNanoseconds = 0U;
        if (!DurationNanoseconds(frame.frame.beginTimestamp, beginNanoseconds) ||
            !DurationNanoseconds(frame.frame.endTimestamp, endNanoseconds) ||
            !DurationNanoseconds(frame.frame.frameDuration, durationNanoseconds))
        {
            return ProfileReportResult2D::AggregationOverflow;
        }

        nlohmann::ordered_json frameJson = nlohmann::ordered_json::object();
        frameJson["frame_index"] = frame.frame.frameIndex;
        frameJson["begin_ns"] = beginNanoseconds;
        frameJson["end_ns"] = endNanoseconds;
        frameJson["duration_ns"] = durationNanoseconds;
        frameJson["scopes"] = nlohmann::ordered_json::array();

        if (frame.scopeTimings.size() != cpuMetrics.registeredScopeCount)
        {
            return ProfileReportResult2D::CpuFrameUnavailable;
        }

        for (std::size_t scopeIndex = 0U; scopeIndex < frame.scopeTimings.size(); ++scopeIndex)
        {
            const auto& timing = frame.scopeTimings[scopeIndex];
            std::uint64_t inclusiveNanoseconds = 0U;
            std::uint64_t exclusiveNanoseconds = 0U;
            if (!DurationNanoseconds(timing.inclusiveTime, inclusiveNanoseconds) ||
                !DurationNanoseconds(timing.exclusiveTime, exclusiveNanoseconds))
            {
                return ProfileReportResult2D::AggregationOverflow;
            }

            frameJson["scopes"].push_back(nlohmann::ordered_json{
                {"name", std::string{cpuProfiler.ScopeName(timing.scope)}},
                {"availability", std::string{ToString(timing.availability)}},
                {"call_count", timing.callCount},
                {"inclusive_ns", inclusiveNanoseconds},
                {"exclusive_ns", exclusiveNanoseconds},
            });

            if (timing.availability != ProfileMetricAvailability2D::Available)
            {
                continue;
            }

            auto& aggregate = aggregates[scopeIndex];
            std::uint64_t nextMeasuredFrameCount = 0U;
            std::uint64_t nextCallCount = 0U;
            std::uint64_t nextInclusive = 0U;
            std::uint64_t nextExclusive = 0U;
            if (!AddChecked(aggregate.measuredFrameCount, 1U, nextMeasuredFrameCount) ||
                !AddChecked(aggregate.callCount, timing.callCount, nextCallCount) ||
                !AddChecked(aggregate.inclusiveNanoseconds, inclusiveNanoseconds, nextInclusive) ||
                !AddChecked(aggregate.exclusiveNanoseconds, exclusiveNanoseconds, nextExclusive))
            {
                return ProfileReportResult2D::AggregationOverflow;
            }

            aggregate.availability = ProfileMetricAvailability2D::Available;
            aggregate.measuredFrameCount = nextMeasuredFrameCount;
            aggregate.callCount = nextCallCount;
            aggregate.inclusiveNanoseconds = nextInclusive;
            aggregate.exclusiveNanoseconds = nextExclusive;
            if (!aggregate.measured)
            {
                aggregate.minInclusiveNanoseconds = inclusiveNanoseconds;
                aggregate.maxInclusiveNanoseconds = inclusiveNanoseconds;
                aggregate.measured = true;
            }
            else
            {
                aggregate.minInclusiveNanoseconds = std::min(aggregate.minInclusiveNanoseconds, inclusiveNanoseconds);
                aggregate.maxInclusiveNanoseconds = std::max(aggregate.maxInclusiveNanoseconds, inclusiveNanoseconds);
            }
        }

        frames.push_back(std::move(frameJson));
    }

    if (cpuAvailability != ProfileMetricAvailability2D::Available)
    {
        for (auto& aggregate : aggregates)
        {
            aggregate.availability = cpuAvailability;
        }
    }

    nlohmann::ordered_json report = nlohmann::ordered_json::object();
    report["schema"] = std::string{ProfileReportSchema2D};
    report["context"] = nlohmann::ordered_json{
        {"engine_version", std::string{context.engineVersion}},
        {"source_revision", std::string{context.sourceRevision}},
        {"workload", std::string{context.workload}},
        {"build_configuration", std::string{context.buildConfiguration}},
        {"operating_system", std::string{context.operatingSystem}},
        {"architecture", std::string{context.architecture}},
        {"compiler", std::string{context.compiler}},
        {"cpu_identity", IdentityJson(context.cpuIdentityAvailability, context.cpuIdentity)},
        {"renderer_backend", std::string{context.rendererBackend}},
        {"timing_source", std::string{context.timingSource}},
        {"fixed_timestep_ns", context.fixedTimestepNanoseconds},
        {"warmup_frame_count", context.warmupFrameCount},
        {"requested_sample_frame_count", context.requestedSampleFrameCount},
    };

    report["structural"] = nlohmann::ordered_json::object();
    report["structural"]["schema"] = std::string{StructuralProfileSchema2D};
    report["structural"]["metrics"] = nlohmann::ordered_json::array();
    for (const auto& metric : structuralSnapshot.Metrics())
    {
        report["structural"]["metrics"].push_back(nlohmann::ordered_json{
            {"name", std::string{metric.Name()}},
            {"kind", std::string{ToString(metric.kind)}},
            {"unit", std::string{metric.Unit()}},
            {"availability", std::string{ToString(metric.availability)}},
            {"value", metric.value},
        });
    }

    report["cpu"] = nlohmann::ordered_json::object();
    report["cpu"]["availability"] = std::string{ToString(cpuAvailability)};
    report["cpu"]["storage"] = nlohmann::ordered_json{
        {"registered_scope_count", cpuMetrics.registeredScopeCount},
        {"retained_scope_capacity", cpuMetrics.retainedScopeCapacity},
        {"retained_frame_capacity", cpuMetrics.retainedFrameCapacity},
        {"retained_frame_count", cpuMetrics.retainedFrameCount},
        {"retained_stack_capacity", cpuMetrics.retainedStackCapacity},
        {"retained_history_timing_slot_count", cpuMetrics.retainedHistoryTimingSlotCount},
        {"committed_frame_count", cpuMetrics.committedFrameCount},
        {"overwritten_frame_count", cpuMetrics.overwrittenFrameCount},
        {"invalid_frame_count", cpuMetrics.invalidFrameCount},
        {"scope_enter_count", cpuMetrics.scopeEnterCount},
        {"scope_exit_count", cpuMetrics.scopeExitCount},
    };
    report["cpu"]["frames"] = std::move(frames);
    report["cpu"]["scopes"] = nlohmann::ordered_json::array();

    for (std::size_t scopeIndex = 0U; scopeIndex < aggregates.size(); ++scopeIndex)
    {
        const auto& aggregate = aggregates[scopeIndex];
        report["cpu"]["scopes"].push_back(nlohmann::ordered_json{
            {"name", std::string{cpuProfiler.ScopeName(ProfileScopeId2D{static_cast<std::uint32_t>(scopeIndex)})}},
            {"availability", std::string{ToString(aggregate.availability)}},
            {"measured_frame_count", aggregate.measuredFrameCount},
            {"call_count", aggregate.callCount},
            {"inclusive_total_ns", aggregate.inclusiveNanoseconds},
            {"exclusive_total_ns", aggregate.exclusiveNanoseconds},
            {"min_inclusive_ns", aggregate.minInclusiveNanoseconds},
            {"max_inclusive_ns", aggregate.maxInclusiveNanoseconds},
        });
    }

    report["gpu"] = UnsupportedGpuJson(context);

    std::string nextJson = report.dump();
    outJson.swap(nextJson);
    return ProfileReportResult2D::Success;
}

ProfileReportResult2D BuildProfileReportJson(
    const StructuralProfileSnapshot2D& structuralSnapshot,
    const CpuProfiler2D& cpuProfiler,
    const ProfileReportContext2D& context,
    const GpuProfileEvidence2D& gpuEvidence,
    std::string& outJson)
{
    const bool hasFrames = !gpuEvidence.frames.empty();
    if ((gpuEvidence.availability == ProfileMetricAvailability2D::Available) != hasFrames)
    {
        return ProfileReportResult2D::InvalidGpuEvidence;
    }

    std::uint64_t totalNanoseconds = 0U;
    std::uint64_t minNanoseconds = 0U;
    std::uint64_t maxNanoseconds = 0U;
    bool measured = false;
    nlohmann::ordered_json gpuFrames = nlohmann::ordered_json::array();
    for (const auto& frame : gpuEvidence.frames)
    {
        std::uint64_t nextTotal = 0U;
        if (!AddChecked(totalNanoseconds, frame.durationNanoseconds, nextTotal))
        {
            return ProfileReportResult2D::AggregationOverflow;
        }
        totalNanoseconds = nextTotal;
        if (!measured)
        {
            minNanoseconds = frame.durationNanoseconds;
            maxNanoseconds = frame.durationNanoseconds;
            measured = true;
        }
        else
        {
            minNanoseconds = std::min(minNanoseconds, frame.durationNanoseconds);
            maxNanoseconds = std::max(maxNanoseconds, frame.durationNanoseconds);
        }
        gpuFrames.push_back(nlohmann::ordered_json{
            {"frame_index", frame.frameIndex},
            {"duration_ns", frame.durationNanoseconds},
        });
    }

    std::string baseJson{};
    const auto baseResult = BuildProfileReportJson(structuralSnapshot, cpuProfiler, context, baseJson);
    if (baseResult != ProfileReportResult2D::Success)
    {
        return baseResult;
    }

    nlohmann::ordered_json report = nlohmann::ordered_json::parse(baseJson);
    const auto availability = gpuEvidence.availability;
    report["gpu"] = nlohmann::ordered_json{
        {"availability", std::string{ToString(availability)}},
        {"backend", std::string{context.rendererBackend}},
        {"device_identity", IdentityJson(
            gpuEvidence.deviceIdentityAvailability,
            gpuEvidence.deviceIdentity)},
        {"driver_identity", IdentityJson(
            gpuEvidence.driverIdentityAvailability,
            gpuEvidence.driverIdentity)},
        {"timing_source", availability == ProfileMetricAvailability2D::Available
            ? nlohmann::ordered_json(std::string{gpuEvidence.timingSource})
            : nlohmann::ordered_json(nullptr)},
        {"frames", std::move(gpuFrames)},
        {"aggregate", nlohmann::ordered_json{
            {"availability", std::string{ToString(availability)}},
            {"measured_frame_count", gpuEvidence.frames.size()},
            {"total_ns", totalNanoseconds},
            {"min_ns", minNanoseconds},
            {"max_ns", maxNanoseconds},
        }},
    };

    std::string nextJson = report.dump();
    outJson.swap(nextJson);
    return ProfileReportResult2D::Success;
}
} // namespace trace2d::profile
