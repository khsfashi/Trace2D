#include <trace2d/ui/UiPresentation2D.hpp>
#include <trace2d/ui/UiTextLayout.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <vector>

namespace trace2d::ui
{
namespace
{
[[nodiscard]] render::TextureHandle PublishTexture(
    assets::ResourceRegistry& registry,
    const std::string_view reference,
    const std::uint32_t width,
    const std::uint32_t height,
    std::vector<std::uint8_t> rgba,
    const assets::TextureResourceColorSpace colorSpace = assets::TextureResourceColorSpace::Linear)
{
    assets::TextureResource texture{};
    texture.width = width;
    texture.height = height;
    texture.colorSpace = colorSpace;
    texture.alphaMode = assets::TextureResourceAlphaMode::Straight;
    texture.cpuRetention = assets::CpuRetentionPolicy::Releasable;
    texture.retentionReason = "U15 presentation test texture";
    texture.canonicalRgba8 = std::move(rgba);
    const auto published = registry.PublishTexture(reference, std::move(texture));
    EXPECT_TRUE(published.Succeeded());
    return published.handle;
}

[[nodiscard]] render::TextureHandle PublishSolidWhite(assets::ResourceRegistry& registry)
{
    return PublishTexture(registry, "generated/ui/white", 1U, 1U, {255U, 255U, 255U, 255U});
}

[[nodiscard]] render::ResolvedViewport2D ResolveTestViewport(
    const std::uint32_t targetWidth,
    const std::uint32_t targetHeight)
{
    render::Viewport2D viewport{};
    viewport.semanticId = "ui";
    viewport.logicalWidth = 320U;
    viewport.logicalHeight = 180U;
    viewport.scaleMode = render::ViewportScaleMode2D::Fit;
    const render::ViewportResolveResult2D resolved =
        render::ResolveViewport2D(viewport, targetWidth, targetHeight);
    EXPECT_TRUE(resolved.Succeeded());
    return resolved.viewport;
}

[[nodiscard]] std::vector<std::uint8_t> LoadPresentationFont()
{
    std::ifstream input(std::filesystem::path{TRACE2D_UI_TEST_FONT_PATH}, std::ios::binary);
    EXPECT_TRUE(input.is_open());
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

[[nodiscard]] text::GlyphAtlasPrepareResult PreparePresentationAtlas(
    assets::ResourceRegistry& registry)
{
    assets::FontResource font{};
    font.canonicalBytes = LoadPresentationFont();
    const auto published = registry.PublishFont("content/fonts/ui-u15.ttf", std::move(font));
    EXPECT_TRUE(published.Succeeded());
    if (!published.Succeeded())
    {
        return {};
    }
    return text::PrepareGlyphAtlas(
        registry,
        published.handle,
        text::GlyphAtlasConfig{256U, 128U, 20U, 1U, 64U});
}

[[nodiscard]] text::GlyphAtlasTextureBinding2D PublishAndBindAtlas(
    assets::ResourceRegistry& registry,
    const text::GlyphAtlas& atlas)
{
    const text::GlyphAtlasConfig config = atlas.Config();
    std::vector<std::uint8_t> rgba(
        static_cast<std::size_t>(config.width) * static_cast<std::size_t>(config.height) * 4U);
    std::size_t requiredBytes = 0U;
    EXPECT_TRUE(text::WriteGlyphAtlasRgba8(atlas, rgba, requiredBytes).Succeeded());
    EXPECT_EQ(requiredBytes, rgba.size());
    const render::TextureHandle texture = PublishTexture(
        registry,
        "generated/ui/u15-atlas",
        config.width,
        config.height,
        std::move(rgba));
    text::GlyphAtlasTextureBinding2D binding{};
    EXPECT_TRUE(text::ResolveGlyphAtlasTextureBinding2D(atlas, registry, texture, binding).Succeeded());
    return binding;
}
} // namespace

TEST(UiPresentation2DTests, RetainedCommandsReuseUnchangedFrameAndClipImageGeometryAndUv)
{
    assets::ResourceRegistry registry("project");
    const render::TextureHandle solidHandle = PublishSolidWhite(registry);
    UiSolidTextureBinding2D solid{};
    ASSERT_TRUE(ResolveUiSolidTextureBinding2D(registry, solidHandle, solid));

    const render::TextureHandle imageHandle = PublishTexture(
        registry,
        "content/ui/image",
        2U,
        2U,
        {
            255U, 0U, 0U, 255U, 0U, 255U, 0U, 255U,
            0U, 0U, 255U, 255U, 255U, 255U, 255U, 255U,
        });

    UiDocument document(320U, 180U);
    document.ReserveElements(3U);
    ASSERT_EQ(
        document.AddElement(UiElement{
            .id = "clipped-panel",
            .kind = UiElementKind::Panel,
            .bounds = UiRect{10U, 10U, 30U, 20U},
            .clipActive = true,
            .clipBounds = UiRect{15U, 10U, 10U, 20U},
        }),
        UiActionResult::Success);
    ASSERT_EQ(
        document.AddElement(UiElement{
            .id = "hp",
            .kind = UiElementKind::Panel,
            .bounds = UiRect{40U, 10U, 20U, 8U},
        }),
        UiActionResult::Success);
    ASSERT_EQ(document.ConfigureProgress("hp", 5U, 10U), UiProgressResult::Success);
    ASSERT_EQ(
        document.AddElement(UiElement{
            .id = "portrait",
            .kind = UiElementKind::Panel,
            .bounds = UiRect{70U, 10U, 20U, 10U},
            .clipActive = true,
            .clipBounds = UiRect{75U, 10U, 10U, 10U},
        }),
        UiActionResult::Success);
    ASSERT_EQ(document.ConfigureImage("portrait", imageHandle, registry), UiImageResult::Success);

    UiPresentationCachePrepareResult prepared =
        PrepareUiPresentationCache(UiPresentationCacheConfig{64U});
    ASSERT_TRUE(prepared.Succeeded());
    const render::ResolvedViewport2D viewport = ResolveTestViewport(640U, 360U);

    const UiPresentationUpdateResult first =
        prepared.cache->Update(document, registry, viewport, solid);
    ASSERT_TRUE(first.Succeeded());
    EXPECT_FALSE(first.reused);
    UiPresentationFrame2D frame = prepared.cache->Frame();
    ASSERT_EQ(frame.presentations.size(), 8U);
    EXPECT_EQ(frame.camera.rasterViewport.targetWidth, 640U);
    EXPECT_EQ(frame.camera.rasterViewport.targetHeight, 360U);

    render::OrthographicView gpuView{};
    EXPECT_TRUE(render::TryBuildOrthographicView(frame.camera, 640U, 360U, gpuView));

    // First panel is clipped from [10,40) to [15,25) without a renderer-specific mask/scissor.
    const render::SpriteDrawQuad& panelQuad = frame.presentations[0].presentation.quad;
    EXPECT_FLOAT_EQ(panelQuad.topLeft.position.x, 15.0F);
    EXPECT_FLOAT_EQ(panelQuad.topRight.position.x, 25.0F);

    // The last command is the Image. Cropping half of its 20px width also crops canonical UVs.
    const render::SpriteDrawQuad& imageQuad = frame.presentations.back().presentation.quad;
    EXPECT_FLOAT_EQ(imageQuad.topLeft.position.x, 75.0F);
    EXPECT_FLOAT_EQ(imageQuad.topRight.position.x, 85.0F);
    EXPECT_FLOAT_EQ(imageQuad.topLeft.uv.x, 0.25F);
    EXPECT_FLOAT_EQ(imageQuad.topRight.uv.x, 0.75F);
    EXPECT_FLOAT_EQ(imageQuad.topLeft.position.y, 170.0F);
    EXPECT_FLOAT_EQ(imageQuad.bottomLeft.position.y, 160.0F);
    EXPECT_EQ(frame.presentations.back().texture, imageHandle);

    const auto* const retainedPointer = frame.presentations.data();
    const UiPresentationMetrics afterFirst = prepared.cache->Metrics();
    EXPECT_EQ(afterFirst.rebuilds, 1U);
    EXPECT_EQ(afterFirst.cacheHits, 0U);

    const UiPresentationUpdateResult second =
        prepared.cache->Update(document, registry, viewport, solid);
    ASSERT_TRUE(second.Succeeded());
    EXPECT_TRUE(second.reused);
    frame = prepared.cache->Frame();
    EXPECT_EQ(frame.presentations.data(), retainedPointer);
    EXPECT_EQ(prepared.cache->Metrics().rebuilds, 1U);
    EXPECT_EQ(prepared.cache->Metrics().cacheHits, 1U);

    ASSERT_EQ(document.SetProgress("hp", 7U, 10U), UiProgressResult::Success);
    const UiPresentationUpdateResult changed =
        prepared.cache->Update(document, registry, viewport, solid);
    ASSERT_TRUE(changed.Succeeded());
    EXPECT_FALSE(changed.reused);
    EXPECT_EQ(prepared.cache->Metrics().rebuilds, 2U);

    const render::ResolvedViewport2D largerViewport = ResolveTestViewport(960U, 540U);
    const UiPresentationUpdateResult resized =
        prepared.cache->Update(document, registry, largerViewport, solid);
    ASSERT_TRUE(resized.Succeeded());
    EXPECT_FALSE(resized.reused);
    frame = prepared.cache->Frame();
    EXPECT_EQ(frame.camera.rasterViewport.targetWidth, 960U);
    EXPECT_EQ(frame.camera.rasterViewport.targetHeight, 540U);
    EXPECT_TRUE(render::TryBuildOrthographicView(frame.camera, 960U, 540U, gpuView));
    EXPECT_EQ(prepared.cache->Metrics().rebuilds, 3U);
}

TEST(UiPresentation2DTests, ProductionTextBridgeUsesExistingGlyphAtlasBindingAndReusesItUnchanged)
{
    assets::ResourceRegistry registry("project");
    const render::TextureHandle solidHandle = PublishSolidWhite(registry);
    UiSolidTextureBinding2D solid{};
    ASSERT_TRUE(ResolveUiSolidTextureBinding2D(registry, solidHandle, solid));

    text::GlyphAtlasPrepareResult atlas = PreparePresentationAtlas(registry);
    ASSERT_TRUE(atlas.Succeeded());

    UiDocument document(320U, 180U);
    ASSERT_EQ(
        document.AddElement(UiElement{
            .id = "label",
            .kind = UiElementKind::Label,
            .bounds = UiRect{10U, 50U, 40U, 24U},
            .clipActive = true,
            .clipBounds = UiRect{15U, 50U, 35U, 24U},
            .text = "A",
        }),
        UiActionResult::Success);

    UiTextLayoutCachePrepareResult textCache = PrepareUiTextLayoutCache(
        UiTextLayoutCacheConfig{
            .text = text::TextLayoutCacheConfig{
                .layout = text::TextLayoutRunConfig{8U, 2U},
                .maxFallbackFonts = 1U,
            },
            .maxComposedUtf8Bytes = 32U,
        });
    ASSERT_TRUE(textCache.Succeeded());
    const text::TextFontAtlasRef atlasRef{atlas.atlas.get()};
    const UiTextLayoutUpdateResult layoutUpdate = textCache.cache->Update(
        document,
        "label",
        std::span<const text::TextFontAtlasRef>(&atlasRef, 1U));
    ASSERT_TRUE(layoutUpdate.Succeeded());
    ASSERT_NE(textCache.cache->Layout(), nullptr);

    const text::GlyphAtlasTextureBinding2D binding = PublishAndBindAtlas(registry, *atlas.atlas);
    const UiTextPresentationInput2D textInput{
        .elementIndex = 0U,
        .layout = textCache.cache->Layout(),
        .fallbackAtlases = std::span<const text::TextFontAtlasRef>(&atlasRef, 1U),
        .bindings = std::span<const text::GlyphAtlasTextureBinding2D>(&binding, 1U),
        .presentationRevision = 1U,
    };

    UiPresentationCachePrepareResult prepared =
        PrepareUiPresentationCache(UiPresentationCacheConfig{32U});
    ASSERT_TRUE(prepared.Succeeded());
    const render::ResolvedViewport2D viewport = ResolveTestViewport(640U, 360U);
    const UiPresentationUpdateResult first = prepared.cache->Update(
        document,
        registry,
        viewport,
        solid,
        std::span<const UiTextPresentationInput2D>(&textInput, 1U));
    ASSERT_TRUE(first.Succeeded());
    EXPECT_FALSE(first.reused);

    const UiPresentationFrame2D frame = prepared.cache->Frame();
    ASSERT_EQ(frame.presentations.size(), 1U);
    EXPECT_EQ(frame.presentations.front().texture, binding.texture);
    EXPECT_GE(frame.presentations.front().presentation.quad.topLeft.position.x, 15.0F);
    EXPECT_EQ(prepared.cache->Metrics().textPresentationsBuilt, 1U);

    const std::uint64_t atlasRevisionBefore = text::GlyphAtlasPixelRevision(*atlas.atlas);
    const UiPresentationUpdateResult second = prepared.cache->Update(
        document,
        registry,
        viewport,
        solid,
        std::span<const UiTextPresentationInput2D>(&textInput, 1U));
    ASSERT_TRUE(second.Succeeded());
    EXPECT_TRUE(second.reused);
    EXPECT_EQ(text::GlyphAtlasPixelRevision(*atlas.atlas), atlasRevisionBefore);
    EXPECT_EQ(prepared.cache->Metrics().rebuilds, 1U);
    EXPECT_EQ(prepared.cache->Metrics().cacheHits, 1U);
}
} // namespace trace2d::ui
