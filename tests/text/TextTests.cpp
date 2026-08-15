#include <trace2d/text/Text.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace trace2d::text
{
namespace
{
[[nodiscard]] std::vector<std::uint8_t> LoadTestFont()
{
    std::ifstream input(std::filesystem::path{TRACE2D_TEXT_TEST_FONT_PATH}, std::ios::binary);
    EXPECT_TRUE(input.is_open());
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

[[nodiscard]] assets::FontResource MakeTestFont()
{
    assets::FontResource resource{};
    resource.canonicalBytes = LoadTestFont();
    return resource;
}
} // namespace

TEST(TextUtf8Tests, DecodesAsciiKoreanAndCjkByCodepointNotByte)
{
    const Utf8DecodeResult result = DecodeUtf8("A한中");
    ASSERT_TRUE(result.Succeeded());
    ASSERT_EQ(result.codepoints.size(), 3U);
    EXPECT_EQ(result.codepoints[0], U'A');
    EXPECT_EQ(result.codepoints[1], U'한');
    EXPECT_EQ(result.codepoints[2], U'中');
}

TEST(TextUtf8Tests, RejectsMalformedUnicodeScalarSequencesDeterministically)
{
    const std::string truncated{"\xF0\x9F", 2U};
    const Utf8DecodeResult truncatedResult = DecodeUtf8(truncated);
    ASSERT_FALSE(truncatedResult.Succeeded());
    EXPECT_EQ(truncatedResult.diagnostic->code, Utf8ErrorCode::TruncatedSequence);
    EXPECT_EQ(truncatedResult.diagnostic->byteOffset, 0U);

    const std::string invalidContinuation{"\xE2\x28\xA1", 3U};
    const Utf8DecodeResult continuationResult = DecodeUtf8(invalidContinuation);
    ASSERT_FALSE(continuationResult.Succeeded());
    EXPECT_EQ(continuationResult.diagnostic->code, Utf8ErrorCode::InvalidContinuationByte);
    EXPECT_EQ(continuationResult.diagnostic->byteOffset, 1U);

    const std::string overlong{"\xE0\x80\x80", 3U};
    const Utf8DecodeResult overlongResult = DecodeUtf8(overlong);
    ASSERT_FALSE(overlongResult.Succeeded());
    EXPECT_EQ(overlongResult.diagnostic->code, Utf8ErrorCode::OverlongEncoding);

    const std::string surrogate{"\xED\xA0\x80", 3U};
    const Utf8DecodeResult surrogateResult = DecodeUtf8(surrogate);
    ASSERT_FALSE(surrogateResult.Succeeded());
    EXPECT_EQ(surrogateResult.diagnostic->code, Utf8ErrorCode::SurrogateCodePoint);

    const std::string outOfRange{"\xF4\x90\x80\x80", 4U};
    const Utf8DecodeResult rangeResult = DecodeUtf8(outOfRange);
    ASSERT_FALSE(rangeResult.Succeeded());
    EXPECT_EQ(rangeResult.diagnostic->code, Utf8ErrorCode::OutOfRangeCodePoint);
}

TEST(TextFontTests, FontResourceUsesTypedProjectRelativeIdentityAndMemoryEvidence)
{
    assets::ResourceRegistry registry("project");
    const auto font = registry.PublishFont("./content//fonts\\test.ttf", MakeTestFont());
    ASSERT_TRUE(font.Succeeded());

    const assets::FontResource* resolved = registry.Resolve(font.handle);
    ASSERT_NE(resolved, nullptr);
    ASSERT_FALSE(resolved->canonicalBytes.empty());

    const auto snapshot = registry.Inspect(font.handle.Untyped());
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_EQ(snapshot->identity.domain, assets::ResourceTypeDomain::Font);
    EXPECT_EQ(snapshot->identity.canonicalReference, "content/fonts/test.ttf");
    EXPECT_TRUE(snapshot->memory.cpuPayloadResident);
    EXPECT_EQ(snapshot->memory.cpuRetention, assets::CpuRetentionPolicy::Required);
    EXPECT_GE(snapshot->memory.knownRetainedCpuBytes, resolved->canonicalBytes.size());
}

TEST(TextFontTests, EmptyOrNonRequiredFontPayloadIsRejectedBeforePublication)
{
    assets::ResourceRegistry registry("project");

    assets::FontResource empty{};
    empty.canonicalBytes.clear();
    const auto emptyResult = registry.PublishFont("content/fonts/empty.ttf", std::move(empty));
    ASSERT_FALSE(emptyResult.Succeeded());
    EXPECT_EQ(emptyResult.diagnostic->code, assets::ResourceErrorCode::InvalidPayload);

    assets::FontResource releasable = MakeTestFont();
    releasable.cpuRetention = assets::CpuRetentionPolicy::Reacquirable;
    const auto releasableResult = registry.PublishFont("content/fonts/releasable.ttf", std::move(releasable));
    ASSERT_FALSE(releasableResult.Succeeded());
    EXPECT_EQ(releasableResult.diagnostic->code, assets::ResourceErrorCode::InvalidPayload);
}

TEST(TextFontTests, PreparedFaceRetainsResourceUntilFaceDestruction)
{
    assets::ResourceRegistry registry("project");
    const auto font = registry.PublishFont("content/fonts/test.ttf", MakeTestFont());
    ASSERT_TRUE(font.Succeeded());

    FontFacePrepareResult prepared = PrepareFontFace(registry, font.handle);
    ASSERT_TRUE(prepared.Succeeded());
    ASSERT_NE(prepared.face, nullptr);

    const auto retained = registry.Inspect(font.handle.Untyped());
    ASSERT_TRUE(retained.has_value());
    EXPECT_EQ(retained->callerRetainCount, 1U);

    const assets::ResourceOperationResult blockedUnload = registry.Unload(font.handle.Untyped());
    ASSERT_FALSE(blockedUnload.Succeeded());
    EXPECT_EQ(blockedUnload.diagnostic->code, assets::ResourceErrorCode::RetainedByCaller);

    prepared.face.reset();
    const auto released = registry.Inspect(font.handle.Untyped());
    ASSERT_TRUE(released.has_value());
    EXPECT_EQ(released->callerRetainCount, 0U);
    EXPECT_TRUE(registry.Unload(font.handle.Untyped()).Succeeded());
}

TEST(TextFontTests, InvalidFontBytesAndStaleHandlesFailWithoutPublishingPreparedFace)
{
    assets::ResourceRegistry registry("project");

    assets::FontResource invalid{};
    invalid.canonicalBytes = {0x00U, 0x01U, 0x02U, 0x03U};
    const auto invalidFont = registry.PublishFont("content/fonts/invalid.ttf", std::move(invalid));
    ASSERT_TRUE(invalidFont.Succeeded());
    const FontFacePrepareResult invalidPrepared = PrepareFontFace(registry, invalidFont.handle);
    ASSERT_FALSE(invalidPrepared.Succeeded());
    EXPECT_EQ(invalidPrepared.diagnostic->code, TextErrorCode::InvalidFontData);

    const auto staleFont = registry.PublishFont("content/fonts/stale.ttf", MakeTestFont());
    ASSERT_TRUE(staleFont.Succeeded());
    ASSERT_TRUE(registry.Unload(staleFont.handle.Untyped()).Succeeded());
    const FontFacePrepareResult stalePrepared = PrepareFontFace(registry, staleFont.handle);
    ASSERT_FALSE(stalePrepared.Succeeded());
    EXPECT_EQ(stalePrepared.diagnostic->code, TextErrorCode::InvalidFontHandle);
}

TEST(TextFontTests, MeasuresUtf8ByUnicodeGlyphAndCachesSelectedPixelHeight)
{
    assets::ResourceRegistry registry("project");
    const auto font = registry.PublishFont("content/fonts/test.ttf", MakeTestFont());
    ASSERT_TRUE(font.Succeeded());
    FontFacePrepareResult prepared = PrepareFontFace(registry, font.handle);
    ASSERT_TRUE(prepared.Succeeded());

    const TextMeasureResult first = prepared.face->MeasureUtf8("A한中", 20U);
    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(first.measure.has_value());
    EXPECT_EQ(first.measure->codepointCount, 3U);
    EXPECT_EQ(first.measure->glyphCount, 3U);
    EXPECT_GT(first.measure->advanceX26_6, 0);
    EXPECT_GT(first.measure->lineHeight26_6, 0);
    EXPECT_EQ(prepared.face->CurrentPixelHeight(), 20U);

    const TextMeasureResult second = prepared.face->MeasureUtf8("한A", 20U);
    ASSERT_TRUE(second.Succeeded());
    EXPECT_EQ(prepared.face->CurrentPixelHeight(), 20U);

    const std::string malformed{"A\xE2\x28\xA1", 4U};
    const TextMeasureResult invalid = prepared.face->MeasureUtf8(malformed, 20U);
    ASSERT_FALSE(invalid.Succeeded());
    EXPECT_EQ(invalid.diagnostic->code, TextErrorCode::InvalidUtf8);
    EXPECT_EQ(invalid.diagnostic->byteOffset, 2U);
}

TEST(TextFontTests, RasterizesRepositoryOwnedKoreanAndCjkGlyphsWithoutOsFontDiscovery)
{
    assets::ResourceRegistry registry("project");
    const auto font = registry.PublishFont("content/fonts/test.ttf", MakeTestFont());
    ASSERT_TRUE(font.Succeeded());
    FontFacePrepareResult prepared = PrepareFontFace(registry, font.handle);
    ASSERT_TRUE(prepared.Succeeded());

    for (const char32_t codepoint : {U'한', U'中'})
    {
        const GlyphRasterResult raster = prepared.face->RasterizeCodepoint(codepoint, 24U);
        ASSERT_TRUE(raster.Succeeded());
        ASSERT_TRUE(raster.glyph.has_value());
        EXPECT_NE(raster.glyph->glyphIndex, 0U);
        EXPECT_GT(raster.glyph->width, 0U);
        EXPECT_GT(raster.glyph->height, 0U);
        EXPECT_EQ(
            raster.glyph->alpha8.size(),
            static_cast<std::size_t>(raster.glyph->width) * static_cast<std::size_t>(raster.glyph->height));
        EXPECT_TRUE(std::any_of(
            raster.glyph->alpha8.begin(),
            raster.glyph->alpha8.end(),
            [](const std::uint8_t value) { return value != 0U; }));
    }
}

TEST(TextFontTests, MissingGlyphAndInvalidPixelHeightProduceTypedDiagnostics)
{
    assets::ResourceRegistry registry("project");
    const auto font = registry.PublishFont("content/fonts/test.ttf", MakeTestFont());
    ASSERT_TRUE(font.Succeeded());
    FontFacePrepareResult prepared = PrepareFontFace(registry, font.handle);
    ASSERT_TRUE(prepared.Succeeded());

    const GlyphRasterResult missing = prepared.face->RasterizeCodepoint(U'Ω', 20U);
    ASSERT_FALSE(missing.Succeeded());
    EXPECT_EQ(missing.diagnostic->code, TextErrorCode::MissingGlyph);
    EXPECT_EQ(missing.diagnostic->codepoint, U'Ω');

    const TextMeasureResult zero = prepared.face->MeasureUtf8("A", 0U);
    ASSERT_FALSE(zero.Succeeded());
    EXPECT_EQ(zero.diagnostic->code, TextErrorCode::InvalidPixelHeight);

    const TextMeasureResult tooLarge = prepared.face->MeasureUtf8("A", 4097U);
    ASSERT_FALSE(tooLarge.Succeeded());
    EXPECT_EQ(tooLarge.diagnostic->code, TextErrorCode::InvalidPixelHeight);
}
} // namespace trace2d::text
