#pragma once

#include <chrono>
#include <cstdint>

namespace trace2d::runtime
{
struct RuntimeConfig
{
    std::chrono::nanoseconds fixedTimestep{std::chrono::nanoseconds{16'666'667}};
    std::uint64_t seed{0};
};

struct RuntimeState
{
    std::uint64_t frame{0};
    std::uint64_t seed{0};
    std::chrono::nanoseconds simulationTime{0};
    std::chrono::nanoseconds accumulatedWallTime{0};

    [[nodiscard]] bool operator==(const RuntimeState&) const noexcept = default;
};

class FixedStepRuntime final
{
public:
    explicit FixedStepRuntime(const RuntimeConfig& config = {});

    void Reset(std::uint64_t seed) noexcept;
    void Step(std::uint64_t count = 1);

    [[nodiscard]] std::uint64_t AccumulateElapsed(std::chrono::nanoseconds elapsed);

    [[nodiscard]] const RuntimeConfig& Config() const noexcept;
    [[nodiscard]] RuntimeState State() const noexcept;

private:
    RuntimeConfig config_{};
    std::uint64_t frame_{0};
    std::uint64_t seed_{0};
    std::chrono::nanoseconds simulationTime_{0};
    std::chrono::nanoseconds accumulatedWallTime_{0};
};
} // namespace trace2d::runtime
