#include <trace2d/render/SpriteBatch2D.hpp>

#include <algorithm>
#include <array>

namespace trace2d::render
{
namespace
{
[[nodiscard]] bool IntersectsClipViewport(
    const OrthographicView& view,
    const SpriteDrawQuad& quad) noexcept
{
    const std::array<Float2, 4U> clip{
        WorldToClip(view, quad.topLeft.position),
        WorldToClip(view, quad.topRight.position),
        WorldToClip(view, quad.bottomRight.position),
        WorldToClip(view, quad.bottomLeft.position),
    };

    float minimumX = clip[0].x;
    float maximumX = clip[0].x;
    float minimumY = clip[0].y;
    float maximumY = clip[0].y;
    for (std::size_t index = 1U; index < clip.size(); ++index)
    {
        minimumX = std::min(minimumX, clip[index].x);
        maximumX = std::max(maximumX, clip[index].x);
        minimumY = std::min(minimumY, clip[index].y);
        maximumY = std::max(maximumY, clip[index].y);
    }

    return maximumX >= -1.0F && minimumX <= 1.0F &&
        maximumY >= -1.0F && minimumY <= 1.0F;
}
} // namespace

SpritePresentationBatchMeasurement2D MeasureContiguousSpritePresentationBatches(
    const std::span<const SpriteBatchItem2D> items) noexcept
{
    SpritePresentationBatchMeasurement2D measurement{};
    measurement.submittedSprites = static_cast<std::uint64_t>(items.size());

    bool hasRun = false;
    SpriteBatchCompatibility2D runCompatibility{};
    for (const SpriteBatchItem2D& item : items)
    {
        if (!item.visible || item.quadCount == 0U)
        {
            ++measurement.culledSprites;
            continue;
        }

        ++measurement.visibleSprites;
        measurement.visibleQuads += item.quadCount;
        if (!hasRun || !(item.compatibility == runCompatibility))
        {
            ++measurement.contiguousRuns;
            runCompatibility = item.compatibility;
            hasRun = true;
        }
    }
    return measurement;
}

bool IsSpritePresentationQuadVisible(
    const OrthographicView& view,
    const SpriteDrawQuad& quad) noexcept
{
    return IntersectsClipViewport(view, quad);
}

bool IsSpritePresentationPrimitiveVisible(
    const OrthographicView& view,
    const std::span<const SpritePrimitivePatch2D> patches) noexcept
{
    for (const SpritePrimitivePatch2D& patch : patches)
    {
        if (IntersectsClipViewport(view, patch.quad))
        {
            return true;
        }
    }
    return false;
}
} // namespace trace2d::render
