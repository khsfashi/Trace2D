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
    GeometryOverflow,
};

enum class SpriteGeometryField : std::uint8_t
{
    None = 0,
    Selection,
    Pose,
    PixelsPerUnit,
    SourceSize,
    Pivot,
    LogicalQuad,
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

// Builds the untrimmed logical Sprite quad from S1 source_size/pivot and an SR1 pose.
// packed_rect/trim storage details are intentionally ignored until SR2.
// O(1), fixed-size caller-owned output, no allocation/GPU/renderer initialization.
[[nodiscard]] SpriteGeometryStatus BuildSpriteLogicalQuad(
    const ResolvedSpriteRegion& selection,
    const scene::SpritePose2D& pose,
    float pixelsPerUnit,
    SpriteLogicalQuad& outQuad) noexcept;

static_assert(std::is_trivially_copyable_v<SpriteGeometryStatus>);
static_assert(std::is_trivially_copyable_v<SpriteLogicalQuad>);
} // namespace trace2d::render
