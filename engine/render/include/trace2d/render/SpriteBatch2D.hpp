#pragma once

#include <trace2d/render/RenderData.hpp>
#include <trace2d/render/SpriteAppearance2D.hpp>
#include <trace2d/render/SpriteGeometry2D.hpp>
#include <trace2d/render/SpriteOrderMask2D.hpp>
#include <trace2d/render/SpritePrimitive2D.hpp>

#include <cstdint>
#include <span>
#include <type_traits>

namespace trace2d::render
{
using SpriteMaterialPipelineIdentity = std::uint32_t;
inline constexpr SpriteMaterialPipelineIdentity InvalidSpriteMaterialPipelineIdentity = 0U;
inline constexpr SpriteMaterialPipelineIdentity BuiltInSpriteMaterialPipelineIdentity = 1U;

// Exact GPU-state compatibility for one SR7 built-in Sprite batch run. It deliberately excludes
// painter order, tint, opacity and geometry: those remain semantic order or per-vertex data rather
// than reasons to globally sort or split otherwise-compatible contiguous work.
struct SpriteBatchCompatibility2D final
{
    TextureHandle texture{InvalidTextureHandle};
    SpriteMaterialPipelineIdentity materialPipeline{BuiltInSpriteMaterialPipelineIdentity};
    SpriteSamplerCompatibility sampler{SpriteSamplerCompatibility::Nearest};
    SpriteBlendCompatibility blend{SpriteBlendCompatibility::Normal};
    SpriteMask2D mask{};

    [[nodiscard]] bool operator==(const SpriteBatchCompatibility2D&) const noexcept = default;
};

struct SpriteBatchItem2D final
{
    SpriteBatchCompatibility2D compatibility{};
    std::uint32_t quadCount{0U};
    bool visible{false};

    [[nodiscard]] bool operator==(const SpriteBatchItem2D&) const noexcept = default;
};

struct SpritePresentationBatchMeasurement2D final
{
    std::uint64_t submittedSprites{0U};
    std::uint64_t visibleSprites{0U};
    std::uint64_t culledSprites{0U};
    std::uint64_t visibleQuads{0U};
    std::uint64_t contiguousRuns{0U};

    [[nodiscard]] bool operator==(const SpritePresentationBatchMeasurement2D&) const noexcept = default;
};

// Measures an already-resolved painter sequence. Invisible/zero-output items emit no pixels and do
// not split a compatible visible run. O(N), allocation-free, deterministic and backend-independent.
[[nodiscard]] SpritePresentationBatchMeasurement2D MeasureContiguousSpritePresentationBatches(
    std::span<const SpriteBatchItem2D> items) noexcept;

// Conservative inclusive clip-space visibility used by the production SR7 path. A primitive Sprite
// is visible when at least one emitted patch intersects the exact resolved presentation view.
[[nodiscard]] bool IsSpritePresentationQuadVisible(
    const OrthographicView& view,
    const SpriteDrawQuad& quad) noexcept;

[[nodiscard]] bool IsSpritePresentationPrimitiveVisible(
    const OrthographicView& view,
    std::span<const SpritePrimitivePatch2D> patches) noexcept;

static_assert(std::is_trivially_copyable_v<SpriteBatchCompatibility2D>);
static_assert(std::is_trivially_copyable_v<SpriteBatchItem2D>);
static_assert(std::is_trivially_copyable_v<SpritePresentationBatchMeasurement2D>);
} // namespace trace2d::render
