#pragma once

#include <trace2d/profile/Profile2D.hpp>
#include <trace2d/profile/StructuralProfile2D.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace trace2d::profile
{
inline constexpr std::string_view ProfileReportSchema2D = "trace2d.profile.report.v1";

enum class ProfileReportResult2D : std::uint8_t
{
    Success = 0,
    StructuralSnapshotNotPrepared,
    CpuFrameUnavailable,
    AggregationOverflow,
    InvalidGpuEvidence,
};

[[nodiscard]] std::string_view ToString(ProfileReportResult2D result) noexcept;

// PERF3 report metadata is caller-supplied so profile collection never performs hidden platform,
// filesystem, CPUID or clock discovery. Empty cpuIdentity with NotMeasured is the canonical
// representation when a workload cannot provide trustworthy machine identity.
struct ProfileReportContext2D final
{
    std::string_view engineVersion{};
    std::string_view sourceRevision{};
    std::string_view workload{};
    std::string_view buildConfiguration{};
    std::string_view operatingSystem{};
    std::string_view architecture{};
    std::string_view compiler{};
    ProfileMetricAvailability2D cpuIdentityAvailability{ProfileMetricAvailability2D::NotMeasured};
    std::string_view cpuIdentity{};
    std::string_view rendererBackend{};
    std::string_view timingSource{};
    std::uint64_t fixedTimestepNanoseconds{0U};
    std::uint64_t warmupFrameCount{0U};
    std::uint64_t requestedSampleFrameCount{0U};
};

struct GpuProfileFrameTiming2D final
{
    std::uint64_t frameIndex{0U};
    std::uint64_t durationNanoseconds{0U};
};

// PERF4 keeps GPU timing as caller-supplied evidence because the pinned public SDL3 GPU surface does
// not expose timestamp/query-pool capture. `Available` requires at least one bounded frame sample;
// unavailable states require an empty frame span. Device/driver identity is optional contextual
// evidence and must use the same explicit availability vocabulary rather than guessed strings.
struct GpuProfileEvidence2D final
{
    ProfileMetricAvailability2D availability{ProfileMetricAvailability2D::NotSupported};
    ProfileMetricAvailability2D deviceIdentityAvailability{ProfileMetricAvailability2D::NotMeasured};
    std::string_view deviceIdentity{};
    ProfileMetricAvailability2D driverIdentityAvailability{ProfileMetricAvailability2D::NotMeasured};
    std::string_view driverIdentity{};
    std::string_view timingSource{};
    std::span<const GpuProfileFrameTiming2D> frames{};
};

// Explicit out-of-band report construction. The function may allocate while building JSON and CPU
// aggregates, but it never mutates the profiler or structural snapshot. On any reported failure,
// outJson is left unchanged so callers cannot accidentally publish partial evidence.
//
// The PERF3-compatible overload records GPU timing as explicitly `not_supported`; use the PERF4
// overload only when a public backend integration can supply trustworthy bounded timing evidence.
[[nodiscard]] ProfileReportResult2D BuildProfileReportJson(
    const StructuralProfileSnapshot2D& structuralSnapshot,
    const CpuProfiler2D& cpuProfiler,
    const ProfileReportContext2D& context,
    std::string& outJson);

[[nodiscard]] ProfileReportResult2D BuildProfileReportJson(
    const StructuralProfileSnapshot2D& structuralSnapshot,
    const CpuProfiler2D& cpuProfiler,
    const ProfileReportContext2D& context,
    const GpuProfileEvidence2D& gpuEvidence,
    std::string& outJson);
} // namespace trace2d::profile
