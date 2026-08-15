#include <trace2d/text/TextLayoutCache.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

namespace trace2d::text
{
namespace
{
[[nodiscard]] std::vector<std::uint8_t> LoadCacheTestFont()
{
    std::ifstream input(std::filesystem::path{TRACE2D_TEXT_TEST_FONT_PATH}, std::ios::binary);
    EXPECT_TRUE(input.is_open());
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

[[nodiscard]] GlyphAtlasPrepareResult PrepareCacheAtlas(assets::ResourceRegistry& registry)
{
    assets::FontResource font{};
    font.canonicalBytes = LoadCacheTestFont();
    const auto published = registry.PublishFont("content/fonts/layout-cache.ttf", std::move(font));
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
} // namespace

TEST(TextLayoutCacheTests, StableSourceRevisionReusesPublishedLayoutWithoutTouchingGlyphCache)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult atlas = PrepareCacheAtlas(registry);
    ASSERT_TRUE(atlas.Succeeded());

    TextLayoutCachePrepareResult prepared = PrepareTextLayoutCache(
        TextLayoutCacheConfig{TextLayoutRunConfig{16U, 4U}, 2U});
    ASSERT_TRUE(prepared.Succeeded());

    const std::array<TextFontAtlasRef, 1U> fallback{
        TextFontAtlasRef{atlas.atlas.get()},
    };
    const TextSourceView source{
        .identity = 42U,
        .revision = 7U,
        .utf8 = "A한中",
    };

    const TextLayoutCacheUpdateResult first = prepared.cache->Update(fallback, source);
    ASSERT_TRUE(first.Succeeded());
    EXPECT_FALSE(first.reused);
    ASSERT_NE(prepared.cache->Layout(), nullptr);
    EXPECT_EQ(first.metrics->glyphCount, 3U);

    const GlyphAtlasMetrics afterFirst = atlas.atlas->Metrics();
    EXPECT_TRUE(prepared.cache->CanReuse(
        fallback,
        TextSourceView{42U, 7U, {}},
        {}));
    EXPECT_FALSE(prepared.cache->CanReuse(
        fallback,
        TextSourceView{42U, 8U, {}},
        {}));
    const GlyphAtlasMetrics afterProbe = atlas.atlas->Metrics();
    EXPECT_EQ(afterProbe.cacheHits, afterFirst.cacheHits);
    EXPECT_EQ(afterProbe.cacheMisses, afterFirst.cacheMisses);
    EXPECT_EQ(afterProbe.rasterizations, afterFirst.rasterizations);

    const TextLayoutCacheUpdateResult second = prepared.cache->Update(fallback, source);
    ASSERT_TRUE(second.Succeeded());
    EXPECT_TRUE(second.reused);
    EXPECT_EQ(second.metrics->glyphCount, first.metrics->glyphCount);
    EXPECT_EQ(second.metrics->lineCount, first.metrics->lineCount);
    EXPECT_EQ(second.metrics->contentWidth26_6, first.metrics->contentWidth26_6);
    EXPECT_EQ(second.metrics->contentHeight26_6, first.metrics->contentHeight26_6);
    EXPECT_EQ(second.metrics->layoutWidth26_6, first.metrics->layoutWidth26_6);
    EXPECT_EQ(second.metrics->layoutHeight26_6, first.metrics->layoutHeight26_6);

    const GlyphAtlasMetrics afterHit = atlas.atlas->Metrics();
    EXPECT_EQ(afterHit.glyphCount, afterFirst.glyphCount);
    EXPECT_EQ(afterHit.cacheHits, afterFirst.cacheHits);
    EXPECT_EQ(afterHit.cacheMisses, afterFirst.cacheMisses);
    EXPECT_EQ(afterHit.rasterizations, afterFirst.rasterizations);
    EXPECT_EQ(afterHit.occupiedBitmapPixels, afterFirst.occupiedBitmapPixels);

    TextSourceView revised = source;
    revised.revision = 8U;
    const TextLayoutCacheUpdateResult relayout = prepared.cache->Update(fallback, revised);
    ASSERT_TRUE(relayout.Succeeded());
    EXPECT_FALSE(relayout.reused);

    const GlyphAtlasMetrics afterRevision = atlas.atlas->Metrics();
    EXPECT_EQ(afterRevision.rasterizations, afterFirst.rasterizations);
    EXPECT_EQ(afterRevision.cacheMisses, afterFirst.cacheMisses);
    EXPECT_EQ(afterRevision.cacheHits, afterFirst.cacheHits + 3U);
}

TEST(TextLayoutCacheTests, FallbackCapacityFailureDoesNotPublishPartialLayout)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult atlas = PrepareCacheAtlas(registry);
    ASSERT_TRUE(atlas.Succeeded());

    TextLayoutCachePrepareResult prepared = PrepareTextLayoutCache(
        TextLayoutCacheConfig{TextLayoutRunConfig{8U, 2U}, 1U});
    ASSERT_TRUE(prepared.Succeeded());

    const std::array<TextFontAtlasRef, 2U> tooMany{
        TextFontAtlasRef{atlas.atlas.get()},
        TextFontAtlasRef{atlas.atlas.get()},
    };
    const TextLayoutCacheUpdateResult failed = prepared.cache->Update(
        tooMany,
        TextSourceView{1U, 1U, std::string_view{"A"}});
    ASSERT_FALSE(failed.Succeeded());
    ASSERT_TRUE(failed.diagnostic.has_value());
    EXPECT_EQ(failed.diagnostic->code, TextLayoutErrorCode::InvalidConfig);
    EXPECT_FALSE(prepared.cache->HasPublishedLayout());
    EXPECT_EQ(prepared.cache->Layout(), nullptr);
}
} // namespace trace2d::text
