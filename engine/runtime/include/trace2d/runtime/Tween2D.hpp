#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace trace2d::runtime
{
using TweenTime2D = std::chrono::nanoseconds;

enum class TweenTimeDomain2D : std::uint8_t
{
    Simulation = 0,
    Presentation,
};

enum class TweenValueType2D : std::uint8_t
{
    Float = 0,
    Float2,
    Color,
};

struct TweenValue2D final
{
    TweenValueType2D type{TweenValueType2D::Float};
    std::array<float, 4U> components{};

    [[nodiscard]] static TweenValue2D Float(float value) noexcept;
    [[nodiscard]] static TweenValue2D Float2(float x, float y) noexcept;
    [[nodiscard]] static TweenValue2D Color(float r, float g, float b, float a) noexcept;

    [[nodiscard]] bool operator==(const TweenValue2D&) const noexcept = default;
};

enum class TweenEasing2D : std::uint8_t
{
    Linear = 0,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
};

enum class TweenLoopMode2D : std::uint8_t
{
    Restart = 0,
    Yoyo,
};

enum class TweenPlaybackState2D : std::uint8_t
{
    Playing = 0,
    Paused,
    Completed,
    Cancelled,
};

enum class TweenCancellationReason2D : std::uint8_t
{
    None = 0,
    Explicit,
    Replaced,
    TargetInvalidated,
    PropertyWriteRejected,
};

enum class Tween2DError : std::uint8_t
{
    None = 0,
    InvalidHandle,
    InvalidTimeDomain,
    InvalidValueType,
    ValueTypeMismatch,
    InvalidEasing,
    InvalidLoopMode,
    InvalidCancellationReason,
    NegativeDelay,
    NonPositiveDuration,
    NegativeDelta,
    InvalidPlaybackTransition,
    LoopCounterOverflow,
    SlotCapacityExceeded,
};

struct Tween2DStatus final
{
    Tween2DError error{Tween2DError::None};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == Tween2DError::None;
    }

    [[nodiscard]] bool operator==(const Tween2DStatus&) const noexcept = default;
};

struct TweenSpec2D final
{
    TweenTimeDomain2D domain{TweenTimeDomain2D::Presentation};
    TweenTime2D delay{0};
    TweenTime2D duration{0};
    TweenValue2D start{};
    TweenValue2D end{};
    TweenEasing2D easing{TweenEasing2D::Linear};
    TweenLoopMode2D loopMode{TweenLoopMode2D::Restart};
    std::uint32_t repeatCount{0U};
    bool infinite{false};

    [[nodiscard]] bool operator==(const TweenSpec2D&) const noexcept = default;
};

struct TweenHandle2D final
{
    static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{InvalidIndex};
    std::uint64_t generation{0U};

    [[nodiscard]] bool Valid() const noexcept
    {
        return index != InvalidIndex && generation != 0U;
    }

    [[nodiscard]] bool operator==(const TweenHandle2D&) const noexcept = default;
};

struct TweenState2D final
{
    TweenTimeDomain2D domain{TweenTimeDomain2D::Presentation};
    TweenPlaybackState2D playback{TweenPlaybackState2D::Playing};
    TweenCancellationReason2D cancellationReason{TweenCancellationReason2D::None};
    TweenTime2D delayElapsed{0};
    TweenTime2D loopElapsed{0};
    std::uint64_t loopIndex{0U};
    bool reverse{false};
    TweenValue2D currentValue{};

    [[nodiscard]] bool operator==(const TweenState2D&) const noexcept = default;
};

struct TweenPoolMetrics2D final
{
    std::uint64_t activeCount{0U};
    std::uint64_t retainedSlotCount{0U};
    std::uint64_t retainedCapacity{0U};
    std::uint64_t highWaterActiveCount{0U};
    std::uint64_t createdCount{0U};
    std::uint64_t reusedSlotCount{0U};

    [[nodiscard]] bool operator==(const TweenPoolMetrics2D&) const noexcept = default;
};

[[nodiscard]] Tween2DStatus ValidateTweenSpec2D(const TweenSpec2D& spec) noexcept;
[[nodiscard]] double EvaluateTweenEasing2D(TweenEasing2D easing, double progress) noexcept;
[[nodiscard]] TweenValue2D InterpolateTweenValue2D(
    const TweenValue2D& start,
    const TweenValue2D& end,
    double easedProgress) noexcept;

class TweenPool2D final
{
public:
    TweenPool2D() = default;

    void Reserve(std::size_t capacity);

    [[nodiscard]] Tween2DStatus Create(const TweenSpec2D& spec, TweenHandle2D& outHandle);
    [[nodiscard]] Tween2DStatus Inspect(TweenHandle2D handle, TweenState2D& outState) const noexcept;
    [[nodiscard]] Tween2DStatus Pause(TweenHandle2D handle) noexcept;
    [[nodiscard]] Tween2DStatus Resume(TweenHandle2D handle) noexcept;
    [[nodiscard]] Tween2DStatus Restart(TweenHandle2D handle) noexcept;
    [[nodiscard]] Tween2DStatus Rebase(
        TweenHandle2D handle,
        TweenValue2D start,
        TweenValue2D end) noexcept;
    [[nodiscard]] Tween2DStatus Cancel(
        TweenHandle2D handle,
        TweenCancellationReason2D reason = TweenCancellationReason2D::Explicit) noexcept;
    [[nodiscard]] Tween2DStatus Step(TweenTimeDomain2D domain, TweenTime2D delta) noexcept;

    [[nodiscard]] TweenPoolMetrics2D Metrics() const noexcept;

private:
    struct Slot final
    {
        TweenSpec2D spec{};
        TweenState2D state{};
        std::uint64_t generation{0U};
        bool occupied{false};
    };

    [[nodiscard]] Slot* ResolveMutable(TweenHandle2D handle) noexcept;
    [[nodiscard]] const Slot* Resolve(TweenHandle2D handle) const noexcept;
    [[nodiscard]] Tween2DStatus AdvanceSlot(Slot& slot, TweenTime2D delta) noexcept;
    void ResetSlotState(Slot& slot) noexcept;
    void IncrementActive() noexcept;
    void DecrementActive() noexcept;

    std::vector<Slot> slots_{};
    std::uint64_t activeCount_{0U};
    std::uint64_t highWaterActiveCount_{0U};
    std::uint64_t createdCount_{0U};
    std::uint64_t reusedSlotCount_{0U};
};
} // namespace trace2d::runtime
