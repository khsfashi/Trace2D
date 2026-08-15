#include <trace2d/text/TextLayout.hpp>

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
[[nodiscard]] std::vector<std::uint8_t> LoadFallbackTestFont(const char* path)
{
    std::ifstream input(std::filesystem::path{path}, std::ios::binary);
    EXPECT_TRUE(input.is_open());
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

[[nodiscard]] assets::FontResource MakeFallbackTestFont(const char* path)
{
    assets::FontResource resource{};
    resource.canonicalBytes = LoadFallbackTestFont(path);
    return resource;
}

[[nodiscard]] GlyphAtlasPrepareResult PrepareFallbackAtlas(
    assets::ResourceRegistry& registry,
    const std::string_view reference,
    const char* path,
    const GlyphAtlasConfig config = GlyphAtlasConfig{256U, 128U, 20U, 1U, 64U})
{
    const auto font = registry.PublishFont(reference, MakeFallbackTestFont(path));
    EXPECT_TRUE(font.Succeeded());
    if (!font.Succeeded())
    {
        return {};
    }
    return PrepareGlyphAtlas(registry, font.handle, config);
}

[[nodiscard]] std::array<TextFontAtlasRef, 2U> MakeChain(
    GlyphAtlas& first,
    GlyphAtlas& second) noexcept
{
    return {TextFontAtlasRef{&first}, TextFontAtlasRef{&second}};
}

void ExpectEquivalentFallbackGlyph(const PositionedGlyph& left, const PositionedGlyph& right)
{
    EXPECT_EQ(left.atlasEntry.codepoint, right.atlasEntry.codepoint);
    EXPECT_EQ(left.atlasEntry.glyphIndex, right.atlasEntry.glyphIndex);
    EXPECT_EQ(left.atlasEntry.x, right.atlasEntry.x);
    EXPECT_EQ(left.atlasEntry.y, right.atlasEntry.y);
    EXPECT_EQ(left.atlasEntry.width, right.atlasEntry.width);
    EXPECT_EQ(left.atlasEntry.height, right.atlasEntry.height);
    EXPECT_EQ(left.atlasEntry.advanceX26_6, right.atlasEntry.advanceX26_6);
    EXPECT_EQ(left.fontSlot, right.fontSlot);
    EXPECT_EQ(left.byteOffset, right.byteOffset);
    EXPECT_EQ(left.lineIndex, right.lineIndex);
    EXPECT_EQ(left.penX26_6, right.penX26_6);
    EXPECT_EQ(left.baselineY26_6, right.baselineY26_6);
}
} // namespace

TEST(TextFallbackTests, OrderedFallbackResolvesMixedCoverageWithoutMissingGlyphCacheMutation)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult primary = PrepareFallbackAtlas(
        registry,
        "content/fonts/fallback-primary.ttf",
        TRACE2D_TEXT_PRIMARY_TEST_FONT_PATH);
    GlyphAtlasPrepareResult fallback = PrepareFallbackAtlas(
        registry,
        "content/fonts/fallback-secondary.ttf",
        TRACE2D_TEXT_TEST_FONT_PATH);
    ASSERT_TRUE(primary.Succeeded());
    ASSERT_TRUE(fallback.Succeeded());

    EXPECT_TRUE(primary.atlas->SupportsCodepoint(U'A'));
    EXPECT_FALSE(primary.atlas->SupportsCodepoint(U'한'));
    EXPECT_TRUE(primary.atlas->SupportsCodepoint(U'中'));
    EXPECT_TRUE(fallback.atlas->SupportsCodepoint(U'한'));

    TextLayoutRunPrepareResult prepared = PrepareTextLayoutRun({16U, 8U});
    ASSERT_TRUE(prepared.Succeeded());
    const auto chain = MakeChain(*primary.atlas, *fallback.atlas);

    const TextLayoutResult first = prepared.run->LayoutUtf8(chain, "A한中");
    ASSERT_TRUE(first.Succeeded());
    ASSERT_EQ(prepared.run->Glyphs().size(), 3U);
    EXPECT_EQ(prepared.run->Glyphs()[0].fontSlot, 0U);
    EXPECT_EQ(prepared.run->Glyphs()[1].fontSlot, 1U);
    EXPECT_EQ(prepared.run->Glyphs()[2].fontSlot, 0U);

    const GlyphAtlasMetrics primaryAfterFirst = primary.atlas->Metrics();
    const GlyphAtlasMetrics fallbackAfterFirst = fallback.atlas->Metrics();
    EXPECT_EQ(primaryAfterFirst.cacheMisses, 2U);
    EXPECT_EQ(primaryAfterFirst.rasterizations, 2U);
    EXPECT_EQ(fallbackAfterFirst.cacheMisses, 1U);
    EXPECT_EQ(fallbackAfterFirst.rasterizations, 1U);

    const auto firstSpan = prepared.run->Glyphs();
    const std::vector<PositionedGlyph> firstGlyphs(firstSpan.begin(), firstSpan.end());
    const TextLayoutMetrics firstMetrics = prepared.run->Metrics();

    const TextLayoutResult second = prepared.run->LayoutUtf8(chain, "A한中");
    ASSERT_TRUE(second.Succeeded());
    EXPECT_EQ(primary.atlas->Metrics().rasterizations, primaryAfterFirst.rasterizations);
    EXPECT_EQ(fallback.atlas->Metrics().rasterizations, fallbackAfterFirst.rasterizations);
    EXPECT_EQ(prepared.run->Metrics().glyphCount, firstMetrics.glyphCount);
    EXPECT_EQ(prepared.run->Metrics().lineCount, firstMetrics.lineCount);
    EXPECT_EQ(prepared.run->Metrics().contentWidth26_6, firstMetrics.contentWidth26_6);
    EXPECT_EQ(prepared.run->Metrics().contentHeight26_6, firstMetrics.contentHeight26_6);

    const auto secondGlyphs = prepared.run->Glyphs();
    ASSERT_EQ(secondGlyphs.size(), firstGlyphs.size());
    for (std::size_t index = 0U; index < firstGlyphs.size(); ++index)
    {
        ExpectEquivalentFallbackGlyph(firstGlyphs[index], secondGlyphs[index]);
    }
}

TEST(TextFallbackTests, CallerOrderDeterministicallySelectsFirstSupportingFont)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult primary = PrepareFallbackAtlas(
        registry,
        "content/fonts/order-primary.ttf",
        TRACE2D_TEXT_PRIMARY_TEST_FONT_PATH);
    GlyphAtlasPrepareResult fallback = PrepareFallbackAtlas(
        registry,
        "content/fonts/order-secondary.ttf",
        TRACE2D_TEXT_TEST_FONT_PATH);
    ASSERT_TRUE(primary.Succeeded());
    ASSERT_TRUE(fallback.Succeeded());
    ASSERT_TRUE(primary.atlas->SupportsCodepoint(U'A'));
    ASSERT_TRUE(fallback.atlas->SupportsCodepoint(U'A'));

    TextLayoutRunPrepareResult prepared = PrepareTextLayoutRun({4U, 2U});
    ASSERT_TRUE(prepared.Succeeded());

    const auto primaryFirst = MakeChain(*primary.atlas, *fallback.atlas);
    ASSERT_TRUE(prepared.run->LayoutUtf8(primaryFirst, "A").Succeeded());
    ASSERT_EQ(prepared.run->Glyphs().size(), 1U);
    EXPECT_EQ(prepared.run->Glyphs()[0].fontSlot, 0U);
    const std::int64_t primaryAdvance = prepared.run->Glyphs()[0].atlasEntry.advanceX26_6;

    const auto fallbackFirst = MakeChain(*fallback.atlas, *primary.atlas);
    ASSERT_TRUE(prepared.run->LayoutUtf8(fallbackFirst, "A").Succeeded());
    ASSERT_EQ(prepared.run->Glyphs().size(), 1U);
    EXPECT_EQ(prepared.run->Glyphs()[0].fontSlot, 0U);
    const std::int64_t fallbackAdvance = prepared.run->Glyphs()[0].atlasEntry.advanceX26_6;

    EXPECT_NE(primaryAdvance, fallbackAdvance);
}

TEST(TextFallbackTests, AllMissingGlyphsAreTypedAndLeavePublishedRunUntouched)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult primary = PrepareFallbackAtlas(
        registry,
        "content/fonts/missing-primary.ttf",
        TRACE2D_TEXT_PRIMARY_TEST_FONT_PATH);
    GlyphAtlasPrepareResult fallback = PrepareFallbackAtlas(
        registry,
        "content/fonts/missing-secondary.ttf",
        TRACE2D_TEXT_TEST_FONT_PATH);
    ASSERT_TRUE(primary.Succeeded());
    ASSERT_TRUE(fallback.Succeeded());

    TextLayoutRunPrepareResult prepared = PrepareTextLayoutRun({4U, 2U});
    ASSERT_TRUE(prepared.Succeeded());
    ASSERT_TRUE(prepared.run->LayoutUtf8(*primary.atlas, "A").Succeeded());
    ASSERT_EQ(prepared.run->Glyphs().size(), 1U);
    const PositionedGlyph published = prepared.run->Glyphs()[0];
    const auto chain = MakeChain(*primary.atlas, *fallback.atlas);
    const GlyphAtlasMetrics primaryBefore = primary.atlas->Metrics();
    const GlyphAtlasMetrics fallbackBefore = fallback.atlas->Metrics();

    const TextLayoutResult missing = prepared.run->LayoutUtf8(chain, "Ω");
    ASSERT_FALSE(missing.Succeeded());
    EXPECT_EQ(missing.diagnostic->code, TextLayoutErrorCode::GlyphResolveFailed);
    ASSERT_TRUE(missing.diagnostic->textDiagnostic.has_value());
    EXPECT_EQ(missing.diagnostic->textDiagnostic->code, TextErrorCode::MissingGlyph);
    EXPECT_EQ(primary.atlas->Metrics().cacheMisses, primaryBefore.cacheMisses);
    EXPECT_EQ(primary.atlas->Metrics().rasterizations, primaryBefore.rasterizations);
    EXPECT_EQ(fallback.atlas->Metrics().cacheMisses, fallbackBefore.cacheMisses);
    EXPECT_EQ(fallback.atlas->Metrics().rasterizations, fallbackBefore.rasterizations);

    ASSERT_EQ(prepared.run->Glyphs().size(), 1U);
    ExpectEquivalentFallbackGlyph(prepared.run->Glyphs()[0], published);
}

TEST(TextFallbackTests, InvalidOrIncompatibleChainsFailBeforeAtlasMutation)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult primary = PrepareFallbackAtlas(
        registry,
        "content/fonts/config-primary.ttf",
        TRACE2D_TEXT_PRIMARY_TEST_FONT_PATH);

    GlyphAtlasConfig incompatibleConfig{};
    incompatibleConfig.width = 256U;
    incompatibleConfig.height = 128U;
    incompatibleConfig.pixelHeight = 24U;
    incompatibleConfig.padding = 1U;
    incompatibleConfig.maxGlyphs = 64U;
    GlyphAtlasPrepareResult incompatible = PrepareFallbackAtlas(
        registry,
        "content/fonts/config-secondary.ttf",
        TRACE2D_TEXT_TEST_FONT_PATH,
        incompatibleConfig);
    ASSERT_TRUE(primary.Succeeded());
    ASSERT_TRUE(incompatible.Succeeded());

    TextLayoutRunPrepareResult prepared = PrepareTextLayoutRun({4U, 2U});
    ASSERT_TRUE(prepared.Succeeded());

    const TextLayoutResult empty = prepared.run->LayoutUtf8(std::span<const TextFontAtlasRef>{}, "A");
    ASSERT_FALSE(empty.Succeeded());
    EXPECT_EQ(empty.diagnostic->code, TextLayoutErrorCode::InvalidConfig);

    const std::array<TextFontAtlasRef, 1U> nullChain{TextFontAtlasRef{nullptr}};
    const TextLayoutResult nullAtlas = prepared.run->LayoutUtf8(nullChain, "A");
    ASSERT_FALSE(nullAtlas.Succeeded());
    EXPECT_EQ(nullAtlas.diagnostic->code, TextLayoutErrorCode::InvalidConfig);

    const auto incompatibleChain = MakeChain(*primary.atlas, *incompatible.atlas);
    const TextLayoutResult incompatibleLayout = prepared.run->LayoutUtf8(incompatibleChain, "A");
    ASSERT_FALSE(incompatibleLayout.Succeeded());
    EXPECT_EQ(incompatibleLayout.diagnostic->code, TextLayoutErrorCode::InvalidConfig);

    EXPECT_EQ(primary.atlas->Metrics().cacheMisses, 0U);
    EXPECT_EQ(primary.atlas->Metrics().rasterizations, 0U);
    EXPECT_EQ(incompatible.atlas->Metrics().cacheMisses, 0U);
    EXPECT_EQ(incompatible.atlas->Metrics().rasterizations, 0U);
}

TEST(TextFallbackTests, SelectedAtlasFailureDoesNotFallThroughToLaterFont)
{
    assets::ResourceRegistry registry("project");

    GlyphAtlasConfig constrainedConfig{};
    constrainedConfig.width = 256U;
    constrainedConfig.height = 128U;
    constrainedConfig.pixelHeight = 20U;
    constrainedConfig.padding = 1U;
    constrainedConfig.maxGlyphs = 1U;
    GlyphAtlasPrepareResult primary = PrepareFallbackAtlas(
        registry,
        "content/fonts/constrained-primary.ttf",
        TRACE2D_TEXT_PRIMARY_TEST_FONT_PATH,
        constrainedConfig);
    GlyphAtlasPrepareResult fallback = PrepareFallbackAtlas(
        registry,
        "content/fonts/constrained-secondary.ttf",
        TRACE2D_TEXT_TEST_FONT_PATH);
    ASSERT_TRUE(primary.Succeeded());
    ASSERT_TRUE(fallback.Succeeded());

    const GlyphAtlasWarmResult fill = primary.atlas->WarmUtf8("中");
    ASSERT_TRUE(fill.Succeeded());
    ASSERT_EQ(primary.atlas->Metrics().glyphCount, 1U);

    TextLayoutRunPrepareResult prepared = PrepareTextLayoutRun({4U, 2U});
    ASSERT_TRUE(prepared.Succeeded());
    const auto chain = MakeChain(*primary.atlas, *fallback.atlas);

    const TextLayoutResult failed = prepared.run->LayoutUtf8(chain, "A");
    ASSERT_FALSE(failed.Succeeded());
    EXPECT_EQ(failed.diagnostic->code, TextLayoutErrorCode::GlyphResolveFailed);
    ASSERT_TRUE(failed.diagnostic->textDiagnostic.has_value());
    EXPECT_EQ(failed.diagnostic->textDiagnostic->code, TextErrorCode::GlyphCacheLimitReached);
    EXPECT_TRUE(prepared.run->Glyphs().empty());
    EXPECT_EQ(fallback.atlas->Metrics().cacheMisses, 0U);
    EXPECT_EQ(fallback.atlas->Metrics().rasterizations, 0U);
}

TEST(TextFallbackTests, MixedFontLinesUseSharedPixelHeightMetricDomain)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult primary = PrepareFallbackAtlas(
        registry,
        "content/fonts/lines-primary.ttf",
        TRACE2D_TEXT_PRIMARY_TEST_FONT_PATH);
    GlyphAtlasPrepareResult fallback = PrepareFallbackAtlas(
        registry,
        "content/fonts/lines-secondary.ttf",
        TRACE2D_TEXT_TEST_FONT_PATH);
    ASSERT_TRUE(primary.Succeeded());
    ASSERT_TRUE(fallback.Succeeded());

    TextLayoutRunPrepareResult prepared = PrepareTextLayoutRun({8U, 4U});
    ASSERT_TRUE(prepared.Succeeded());

    TextLayoutOptions options{};
    options.boxWidth26_6 = 4096;
    options.wrapMode = TextWrapMode::GlyphBoundary;
    options.horizontalAlignment = TextHorizontalAlignment::Center;
    const auto chain = MakeChain(*primary.atlas, *fallback.atlas);
    const TextLayoutResult laidOut = prepared.run->LayoutUtf8(chain, "한\nA");
    ASSERT_TRUE(laidOut.Succeeded());
    EXPECT_EQ(laidOut.metrics->lineCount, 2U);
    ASSERT_EQ(prepared.run->Glyphs().size(), 2U);
    EXPECT_EQ(prepared.run->Glyphs()[0].fontSlot, 1U);
    EXPECT_EQ(prepared.run->Glyphs()[0].lineIndex, 0U);
    EXPECT_EQ(prepared.run->Glyphs()[1].fontSlot, 0U);
    EXPECT_EQ(prepared.run->Glyphs()[1].lineIndex, 1U);

    const std::int64_t lineHeight26_6 =
        static_cast<std::int64_t>(primary.atlas->Config().pixelHeight) * 64;
    EXPECT_EQ(laidOut.metrics->contentHeight26_6, lineHeight26_6 * 2);
    EXPECT_EQ(
        prepared.run->Glyphs()[1].baselineY26_6 - prepared.run->Glyphs()[0].baselineY26_6,
        lineHeight26_6);
}
} // namespace trace2d::text
