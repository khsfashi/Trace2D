#include <trace2d/render/RenderData.hpp>

#include <cmath>

namespace trace2d::render
{
namespace
{
[[nodiscard]] bool IsFinite(Float2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}
} // namespace

bool SpriteDrawOrderLess::operator()(const SpriteRenderData& left, const SpriteRenderData& right) const noexcept
{
    if (left.layer != right.layer)
    {
        return left.layer < right.layer;
    }

    return left.stableOrder < right.stableOrder;
}

bool TryBuildOrthographicView(
    const OrthographicCamera& camera,
    std::uint32_t targetWidth,
    std::uint32_t targetHeight,
    OrthographicView& outView) noexcept
{
    outView = {};

    if (targetWidth == 0 || targetHeight == 0 || !IsFinite(camera.center) ||
        !std::isfinite(camera.verticalSize) || camera.verticalSize <= 0.0F)
    {
        return false;
    }

    const float halfHeight = camera.verticalSize * 0.5F;
    const float aspectRatio = static_cast<float>(targetWidth) / static_cast<float>(targetHeight);
    const float halfWidth = halfHeight * aspectRatio;

    if (!std::isfinite(halfHeight) || !std::isfinite(aspectRatio) || !std::isfinite(halfWidth) ||
        halfHeight <= 0.0F || halfWidth <= 0.0F)
    {
        return false;
    }

    const Float2 clipScale{1.0F / halfWidth, 1.0F / halfHeight};
    if (!IsFinite(clipScale))
    {
        return false;
    }

    outView.center = camera.center;
    outView.halfExtents = Float2{halfWidth, halfHeight};
    outView.clipScale = clipScale;
    return true;
}

Float2 WorldToClip(const OrthographicView& view, Float2 worldPosition) noexcept
{
    return Float2{
        (worldPosition.x - view.center.x) * view.clipScale.x,
        (worldPosition.y - view.center.y) * view.clipScale.y,
    };
}

bool IsSpriteVisible(const OrthographicView& view, const SpriteRenderData& sprite) noexcept
{
    const float viewLeft = view.center.x - view.halfExtents.x;
    const float viewRight = view.center.x + view.halfExtents.x;
    const float viewBottom = view.center.y - view.halfExtents.y;
    const float viewTop = view.center.y + view.halfExtents.y;

    const float spriteLeft = sprite.center.x - sprite.halfExtents.x;
    const float spriteRight = sprite.center.x + sprite.halfExtents.x;
    const float spriteBottom = sprite.center.y - sprite.halfExtents.y;
    const float spriteTop = sprite.center.y + sprite.halfExtents.y;

    return spriteRight >= viewLeft && spriteLeft <= viewRight && spriteTop >= viewBottom &&
           spriteBottom <= viewTop;
}

SpriteBatchMeasurement MeasureContiguousTextureBatching(
    const OrthographicView& view,
    const std::span<const SpriteRenderData> sprites) noexcept
{
    SpriteBatchMeasurement measurement{};
    TextureHandle previousVisibleTexture = InvalidTextureHandle;
    bool hasVisibleRun = false;

    for (const SpriteRenderData& sprite : sprites)
    {
        if (!IsSpriteVisible(view, sprite))
        {
            ++measurement.culledSprites;
            continue;
        }

        ++measurement.visibleSprites;

        if (!hasVisibleRun || sprite.texture != previousVisibleTexture)
        {
            ++measurement.contiguousTextureRuns;
            previousVisibleTexture = sprite.texture;
            hasVisibleRun = true;
        }
    }

    return measurement;
}
} // namespace trace2d::render
