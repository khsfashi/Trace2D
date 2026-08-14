#include <trace2d/runtime/FixedStepRuntime.hpp>

#include <limits>
#include <stdexcept>

namespace trace2d::runtime
{
FixedStepRuntime::FixedStepRuntime(const RuntimeConfig& config)
    : config_{config}
    , seed_{config.seed}
{
    if (config_.fixedTimestep.count() <= 0)
    {
        throw std::invalid_argument{"Runtime fixed timestep must be greater than zero."};
    }
}

void FixedStepRuntime::Reset(const std::uint64_t seed) noexcept
{
    frame_ = 0;
    seed_ = seed;
    simulationTime_ = std::chrono::nanoseconds{0};
    accumulatedWallTime_ = std::chrono::nanoseconds{0};
}

void FixedStepRuntime::Step(const std::uint64_t count)
{
    if (count == 0)
    {
        return;
    }

    if (count > std::numeric_limits<std::uint64_t>::max() - frame_)
    {
        throw std::overflow_error{"Runtime frame counter overflow."};
    }

    const std::chrono::nanoseconds::rep timestepNanoseconds = config_.fixedTimestep.count();
    const std::chrono::nanoseconds::rep remainingNanoseconds =
        std::numeric_limits<std::chrono::nanoseconds::rep>::max() - simulationTime_.count();
    const auto maxAdditionalFrames = static_cast<std::uint64_t>(remainingNanoseconds / timestepNanoseconds);

    if (count > maxAdditionalFrames)
    {
        throw std::overflow_error{"Runtime simulation time overflow."};
    }

    frame_ += count;
    simulationTime_ += config_.fixedTimestep * static_cast<std::chrono::nanoseconds::rep>(count);
}

std::uint64_t FixedStepRuntime::ConsumeElapsed(const std::chrono::nanoseconds elapsed)
{
    if (elapsed.count() <= 0)
    {
        return 0;
    }

    const std::chrono::nanoseconds::rep currentNanoseconds = accumulatedWallTime_.count();
    if (elapsed.count() > std::numeric_limits<std::chrono::nanoseconds::rep>::max() - currentNanoseconds)
    {
        throw std::overflow_error{"Runtime wall-clock accumulator overflow."};
    }

    const std::chrono::nanoseconds::rep totalNanoseconds = currentNanoseconds + elapsed.count();
    const std::chrono::nanoseconds::rep timestepNanoseconds = config_.fixedTimestep.count();
    const auto availableFrames = static_cast<std::uint64_t>(totalNanoseconds / timestepNanoseconds);
    accumulatedWallTime_ = std::chrono::nanoseconds{totalNanoseconds % timestepNanoseconds};
    return availableFrames;
}

std::uint64_t FixedStepRuntime::AccumulateElapsed(const std::chrono::nanoseconds elapsed)
{
    const std::chrono::nanoseconds previousAccumulator = accumulatedWallTime_;
    const std::uint64_t availableFrames = ConsumeElapsed(elapsed);

    try
    {
        Step(availableFrames);
    }
    catch (...)
    {
        accumulatedWallTime_ = previousAccumulator;
        throw;
    }

    return availableFrames;
}

const RuntimeConfig& FixedStepRuntime::Config() const noexcept
{
    return config_;
}

RuntimeState FixedStepRuntime::State() const noexcept
{
    return RuntimeState{
        .frame = frame_,
        .seed = seed_,
        .simulationTime = simulationTime_,
        .accumulatedWallTime = accumulatedWallTime_,
    };
}
} // namespace trace2d::runtime
