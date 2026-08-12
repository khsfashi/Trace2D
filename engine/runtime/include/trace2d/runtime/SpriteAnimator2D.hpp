#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
#include <vector>

namespace trace2d::runtime
{
using SpriteAnimationTime2D = std::chrono::nanoseconds;

struct SpriteAnimationFrame2D final
{
    std::uint32_t regionIndex{0};
    SpriteAnimationTime2D duration{0};

    [[nodiscard]] bool operator==(const SpriteAnimationFrame2D&) const noexcept = default;
};

enum class SpriteAnimationClipError : std::uint8_t
{
    None = 0,
    EmptyFrames,
    TooManyFrames,
    RegionIndexOutOfRange,
    NonPositiveFrameDuration,
    DurationOverflow,
    NotPrepared,
    TimeOutOfRange,
};

struct SpriteAnimationClipStatus final
{
    SpriteAnimationClipError error{SpriteAnimationClipError::None};
    std::uint32_t frameIndex{0};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == SpriteAnimationClipError::None;
    }

    [[nodiscard]] bool operator==(const SpriteAnimationClipStatus&) const noexcept = default;
};

class SpriteAnimationClip2D final
{
public:
    SpriteAnimationClip2D() = default;
    SpriteAnimationClip2D(const SpriteAnimationClip2D&) = delete;
    SpriteAnimationClip2D& operator=(const SpriteAnimationClip2D&) = delete;
    SpriteAnimationClip2D(SpriteAnimationClip2D&&) noexcept = default;
    SpriteAnimationClip2D& operator=(SpriteAnimationClip2D&&) noexcept = default;
    ~SpriteAnimationClip2D() = default;

    [[nodiscard]] static SpriteAnimationClipStatus Prepare(
        std::span<const SpriteAnimationFrame2D> frames,
        std::uint32_t spriteRegionCount,
        SpriteAnimationClip2D& outClip);

    [[nodiscard]] bool Prepared() const noexcept;
    [[nodiscard]] std::uint32_t FrameCount() const noexcept;
    [[nodiscard]] SpriteAnimationTime2D Duration() const noexcept;
    [[nodiscard]] std::span<const SpriteAnimationFrame2D> Frames() const noexcept;
    [[nodiscard]] std::span<const SpriteAnimationTime2D> FrameBoundaries() const noexcept;

    [[nodiscard]] SpriteAnimationClipStatus ResolveFrameIndex(
        SpriteAnimationTime2D time,
        std::uint32_t& outFrameIndex) const noexcept;

private:
    std::vector<SpriteAnimationFrame2D> frames_{};
    std::vector<SpriteAnimationTime2D> frameBoundaries_{};
    SpriteAnimationTime2D duration_{0};
    bool prepared_{false};
};

struct SpriteAnimationSpeed2D final
{
    std::uint32_t numerator{1};
    std::uint32_t denominator{1};

    [[nodiscard]] bool operator==(const SpriteAnimationSpeed2D&) const noexcept = default;
};

[[nodiscard]] bool NormalizeSpriteAnimationSpeed(
    SpriteAnimationSpeed2D requested,
    SpriteAnimationSpeed2D& outSpeed) noexcept;
[[nodiscard]] bool IsCanonicalSpriteAnimationSpeed(SpriteAnimationSpeed2D speed) noexcept;

enum class SpriteAnimationPlaybackState : std::uint8_t
{
    Stopped = 0,
    Playing,
    Paused,
};

enum class SpriteAnimationLoopMode : std::uint8_t
{
    Once = 0,
    Loop,
    PingPong,
};

enum class SpriteAnimationDirection : std::uint8_t
{
    Forward = 0,
    Reverse,
};

struct SpriteAnimator2DState final
{
    const SpriteAnimationClip2D* clip{nullptr};
    SpriteAnimationTime2D time{0};
    std::uint32_t frameIndex{0};
    SpriteAnimationPlaybackState playback{SpriteAnimationPlaybackState::Stopped};
    SpriteAnimationLoopMode loopMode{SpriteAnimationLoopMode::Once};
    SpriteAnimationDirection direction{SpriteAnimationDirection::Forward};
    bool completed{false};
    SpriteAnimationSpeed2D speed{};

    [[nodiscard]] bool operator==(const SpriteAnimator2DState&) const noexcept = default;
};

enum class SpriteAnimator2DError : std::uint8_t
{
    None = 0,
    NullClip,
    UnpreparedClip,
    TimeOutOfRange,
    FrameIndexMismatch,
    InvalidPlaybackState,
    InvalidLoopMode,
    InvalidDirection,
    InvalidSpeed,
    InvalidCompletionState,
};

struct SpriteAnimator2DStatus final
{
    SpriteAnimator2DError error{SpriteAnimator2DError::None};
    std::uint32_t expectedFrameIndex{0};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == SpriteAnimator2DError::None;
    }

    [[nodiscard]] bool operator==(const SpriteAnimator2DStatus&) const noexcept = default;
};

[[nodiscard]] SpriteAnimator2DStatus MakeSpriteAnimator2DState(
    const SpriteAnimationClip2D& clip,
    SpriteAnimationTime2D time,
    SpriteAnimationPlaybackState playback,
    SpriteAnimationLoopMode loopMode,
    SpriteAnimationDirection direction,
    bool completed,
    SpriteAnimationSpeed2D speed,
    SpriteAnimator2DState& outState) noexcept;

[[nodiscard]] SpriteAnimator2DStatus ValidateSpriteAnimator2DState(
    const SpriteAnimator2DState& state) noexcept;

class SpriteAnimator2D final
{
public:
    [[nodiscard]] SpriteAnimator2DStatus RestoreState(const SpriteAnimator2DState& state) noexcept;

    [[nodiscard]] bool HasState() const noexcept;
    [[nodiscard]] const SpriteAnimator2DState& State() const noexcept;
    [[nodiscard]] const SpriteAnimationFrame2D* CurrentFrame() const noexcept;
    [[nodiscard]] bool TryGetCurrentRegionIndex(std::uint32_t& outRegionIndex) const noexcept;

private:
    SpriteAnimator2DState state_{};
};

static_assert(std::is_trivially_copyable_v<SpriteAnimationFrame2D>);
static_assert(std::is_trivially_copyable_v<SpriteAnimationSpeed2D>);
static_assert(std::is_trivially_copyable_v<SpriteAnimator2DState>);
} // namespace trace2d::runtime
