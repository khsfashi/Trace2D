#pragma once

#include <trace2d/assets/ResourceRegistry.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::text
{
enum class Utf8ErrorCode : std::uint8_t
{
    InvalidLeadingByte = 0,
    TruncatedSequence,
    InvalidContinuationByte,
    OverlongEncoding,
    SurrogateCodePoint,
    OutOfRangeCodePoint,
};

[[nodiscard]] std::string_view ToString(Utf8ErrorCode value) noexcept;

struct Utf8Diagnostic final
{
    Utf8ErrorCode code{Utf8ErrorCode::InvalidLeadingByte};
    std::size_t byteOffset{0};
};

struct Utf8DecodeResult final
{
    std::vector<char32_t> codepoints{};
    std::optional<Utf8Diagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return !diagnostic.has_value();
    }
};

[[nodiscard]] Utf8DecodeResult DecodeUtf8(std::string_view text);

enum class TextErrorCode : std::uint8_t
{
    InvalidFontHandle = 0,
    FontRetainFailed,
    FreeTypeInitFailed,
    InvalidFontData,
    InvalidPixelHeight,
    InvalidUtf8,
    MissingGlyph,
    SizeSelectionFailed,
    GlyphLoadFailed,
    GlyphRenderFailed,
    BitmapSizeOverflow,
    InvalidGlyphAtlasConfig,
    GlyphAtlasSizeOverflow,
    GlyphAtlasAllocationFailed,
    GlyphCacheLimitReached,
    GlyphAtlasFull,
};

[[nodiscard]] std::string_view ToString(TextErrorCode value) noexcept;

struct TextDiagnostic final
{
    TextErrorCode code{TextErrorCode::InvalidFontHandle};
    std::string message{};
    std::size_t byteOffset{0};
    char32_t codepoint{0};
};

struct TextMeasure final
{
    std::size_t codepointCount{0};
    std::size_t glyphCount{0};
    std::int64_t advanceX26_6{0};
    std::int64_t ascender26_6{0};
    std::int64_t descender26_6{0};
    std::int64_t lineHeight26_6{0};
};

struct TextMeasureResult final
{
    std::optional<TextMeasure> measure{};
    std::optional<TextDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return measure.has_value() && !diagnostic.has_value();
    }
};

struct GlyphBitmap final
{
    std::uint32_t glyphIndex{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::int32_t bearingX{0};
    std::int32_t bearingY{0};
    std::int64_t advanceX26_6{0};
    std::vector<std::uint8_t> alpha8{};
};

struct GlyphRasterResult final
{
    std::optional<GlyphBitmap> glyph{};
    std::optional<TextDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return glyph.has_value() && !diagnostic.has_value();
    }
};

struct FontFacePrepareResult;

class FontFace final
{
public:
    FontFace(const FontFace&) = delete;
    FontFace& operator=(const FontFace&) = delete;
    FontFace(FontFace&&) noexcept;
    FontFace& operator=(FontFace&&) noexcept;
    ~FontFace();

    [[nodiscard]] TextMeasureResult MeasureUtf8(std::string_view text, std::uint32_t pixelHeight);
    [[nodiscard]] GlyphRasterResult RasterizeCodepoint(char32_t codepoint, std::uint32_t pixelHeight);
    // Coverage-only probe. It does not select a pixel size, rasterize, or allocate diagnostic storage.
    [[nodiscard]] bool SupportsCodepoint(char32_t codepoint) const noexcept;
    [[nodiscard]] assets::ResourceHandle<assets::FontResource> ResourceHandle() const noexcept;
    [[nodiscard]] std::uint32_t CurrentPixelHeight() const noexcept;

private:
    struct Impl;
    explicit FontFace(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_{};

    friend FontFacePrepareResult PrepareFontFace(
        assets::ResourceRegistry& registry,
        assets::ResourceHandle<assets::FontResource> handle);
};

struct FontFacePrepareResult final
{
    std::unique_ptr<FontFace> face{};
    std::optional<TextDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return face != nullptr && !diagnostic.has_value();
    }
};

// The registry must outlive every prepared FontFace created from it. Normal unload/release-unused
// is blocked by the retained handle; project-wide force-clear/shutdown must destroy prepared faces first.
[[nodiscard]] FontFacePrepareResult PrepareFontFace(
    assets::ResourceRegistry& registry,
    assets::ResourceHandle<assets::FontResource> handle);

struct GlyphAtlasConfig final
{
    std::uint32_t width{1024U};
    std::uint32_t height{1024U};
    std::uint32_t pixelHeight{32U};
    std::uint32_t padding{1U};
    std::uint32_t maxGlyphs{4096U};
};

struct GlyphAtlasEntry final
{
    char32_t codepoint{0};
    std::uint32_t glyphIndex{0};
    std::uint32_t x{0};
    std::uint32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::int32_t bearingX{0};
    std::int32_t bearingY{0};
    std::int64_t advanceX26_6{0};
};

struct GlyphAtlasMetrics final
{
    std::size_t glyphCount{0};
    std::uint64_t cacheHits{0};
    std::uint64_t cacheMisses{0};
    std::uint64_t rasterizations{0};
    std::size_t retainedAtlasBytes{0};
    std::uint64_t occupiedBitmapPixels{0};
    std::size_t lookupSlotCount{0};
};

struct GlyphAtlasResolveResult final
{
    std::optional<GlyphAtlasEntry> entry{};
    bool cacheHit{false};
    std::optional<TextDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return entry.has_value() && !diagnostic.has_value();
    }
};

struct GlyphAtlasWarmResult final
{
    std::size_t codepointCount{0};
    std::size_t uniqueGlyphsAdded{0};
    std::size_t cacheHits{0};
    std::optional<TextDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return !diagnostic.has_value();
    }
};

struct GlyphAtlasPrepareResult;

// A GlyphAtlas owns one prepared FontFace at one fixed pixel height. It is intentionally bounded:
// cache exhaustion is reported instead of triggering implicit growth/eviction. The type is not
// internally synchronized; use one atlas from one text-preparation thread unless externally guarded.
class GlyphAtlas final
{
public:
    GlyphAtlas(const GlyphAtlas&) = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;
    GlyphAtlas(GlyphAtlas&&) noexcept;
    GlyphAtlas& operator=(GlyphAtlas&&) noexcept;
    ~GlyphAtlas();

    [[nodiscard]] GlyphAtlasResolveResult ResolveCodepoint(char32_t codepoint);
    [[nodiscard]] GlyphAtlasWarmResult WarmUtf8(std::string_view text);
    // Fallback layout uses these probes before ResolveCodepoint so a normal missing-glyph path
    // never constructs an error string or mutates cache metrics.
    [[nodiscard]] bool IsPrepared() const noexcept;
    [[nodiscard]] bool SupportsCodepoint(char32_t codepoint) const noexcept;
    [[nodiscard]] GlyphAtlasConfig Config() const noexcept;
    [[nodiscard]] GlyphAtlasMetrics Metrics() const noexcept;
    [[nodiscard]] std::span<const std::uint8_t> Alpha8() const noexcept;
    [[nodiscard]] assets::ResourceHandle<assets::FontResource> ResourceHandle() const noexcept;

private:
    struct Impl;
    explicit GlyphAtlas(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_{};

    friend GlyphAtlasPrepareResult PrepareGlyphAtlas(
        assets::ResourceRegistry& registry,
        assets::ResourceHandle<assets::FontResource> handle,
        GlyphAtlasConfig config);
};

struct GlyphAtlasPrepareResult final
{
    std::unique_ptr<GlyphAtlas> atlas{};
    std::optional<TextDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return atlas != nullptr && !diagnostic.has_value();
    }
};

[[nodiscard]] GlyphAtlasPrepareResult PrepareGlyphAtlas(
    assets::ResourceRegistry& registry,
    assets::ResourceHandle<assets::FontResource> handle,
    GlyphAtlasConfig config = {});
} // namespace trace2d::text
