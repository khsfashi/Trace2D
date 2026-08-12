#pragma once

#include <trace2d/runtime/SpriteAnimator2D.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::agent
{
inline constexpr std::size_t MaxSpriteAnimationAgentEmissions = 4'096U;

enum class SpriteAnimationInspectionErrorCode : std::uint8_t
{
    AnimatorUnavailable = 0,
    AnimatorStateUnavailable,
    ClipUnavailable,
    InvalidAction,
    InvalidAssertion,
    TypeMismatch,
    StateMismatch,
    RuntimeRejected,
    OutputCapacityExceeded,
};

[[nodiscard]] std::string_view ToString(SpriteAnimationInspectionErrorCode code) noexcept;
[[nodiscard]] std::string_view ToString(runtime::SpriteAnimationPlaybackState state) noexcept;
[[nodiscard]] std::string_view ToString(runtime::SpriteAnimationLoopMode mode) noexcept;
[[nodiscard]] std::string_view ToString(runtime::SpriteAnimationDirection direction) noexcept;
[[nodiscard]] std::string_view ToString(runtime::SpriteAnimationEmissionKind kind) noexcept;
[[nodiscard]] std::string_view ToString(runtime::SpriteAnimator2DError error) noexcept;

enum class SpriteAnimationValueKind : std::uint8_t
{
    Boolean = 0,
    SignedInteger,
    UnsignedInteger,
    String,
};

[[nodiscard]] std::string_view ToString(SpriteAnimationValueKind kind) noexcept;

struct SpriteAnimationValue final
{
    SpriteAnimationValueKind kind{SpriteAnimationValueKind::UnsignedInteger};
    bool booleanValue{false};
    std::int64_t signedIntegerValue{0};
    std::uint64_t unsignedIntegerValue{0};
    std::string stringValue{};

    [[nodiscard]] static SpriteAnimationValue Boolean(bool value) noexcept;
    [[nodiscard]] static SpriteAnimationValue Signed(std::int64_t value) noexcept;
    [[nodiscard]] static SpriteAnimationValue Unsigned(std::uint64_t value) noexcept;
    [[nodiscard]] static SpriteAnimationValue String(std::string value) noexcept;

    [[nodiscard]] bool operator==(const SpriteAnimationValue&) const noexcept = default;
};

struct SpriteAnimatorBinding final
{
    std::string_view entitySemanticId{};
    runtime::SpriteAnimator2D* animator{nullptr};
};

struct SpriteAnimationInspectionError final
{
    SpriteAnimationInspectionErrorCode code{SpriteAnimationInspectionErrorCode::AnimatorUnavailable};
    runtime::SpriteAnimator2DError runtimeError{runtime::SpriteAnimator2DError::None};
    std::string message{};

    [[nodiscard]] bool operator==(const SpriteAnimationInspectionError&) const noexcept = default;
};

struct SpriteAnimatorSnapshot final
{
    std::string entitySemanticId{};
    std::int64_t clipDurationNanoseconds{0};
    std::uint32_t clipFrameCount{0};
    std::uint32_t clipEventCount{0};
    std::int64_t timeNanoseconds{0};
    std::uint32_t frameIndex{0};
    std::uint32_t regionIndex{0};
    runtime::SpriteAnimationPlaybackState playback{runtime::SpriteAnimationPlaybackState::Stopped};
    runtime::SpriteAnimationLoopMode loopMode{runtime::SpriteAnimationLoopMode::Once};
    runtime::SpriteAnimationDirection direction{runtime::SpriteAnimationDirection::Forward};
    bool completed{false};
    std::uint32_t speedNumerator{1};
    std::uint32_t speedDenominator{1};
    std::uint32_t speedRemainder{0};

    [[nodiscard]] bool operator==(const SpriteAnimatorSnapshot&) const noexcept = default;
};

struct SpriteAnimatorInspectionResult final
{
    std::optional<SpriteAnimatorSnapshot> snapshot{};
    std::optional<SpriteAnimationInspectionError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return snapshot.has_value() && !error.has_value();
    }
};

enum class SpriteAnimationActionKind : std::uint8_t
{
    Play = 0,
    Pause,
    Stop,
    Reset,
    Restart,
    Seek,
    SetSpeed,
    SetDirection,
    Advance,
};

[[nodiscard]] std::string_view ToString(SpriteAnimationActionKind kind) noexcept;

struct SpriteAnimationAction final
{
    SpriteAnimationActionKind kind{SpriteAnimationActionKind::Play};
    runtime::SpriteAnimationTime2D time{0};
    runtime::SpriteAnimationSpeed2D speed{};
    runtime::SpriteAnimationDirection direction{runtime::SpriteAnimationDirection::Forward};
    std::size_t emissionCapacity{0};

    [[nodiscard]] bool operator==(const SpriteAnimationAction&) const noexcept = default;
};

struct SpriteAnimationEmissionSnapshot final
{
    runtime::SpriteAnimationEmissionKind kind{runtime::SpriteAnimationEmissionKind::AuthoredEvent};
    runtime::SpriteAnimationEventId2D eventId{0};
    std::uint32_t authoredOrdinal{0};
    std::int64_t timeNanoseconds{0};
    runtime::SpriteAnimationDirection direction{runtime::SpriteAnimationDirection::Forward};

    [[nodiscard]] bool operator==(const SpriteAnimationEmissionSnapshot&) const noexcept = default;
};

struct SpriteAnimationActionResult final
{
    SpriteAnimationAction action{};
    std::optional<SpriteAnimatorSnapshot> snapshot{};
    std::vector<SpriteAnimationEmissionSnapshot> emissions{};
    std::optional<SpriteAnimationInspectionError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return snapshot.has_value() && !error.has_value();
    }
};

enum class SpriteAnimationAssertionField : std::uint8_t
{
    ClipDurationNanoseconds = 0,
    ClipFrameCount,
    ClipEventCount,
    TimeNanoseconds,
    FrameIndex,
    RegionIndex,
    Playback,
    LoopMode,
    Direction,
    Completed,
    SpeedNumerator,
    SpeedDenominator,
    SpeedRemainder,
};

[[nodiscard]] std::string_view ToString(SpriteAnimationAssertionField field) noexcept;

struct SpriteAnimationAssertion final
{
    SpriteAnimationAssertionField field{SpriteAnimationAssertionField::FrameIndex};
    SpriteAnimationValue expected{};

    [[nodiscard]] bool operator==(const SpriteAnimationAssertion&) const noexcept = default;
};

struct SpriteAnimationAssertionContext final
{
    std::string entitySemanticId{};
    std::int64_t timeNanoseconds{0};
    std::uint32_t frameIndex{0};
    std::uint32_t regionIndex{0};
    runtime::SpriteAnimationPlaybackState playback{runtime::SpriteAnimationPlaybackState::Stopped};
    runtime::SpriteAnimationLoopMode loopMode{runtime::SpriteAnimationLoopMode::Once};
    runtime::SpriteAnimationDirection direction{runtime::SpriteAnimationDirection::Forward};
    bool completed{false};

    [[nodiscard]] bool operator==(const SpriteAnimationAssertionContext&) const noexcept = default;
};

struct SpriteAnimationAssertionResult final
{
    SpriteAnimationAssertion assertion{};
    std::optional<SpriteAnimationValue> observed{};
    SpriteAnimationAssertionContext context{};
    std::optional<SpriteAnimationInspectionError> error{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return observed.has_value() && !error.has_value();
    }
};
} // namespace trace2d::agent
