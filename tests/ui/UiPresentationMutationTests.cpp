#include <trace2d/ui/UiPresentation2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace trace2d::ui
{
namespace
{
[[nodiscard]] render::TextureHandle PublishMutationWhite(assets::ResourceRegistry& resources)
{
    assets::TextureResource texture{};
    texture.width = 1U;
    texture.height = 1U;
    texture.colorSpace = assets::TextureResourceColorSpace::Linear;
    texture.alphaMode = assets::TextureResourceAlphaMode::Straight;
    texture.cpuRetention = assets::CpuRetentionPolicy::Releasable;
    texture.retentionReason = "U15 bounded mutation test";
    texture.canonicalRgba8 = std::vector<std::uint8_t>{255U, 255U, 255U, 255U};
    const auto published = resources.PublishTexture("generated/ui/u15-mutation-white", std::move(texture));
    EXPECT_TRUE(published.Succeeded());
    return published.handle;
}

[[nodiscard]] render::ResolvedViewport2D ResolveMutationViewport()
{
    render::Viewport2D authored{};
    authored.semanticId = "ui";
    authored.logicalWidth = 320U;
    authored.logicalHeight = 180U;
    authored.scaleMode = render::ViewportScaleMode2D::Fit;
    const render::ViewportResolveResult2D resolved = render::ResolveViewport2D(authored, 640U, 360U);
    EXPECT_TRUE(resolved.Succeeded());
    return resolved.viewport;
}
} // namespace

TEST(UiPresentationMutationTests, ProgressValuePatchesOnlyItsRetainedSegment)
{
    assets::ResourceRegistry resources{"project"};
    const render::TextureHandle whiteHandle = PublishMutationWhite(resources);
    UiSolidTextureBinding2D white{};
    ASSERT_TRUE(ResolveUiSolidTextureBinding2D(resources, whiteHandle, white));

    UiDocument document(320U, 180U);
    document.ReserveElements(2U);
    ASSERT_EQ(
        document.AddElement(UiElement{
            .id = "background",
            .kind = UiElementKind::Panel,
            .bounds = UiRect{0U, 0U, 320U, 180U},
        }),
        UiActionResult::Success);
    ASSERT_EQ(
        document.AddElement(UiElement{
            .id = "health",
            .kind = UiElementKind::Panel,
            .bounds = UiRect{40U, 10U, 20U, 8U},
        }),
        UiActionResult::Success);
    ASSERT_EQ(document.ConfigureProgress("health", 5U, 10U), UiProgressResult::Success);

    UiPresentationCachePrepareResult prepared =
        PrepareUiPresentationCache(UiPresentationCacheConfig{64U, 8U});
    ASSERT_TRUE(prepared.Succeeded());
    const render::ResolvedViewport2D viewport = ResolveMutationViewport();

    const UiPresentationUpdateResult first = prepared.cache->Update(document, resources, viewport, white);
    ASSERT_TRUE(first.Succeeded());
    ASSERT_FALSE(first.reused);
    const UiPresentationFrame2D firstFrame = prepared.cache->Frame();
    ASSERT_EQ(firstFrame.presentations.size(), 7U);
    const auto* const retainedStorage = firstFrame.presentations.data();
    const render::SpritePresentationRenderData untouchedBackground = firstFrame.presentations[0];
    EXPECT_FLOAT_EQ(firstFrame.presentations[2].presentation.quad.topRight.position.x, 50.0F);

    const UiPresentationMetrics beforeMutation = prepared.cache->Metrics();
    EXPECT_EQ(beforeMutation.rebuilds, 1U);
    EXPECT_EQ(beforeMutation.fullRebuilds, 1U);
    EXPECT_EQ(beforeMutation.partialRebuilds, 0U);
    EXPECT_EQ(beforeMutation.elementsRebuilt, 2U);
    EXPECT_EQ(beforeMutation.presentationsRebuilt, 7U);

    ASSERT_EQ(document.SetProgress("health", 7U, 10U), UiProgressResult::Success);
    const UiPresentationUpdateResult changed = prepared.cache->Update(document, resources, viewport, white);
    ASSERT_TRUE(changed.Succeeded());
    EXPECT_FALSE(changed.reused);

    const UiPresentationFrame2D changedFrame = prepared.cache->Frame();
    ASSERT_EQ(changedFrame.presentations.size(), 7U);
    EXPECT_EQ(changedFrame.presentations.data(), retainedStorage);
    EXPECT_EQ(changedFrame.presentations[0].texture, untouchedBackground.texture);
    EXPECT_EQ(
        changedFrame.presentations[0].order.stableOrder,
        untouchedBackground.order.stableOrder);
    EXPECT_EQ(
        changedFrame.presentations[0].presentation.quad.topLeft.position,
        untouchedBackground.presentation.quad.topLeft.position);
    EXPECT_EQ(
        changedFrame.presentations[0].presentation.quad.bottomRight.position,
        untouchedBackground.presentation.quad.bottomRight.position);
    EXPECT_FLOAT_EQ(changedFrame.presentations[2].presentation.quad.topRight.position.x, 54.0F);

    const UiPresentationMetrics afterMutation = prepared.cache->Metrics();
    EXPECT_EQ(afterMutation.rebuilds, 2U);
    EXPECT_EQ(afterMutation.fullRebuilds, 1U);
    EXPECT_EQ(afterMutation.partialRebuilds, 1U);
    EXPECT_EQ(afterMutation.elementsRebuilt, beforeMutation.elementsRebuilt + 1U);
    EXPECT_EQ(afterMutation.presentationsRebuilt, beforeMutation.presentationsRebuilt + 6U);
}

TEST(UiPresentationMutationTests, FailedFullRebuildDoesNotReuseDestroyedPreviousFrame)
{
    assets::ResourceRegistry resources{"project"};
    const render::TextureHandle whiteHandle = PublishMutationWhite(resources);
    UiSolidTextureBinding2D white{};
    ASSERT_TRUE(ResolveUiSolidTextureBinding2D(resources, whiteHandle, white));

    UiDocument document(320U, 180U);
    document.ReserveElements(1U);
    ASSERT_EQ(
        document.AddElement(UiElement{
            .id = "health",
            .kind = UiElementKind::Panel,
            .bounds = UiRect{40U, 10U, 20U, 8U},
        }),
        UiActionResult::Success);
    ASSERT_EQ(document.ConfigureProgress("health", 0U, 10U), UiProgressResult::Success);

    // A zero-width fill is exactly five retained commands: track + four border quads.
    UiPresentationCachePrepareResult prepared =
        PrepareUiPresentationCache(UiPresentationCacheConfig{5U, 1U});
    ASSERT_TRUE(prepared.Succeeded());
    const render::ResolvedViewport2D viewport = ResolveMutationViewport();

    const UiPresentationUpdateResult first = prepared.cache->Update(document, resources, viewport, white);
    ASSERT_TRUE(first.Succeeded());
    ASSERT_FALSE(first.reused);
    ASSERT_EQ(prepared.cache->Frame().presentations.size(), 5U);

    // Enabling the fill changes the retained segment to six commands. The bounded partial path
    // falls back to a full rebuild, which then fails at the configured presentation capacity.
    ASSERT_EQ(document.SetProgress("health", 1U, 10U), UiProgressResult::Success);
    const UiPresentationUpdateResult failed = prepared.cache->Update(document, resources, viewport, white);
    ASSERT_FALSE(failed.Succeeded());
    ASSERT_TRUE(failed.diagnostic.has_value());
    EXPECT_EQ(failed.diagnostic->code, UiPresentationErrorCode::PresentationCapacityExceeded);
    EXPECT_TRUE(prepared.cache->Frame().presentations.empty());

    // Restoring the exact original visual signature must rebuild the destroyed frame. Before the
    // cache-invalidation fix, the stale signature incorrectly returned reused=true with zero draws.
    ASSERT_EQ(document.SetProgress("health", 0U, 10U), UiProgressResult::Success);
    const UiPresentationUpdateResult restored = prepared.cache->Update(document, resources, viewport, white);
    ASSERT_TRUE(restored.Succeeded());
    EXPECT_FALSE(restored.reused);
    EXPECT_EQ(prepared.cache->Frame().presentations.size(), 5U);

    const UiPresentationMetrics metrics = prepared.cache->Metrics();
    EXPECT_EQ(metrics.rebuilds, 2U);
    EXPECT_EQ(metrics.fullRebuilds, 2U);
    EXPECT_EQ(metrics.cacheHits, 0U);
}
} // namespace trace2d::ui
