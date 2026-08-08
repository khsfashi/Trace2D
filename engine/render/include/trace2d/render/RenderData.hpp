#pragma once

#include <cstdint>
#include <span>
#include <type_traits>

namespace trace2d::render
{
using TextureHandle = std::uint32_t;
inline constexpr TextureHandle InvalidTextureHandle = 0;

struct Float2 final
{
    float x{0.0F};
    float y{0.0F};

    [[nodiscard]] bool operator==(const Float2&) const noexcept = default;
};

struct OrthographicCamera final
{
    Float2 center{};
    float verticalSize{10.0F};

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

static_assert(std::is_trivially_copyable_v<Float2>);
static_assert(std::is_trivially_copyable_v<OrthographicCamera>);
static_assert(std::is_trivially_copyable_v<OrthographicView>);
static_assert(std::is_trivially_copyable_v<SpriteRenderData>);
static_assert(std::is_trivially_copyable_v<SpriteInstanceData>);
static_assert(std::is_trivially_copyable_v<SpriteBatchMeasurement>);
static_assert(sizeof(SpriteInstanceData) == 16);
} // namespace trace2d::render
