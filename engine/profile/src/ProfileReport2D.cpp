#include <trace2d/profile/ProfileReport2D.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
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
        {"cpu_identity", nlohmann::ordered_json{
            {"availability", std::string{ToString(context.cpuIdentityAvailability)}},
            {"value", context.cpuIdentityAvailability == ProfileMetricAvailability2D::Available
                ? nlohmann::ordered_json{std::string{context.cpuIdentity}}
                : nlohmann::ordered_json{nullptr}},
        }},
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

    std::string nextJson = report.dump();
    outJson.swap(nextJson);
    return ProfileReportResult2D::Success;
}
} // namespace trace2d::profile
