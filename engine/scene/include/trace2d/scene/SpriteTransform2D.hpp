#pragma once

#include <trace2d/scene/Scene.hpp>

#include <cstdint>
#include <string_view>
#include <type_traits>

namespace trace2d::scene
{
enum class SpritePoseError : std::uint8_t
{
    None = 0,
    NonFiniteTransform,
    NonFiniteAlpha,
    AlphaOutOfRange,
    InterpolationOverflow,
};

enum class SpritePoseField : std::uint8_t
{
    None = 0,
    Position,
    RotationRadians,
    Scale,
    Alpha,
};

[[nodiscard]] std::string_view ToString(SpritePoseError value) noexcept;
[[nodiscard]] std::string_view ToString(SpritePoseField value) noexcept;

struct SpritePoseStatus final
{
    SpritePoseError error{SpritePoseError::None};
    SpritePoseField field{SpritePoseField::None};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == SpritePoseError::None;
    }

    [[nodiscard]] bool operator==(const SpritePoseStatus&) const noexcept = default;
};

struct SpritePose2D final
{
    Transform2D transform{};
    bool flipX{false};
    bool flipY{false};

    [[nodiscard]] bool operator==(const SpritePose2D&) const noexcept = default;
};

struct SpritePoseHistory2D final
{
    SpritePose2D previousFixed{};
    SpritePose2D currentFixed{};

    [[nodiscard]] bool operator==(const SpritePoseHistory2D&) const noexcept = default;
};

[[nodiscard]] SpritePoseStatus ValidateSpritePose(const SpritePose2D& pose) noexcept;

// Creation/reset/load/teleport/warp/snap semantics. On failure history is unchanged.
[[nodiscard]] SpritePoseStatus SnapSpritePoseHistory(
    SpritePoseHistory2D& history,
    const SpritePose2D& authoritativePose) noexcept;

// Successful fixed-step commit. On failure history is unchanged.
// Not calling this function means an aborted/uncommitted step cannot advance history.
[[nodiscard]] SpritePoseStatus CommitSpriteFixedPose(
    SpritePoseHistory2D& history,
    const SpritePose2D& authoritativePose) noexcept;

// Exact-frame presentation: authoritative current state, no interpolation/remainder input.
[[nodiscard]] SpritePoseStatus ResolveSpriteAuthoritativeCurrent(
    const SpritePoseHistory2D& history,
    SpritePose2D& outPose) noexcept;

// Interactive presentation. Continuous transform fields interpolate, while flipX/flipY
// always come from currentFixed because they are discrete authoritative semantics.
[[nodiscard]] SpritePoseStatus InterpolateSpritePose(
    const SpritePoseHistory2D& history,
    float alpha,
    SpritePose2D& outPose) noexcept;

static_assert(std::is_trivially_copyable_v<SpritePoseStatus>);
static_assert(std::is_trivially_copyable_v<SpritePose2D>);
static_assert(std::is_trivially_copyable_v<SpritePoseHistory2D>);
} // namespace trace2d::scene
