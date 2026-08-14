#pragma once

#include <trace2d/assets/ResourceRegistry.hpp>

#include <cstdint>
#include <span>
#include <type_traits>

namespace trace2d::render
{
using TextureHandle = trace2d::assets::ResourceHandle<trace2d::assets::TextureResource>;
inline constexpr TextureHandle InvalidTextureHandle{};

struct Float2 final
{
    float x{0.0F};
    float y{0.0F};

    [[nodiscard]] bool operator==(const Float2&) const noexcept = default;
};

// Presentation-only GPU raster viewport. Disabled preserves the historical full-target Renderer
// behavior. When enabled, target dimensions are frozen with the resolved C0 presentation state so
// a later OS/window resize cannot silently reuse stale fit/fill/stretch coefficients.
struct PresentationRasterViewport2D final
{
    bool enabled{false};
    std::uint32_t targetWidth{0U};
    std::uint32_t targetHeight{0U};
    Float2 origin{};
    Float2 size{};

    [[nodiscard]] bool operator==(const PresentationRasterViewport2D&) const noexcept = default;
};

struct OrthographicCamera final
{
    Float2 center{};
    float verticalSize{10.0F};

    // Presentation-only multiplier applied after the render-target aspect projection is derived.
    // Default {1,1} preserves the pre-C0 renderer contract exactly. C0 resolved cameras pair this
    // with rasterViewport so world-to-clip stays the logical-viewport projection while the GPU
    // viewport performs fit/fill/stretch mapping without per-object camera work.
    Float2 presentationScale{1.0F, 1.0F};
    PresentationRasterViewport2D rasterViewport{};

    [[nodiscard]] bool operator==(const OrthographicCamera&) const noexcept = default;
};

struct OrthographicView final
{
    Float2 center{};
    Float2 halfExtents{};
    Float2 clipScale{};

    [[nodiscard]] bool operator==(const OrthographicView&) const noexcept = default;
};

struct SpriteRenderData final
{
    Float2 center{};
    Float2 halfExtents{0.5F, 0.5F};
    TextureHandle texture{InvalidTextureHandle};
    std::int32_t layer{0};
    std::uint64_t stableOrder{0};

    [[nodiscard]] bool operator==(const SpriteRenderData&) const noexcept = default;
};

struct SpriteInstanceData final
{
    Float2 centerClip{};
    Float2 halfClip{};

    [[nodiscard]] bool operator==(const SpriteInstanceData&) const noexcept = default;
};

struct SpriteBatchMeasurement final
{
    std::uint64_t visibleSprites{0};
    std::uint64_t culledSprites{0};
    std::uint64_t contiguousTextureRuns{0};

    [[nodiscard]] bool operator==(const SpriteBatchMeasurement&) const noexcept = default;
};

struct SpriteDrawOrderLess final
{
    [[nodiscard]] bool operator()(const SpriteRenderData& left, const SpriteRenderData& right) const noexcept;
};

[[nodiscard]] bool TryBuildOrthographicView(
    const OrthographicCamera& camera,
    std::uint32_t targetWidth,
    std::uint32_t targetHeight,
    OrthographicView& outView) noexcept;

[[nodiscard]] Float2 WorldToClip(const OrthographicView& view, Float2 worldPosition) noexcept;

[[nodiscard]] SpriteInstanceData BuildSpriteInstanceData(
    const OrthographicView& view,
    const SpriteRenderData& sprite) noexcept;

[[nodiscard]] bool IsSpriteVisible(const OrthographicView& view, const SpriteRenderData& sprite) noexcept;

[[nodiscard]] SpriteBatchMeasurement MeasureContiguousTextureBatching(
    const OrthographicView& view,
    std::span<const SpriteRenderData> sprites) noexcept;

static_assert(std::is_trivially_copyable_v<TextureHandle>);
static_assert(std::is_trivially_copyable_v<Float2>);
static_assert(std::is_trivially_copyable_v<PresentationRasterViewport2D>);
static_assert(std::is_trivially_copyable_v<OrthographicCamera>);
static_assert(std::is_trivially_copyable_v<OrthographicView>);
static_assert(std::is_trivially_copyable_v<SpriteRenderData>);
static_assert(std::is_trivially_copyable_v<SpriteInstanceData>);
static_assert(std::is_trivially_copyable_v<SpriteBatchMeasurement>);
static_assert(sizeof(SpriteInstanceData) == 16);
} // namespace trace2d::render
