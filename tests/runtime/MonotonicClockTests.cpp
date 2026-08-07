#include <trace2d/runtime/MonotonicClock.hpp>

#include <gtest/gtest.h>

namespace
{
TEST(MonotonicClockTests, ConsecutiveReadsDoNotMoveBackward)
{
    const trace2d::runtime::MonotonicClock clock{};

    const trace2d::runtime::WallClockTimePoint first = clock.Now();
    const trace2d::runtime::WallClockTimePoint second = clock.Now();

    EXPECT_GE(second, first);
}
} // namespace
