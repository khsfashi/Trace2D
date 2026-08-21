#include <trace2d/profile/StructuralProfile2D.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <utility>

namespace trace2d::profile
{
namespace
{
[[nodiscard]] bool ValidMetricName(const std::string_view name) noexcept
{
    if (name.empty() || name.size() > StructuralProfileMetricNameCapacity2D)
    {
        return false;
    }

    const auto first = static_cast<unsigned char>(name.front());
    if (first < static_cast<unsigned char>('a') || first > static_cast<unsigned char>('z'))
    {
        return false;
    }

    for (const char character : name)
    {
        const auto value = static_cast<unsigned char>(character);
        const bool lower = value >= static_cast<unsigned char>('a') && value <= static_cast<unsigned char>('z');
        const bool digit = value >= static_cast<unsigned char>('0') && value <= static_cast<unsigned char>('9');
        if (!lower && !digit && character != '.' && character != '_')
        {
            return false;
        }
    }

    return true;
}

[[nodiscard]] bool ValidMetricUnit(const std::string_view unit) noexcept
{
    if (unit.empty() || unit.size() > StructuralProfileMetricUnitCapacity2D)
    {
        return false;
    }

    return std::all_of(
        unit.begin(),
        unit.end(),
        [](const char character)
        {
            const auto value = static_cast<unsigned char>(character);
            return value >= 0x21U && value <= 0x7EU;
        });
}
} // namespace

std::string_view ToString(const StructuralProfileMetricKind2D kind) noexcept
{
    switch (kind)
    {
    case StructuralProfileMetricKind2D::Gauge:
        return "gauge";
    case StructuralProfileMetricKind2D::Counter:
        return "counter";
    }

    return "unknown";
}

std::string_view ToString(const StructuralProfileResult2D result) noexcept
{
    switch (result)
    {
    case StructuralProfileResult2D::Success:
        return "success";
    case StructuralProfileResult2D::NotPrepared:
        return "not_prepared";
    case StructuralProfileResult2D::AlreadyPrepared:
        return "already_prepared";
    case StructuralProfileResult2D::InvalidCapacity:
        return "invalid_capacity";
    case StructuralProfileResult2D::InvalidMetricName:
        return "invalid_metric_name";
    case StructuralProfileResult2D::InvalidMetricUnit:
        return "invalid_metric_unit";
    case StructuralProfileResult2D::DuplicateMetricName:
        return "duplicate_metric_name";
    case StructuralProfileResult2D::MetricCapacityExceeded:
        return "metric_capacity_exceeded";
    case StructuralProfileResult2D::ValueOverflow:
        return "value_overflow";
    }

    return "unknown";
}

StructuralProfileResult2D StructuralProfileSnapshot2D::Prepare(const std::size_t metricCapacity)
{
    if (prepared_)
    {
        return StructuralProfileResult2D::AlreadyPrepared;
    }
    if (metricCapacity == 0U)
    {
        return StructuralProfileResult2D::InvalidCapacity;
    }

    std::vector<StructuralProfileMetric2D> metrics(metricCapacity);
    metrics_.swap(metrics);
    metricCount_ = 0U;
    prepared_ = true;
    return StructuralProfileResult2D::Success;
}

StructuralProfileResult2D StructuralProfileSnapshot2D::Clear() noexcept
{
    if (!prepared_)
    {
        return StructuralProfileResult2D::NotPrepared;
    }

    metricCount_ = 0U;
    ++clearCount_;
    return StructuralProfileResult2D::Success;
}

StructuralProfileResult2D StructuralProfileSnapshot2D::AddMetric(
    const std::string_view name,
    const std::string_view unit,
    const StructuralProfileMetricKind2D kind,
    const ProfileMetricAvailability2D availability,
    const std::uint64_t value) noexcept
{
    if (!prepared_)
    {
        ++rejectedMetricCount_;
        return StructuralProfileResult2D::NotPrepared;
    }
    if (!ValidMetricName(name))
    {
        ++rejectedMetricCount_;
        return StructuralProfileResult2D::InvalidMetricName;
    }
    if (!ValidMetricUnit(unit))
    {
        ++rejectedMetricCount_;
        return StructuralProfileResult2D::InvalidMetricUnit;
    }

    for (std::size_t index = 0U; index < metricCount_; ++index)
    {
        if (metrics_[index].Name() == name)
        {
            ++rejectedMetricCount_;
            return StructuralProfileResult2D::DuplicateMetricName;
        }
    }

    if (metricCount_ >= metrics_.size())
    {
        ++rejectedMetricCount_;
        return StructuralProfileResult2D::MetricCapacityExceeded;
    }

    auto& metric = metrics_[metricCount_];
    metric = {};
    std::copy(name.begin(), name.end(), metric.name.begin());
    metric.nameLength = static_cast<std::uint16_t>(name.size());
    std::copy(unit.begin(), unit.end(), metric.unit.begin());
    metric.unitLength = static_cast<std::uint8_t>(unit.size());
    metric.kind = kind;
    metric.availability = availability;
    metric.value = value;
    ++metricCount_;
    return StructuralProfileResult2D::Success;
}

bool StructuralProfileSnapshot2D::Prepared() const noexcept
{
    return prepared_;
}

std::span<const StructuralProfileMetric2D> StructuralProfileSnapshot2D::Metrics() const noexcept
{
    return {metrics_.data(), metricCount_};
}

StructuralProfileStorageMetrics2D StructuralProfileSnapshot2D::StorageMetrics() const noexcept
{
    return StructuralProfileStorageMetrics2D{
        metricCount_,
        metrics_.size(),
        clearCount_,
        rejectedMetricCount_,
    };
}

std::string BuildStructuralProfileJson(
    const StructuralProfileSnapshot2D& snapshot,
    const StructuralProfileReportContext2D& context)
{
    nlohmann::ordered_json report = nlohmann::ordered_json::object();
    report["schema"] = std::string{StructuralProfileSchema2D};
    report["context"] = nlohmann::ordered_json{
        {"engine_version", std::string{context.engineVersion}},
        {"source_revision", std::string{context.sourceRevision}},
        {"workload", std::string{context.workload}},
        {"build_configuration", std::string{context.buildConfiguration}},
        {"operating_system", std::string{context.operatingSystem}},
        {"compiler", std::string{context.compiler}},
        {"renderer_backend", std::string{context.rendererBackend}},
        {"frame_index", context.frameIndex},
    };
    report["metrics"] = nlohmann::ordered_json::array();

    for (const auto& metric : snapshot.Metrics())
    {
        report["metrics"].push_back(nlohmann::ordered_json{
            {"name", std::string{metric.Name()}},
            {"kind", std::string{ToString(metric.kind)}},
            {"unit", std::string{metric.Unit()}},
            {"availability", std::string{ToString(metric.availability)}},
            {"value", metric.value},
        });
    }

    return report.dump();
}
} // namespace trace2d::profile
