#include <trace2d/runtime/MonotonicClock.hpp>

namespace trace2d::runtime
{
WallClockTimePoint MonotonicClock::Now() const noexcept
{
    return std::chrono::time_point_cast<WallClockDuration>(std::chrono::steady_clock::now());
}
} // namespace trace2d::runtime
