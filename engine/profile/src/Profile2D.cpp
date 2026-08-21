#include <trace2d/profile/Profile2D.hpp>

#include <algorithm>
#include <chrono>
#include <limits>

namespace trace2d::profile
{
namespace
{
[[nodiscard]] bool AddDurationChecked(
    const ProfileTimestamp2D lhs,
    const ProfileTimestamp2D rhs,
    ProfileTimestamp2D& out) noexcept
{
    using Rep = ProfileTimestamp2D::rep;
    constexpr auto maxValue = std::numeric_limits<Rep>::max();

    if (lhs.count() < 0 || rhs.count() < 0 || lhs.count() > maxValue - rhs.count())
    {
        return false;
    }

    out = ProfileTimestamp2D{lhs.count() + rhs.count()};
    return true;
}
} // namespace

std::string_view ToString(const ProfileMetricAvailability2D availability) noexcept
{
    switch (availability)
    {
    case ProfileMetricAvailability2D::Available:
        return "available";
    case ProfileMetricAvailability2D::NotSupported:
        return "not_supported";
    case ProfileMetricAvailability2D::NotEnabled:
        return "not_enabled";
    case ProfileMetricAvailability2D::NotMeasured:
        return "not_measured";
    }

    return "unknown";
}

std::string_view ToString(const ProfileResult2D result) noexcept
{
    switch (result)
    {
    case ProfileResult2D::Success:
        return "success";
    case ProfileResult2D::Disabled:
        return "disabled";
    case ProfileResult2D::NotPrepared:
        return "not_prepared";
    case ProfileResult2D::AlreadyPrepared:
        return "already_prepared";
    case ProfileResult2D::InvalidCapacity:
        return "invalid_capacity";
    case ProfileResult2D::InvalidScopeName:
        return "invalid_scope_name";
    case ProfileResult2D::DuplicateScopeName:
        return "duplicate_scope_name";
    case ProfileResult2D::ScopeCapacityExceeded:
        return "scope_capacity_exceeded";
    case ProfileResult2D::ScopeRegistryFrozen:
        return "scope_registry_frozen";
    case ProfileResult2D::InvalidScopeId:
        return "invalid_scope_id";
    case ProfileResult2D::FrameAlreadyActive:
        return "frame_already_active";
    case ProfileResult2D::FrameNotActive:
        return "frame_not_active";
    case ProfileResult2D::FrameNotAvailable:
        return "frame_not_available";
    case ProfileResult2D::ScopeStackCapacityExceeded:
        return "scope_stack_capacity_exceeded";
    case ProfileResult2D::ScopeStackEmpty:
        return "scope_stack_empty";
    case ProfileResult2D::ScopeMismatch:
        return "scope_mismatch";
    case ProfileResult2D::NonMonotonicTimestamp:
        return "non_monotonic_timestamp";
    case ProfileResult2D::TimingOverflow:
        return "timing_overflow";
    case ProfileResult2D::UnclosedScopes:
        return "unclosed_scopes";
    }

    return "unknown";
}

ProfileTimestamp2D SteadyProfileTimestamp2D() noexcept
{
    return std::chrono::duration_cast<ProfileTimestamp2D>(
        std::chrono::steady_clock::now().time_since_epoch());
}

ProfileResult2D CpuProfiler2D::Prepare(
    const std::size_t scopeCapacity,
    const std::size_t frameCapacity,
    const std::size_t stackCapacity)
{
    if (prepared_)
    {
        return ProfileResult2D::AlreadyPrepared;
    }

    constexpr auto maxScopeCount = static_cast<std::size_t>(ProfileScopeId2D::InvalidValue);
    if (scopeCapacity == 0U || frameCapacity == 0U || stackCapacity == 0U ||
        scopeCapacity >= maxScopeCount || frameCapacity > (std::numeric_limits<std::size_t>::max() / scopeCapacity))
    {
        return ProfileResult2D::InvalidCapacity;
    }

    const auto historyTimingCount = scopeCapacity * frameCapacity;

    // Construct all retained storage first. If allocation throws, the profiler remains unprepared
    // rather than publishing a partially prepared capacity contract.
    std::vector<ScopeDefinition> scopeDefinitions(scopeCapacity);
    std::vector<CpuScopeTiming2D> currentTimings(scopeCapacity);
    std::vector<CpuProfileFrame2D> frameHistory(frameCapacity);
    std::vector<CpuScopeTiming2D> timingHistory(historyTimingCount);
    std::vector<ScopeStackEntry> scopeStack(stackCapacity);

    scopeDefinitions_.swap(scopeDefinitions);
    currentTimings_.swap(currentTimings);
    frameHistory_.swap(frameHistory);
    timingHistory_.swap(timingHistory);
    scopeStack_.swap(scopeStack);
    prepared_ = true;
    return ProfileResult2D::Success;
}

ProfileResult2D CpuProfiler2D::RegisterScope(
    const std::string_view name,
    ProfileScopeId2D& outScope) noexcept
{
    outScope = {};

    if (!prepared_)
    {
        return ProfileResult2D::NotPrepared;
    }
    if (scopeRegistryFrozen_)
    {
        return ProfileResult2D::ScopeRegistryFrozen;
    }
    if (name.empty() || name.size() > ProfileScopeNameCapacity2D ||
        std::find(name.begin(), name.end(), '\0') != name.end())
    {
        return ProfileResult2D::InvalidScopeName;
    }

    for (std::size_t index = 0U; index < registeredScopeCount_; ++index)
    {
        const auto& definition = scopeDefinitions_[index];
        const std::string_view existing{definition.name.data(), definition.nameLength};
        if (existing == name)
        {
            return ProfileResult2D::DuplicateScopeName;
        }
    }

    if (registeredScopeCount_ >= scopeDefinitions_.size())
    {
        return ProfileResult2D::ScopeCapacityExceeded;
    }

    auto& definition = scopeDefinitions_[registeredScopeCount_];
    std::copy(name.begin(), name.end(), definition.name.begin());
    definition.nameLength = static_cast<std::uint16_t>(name.size());

    outScope.value = static_cast<std::uint32_t>(registeredScopeCount_);
    ++registeredScopeCount_;
    return ProfileResult2D::Success;
}

std::string_view CpuProfiler2D::ScopeName(const ProfileScopeId2D scope) const noexcept
{
    if (!ScopeValid(scope))
    {
        return {};
    }

    const auto& definition = scopeDefinitions_[scope.value];
    return {definition.name.data(), definition.nameLength};
}

void CpuProfiler2D::SetEnabled(const bool enabled) noexcept
{
    if (enabled_ == enabled)
    {
        return;
    }

    if (!enabled && frameActive_)
    {
        frameActive_ = false;
        scopeStackDepth_ = 0U;
        ++invalidFrameCount_;
    }

    enabled_ = enabled;
}

bool CpuProfiler2D::Enabled() const noexcept
{
    return enabled_;
}

ProfileResult2D CpuProfiler2D::BeginFrame(
    const std::uint64_t frameIndex,
    const ProfileTimestamp2D timestamp) noexcept
{
    if (!enabled_)
    {
        return ProfileResult2D::Disabled;
    }
    if (!prepared_)
    {
        return ProfileResult2D::NotPrepared;
    }
    if (frameActive_)
    {
        return ProfileResult2D::FrameAlreadyActive;
    }
    if (timestamp.count() < 0 || (scopeRegistryFrozen_ && timestamp < lastTimestamp_))
    {
        return ProfileResult2D::NonMonotonicTimestamp;
    }

    scopeRegistryFrozen_ = true;
    for (std::size_t index = 0U; index < registeredScopeCount_; ++index)
    {
        currentTimings_[index] = CpuScopeTiming2D{
            ProfileScopeId2D{static_cast<std::uint32_t>(index)},
            ProfileMetricAvailability2D::NotMeasured,
            0U,
            ProfileTimestamp2D{0},
            ProfileTimestamp2D{0},
        };
    }

    activeFrame_ = CpuProfileFrame2D{
        frameIndex,
        timestamp,
        timestamp,
        ProfileTimestamp2D{0},
    };
    lastTimestamp_ = timestamp;
    scopeStackDepth_ = 0U;
    frameActive_ = true;
    return ProfileResult2D::Success;
}

ProfileResult2D CpuProfiler2D::EnterScope(
    const ProfileScopeId2D scope,
    const ProfileTimestamp2D timestamp) noexcept
{
    if (!enabled_)
    {
        return ProfileResult2D::Disabled;
    }
    if (!prepared_)
    {
        return ProfileResult2D::NotPrepared;
    }
    if (!frameActive_)
    {
        return ProfileResult2D::FrameNotActive;
    }
    if (!ScopeValid(scope))
    {
        return RejectActiveFrame(ProfileResult2D::InvalidScopeId);
    }
    if (timestamp < lastTimestamp_)
    {
        return RejectActiveFrame(ProfileResult2D::NonMonotonicTimestamp);
    }
    if (scopeStackDepth_ >= scopeStack_.size())
    {
        return RejectActiveFrame(ProfileResult2D::ScopeStackCapacityExceeded);
    }

    scopeStack_[scopeStackDepth_] = ScopeStackEntry{
        scope,
        timestamp,
        ProfileTimestamp2D{0},
    };
    ++scopeStackDepth_;
    lastTimestamp_ = timestamp;
    ++scopeEnterCount_;
    return ProfileResult2D::Success;
}

ProfileResult2D CpuProfiler2D::ExitScope(
    const ProfileScopeId2D scope,
    const ProfileTimestamp2D timestamp) noexcept
{
    if (!enabled_)
    {
        return ProfileResult2D::Disabled;
    }
    if (!prepared_)
    {
        return ProfileResult2D::NotPrepared;
    }
    if (!frameActive_)
    {
        return ProfileResult2D::FrameNotActive;
    }
    if (!ScopeValid(scope))
    {
        return RejectActiveFrame(ProfileResult2D::InvalidScopeId);
    }
    if (timestamp < lastTimestamp_)
    {
        return RejectActiveFrame(ProfileResult2D::NonMonotonicTimestamp);
    }
    if (scopeStackDepth_ == 0U)
    {
        return RejectActiveFrame(ProfileResult2D::ScopeStackEmpty);
    }

    const auto& stackEntry = scopeStack_[scopeStackDepth_ - 1U];
    if (stackEntry.scope != scope)
    {
        return RejectActiveFrame(ProfileResult2D::ScopeMismatch);
    }
    if (timestamp < stackEntry.beginTimestamp)
    {
        return RejectActiveFrame(ProfileResult2D::NonMonotonicTimestamp);
    }

    const auto duration = timestamp - stackEntry.beginTimestamp;
    if (stackEntry.childTime > duration)
    {
        return RejectActiveFrame(ProfileResult2D::TimingOverflow);
    }
    const auto exclusive = duration - stackEntry.childTime;

    auto& timing = currentTimings_[scope.value];
    if (timing.callCount == std::numeric_limits<std::uint64_t>::max())
    {
        return RejectActiveFrame(ProfileResult2D::TimingOverflow);
    }

    ProfileTimestamp2D nextInclusive{};
    ProfileTimestamp2D nextExclusive{};
    if (!AddDurationChecked(timing.inclusiveTime, duration, nextInclusive) ||
        !AddDurationChecked(timing.exclusiveTime, exclusive, nextExclusive))
    {
        return RejectActiveFrame(ProfileResult2D::TimingOverflow);
    }

    if (scopeStackDepth_ > 1U)
    {
        auto& parent = scopeStack_[scopeStackDepth_ - 2U];
        ProfileTimestamp2D nextChildTime{};
        if (!AddDurationChecked(parent.childTime, duration, nextChildTime))
        {
            return RejectActiveFrame(ProfileResult2D::TimingOverflow);
        }
        parent.childTime = nextChildTime;
    }

    timing.availability = ProfileMetricAvailability2D::Available;
    ++timing.callCount;
    timing.inclusiveTime = nextInclusive;
    timing.exclusiveTime = nextExclusive;

    --scopeStackDepth_;
    lastTimestamp_ = timestamp;
    ++scopeExitCount_;
    return ProfileResult2D::Success;
}

ProfileResult2D CpuProfiler2D::EndFrame(const ProfileTimestamp2D timestamp) noexcept
{
    if (!enabled_)
    {
        return ProfileResult2D::Disabled;
    }
    if (!prepared_)
    {
        return ProfileResult2D::NotPrepared;
    }
    if (!frameActive_)
    {
        return ProfileResult2D::FrameNotActive;
    }
    if (timestamp < lastTimestamp_)
    {
        return RejectActiveFrame(ProfileResult2D::NonMonotonicTimestamp);
    }
    if (scopeStackDepth_ != 0U)
    {
        return RejectActiveFrame(ProfileResult2D::UnclosedScopes);
    }

    return CommitActiveFrame(timestamp);
}

ProfileResult2D CpuProfiler2D::InspectFrameFromLatest(
    const std::size_t offsetFromLatest,
    CpuProfileFrameView2D& outFrame) const noexcept
{
    outFrame = {};

    if (!prepared_)
    {
        return ProfileResult2D::NotPrepared;
    }
    if (offsetFromLatest >= frameHistoryCount_)
    {
        return ProfileResult2D::FrameNotAvailable;
    }

    const auto frameCapacity = frameHistory_.size();
    const auto latestIndex = (nextFrameHistoryIndex_ + frameCapacity - 1U) % frameCapacity;
    const auto historyIndex = (latestIndex + frameCapacity - offsetFromLatest) % frameCapacity;
    const auto timingOffset = historyIndex * scopeDefinitions_.size();

    outFrame.frame = frameHistory_[historyIndex];
    outFrame.scopeTimings = std::span<const CpuScopeTiming2D>{
        timingHistory_.data() + timingOffset,
        registeredScopeCount_,
    };
    return ProfileResult2D::Success;
}

CpuProfilerMetrics2D CpuProfiler2D::Metrics() const noexcept
{
    return CpuProfilerMetrics2D{
        registeredScopeCount_,
        scopeDefinitions_.size(),
        frameHistory_.size(),
        frameHistoryCount_,
        scopeStack_.size(),
        timingHistory_.size(),
        committedFrameCount_,
        overwrittenFrameCount_,
        invalidFrameCount_,
        scopeEnterCount_,
        scopeExitCount_,
    };
}

bool CpuProfiler2D::ScopeValid(const ProfileScopeId2D scope) const noexcept
{
    return scope.Valid() && static_cast<std::size_t>(scope.value) < registeredScopeCount_;
}

ProfileResult2D CpuProfiler2D::RejectActiveFrame(const ProfileResult2D result) noexcept
{
    frameActive_ = false;
    scopeStackDepth_ = 0U;
    ++invalidFrameCount_;
    return result;
}

ProfileResult2D CpuProfiler2D::CommitActiveFrame(const ProfileTimestamp2D timestamp) noexcept
{
    activeFrame_.endTimestamp = timestamp;
    activeFrame_.frameDuration = timestamp - activeFrame_.beginTimestamp;

    const auto historyIndex = nextFrameHistoryIndex_;
    frameHistory_[historyIndex] = activeFrame_;

    const auto timingOffset = historyIndex * scopeDefinitions_.size();
    std::copy_n(
        currentTimings_.begin(),
        registeredScopeCount_,
        timingHistory_.begin() + static_cast<std::ptrdiff_t>(timingOffset));

    if (frameHistoryCount_ == frameHistory_.size())
    {
        ++overwrittenFrameCount_;
    }
    else
    {
        ++frameHistoryCount_;
    }

    nextFrameHistoryIndex_ = (historyIndex + 1U) % frameHistory_.size();
    ++committedFrameCount_;
    frameActive_ = false;
    lastTimestamp_ = timestamp;
    return ProfileResult2D::Success;
}
} // namespace trace2d::profile
