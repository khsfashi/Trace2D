#pragma once

#include <trace2d/render/SpriteRenderContract.hpp>
#include <trace2d/scene/SpriteTransform2D.hpp>

#include <cstdint>
#include <string_view>
#include <type_traits>

namespace trace2d::render
{
enum class SpriteGeometryError : std::uint8_t
{
    None = 0,
    UnresolvedSelection,
    InvalidPose,
    InvalidPixelsPerUnit,
    InvalidSourceSize,
    InvalidPivot,
    InvalidPageSize,
    InvalidTrimRect,
    InvalidPackedRect,
    PackedExtentMismatch,
    UnsupportedPackedRotation,
    GeometryOverflow,
    UvOverflow,
};

enum class SpriteGeometryField : std::uint8_t
{
    None = 0,
    Selection,
    Pose,
    PixelsPerUnit,
    SourceSize,
    Pivot,
    PageSize,
    TrimRect,
    PackedRect,
    PackedRotation,
    LogicalQuad,
    DrawQuad,
    Uv,
};

[[nodiscard]] std::string_view ToString(SpriteGeometryError value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteGeometryField value) noexcept;

struct SpriteGeometryStatus final
{
    SpriteGeometryError error{SpriteGeometryError::None};
    SpriteGeometryField field{SpriteGeometryField::None};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == SpriteGeometryError::None;
    }

    [[nodiscard]] bool operator==(const SpriteGeometryStatus&) const noexcept = default;
};

struct SpriteLogicalQuad final
{
    Float2 topLeft{};
    Float2 topRight{};
    Float2 bottomRight{};
    Float2 bottomLeft{};

    [[nodiscard]] bool operator==(const SpriteLogicalQuad&) const noexcept = default;
};

struct SpriteDrawVertex final
{
    Float2 position{};
    Float2 uv{};

    [[nodiscard]] bool operator==(const SpriteDrawVertex&) const noexcept = default;
};

struct SpriteDrawQuad final
{
    SpriteDrawVertex topLeft{};
    SpriteDrawVertex topRight{};
    SpriteDrawVertex bottomRight{};
    SpriteDrawVertex bottomLeft{};

    [[nodiscard]] bool operator==(const SpriteDrawQuad&) const noexcept = default;
};

// Builds the untrimmed logical Sprite quad from S1 source_size/pivot and an SR1 pose.
// packed_rect/trim storage details are intentionally ignored.
// O(1), fixed-size caller-owned output, no allocation/GPU/renderer initialization.
[[nodiscard]] SpriteGeometryStatus BuildSpriteLogicalQuad(
    const ResolvedSpriteRegion& selection,
    const scene::SpritePose2D& pose,
    float pixelsPerUnit,
    SpriteLogicalQuad& outQuad) noexcept;

// Builds the visible trimmed Sprite quad and canonical page-space UVs from an already
// resolved SR0 selection plus SR1 pose. Canonical UV origin is atlas top-left, +u right,
// +v down, with pixel-edge normalized coordinates and no half-texel offset. Packed cw90
// storage changes UV corner mapping only; logical placement still comes from trim source space.
// O(1), one sin/cos pair maximum, fixed-size caller-owned output, no allocation/name/file/GPU work.
[[nodiscard]] SpriteGeometryStatus BuildSpriteDrawQuad(
    const ResolvedSpriteRegion& selection,
    const scene::SpritePose2D& pose,
    float pixelsPerUnit,
    SpriteDrawQuad& outQuad) noexcept;

static_assert(std::is_trivially_copyable_v<SpriteGeometryStatus>);
static_assert(std::is_trivially_copyable_v<SpriteLogicalQuad>);
static_assert(std::is_trivially_copyable_v<SpriteDrawVertex>);
static_assert(std::is_trivially_copyable_v<SpriteDrawQuad>);
} // namespace trace2d::render
