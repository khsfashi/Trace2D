#include <trace2d/runtime/FixedStepRuntime.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <stdexcept>

namespace
{
using namespace std::chrono_literals;

TEST(FixedStepRuntimeTests, RejectsNonPositiveFixedTimestep)
{
    trace2d::runtime::RuntimeConfig config{};
    config.fixedTimestep = 0ns;

    EXPECT_THROW((void)trace2d::runtime::FixedStepRuntime{config}, std::invalid_argument);
}

TEST(FixedStepRuntimeTests, ZeroStepLeavesStateUnchanged)
{
    trace2d::runtime::RuntimeConfig config{};
    config.fixedTimestep = 10ms;
    config.seed = 42;
    trace2d::runtime::FixedStepRuntime runtime{config};

    const trace2d::runtime::RuntimeState before = runtime.State();
    runtime.Step(0);

    EXPECT_EQ(runtime.State(), before);
}

TEST(FixedStepRuntimeTests, AdvancesOneAndMultipleFramesWithoutSleeping)
{
    trace2d::runtime::RuntimeConfig config{};
    config.fixedTimestep = 10ms;
    trace2d::runtime::FixedStepRuntime runtime{config};

    runtime.Step();
    EXPECT_EQ(runtime.State().frame, 1U);
    EXPECT_EQ(runtime.State().simulationTime, 10ms);

    runtime.Step(119);
    EXPECT_EQ(runtime.State().frame, 120U);
    EXPECT_EQ(runtime.State().simulationTime, 1200ms);
}

TEST(FixedStepRuntimeTests, SameSeedAndStepsProduceIdenticalState)
{
    trace2d::runtime::RuntimeConfig config{};
    config.fixedTimestep = 8ms;
    config.seed = 0xC0FFEEU;

    trace2d::runtime::FixedStepRuntime first{config};
    trace2d::runtime::FixedStepRuntime second{config};

    first.Step(120);
    second.Step(120);

    EXPECT_EQ(first.State(), second.State());
}

TEST(FixedStepRuntimeTests, ResetRestoresFrameTimeAccumulatorAndSeed)
{
    trace2d::runtime::RuntimeConfig config{};
    config.fixedTimestep = 10ms;
    config.seed = 1;
    trace2d::runtime::FixedStepRuntime runtime{config};

    EXPECT_EQ(runtime.AccumulateElapsed(25ms), 2U);
    runtime.Reset(99);

    const trace2d::runtime::RuntimeState state = runtime.State();
    EXPECT_EQ(state.frame, 0U);
    EXPECT_EQ(state.seed, 99U);
    EXPECT_EQ(state.simulationTime, 0ns);
    EXPECT_EQ(state.accumulatedWallTime, 0ns);
}

TEST(FixedStepRuntimeTests, WallClockAccumulationPreservesSubstepRemainder)
{
    trace2d::runtime::RuntimeConfig config{};
    config.fixedTimestep = 10ms;
    trace2d::runtime::FixedStepRuntime runtime{config};

    EXPECT_EQ(runtime.AccumulateElapsed(6ms), 0U);
    EXPECT_EQ(runtime.State().accumulatedWallTime, 6ms);

    EXPECT_EQ(runtime.AccumulateElapsed(9ms), 1U);
    EXPECT_EQ(runtime.State().frame, 1U);
    EXPECT_EQ(runtime.State().simulationTime, 10ms);
    EXPECT_EQ(runtime.State().accumulatedWallTime, 5ms);

    EXPECT_EQ(runtime.AccumulateElapsed(25ms), 3U);
    EXPECT_EQ(runtime.State().frame, 4U);
    EXPECT_EQ(runtime.State().simulationTime, 40ms);
    EXPECT_EQ(runtime.State().accumulatedWallTime, 0ns);
}

TEST(FixedStepRuntimeTests, NonPositiveElapsedTimeDoesNotAdvance)
{
    trace2d::runtime::FixedStepRuntime runtime{};

    EXPECT_EQ(runtime.AccumulateElapsed(0ns), 0U);
    EXPECT_EQ(runtime.AccumulateElapsed(-1ns), 0U);
    EXPECT_EQ(runtime.State().frame, 0U);
}
} // namespace
