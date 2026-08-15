#include <trace2d/text/TextPresentation2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <vector>

namespace trace2d::text
{
namespace
{
[[nodiscard]] std::vector<std::uint8_t> LoadPresentationTestFont(const char* path)
{
    std::ifstream input(std::filesystem::path{path}, std::ios::binary);
    EXPECT_TRUE(input.is_open());
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

[[nodiscard]] GlyphAtlasPrepareResult PreparePresentationAtlas(
    assets::ResourceRegistry& registry,
    const std::string_view reference,
    const char* path)
{
    assets::FontResource font{};
    font.canonicalBytes = LoadPresentationTestFont(path);
    const auto published = registry.PublishFont(reference, std::move(font));
    EXPECT_TRUE(published.Succeeded());
    if (!published.Succeeded())
    {
        return {};
    }
    return PrepareGlyphAtlas(
        registry,
        published.handle,
        GlyphAtlasConfig{256U, 128U, 20U, 1U, 64U});
}

[[nodiscard]] render::TextureHandle PublishPresentationAtlasTexture(
    assets::ResourceRegistry& registry,
    const std::string_view reference,
    const GlyphAtlas& atlas,
    const assets::TextureResourceColorSpace colorSpace = assets::TextureResourceColorSpace::Linear)
{
    const GlyphAtlasConfig config = atlas.Config();
    assets::TextureResource texture{};
    texture.width = config.width;
    texture.height = config.height;
    texture.colorSpace = colorSpace;
    texture.alphaMode = assets::TextureResourceAlphaMode::Straight;
    texture.cpuRetention = assets::CpuRetentionPolicy::Releasable;
    texture.retentionReason = "generated glyph atlas can be rebuilt from retained FontResource";
    texture.canonicalRgba8.resize(
        static_cast<std::size_t>(config.width) * static_cast<std::size_t>(config.height) * 4U);

    std::size_t requiredBytes = 0U;
    EXPECT_TRUE(WriteGlyphAtlasRgba8(atlas, texture.canonicalRgba8, requiredBytes).Succeeded());
    EXPECT_EQ(requiredBytes, texture.canonicalRgba8.size());

    const auto published = registry.PublishTexture(reference, std::move(texture));
    EXPECT_TRUE(published.Succeeded());
    return published.handle;
}

[[nodiscard]] GlyphAtlasTextureBinding2D BindPresentationAtlas(
    assets::ResourceRegistry& registry,
    const GlyphAtlas& atlas,
    const render::TextureHandle texture)
{
    GlyphAtlasTextureBinding2D binding{};
    EXPECT_TRUE(ResolveGlyphAtlasTextureBinding2D(atlas, registry, texture, binding).Succeeded());
    return binding;
}
} // namespace

TEST(TextPresentation2DTests, AtlasExpansionIsDeterministicAndBindingBecomesStaleOnlyWhenPixelsGrow)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult prepared = PreparePresentationAtlas(
        registry,
        "content/fonts/presentation.ttf",
        TRACE2D_TEXT_TEST_FONT_PATH);
    ASSERT_TRUE(prepared.Succeeded());
    ASSERT_TRUE(prepared.atlas->WarmUtf8("A").Succeeded());

    const std::uint64_t firstRevision = GlyphAtlasPixelRevision(*prepared.atlas);
    ASSERT_GT(firstRevision, 0U);
    const GlyphAtlasConfig config = prepared.atlas->Config();
    const std::size_t expectedBytes =
        static_cast<std::size_t>(config.width) * static_cast<std::size_t>(config.height) * 4U;

    std::vector<std::uint8_t> tooSmall(expectedBytes - 1U, 17U);
    const std::vector<std::uint8_t> unchanged = tooSmall;
    std::size_t requiredBytes = 0U;
    const TextPresentationStatus capacity =
        WriteGlyphAtlasRgba8(*prepared.atlas, tooSmall, requiredBytes);
    EXPECT_EQ(capacity.error, TextPresentationError::InsufficientRgbaCapacity);
    EXPECT_EQ(requiredBytes, expectedBytes);
    EXPECT_EQ(tooSmall, unchanged);

    std::vector<std::uint8_t> rgba(expectedBytes, 0U);
    ASSERT_TRUE(WriteGlyphAtlasRgba8(*prepared.atlas, rgba, requiredBytes).Succeeded());
    const std::span<const std::uint8_t> alpha = prepared.atlas->Alpha8();
    ASSERT_EQ(alpha.size() * 4U, rgba.size());
    for (std::size_t pixel = 0U; pixel < alpha.size(); ++pixel)
    {
        const std::size_t offset = pixel * 4U;
        EXPECT_EQ(rgba[offset], 255U);
        EXPECT_EQ(rgba[offset + 1U], 255U);
        EXPECT_EQ(rgba[offset + 2U], 255U);
        EXPECT_EQ(rgba[offset + 3U], alpha[pixel]);
    }

    const render::TextureHandle texture = PublishPresentationAtlasTexture(
        registry,
        "generated/text/presentation-atlas.r1",
        *prepared.atlas);
    GlyphAtlasTextureBinding2D binding = BindPresentationAtlas(registry, *prepared.atlas, texture);
    EXPECT_EQ(binding.pixelRevision, firstRevision);

    ASSERT_TRUE(prepared.atlas->WarmUtf8("A").Succeeded());
    EXPECT_EQ(GlyphAtlasPixelRevision(*prepared.atlas), firstRevision);

    ASSERT_TRUE(prepared.atlas->WarmUtf8("한").Succeeded());
    EXPECT_GT(GlyphAtlasPixelRevision(*prepared.atlas), firstRevision);

    TextLayoutRunPrepareResult layout = PrepareTextLayoutRun({4U, 2U});
    ASSERT_TRUE(layout.Succeeded());
    ASSERT_TRUE(layout.run->LayoutUtf8(*prepared.atlas, "A").Succeeded());
    const TextFontAtlasRef atlasRef{prepared.atlas.get()};
    std::size_t requiredCount = 0U;
    TextPresentationMeasurement2D measurement{};
    const TextPresentationStatus stale = BuildTextPresentation2D(
        *layout.run,
        std::span<const TextFontAtlasRef>(&atlasRef, 1U),
        std::span<const GlyphAtlasTextureBinding2D>(&binding, 1U),
        {},
        {},
        requiredCount,
        measurement);
    EXPECT_EQ(stale.error, TextPresentationError::StaleAtlasBinding);
}

TEST(TextPresentation2DTests, BindingRejectsTextureMetadataThatWouldChangeGlyphSamplingSemantics)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult prepared = PreparePresentationAtlas(
        registry,
        "content/fonts/metadata.ttf",
        TRACE2D_TEXT_TEST_FONT_PATH);
    ASSERT_TRUE(prepared.Succeeded());
    ASSERT_TRUE(prepared.atlas->WarmUtf8("한").Succeeded());

    const render::TextureHandle srgbTexture = PublishPresentationAtlasTexture(
        registry,
        "generated/text/metadata-srgb.r1",
        *prepared.atlas,
        assets::TextureResourceColorSpace::Srgb);
    GlyphAtlasTextureBinding2D binding{};
    const TextPresentationStatus status =
        ResolveGlyphAtlasTextureBinding2D(*prepared.atlas, registry, srgbTexture, binding);
    EXPECT_EQ(status.error, TextPresentationError::TextureColorSpaceMismatch);
    EXPECT_EQ(binding, GlyphAtlasTextureBinding2D{});
}

TEST(TextPresentation2DTests, MixedFallbackLayoutEmitsExistingSpriteQuadsInExactGlyphOrder)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult primary = PreparePresentationAtlas(
        registry,
        "content/fonts/presentation-primary.ttf",
        TRACE2D_TEXT_PRIMARY_TEST_FONT_PATH);
    GlyphAtlasPrepareResult fallback = PreparePresentationAtlas(
        registry,
        "content/fonts/presentation-fallback.ttf",
        TRACE2D_TEXT_TEST_FONT_PATH);
    ASSERT_TRUE(primary.Succeeded());
    ASSERT_TRUE(fallback.Succeeded());

    const std::array<TextFontAtlasRef, 2U> chain{
        TextFontAtlasRef{primary.atlas.get()},
        TextFontAtlasRef{fallback.atlas.get()},
    };
    TextLayoutRunPrepareResult layout = PrepareTextLayoutRun({8U, 4U});
    ASSERT_TRUE(layout.Succeeded());
    ASSERT_TRUE(layout.run->LayoutUtf8(chain, "A한中").Succeeded());
    ASSERT_EQ(layout.run->Glyphs().size(), 3U);
    EXPECT_EQ(layout.run->Glyphs()[0].fontSlot, 0U);
    EXPECT_EQ(layout.run->Glyphs()[1].fontSlot, 1U);
    EXPECT_EQ(layout.run->Glyphs()[2].fontSlot, 0U);

    const render::TextureHandle primaryTexture = PublishPresentationAtlasTexture(
        registry,
        "generated/text/primary.r1",
        *primary.atlas);
    const render::TextureHandle fallbackTexture = PublishPresentationAtlasTexture(
        registry,
        "generated/text/fallback.r1",
        *fallback.atlas);
    const std::array<GlyphAtlasTextureBinding2D, 2U> bindings{
        BindPresentationAtlas(registry, *primary.atlas, primaryTexture),
        BindPresentationAtlas(registry, *fallback.atlas, fallbackTexture),
    };

    TextPresentationConfig2D config{};
    config.origin = render::Float2{10.0F, 20.0F};
    config.pixelsPerUnit = 20.0F;
    config.painterLayer = 4;
    config.painterOrder = 7;
    config.stableOrderBase = 500U;
    config.tint = render::SpriteLinearRgba{0.25F, 0.5F, 0.75F, 0.8F};
    config.opacity = 0.5F;
    config.sampler = render::SpriteSamplerCompatibility::Linear;

    std::array<render::SpritePresentationRenderData, 2U> tooSmall{};
    tooSmall[0].order.stableOrder = 777U;
    tooSmall[1].order.stableOrder = 888U;
    std::size_t requiredCount = 0U;
    TextPresentationMeasurement2D measurement{};
    const TextPresentationStatus insufficient = BuildTextPresentation2D(
        *layout.run,
        chain,
        bindings,
        config,
        tooSmall,
        requiredCount,
        measurement);
    EXPECT_EQ(insufficient.error, TextPresentationError::InsufficientOutputCapacity);
    EXPECT_EQ(requiredCount, 3U);
    EXPECT_EQ(tooSmall[0].order.stableOrder, 777U);
    EXPECT_EQ(tooSmall[1].order.stableOrder, 888U);
    EXPECT_EQ(measurement.layoutGlyphs, 3U);
    EXPECT_EQ(measurement.zeroAreaGlyphs, 0U);
    EXPECT_EQ(measurement.emittedQuads, 3U);
    EXPECT_EQ(measurement.contiguousTextureRuns, 3U);

    std::vector<render::SpritePresentationRenderData> output(requiredCount);
    ASSERT_TRUE(BuildTextPresentation2D(
        *layout.run,
        chain,
        bindings,
        config,
        output,
        requiredCount,
        measurement).Succeeded());
    ASSERT_EQ(output.size(), 3U);
    EXPECT_EQ(output[0].texture, primaryTexture);
    EXPECT_EQ(output[1].texture, fallbackTexture);
    EXPECT_EQ(output[2].texture, primaryTexture);

    for (std::size_t index = 0U; index < output.size(); ++index)
    {
        EXPECT_EQ(output[index].order.layer, 4);
        EXPECT_EQ(output[index].order.order, 7);
        EXPECT_EQ(output[index].order.stableOrder, 500U + index);
        EXPECT_EQ(output[index].presentation.appearance.tint, config.tint);
        EXPECT_EQ(output[index].presentation.appearance.opacity, config.opacity);
        EXPECT_EQ(
            output[index].presentation.appearance.textureEncoding,
            render::SpriteTextureEncoding::Linear);
        EXPECT_EQ(
            output[index].presentation.appearance.sourceAlphaMode,
            assets::SpriteAlphaMode::Straight);
        EXPECT_EQ(
            output[index].presentation.appearance.blend,
            render::SpriteBlendCompatibility::Normal);
    }

    const PositionedGlyph& firstGlyph = layout.run->Glyphs()[0];
    const double expectedLeft =
        static_cast<double>(config.origin.x) +
        (static_cast<double>(firstGlyph.penX26_6) / 64.0 +
         static_cast<double>(firstGlyph.atlasEntry.bearingX)) /
            static_cast<double>(config.pixelsPerUnit);
    const double expectedTop =
        static_cast<double>(config.origin.y) +
        (static_cast<double>(firstGlyph.baselineY26_6) / 64.0 -
         static_cast<double>(firstGlyph.atlasEntry.bearingY)) /
            static_cast<double>(config.pixelsPerUnit);
    EXPECT_FLOAT_EQ(output[0].presentation.quad.topLeft.position.x, static_cast<float>(expectedLeft));
    EXPECT_FLOAT_EQ(output[0].presentation.quad.topLeft.position.y, static_cast<float>(expectedTop));
}

TEST(TextPresentation2DTests, ZeroAreaGlyphsKeepLayoutAdvanceButDoNotSubmitSpriteQuads)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult prepared = PreparePresentationAtlas(
        registry,
        "content/fonts/spaces.ttf",
        TRACE2D_TEXT_TEST_FONT_PATH);
    ASSERT_TRUE(prepared.Succeeded());

    TextLayoutRunPrepareResult layout = PrepareTextLayoutRun({8U, 2U});
    ASSERT_TRUE(layout.Succeeded());
    ASSERT_TRUE(layout.run->LayoutUtf8(*prepared.atlas, "A A").Succeeded());
    ASSERT_EQ(layout.run->Glyphs().size(), 3U);
    ASSERT_EQ(layout.run->Glyphs()[1].atlasEntry.codepoint, U' ');
    ASSERT_EQ(layout.run->Glyphs()[1].atlasEntry.width, 0U);
    ASSERT_EQ(layout.run->Glyphs()[1].atlasEntry.height, 0U);

    const render::TextureHandle texture = PublishPresentationAtlasTexture(
        registry,
        "generated/text/spaces.r1",
        *prepared.atlas);
    const GlyphAtlasTextureBinding2D binding = BindPresentationAtlas(registry, *prepared.atlas, texture);
    const TextFontAtlasRef atlasRef{prepared.atlas.get()};

    std::array<render::SpritePresentationRenderData, 2U> output{};
    std::size_t requiredCount = 0U;
    TextPresentationMeasurement2D measurement{};
    ASSERT_TRUE(BuildTextPresentation2D(
        *layout.run,
        std::span<const TextFontAtlasRef>(&atlasRef, 1U),
        std::span<const GlyphAtlasTextureBinding2D>(&binding, 1U),
        {},
        output,
        requiredCount,
        measurement).Succeeded());
    EXPECT_EQ(requiredCount, 2U);
    EXPECT_EQ(measurement.layoutGlyphs, 3U);
    EXPECT_EQ(measurement.zeroAreaGlyphs, 1U);
    EXPECT_EQ(measurement.emittedQuads, 2U);
    EXPECT_EQ(output[0].order.stableOrder, 0U);
    EXPECT_EQ(output[1].order.stableOrder, 2U);
}
} // namespace trace2d::text
