#pragma once

#include <trace2d/profile/Profile2D.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::profile
{
inline constexpr std::string_view StructuralProfileSchema2D = "trace2d.profile.structural.v1";
inline constexpr std::size_t StructuralProfileMetricNameCapacity2D = 96U;
inline constexpr std::size_t StructuralProfileMetricUnitCapacity2D = 16U;

enum class StructuralProfileMetricKind2D : std::uint8_t
{
    Gauge = 0,
    Counter,
};

[[nodiscard]] std::string_view ToString(StructuralProfileMetricKind2D kind) noexcept;

enum class StructuralProfileResult2D : std::uint8_t
{
    Success = 0,
    NotPrepared,
    AlreadyPrepared,
    InvalidCapacity,
    InvalidMetricName,
    InvalidMetricUnit,
    DuplicateMetricName,
    MetricCapacityExceeded,
    ValueOverflow,
};

[[nodiscard]] std::string_view ToString(StructuralProfileResult2D result) noexcept;

struct StructuralProfileMetric2D final
{
    std::array<char, StructuralProfileMetricNameCapacity2D> name{};
    std::uint16_t nameLength{0U};
    std::array<char, StructuralProfileMetricUnitCapacity2D> unit{};
    std::uint8_t unitLength{0U};
    StructuralProfileMetricKind2D kind{StructuralProfileMetricKind2D::Gauge};
    ProfileMetricAvailability2D availability{ProfileMetricAvailability2D::NotMeasured};
    std::uint64_t value{0U};

    [[nodiscard]] std::string_view Name() const noexcept
    {
        return {name.data(), nameLength};
    }

    [[nodiscard]] std::string_view Unit() const noexcept
    {
        return {unit.data(), unitLength};
    }
};

struct StructuralProfileStorageMetrics2D final
{
    std::size_t metricCount{0U};
    std::size_t retainedMetricCapacity{0U};
    std::uint64_t clearCount{0U};
    std::uint64_t rejectedMetricCount{0U};

    [[nodiscard]] bool operator==(const StructuralProfileStorageMetrics2D&) const noexcept = default;
};

// PERF2 snapshot storage is prepared once and reused. AddMetric() writes only retained fixed-size
// records: it performs no heap allocation, JSON construction, hashing or string ownership work.
// Duplicate detection is a bounded linear scan because structural composition is explicit capture
// work rather than a normal frame hot path.
class StructuralProfileSnapshot2D final
{
public:
    StructuralProfileSnapshot2D() = default;

    [[nodiscard]] StructuralProfileResult2D Prepare(std::size_t metricCapacity);
    [[nodiscard]] StructuralProfileResult2D Clear() noexcept;

    [[nodiscard]] StructuralProfileResult2D AddMetric(
        std::string_view name,
        std::string_view unit,
        StructuralProfileMetricKind2D kind,
        ProfileMetricAvailability2D availability,
        std::uint64_t value) noexcept;

    [[nodiscard]] bool Prepared() const noexcept;
    [[nodiscard]] std::span<const StructuralProfileMetric2D> Metrics() const noexcept;
    [[nodiscard]] StructuralProfileStorageMetrics2D StorageMetrics() const noexcept;

private:
    std::vector<StructuralProfileMetric2D> metrics_{};
    std::size_t metricCount_{0U};
    std::uint64_t clearCount_{0U};
    std::uint64_t rejectedMetricCount_{0U};
    bool prepared_{false};
};

// Context strings are consumed only during the explicit report call and are not retained by the
// snapshot. This keeps normal structural capture independent of build/OS discovery and formatting.
struct StructuralProfileReportContext2D final
{
    std::string_view engineVersion{};
    std::string_view sourceRevision{};
    std::string_view workload{};
    std::string_view buildConfiguration{};
    std::string_view operatingSystem{};
    std::string_view compiler{};
    std::string_view rendererBackend{};
    std::uint64_t frameIndex{0U};
};

// Explicit out-of-band formatting boundary. This function may allocate; callers must not invoke it
// from deterministic fixed-step or ordinary render/audio/physics hot paths.
[[nodiscard]] std::string BuildStructuralProfileJson(
    const StructuralProfileSnapshot2D& snapshot,
    const StructuralProfileReportContext2D& context);
} // namespace trace2d::profile
