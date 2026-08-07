#pragma once

#include <chrono>

namespace trace2d::runtime
{
using WallClockDuration = std::chrono::nanoseconds;
using WallClockTimePoint = std::chrono::time_point<std::chrono::steady_clock, WallClockDuration>;

class MonotonicClock final
{
public:
    [[nodiscard]] WallClockTimePoint Now() const noexcept;
};
} // namespace trace2d::runtime
