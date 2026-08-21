#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace trace2d::profile
{
using ProfileTimestamp2D = std::chrono::nanoseconds;

inline constexpr std::size_t ProfileScopeNameCapacity2D = 96U;

enum class ProfileMetricAvailability2D : std::uint8_t
{
    Available = 0,
    NotSupported,
    NotEnabled,
    NotMeasured,
};

[[nodiscard]] std::string_view ToString(ProfileMetricAvailability2D availability) noexcept;

enum class ProfileResult2D : std::uint8_t
{
    Success = 0,
    Disabled,
    NotPrepared,
    AlreadyPrepared,
    InvalidCapacity,
    InvalidScopeName,
    DuplicateScopeName,
    ScopeCapacityExceeded,
    ScopeRegistryFrozen,
    InvalidScopeId,
    FrameAlreadyActive,
    FrameNotActive,
    FrameNotAvailable,
    ScopeStackCapacityExceeded,
    ScopeStackEmpty,
    ScopeMismatch,
    NonMonotonicTimestamp,
    TimingOverflow,
    UnclosedScopes,
};

[[nodiscard]] std::string_view ToString(ProfileResult2D result) noexcept;

struct ProfileScopeId2D final
{
    static constexpr std::uint32_t InvalidValue = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t value{InvalidValue};

    [[nodiscard]] bool Valid() const noexcept
    {
        return value != InvalidValue;
    }

    [[nodiscard]] bool operator==(const ProfileScopeId2D&) const noexcept = default;
};

struct CpuScopeTiming2D final
{
    ProfileScopeId2D scope{};
    ProfileMetricAvailability2D availability{ProfileMetricAvailability2D::NotMeasured};
    std::uint64_t callCount{0U};
    ProfileTimestamp2D inclusiveTime{0};
    ProfileTimestamp2D exclusiveTime{0};

    [[nodiscard]] bool operator==(const CpuScopeTiming2D&) const noexcept = default;
};

struct CpuProfileFrame2D final
{
    std::uint64_t frameIndex{0U};
    ProfileTimestamp2D beginTimestamp{0};
    ProfileTimestamp2D endTimestamp{0};
    ProfileTimestamp2D frameDuration{0};

    [[nodiscard]] bool operator==(const CpuProfileFrame2D&) const noexcept = default;
};

// View into retained profiler storage. The view remains valid until a later committed frame
// overwrites the same ring slot or the profiler is destroyed.
struct CpuProfileFrameView2D final
{
    CpuProfileFrame2D frame{};
    std::span<const CpuScopeTiming2D> scopeTimings{};
};

struct CpuProfilerMetrics2D final
{
    std::size_t registeredScopeCount{0U};
    std::size_t retainedScopeCapacity{0U};
    std::size_t retainedFrameCapacity{0U};
    std::size_t retainedFrameCount{0U};
    std::size_t retainedStackCapacity{0U};
    std::size_t retainedHistoryTimingSlotCount{0U};
    std::uint64_t committedFrameCount{0U};
    std::uint64_t overwrittenFrameCount{0U};
    std::uint64_t invalidFrameCount{0U};
    std::uint64_t scopeEnterCount{0U};
    std::uint64_t scopeExitCount{0U};

    [[nodiscard]] bool operator==(const CpuProfilerMetrics2D&) const noexcept = default;
};

// Returns a monotonic steady-clock timestamp for real profiling workloads. CpuProfiler2D itself
// never queries a clock: deterministic tests and controlled workloads may supply timestamps from
// another monotonic source.
[[nodiscard]] ProfileTimestamp2D SteadyProfileTimestamp2D() noexcept;

// PERF1 is deliberately single-threaded. Scope definitions are setup-time state and are frozen on
// the first enabled BeginFrame(). Once Prepare() has retained all arrays, EnterScope()/ExitScope()
// perform no string lookup, hashing, vector growth, JSON/report building or clock query.
class CpuProfiler2D final
{
public:
    CpuProfiler2D() = default;

    [[nodiscard]] ProfileResult2D Prepare(
        std::size_t scopeCapacity,
        std::size_t frameCapacity,
        std::size_t stackCapacity);

    [[nodiscard]] ProfileResult2D RegisterScope(
        std::string_view name,
        ProfileScopeId2D& outScope) noexcept;
    [[nodiscard]] std::string_view ScopeName(ProfileScopeId2D scope) const noexcept;

    void SetEnabled(bool enabled) noexcept;
    [[nodiscard]] bool Enabled() const noexcept;

    [[nodiscard]] ProfileResult2D BeginFrame(
        std::uint64_t frameIndex,
        ProfileTimestamp2D timestamp) noexcept;
    [[nodiscard]] ProfileResult2D EnterScope(
        ProfileScopeId2D scope,
        ProfileTimestamp2D timestamp) noexcept;
    [[nodiscard]] ProfileResult2D ExitScope(
        ProfileScopeId2D scope,
        ProfileTimestamp2D timestamp) noexcept;
    [[nodiscard]] ProfileResult2D EndFrame(ProfileTimestamp2D timestamp) noexcept;

    [[nodiscard]] ProfileResult2D InspectFrameFromLatest(
        std::size_t offsetFromLatest,
        CpuProfileFrameView2D& outFrame) const noexcept;

    [[nodiscard]] CpuProfilerMetrics2D Metrics() const noexcept;

private:
    struct ScopeDefinition final
    {
        std::array<char, ProfileScopeNameCapacity2D> name{};
        std::uint16_t nameLength{0U};
    };

    struct ScopeStackEntry final
    {
        ProfileScopeId2D scope{};
        ProfileTimestamp2D beginTimestamp{0};
        ProfileTimestamp2D childTime{0};
    };

    [[nodiscard]] bool ScopeValid(ProfileScopeId2D scope) const noexcept;
    [[nodiscard]] ProfileResult2D RejectActiveFrame(ProfileResult2D result) noexcept;
    [[nodiscard]] ProfileResult2D CommitActiveFrame(ProfileTimestamp2D timestamp) noexcept;

    std::vector<ScopeDefinition> scopeDefinitions_{};
    std::vector<CpuScopeTiming2D> currentTimings_{};
    std::vector<CpuProfileFrame2D> frameHistory_{};
    std::vector<CpuScopeTiming2D> timingHistory_{};
    std::vector<ScopeStackEntry> scopeStack_{};

    std::size_t registeredScopeCount_{0U};
    std::size_t frameHistoryCount_{0U};
    std::size_t nextFrameHistoryIndex_{0U};
    std::size_t scopeStackDepth_{0U};

    std::uint64_t committedFrameCount_{0U};
    std::uint64_t overwrittenFrameCount_{0U};
    std::uint64_t invalidFrameCount_{0U};
    std::uint64_t scopeEnterCount_{0U};
    std::uint64_t scopeExitCount_{0U};

    CpuProfileFrame2D activeFrame_{};
    ProfileTimestamp2D lastTimestamp_{0};
    bool prepared_{false};
    bool enabled_{false};
    bool scopeRegistryFrozen_{false};
    bool frameActive_{false};
};
} // namespace trace2d::profile
