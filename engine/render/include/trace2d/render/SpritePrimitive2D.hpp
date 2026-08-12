#pragma once

#include <trace2d/render/SpriteAppearance2D.hpp>
#include <trace2d/render/SpriteGeometry2D.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

namespace trace2d::render
{
// Runtime presentation intent. The same canonical SpriteRegion may be presented through any mode;
// only SpriteRegion::border is canonical authored 9-slice metadata.
enum class SpritePrimitiveMode : std::uint8_t
{
    Quad = 0,
    Sliced = 1,
    Tiled = 2,
};

// Safety/diagnostic bound for one top-level Sprite. This is not a performance recommendation.
// SR7 may optimize compatible primitive work, but SR5 never expands one Sprite without a bound.
inline constexpr std::size_t MaximumSpritePrimitiveQuads = 4096U;

struct SpritePrimitive2D final
{
    SpritePrimitiveMode mode{SpritePrimitiveMode::Quad};

    // Used only by Sliced/Tiled. Units are source-pixel-equivalent dimensions before PPU and
    // the SR1 transform. Fractional values are permitted when finite and strictly positive.
    Float2 targetSizeSourcePixels{};

    [[nodiscard]] bool operator==(const SpritePrimitive2D&) const noexcept = default;
};

enum class SpritePrimitiveError : std::uint8_t
{
    None = 0,
    UnresolvedSelection,
    InvalidSourceSize,
    InvalidBorder,
    InvalidTargetSize,
    InvalidPageSize,
    InvalidTrimRect,
    InvalidPackedRect,
    PackedExtentMismatch,
    UnsupportedPackedRotation,
    UnsupportedMode,
    CountOverflow,
    ExpansionLimit,
    InsufficientCapacity,
    Geometry,
    UvOverflow,
    SampleBoundsOverflow,
};

enum class SpritePrimitiveField : std::uint8_t
{
    None = 0,
    Selection,
    SourceSize,
    Border,
    TargetSize,
    PageSize,
    TrimRect,
    PackedRect,
    PackedRotation,
    Mode,
    PatchCount,
    OutputCapacity,
    Geometry,
    Uv,
    SampleBounds,
};

[[nodiscard]] std::string_view ToString(SpritePrimitiveMode value) noexcept;
[[nodiscard]] std::string_view ToString(SpritePrimitiveError value) noexcept;
[[nodiscard]] std::string_view ToString(SpritePrimitiveField value) noexcept;

struct SpritePrimitiveStatus final
{
    SpritePrimitiveError error{SpritePrimitiveError::None};
    SpritePrimitiveField field{SpritePrimitiveField::None};
    SpriteGeometryStatus geometry{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == SpritePrimitiveError::None;
    }

    [[nodiscard]] bool operator==(const SpritePrimitiveStatus&) const noexcept = default;
};

// One derived visible patch. UVs remain pixel-edge geometry truth. sampleBounds are a separate
// texel-center guard for this exact sampled sub-rectangle and replace the broader SR3 region guard
// only while this patch is submitted.
struct SpritePrimitivePatch2D final
{
    SpriteDrawQuad quad{};
    SpriteSampleBounds sampleBounds{};

    [[nodiscard]] bool operator==(const SpritePrimitivePatch2D&) const noexcept = default;
};

// Counts the exact visible patches after trim intersection and repetition. No pose/PPU/GPU state is
// required. O(1) for Quad/Sliced and O(1) arithmetic for Tiled; it never loops once per tile.
// On success outPatchCount may be zero for an intentionally fully-trimmed-away resized partition.
[[nodiscard]] SpritePrimitiveStatus CountSpritePrimitivePatches(
    const ResolvedSpriteRegion& selection,
    const SpritePrimitive2D& primitive,
    std::size_t& outPatchCount) noexcept;

// Builds patches into caller-owned storage. On InsufficientCapacity, outPatchCount is set to the
// required count and no partial output is written. Successful Sliced/Tiled output is deterministic:
// source 9-slice cells are visited row-major, and repeated pieces are visited top-to-bottom then
// left-to-right inside each cell. Ordinary Quad delegates to the existing SR2 draw quad semantics.
//
// The canonical pivot remains authoritative. For a resized primitive, its normalized position in
// the untrimmed source rectangle is preserved in the target rectangle; the asset pivot is never
// mutated. SR1 scale/flip/rotation/translation then apply exactly once.
[[nodiscard]] SpritePrimitiveStatus BuildSpritePrimitivePatches(
    const ResolvedSpriteRegion& selection,
    const scene::SpritePose2D& pose,
    float pixelsPerUnit,
    const SpritePrimitive2D& primitive,
    std::span<SpritePrimitivePatch2D> output,
    std::size_t& outPatchCount) noexcept;

static_assert(std::is_trivially_copyable_v<SpritePrimitive2D>);
static_assert(std::is_trivially_copyable_v<SpritePrimitiveStatus>);
static_assert(std::is_trivially_copyable_v<SpritePrimitivePatch2D>);
} // namespace trace2d::render
