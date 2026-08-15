#include <trace2d/text/TextLayout.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace trace2d::text
{
namespace
{
[[nodiscard]] std::vector<std::uint8_t> LoadLayoutTestFont()
{
    std::ifstream input(std::filesystem::path{TRACE2D_TEXT_TEST_FONT_PATH}, std::ios::binary);
    EXPECT_TRUE(input.is_open());
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

[[nodiscard]] assets::FontResource MakeLayoutTestFont()
{
    assets::FontResource resource{};
    resource.canonicalBytes = LoadLayoutTestFont();
    return resource;
}

[[nodiscard]] GlyphAtlasPrepareResult PrepareLayoutAtlas(assets::ResourceRegistry& registry)
{
    const auto font = registry.PublishFont("content/fonts/layout-test.ttf", MakeLayoutTestFont());
    EXPECT_TRUE(font.Succeeded());

    GlyphAtlasConfig config{};
    config.width = 256U;
    config.height = 128U;
    config.pixelHeight = 20U;
    config.padding = 1U;
    config.maxGlyphs = 64U;
    return PrepareGlyphAtlas(registry, font.handle, config);
}

void ExpectSameGlyph(const PositionedGlyph& left, const PositionedGlyph& right)
{
    EXPECT_EQ(left.atlasEntry.codepoint, right.atlasEntry.codepoint);
    EXPECT_EQ(left.atlasEntry.glyphIndex, right.atlasEntry.glyphIndex);
    EXPECT_EQ(left.atlasEntry.x, right.atlasEntry.x);
    EXPECT_EQ(left.atlasEntry.y, right.atlasEntry.y);
    EXPECT_EQ(left.atlasEntry.width, right.atlasEntry.width);
    EXPECT_EQ(left.atlasEntry.height, right.atlasEntry.height);
    EXPECT_EQ(left.atlasEntry.advanceX26_6, right.atlasEntry.advanceX26_6);
    EXPECT_EQ(left.byteOffset, right.byteOffset);
    EXPECT_EQ(left.lineIndex, right.lineIndex);
    EXPECT_EQ(left.penX26_6, right.penX26_6);
    EXPECT_EQ(left.baselineY26_6, right.baselineY26_6);
}
} // namespace

TEST(TextLayoutTests, RepeatedAsciiKoreanAndCjkLayoutIsStableWithoutRerasterization)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult atlas = PrepareLayoutAtlas(registry);
    ASSERT_TRUE(atlas.Succeeded());

    TextLayoutRunConfig runConfig{};
    runConfig.maxGlyphs = 16U;
    runConfig.maxLines = 8U;
    TextLayoutRunPrepareResult prepared = PrepareTextLayoutRun(runConfig);
    ASSERT_TRUE(prepared.Succeeded());

    const TextLayoutResult first = prepared.run->LayoutUtf8(*atlas.atlas, "A한中");
    ASSERT_TRUE(first.Succeeded());
    EXPECT_EQ(first.metrics->glyphCount, 3U);
    EXPECT_EQ(first.metrics->lineCount, 1U);
    EXPECT_GT(first.metrics->contentWidth26_6, 0);
    EXPECT_GT(first.metrics->contentHeight26_6, 0);

    const GlyphAtlasMetrics afterFirst = atlas.atlas->Metrics();
    EXPECT_EQ(afterFirst.rasterizations, 3U);

    const auto firstSpan = prepared.run->Glyphs();
    const std::vector<PositionedGlyph> firstGlyphs(firstSpan.begin(), firstSpan.end());
    const TextLayoutMetrics firstMetrics = prepared.run->Metrics();

    const TextLayoutResult second = prepared.run->LayoutUtf8(*atlas.atlas, "A한中");
    ASSERT_TRUE(second.Succeeded());
    EXPECT_EQ(atlas.atlas->Metrics().rasterizations, afterFirst.rasterizations);
    EXPECT_EQ(prepared.run->Metrics().glyphCount, firstMetrics.glyphCount);
    EXPECT_EQ(prepared.run->Metrics().lineCount, firstMetrics.lineCount);
    EXPECT_EQ(prepared.run->Metrics().contentWidth26_6, firstMetrics.contentWidth26_6);
    EXPECT_EQ(prepared.run->Metrics().contentHeight26_6, firstMetrics.contentHeight26_6);

    const auto secondGlyphs = prepared.run->Glyphs();
    ASSERT_EQ(secondGlyphs.size(), firstGlyphs.size());
    for (std::size_t index = 0U; index < firstGlyphs.size(); ++index)
    {
        ExpectSameGlyph(firstGlyphs[index], secondGlyphs[index]);
    }
}

TEST(TextLayoutTests, ExplicitNewlinesAndFiniteWidthWrapDeterministically)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult atlas = PrepareLayoutAtlas(registry);
    ASSERT_TRUE(atlas.Succeeded());
    TextLayoutRunPrepareResult prepared = PrepareTextLayoutRun({16U, 8U});
    ASSERT_TRUE(prepared.Succeeded());

    const TextLayoutResult probe = prepared.run->LayoutUtf8(*atlas.atlas, "A");
    ASSERT_TRUE(probe.Succeeded());
    ASSERT_EQ(prepared.run->Glyphs().size(), 1U);
    const std::int64_t advance = prepared.run->Glyphs()[0].atlasEntry.advanceX26_6;
    ASSERT_GT(advance, 0);

    TextLayoutOptions wrap{};
    wrap.boxWidth26_6 = advance;
    wrap.wrapMode = TextWrapMode::GlyphBoundary;
    const TextLayoutResult wrapped = prepared.run->LayoutUtf8(*atlas.atlas, "AAA", wrap);
    ASSERT_TRUE(wrapped.Succeeded());
    EXPECT_EQ(wrapped.metrics->glyphCount, 3U);
    EXPECT_EQ(wrapped.metrics->lineCount, 3U);
    ASSERT_EQ(prepared.run->Lines().size(), 3U);
    for (const TextLayoutLine& line : prepared.run->Lines())
    {
        EXPECT_EQ(line.glyphCount, 1U);
        EXPECT_EQ(line.advanceWidth26_6, advance);
    }

    const TextLayoutResult newline = prepared.run->LayoutUtf8(*atlas.atlas, "A\r\nA\n");
    ASSERT_TRUE(newline.Succeeded());
    EXPECT_EQ(newline.metrics->glyphCount, 2U);
    EXPECT_EQ(newline.metrics->lineCount, 3U);
    ASSERT_EQ(prepared.run->Lines().size(), 3U);
    EXPECT_EQ(prepared.run->Lines()[0].glyphCount, 1U);
    EXPECT_EQ(prepared.run->Lines()[1].glyphCount, 1U);
    EXPECT_EQ(prepared.run->Lines()[2].glyphCount, 0U);
}

TEST(TextLayoutTests, HorizontalAndVerticalAlignmentUseStableIntegerOffsets)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult atlas = PrepareLayoutAtlas(registry);
    ASSERT_TRUE(atlas.Succeeded());
    TextLayoutRunPrepareResult prepared = PrepareTextLayoutRun({16U, 8U});
    ASSERT_TRUE(prepared.Succeeded());

    ASSERT_TRUE(prepared.run->LayoutUtf8(*atlas.atlas, "A").Succeeded());
    const std::int64_t advance = prepared.run->Glyphs()[0].atlasEntry.advanceX26_6;
    const std::int64_t lineHeight = static_cast<std::int64_t>(atlas.atlas->Config().pixelHeight) * 64;
    ASSERT_GT(advance, 0);
    ASSERT_GT(lineHeight, 0);

    TextLayoutOptions options{};
    options.boxWidth26_6 = advance * 4;
    options.boxHeight26_6 = lineHeight * 5;
    options.horizontalAlignment = TextHorizontalAlignment::Center;
    options.verticalAlignment = TextVerticalAlignment::Middle;
    const TextLayoutResult centered = prepared.run->LayoutUtf8(*atlas.atlas, "A\nAA", options);
    ASSERT_TRUE(centered.Succeeded());
    ASSERT_EQ(prepared.run->Lines().size(), 2U);
    EXPECT_EQ(prepared.run->Lines()[0].offsetX26_6, (advance * 3) / 2);
    EXPECT_EQ(prepared.run->Lines()[1].offsetX26_6, advance);
    EXPECT_EQ(prepared.run->Lines()[0].baselineY26_6, lineHeight + (lineHeight * 3) / 2);
    EXPECT_EQ(prepared.run->Lines()[1].baselineY26_6, lineHeight * 2 + (lineHeight * 3) / 2);

    options.horizontalAlignment = TextHorizontalAlignment::Right;
    options.verticalAlignment = TextVerticalAlignment::Bottom;
    const TextLayoutResult bottomRight = prepared.run->LayoutUtf8(*atlas.atlas, "A\nAA", options);
    ASSERT_TRUE(bottomRight.Succeeded());
    EXPECT_EQ(prepared.run->Lines()[0].offsetX26_6, advance * 3);
    EXPECT_EQ(prepared.run->Lines()[1].offsetX26_6, advance * 2);
    EXPECT_EQ(prepared.run->Lines()[0].baselineY26_6, lineHeight * 4);
    EXPECT_EQ(prepared.run->Lines()[1].baselineY26_6, lineHeight * 5);

    options.horizontalAlignment = TextHorizontalAlignment::Left;
    options.verticalAlignment = TextVerticalAlignment::Top;
    ASSERT_TRUE(prepared.run->LayoutUtf8(*atlas.atlas, "A\nAA", options).Succeeded());
    EXPECT_EQ(prepared.run->Lines()[0].offsetX26_6, 0);
    EXPECT_EQ(prepared.run->Lines()[1].offsetX26_6, 0);
    EXPECT_EQ(prepared.run->Lines()[0].baselineY26_6, lineHeight);
}

TEST(TextLayoutTests, ValidationAndCapacityFailuresLeavePublishedRunUntouched)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult atlas = PrepareLayoutAtlas(registry);
    ASSERT_TRUE(atlas.Succeeded());
    TextLayoutRunPrepareResult prepared = PrepareTextLayoutRun({2U, 2U});
    ASSERT_TRUE(prepared.Succeeded());

    const TextLayoutResult initial = prepared.run->LayoutUtf8(*atlas.atlas, "A");
    ASSERT_TRUE(initial.Succeeded());
    ASSERT_EQ(prepared.run->Glyphs().size(), 1U);
    const PositionedGlyph published = prepared.run->Glyphs()[0];
    const TextLayoutMetrics publishedMetrics = prepared.run->Metrics();
    const GlyphAtlasMetrics atlasBeforeFailures = atlas.atlas->Metrics();

    const std::string malformed{"\xE2\x28\xA1", 3U};
    const TextLayoutResult invalidUtf8 = prepared.run->LayoutUtf8(*atlas.atlas, malformed);
    ASSERT_FALSE(invalidUtf8.Succeeded());
    EXPECT_EQ(invalidUtf8.diagnostic->code, TextLayoutErrorCode::InvalidUtf8);
    EXPECT_EQ(invalidUtf8.diagnostic->byteOffset, 1U);
    EXPECT_EQ(atlas.atlas->Metrics().rasterizations, atlasBeforeFailures.rasterizations);

    const TextLayoutResult glyphCapacity = prepared.run->LayoutUtf8(*atlas.atlas, "AAA");
    ASSERT_FALSE(glyphCapacity.Succeeded());
    EXPECT_EQ(glyphCapacity.diagnostic->code, TextLayoutErrorCode::GlyphCapacityExceeded);

    const TextLayoutResult lineCapacity = prepared.run->LayoutUtf8(*atlas.atlas, "A\nA\nA");
    ASSERT_FALSE(lineCapacity.Succeeded());
    EXPECT_EQ(lineCapacity.diagnostic->code, TextLayoutErrorCode::GlyphCapacityExceeded);

    const TextLayoutResult missing = prepared.run->LayoutUtf8(*atlas.atlas, "Ω");
    ASSERT_FALSE(missing.Succeeded());
    EXPECT_EQ(missing.diagnostic->code, TextLayoutErrorCode::GlyphResolveFailed);
    ASSERT_TRUE(missing.diagnostic->textDiagnostic.has_value());
    EXPECT_EQ(missing.diagnostic->textDiagnostic->code, TextErrorCode::MissingGlyph);

    EXPECT_EQ(prepared.run->Metrics().glyphCount, publishedMetrics.glyphCount);
    EXPECT_EQ(prepared.run->Metrics().lineCount, publishedMetrics.lineCount);
    ASSERT_EQ(prepared.run->Glyphs().size(), 1U);
    ExpectSameGlyph(prepared.run->Glyphs()[0], published);
}

TEST(TextLayoutTests, LineCapacityFailureIsTypedAndTransactional)
{
    assets::ResourceRegistry registry("project");
    GlyphAtlasPrepareResult atlas = PrepareLayoutAtlas(registry);
    ASSERT_TRUE(atlas.Succeeded());
    TextLayoutRunPrepareResult prepared = PrepareTextLayoutRun({4U, 2U});
    ASSERT_TRUE(prepared.Succeeded());

    ASSERT_TRUE(prepared.run->LayoutUtf8(*atlas.atlas, "A").Succeeded());
    const PositionedGlyph published = prepared.run->Glyphs()[0];

    const TextLayoutResult tooManyLines = prepared.run->LayoutUtf8(*atlas.atlas, "A\nA\nA");
    ASSERT_FALSE(tooManyLines.Succeeded());
    EXPECT_EQ(tooManyLines.diagnostic->code, TextLayoutErrorCode::LineCapacityExceeded);
    ASSERT_EQ(prepared.run->Glyphs().size(), 1U);
    ExpectSameGlyph(prepared.run->Glyphs()[0], published);
}
} // namespace trace2d::text
