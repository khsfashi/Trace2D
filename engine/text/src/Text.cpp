#include <trace2d/text/Text.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace trace2d::text
{
namespace
{
constexpr std::uint32_t MaxPixelHeight = 4096U;
constexpr std::uint32_t MaxAtlasDimension = 4096U;
constexpr std::uint64_t MaxAtlasPixels = 16ULL * 1024ULL * 1024ULL;
constexpr std::uint32_t MaxAtlasPadding = 64U;
constexpr std::uint32_t MaxCachedGlyphs = 65536U;
constexpr FT_Int32 OutlineLoadFlags = FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP;

[[nodiscard]] bool DecodeNextUtf8(
    const std::string_view text,
    std::size_t& cursor,
    char32_t& codepoint,
    Utf8Diagnostic& diagnostic) noexcept
{
    const std::size_t start = cursor;
    if (start >= text.size())
    {
        return false;
    }

    const auto byteAt = [&text](const std::size_t index) noexcept {
        return static_cast<std::uint8_t>(static_cast<unsigned char>(text[index]));
    };

    const std::uint8_t first = byteAt(start);
    if (first <= 0x7FU)
    {
        codepoint = static_cast<char32_t>(first);
        cursor = start + 1U;
        return true;
    }

    std::size_t length = 0U;
    std::uint32_t value = 0U;
    std::uint32_t minimum = 0U;
    if (first >= 0xC2U && first <= 0xDFU)
    {
        length = 2U;
        value = static_cast<std::uint32_t>(first & 0x1FU);
        minimum = 0x80U;
    }
    else if (first >= 0xE0U && first <= 0xEFU)
    {
        length = 3U;
        value = static_cast<std::uint32_t>(first & 0x0FU);
        minimum = 0x800U;
    }
    else if (first >= 0xF0U && first <= 0xF4U)
    {
        length = 4U;
        value = static_cast<std::uint32_t>(first & 0x07U);
        minimum = 0x10000U;
    }
    else
    {
        diagnostic = Utf8Diagnostic{Utf8ErrorCode::InvalidLeadingByte, start};
        return false;
    }

    if (text.size() - start < length)
    {
        diagnostic = Utf8Diagnostic{Utf8ErrorCode::TruncatedSequence, start};
        return false;
    }

    for (std::size_t offset = 1U; offset < length; ++offset)
    {
        const std::uint8_t continuation = byteAt(start + offset);
        if ((continuation & 0xC0U) != 0x80U)
        {
            diagnostic = Utf8Diagnostic{Utf8ErrorCode::InvalidContinuationByte, start + offset};
            return false;
        }
        value = (value << 6U) | static_cast<std::uint32_t>(continuation & 0x3FU);
    }

    if (value < minimum)
    {
        diagnostic = Utf8Diagnostic{Utf8ErrorCode::OverlongEncoding, start};
        return false;
    }
    if (value >= 0xD800U && value <= 0xDFFFU)
    {
        diagnostic = Utf8Diagnostic{Utf8ErrorCode::SurrogateCodePoint, start};
        return false;
    }
    if (value > 0x10FFFFU)
    {
        diagnostic = Utf8Diagnostic{Utf8ErrorCode::OutOfRangeCodePoint, start};
        return false;
    }

    codepoint = static_cast<char32_t>(value);
    cursor = start + length;
    return true;
}

[[nodiscard]] TextDiagnostic InvalidUtf8Diagnostic(const Utf8Diagnostic diagnostic)
{
    TextDiagnostic output{};
    output.code = TextErrorCode::InvalidUtf8;
    output.byteOffset = diagnostic.byteOffset;
    output.message = "invalid UTF-8: ";
    output.message.append(ToString(diagnostic.code));
    return output;
}

[[nodiscard]] TextDiagnostic Diagnostic(
    const TextErrorCode code,
    std::string message,
    const std::size_t byteOffset = 0U,
    const char32_t codepoint = 0)
{
    return TextDiagnostic{code, std::move(message), byteOffset, codepoint};
}

[[nodiscard]] bool IsUnicodeScalar(const char32_t codepoint) noexcept
{
    return codepoint <= static_cast<char32_t>(0x10FFFFU) &&
           !(codepoint >= static_cast<char32_t>(0xD800U) && codepoint <= static_cast<char32_t>(0xDFFFU));
}

[[nodiscard]] std::optional<TextDiagnostic> ValidateGlyphAtlasConfig(
    const GlyphAtlasConfig config,
    std::size_t& atlasPixelCount,
    std::size_t& lookupSlotCount) noexcept
{
    if (config.width == 0U || config.height == 0U ||
        config.width > MaxAtlasDimension || config.height > MaxAtlasDimension)
    {
        return Diagnostic(
            TextErrorCode::InvalidGlyphAtlasConfig,
            "glyph atlas width and height must each be in [1, 4096]");
    }
    if (config.pixelHeight == 0U || config.pixelHeight > MaxPixelHeight)
    {
        return Diagnostic(
            TextErrorCode::InvalidGlyphAtlasConfig,
            "glyph atlas pixel height must be in [1, 4096]");
    }
    if (config.padding > MaxAtlasPadding)
    {
        return Diagnostic(
            TextErrorCode::InvalidGlyphAtlasConfig,
            "glyph atlas padding must be in [0, 64]");
    }
    if (config.maxGlyphs == 0U || config.maxGlyphs > MaxCachedGlyphs)
    {
        return Diagnostic(
            TextErrorCode::InvalidGlyphAtlasConfig,
            "glyph atlas maxGlyphs must be in [1, 65536]");
    }

    const std::uint64_t pixelCount =
        static_cast<std::uint64_t>(config.width) * static_cast<std::uint64_t>(config.height);
    if (pixelCount > MaxAtlasPixels || pixelCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
    {
        return Diagnostic(
            TextErrorCode::GlyphAtlasSizeOverflow,
            "glyph atlas pixel storage exceeds the bounded size limit");
    }
    atlasPixelCount = static_cast<std::size_t>(pixelCount);

    std::size_t requiredSlots = static_cast<std::size_t>(config.maxGlyphs) * 2U;
    lookupSlotCount = 1U;
    while (lookupSlotCount < requiredSlots)
    {
        if (lookupSlotCount > std::numeric_limits<std::size_t>::max() / 2U)
        {
            return Diagnostic(
                TextErrorCode::GlyphAtlasSizeOverflow,
                "glyph atlas lookup table size overflows size_t");
        }
        lookupSlotCount *= 2U;
    }
    return std::nullopt;
}

[[nodiscard]] std::size_t HashCodepoint(const char32_t codepoint, const std::size_t mask) noexcept
{
    std::uint32_t value = static_cast<std::uint32_t>(codepoint);
    value ^= value >> 16U;
    value *= 0x7FEB352DU;
    value ^= value >> 15U;
    value *= 0x846CA68BU;
    value ^= value >> 16U;
    return static_cast<std::size_t>(value) & mask;
}
} // namespace

std::string_view ToString(const Utf8ErrorCode value) noexcept
{
    switch (value)
    {
    case Utf8ErrorCode::InvalidLeadingByte:
        return "invalid_leading_byte";
    case Utf8ErrorCode::TruncatedSequence:
        return "truncated_sequence";
    case Utf8ErrorCode::InvalidContinuationByte:
        return "invalid_continuation_byte";
    case Utf8ErrorCode::OverlongEncoding:
        return "overlong_encoding";
    case Utf8ErrorCode::SurrogateCodePoint:
        return "surrogate_codepoint";
    case Utf8ErrorCode::OutOfRangeCodePoint:
        return "out_of_range_codepoint";
    }
    return "unknown";
}

Utf8DecodeResult DecodeUtf8(const std::string_view text)
{
    Utf8DecodeResult output{};
    output.codepoints.reserve(text.size());

    std::size_t cursor = 0U;
    while (cursor < text.size())
    {
        char32_t codepoint = 0;
        Utf8Diagnostic diagnostic{};
        if (!DecodeNextUtf8(text, cursor, codepoint, diagnostic))
        {
            output.codepoints.clear();
            output.diagnostic = diagnostic;
            return output;
        }
        output.codepoints.push_back(codepoint);
    }
    return output;
}

std::string_view ToString(const TextErrorCode value) noexcept
{
    switch (value)
    {
    case TextErrorCode::InvalidFontHandle:
        return "invalid_font_handle";
    case TextErrorCode::FontRetainFailed:
        return "font_retain_failed";
    case TextErrorCode::FreeTypeInitFailed:
        return "freetype_init_failed";
    case TextErrorCode::InvalidFontData:
        return "invalid_font_data";
    case TextErrorCode::InvalidPixelHeight:
        return "invalid_pixel_height";
    case TextErrorCode::InvalidUtf8:
        return "invalid_utf8";
    case TextErrorCode::MissingGlyph:
        return "missing_glyph";
    case TextErrorCode::SizeSelectionFailed:
        return "size_selection_failed";
    case TextErrorCode::GlyphLoadFailed:
        return "glyph_load_failed";
    case TextErrorCode::GlyphRenderFailed:
        return "glyph_render_failed";
    case TextErrorCode::BitmapSizeOverflow:
        return "bitmap_size_overflow";
    case TextErrorCode::InvalidGlyphAtlasConfig:
        return "invalid_glyph_atlas_config";
    case TextErrorCode::GlyphAtlasSizeOverflow:
        return "glyph_atlas_size_overflow";
    case TextErrorCode::GlyphAtlasAllocationFailed:
        return "glyph_atlas_allocation_failed";
    case TextErrorCode::GlyphCacheLimitReached:
        return "glyph_cache_limit_reached";
    case TextErrorCode::GlyphAtlasFull:
        return "glyph_atlas_full";
    }
    return "unknown";
}

struct FontFace::Impl final
{
    assets::ResourceRegistry* registry{nullptr};
    assets::ResourceHandle<assets::FontResource> handle{};
    FT_Library library{nullptr};
    FT_Face face{nullptr};
    std::uint32_t currentPixelHeight{0};
    bool retained{false};

    ~Impl()
    {
        if (face != nullptr)
        {
            FT_Done_Face(face);
            face = nullptr;
        }
        if (library != nullptr)
        {
            FT_Done_FreeType(library);
            library = nullptr;
        }
        if (retained && registry != nullptr)
        {
            (void)registry->Release(handle.Untyped());
        }
    }

    [[nodiscard]] std::optional<TextDiagnostic> EnsurePixelHeight(const std::uint32_t pixelHeight)
    {
        if (pixelHeight == 0U || pixelHeight > MaxPixelHeight)
        {
            return Diagnostic(
                TextErrorCode::InvalidPixelHeight,
                "pixel height must be in [1, 4096]");
        }
        if (currentPixelHeight == pixelHeight)
        {
            return std::nullopt;
        }
        if (FT_Set_Pixel_Sizes(face, 0U, static_cast<FT_UInt>(pixelHeight)) != 0)
        {
            return Diagnostic(
                TextErrorCode::SizeSelectionFailed,
                "FreeType failed to select the requested pixel height");
        }
        currentPixelHeight = pixelHeight;
        return std::nullopt;
    }
};

FontFace::FontFace(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{
}

FontFace::FontFace(FontFace&&) noexcept = default;
FontFace& FontFace::operator=(FontFace&&) noexcept = default;
FontFace::~FontFace() = default;

TextMeasureResult FontFace::MeasureUtf8(const std::string_view text, const std::uint32_t pixelHeight)
{
    TextMeasureResult output{};
    if (impl_ == nullptr || impl_->face == nullptr)
    {
        output.diagnostic = Diagnostic(TextErrorCode::InvalidFontHandle, "font face is not prepared");
        return output;
    }
    if (const auto sizeDiagnostic = impl_->EnsurePixelHeight(pixelHeight); sizeDiagnostic.has_value())
    {
        output.diagnostic = *sizeDiagnostic;
        return output;
    }

    TextMeasure measure{};
    measure.ascender26_6 = static_cast<std::int64_t>(impl_->face->size->metrics.ascender);
    measure.descender26_6 = static_cast<std::int64_t>(impl_->face->size->metrics.descender);
    measure.lineHeight26_6 = static_cast<std::int64_t>(impl_->face->size->metrics.height);

    FT_UInt previousGlyph = 0U;
    std::size_t cursor = 0U;
    while (cursor < text.size())
    {
        const std::size_t codepointOffset = cursor;
        char32_t codepoint = 0;
        Utf8Diagnostic utf8Diagnostic{};
        if (!DecodeNextUtf8(text, cursor, codepoint, utf8Diagnostic))
        {
            output.diagnostic = InvalidUtf8Diagnostic(utf8Diagnostic);
            return output;
        }

        const FT_UInt glyphIndex = FT_Get_Char_Index(impl_->face, static_cast<FT_ULong>(codepoint));
        if (glyphIndex == 0U)
        {
            output.diagnostic = Diagnostic(
                TextErrorCode::MissingGlyph,
                "font does not contain a glyph for the requested codepoint",
                codepointOffset,
                codepoint);
            return output;
        }

        if (previousGlyph != 0U && FT_HAS_KERNING(impl_->face) != 0)
        {
            FT_Vector kerning{};
            if (FT_Get_Kerning(impl_->face, previousGlyph, glyphIndex, FT_KERNING_DEFAULT, &kerning) != 0)
            {
                output.diagnostic = Diagnostic(
                    TextErrorCode::GlyphLoadFailed,
                    "FreeType failed to resolve pair kerning",
                    codepointOffset,
                    codepoint);
                return output;
            }
            measure.advanceX26_6 += static_cast<std::int64_t>(kerning.x);
        }

        if (FT_Load_Glyph(impl_->face, glyphIndex, OutlineLoadFlags) != 0)
        {
            output.diagnostic = Diagnostic(
                TextErrorCode::GlyphLoadFailed,
                "FreeType failed to load glyph metrics",
                codepointOffset,
                codepoint);
            return output;
        }

        measure.advanceX26_6 += static_cast<std::int64_t>(impl_->face->glyph->advance.x);
        ++measure.codepointCount;
        ++measure.glyphCount;
        previousGlyph = glyphIndex;
    }

    output.measure = measure;
    return output;
}

GlyphRasterResult FontFace::RasterizeCodepoint(const char32_t codepoint, const std::uint32_t pixelHeight)
{
    GlyphRasterResult output{};
    if (impl_ == nullptr || impl_->face == nullptr)
    {
        output.diagnostic = Diagnostic(TextErrorCode::InvalidFontHandle, "font face is not prepared");
        return output;
    }
    if (!IsUnicodeScalar(codepoint))
    {
        output.diagnostic = Diagnostic(
            TextErrorCode::MissingGlyph,
            "codepoint is outside the Unicode scalar-value range",
            0U,
            codepoint);
        return output;
    }
    if (const auto sizeDiagnostic = impl_->EnsurePixelHeight(pixelHeight); sizeDiagnostic.has_value())
    {
        output.diagnostic = *sizeDiagnostic;
        return output;
    }

    const FT_UInt glyphIndex = FT_Get_Char_Index(impl_->face, static_cast<FT_ULong>(codepoint));
    if (glyphIndex == 0U)
    {
        output.diagnostic = Diagnostic(
            TextErrorCode::MissingGlyph,
            "font does not contain a glyph for the requested codepoint",
            0U,
            codepoint);
        return output;
    }
    if (FT_Load_Glyph(impl_->face, glyphIndex, OutlineLoadFlags) != 0)
    {
        output.diagnostic = Diagnostic(
            TextErrorCode::GlyphLoadFailed,
            "FreeType failed to load the requested glyph",
            0U,
            codepoint);
        return output;
    }
    if (FT_Render_Glyph(impl_->face->glyph, FT_RENDER_MODE_NORMAL) != 0)
    {
        output.diagnostic = Diagnostic(
            TextErrorCode::GlyphRenderFailed,
            "FreeType failed to rasterize the requested glyph",
            0U,
            codepoint);
        return output;
    }

    const FT_Bitmap& bitmap = impl_->face->glyph->bitmap;
    GlyphBitmap glyph{};
    glyph.glyphIndex = static_cast<std::uint32_t>(glyphIndex);
    glyph.width = static_cast<std::uint32_t>(bitmap.width);
    glyph.height = static_cast<std::uint32_t>(bitmap.rows);
    glyph.bearingX = static_cast<std::int32_t>(impl_->face->glyph->bitmap_left);
    glyph.bearingY = static_cast<std::int32_t>(impl_->face->glyph->bitmap_top);
    glyph.advanceX26_6 = static_cast<std::int64_t>(impl_->face->glyph->advance.x);

    if (glyph.width != 0U && glyph.height != 0U)
    {
        const std::size_t width = static_cast<std::size_t>(glyph.width);
        const std::size_t height = static_cast<std::size_t>(glyph.height);
        if (height > std::numeric_limits<std::size_t>::max() / width)
        {
            output.diagnostic = Diagnostic(TextErrorCode::BitmapSizeOverflow, "glyph bitmap size overflows size_t");
            return output;
        }
        if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY || bitmap.buffer == nullptr || bitmap.num_grays < 2U)
        {
            output.diagnostic = Diagnostic(
                TextErrorCode::GlyphRenderFailed,
                "FreeType normal render mode did not produce an 8-bit grayscale glyph bitmap");
            return output;
        }

        glyph.alpha8.resize(width * height);
        const std::ptrdiff_t pitch = static_cast<std::ptrdiff_t>(bitmap.pitch);
        const std::ptrdiff_t stride = pitch >= 0 ? pitch : -pitch;
        if (stride < static_cast<std::ptrdiff_t>(width))
        {
            output.diagnostic = Diagnostic(TextErrorCode::GlyphRenderFailed, "glyph bitmap pitch is smaller than its width");
            return output;
        }

        for (std::size_t row = 0U; row < height; ++row)
        {
            const std::size_t sourceRow = pitch >= 0 ? row : (height - 1U - row);
            const auto* source = bitmap.buffer + static_cast<std::ptrdiff_t>(sourceRow) * stride;
            auto* destination = glyph.alpha8.data() + row * width;
            if (bitmap.num_grays == 256U)
            {
                std::copy_n(source, width, destination);
            }
            else
            {
                const std::uint32_t denominator = static_cast<std::uint32_t>(bitmap.num_grays - 1U);
                for (std::size_t column = 0U; column < width; ++column)
                {
                    const std::uint32_t value = static_cast<std::uint32_t>(source[column]);
                    destination[column] = static_cast<std::uint8_t>((value * 255U) / denominator);
                }
            }
        }
    }

    output.glyph = std::move(glyph);
    return output;
}

assets::ResourceHandle<assets::FontResource> FontFace::ResourceHandle() const noexcept
{
    return impl_ != nullptr ? impl_->handle : assets::ResourceHandle<assets::FontResource>{};
}

std::uint32_t FontFace::CurrentPixelHeight() const noexcept
{
    return impl_ != nullptr ? impl_->currentPixelHeight : 0U;
}

FontFacePrepareResult PrepareFontFace(
    assets::ResourceRegistry& registry,
    const assets::ResourceHandle<assets::FontResource> handle)
{
    FontFacePrepareResult output{};
    const assets::FontResource* resource = registry.Resolve(handle);
    if (resource == nullptr)
    {
        output.diagnostic = Diagnostic(TextErrorCode::InvalidFontHandle, "font resource handle is stale or invalid");
        return output;
    }
    if (resource->canonicalBytes.empty())
    {
        output.diagnostic = Diagnostic(TextErrorCode::InvalidFontData, "font resource contains no canonical bytes");
        return output;
    }
    if (resource->canonicalBytes.size() > static_cast<std::size_t>(std::numeric_limits<FT_Long>::max()))
    {
        output.diagnostic = Diagnostic(TextErrorCode::InvalidFontData, "font resource is too large for FreeType memory-face input");
        return output;
    }
    if (resource->faceIndex > static_cast<std::uint32_t>(std::numeric_limits<FT_Long>::max()))
    {
        output.diagnostic = Diagnostic(TextErrorCode::InvalidFontData, "font face index exceeds FreeType range");
        return output;
    }

    const assets::ResourceOperationResult retain = registry.Retain(handle.Untyped());
    if (!retain.Succeeded())
    {
        output.diagnostic = Diagnostic(TextErrorCode::FontRetainFailed, "failed to retain font resource for prepared face lifetime");
        return output;
    }

    auto impl = std::make_unique<FontFace::Impl>();
    impl->registry = &registry;
    impl->handle = handle;
    impl->retained = true;

    if (FT_Init_FreeType(&impl->library) != 0)
    {
        output.diagnostic = Diagnostic(TextErrorCode::FreeTypeInitFailed, "FreeType library initialization failed");
        return output;
    }

    if (FT_New_Memory_Face(
            impl->library,
            reinterpret_cast<const FT_Byte*>(resource->canonicalBytes.data()),
            static_cast<FT_Long>(resource->canonicalBytes.size()),
            static_cast<FT_Long>(resource->faceIndex),
            &impl->face) != 0)
    {
        output.diagnostic = Diagnostic(TextErrorCode::InvalidFontData, "FreeType rejected the canonical font bytes or face index");
        return output;
    }

    output.face = std::unique_ptr<FontFace>(new FontFace(std::move(impl)));
    return output;
}

struct GlyphAtlas::Impl final
{
    struct LookupSlot final
    {
        char32_t codepoint{0};
        std::size_t entryIndex{0};
        bool occupied{false};
    };

    std::unique_ptr<FontFace> face{};
    GlyphAtlasConfig config{};
    std::vector<std::uint8_t> alpha8{};
    std::vector<GlyphAtlasEntry> entries{};
    std::vector<LookupSlot> lookupSlots{};
    std::uint32_t nextX{0};
    std::uint32_t nextY{0};
    std::uint32_t rowHeight{0};
    GlyphAtlasMetrics metrics{};

    [[nodiscard]] std::optional<std::size_t> FindEntryIndex(const char32_t codepoint) const noexcept
    {
        if (lookupSlots.empty())
        {
            return std::nullopt;
        }
        const std::size_t mask = lookupSlots.size() - 1U;
        std::size_t slotIndex = HashCodepoint(codepoint, mask);
        for (std::size_t probe = 0U; probe < lookupSlots.size(); ++probe)
        {
            const LookupSlot& slot = lookupSlots[slotIndex];
            if (!slot.occupied)
            {
                return std::nullopt;
            }
            if (slot.codepoint == codepoint)
            {
                return slot.entryIndex;
            }
            slotIndex = (slotIndex + 1U) & mask;
        }
        return std::nullopt;
    }

    [[nodiscard]] bool InsertLookup(const char32_t codepoint, const std::size_t entryIndex) noexcept
    {
        if (lookupSlots.empty())
        {
            return false;
        }
        const std::size_t mask = lookupSlots.size() - 1U;
        std::size_t slotIndex = HashCodepoint(codepoint, mask);
        for (std::size_t probe = 0U; probe < lookupSlots.size(); ++probe)
        {
            LookupSlot& slot = lookupSlots[slotIndex];
            if (!slot.occupied)
            {
                slot.codepoint = codepoint;
                slot.entryIndex = entryIndex;
                slot.occupied = true;
                return true;
            }
            slotIndex = (slotIndex + 1U) & mask;
        }
        return false;
    }
};

GlyphAtlas::GlyphAtlas(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{
}

GlyphAtlas::GlyphAtlas(GlyphAtlas&&) noexcept = default;
GlyphAtlas& GlyphAtlas::operator=(GlyphAtlas&&) noexcept = default;
GlyphAtlas::~GlyphAtlas() = default;

GlyphAtlasResolveResult GlyphAtlas::ResolveCodepoint(const char32_t codepoint)
{
    GlyphAtlasResolveResult output{};
    if (impl_ == nullptr || impl_->face == nullptr)
    {
        output.diagnostic = Diagnostic(TextErrorCode::InvalidFontHandle, "glyph atlas has no prepared font face");
        return output;
    }
    if (!IsUnicodeScalar(codepoint))
    {
        output.diagnostic = Diagnostic(
            TextErrorCode::MissingGlyph,
            "codepoint is outside the Unicode scalar-value range",
            0U,
            codepoint);
        return output;
    }

    if (const auto cachedIndex = impl_->FindEntryIndex(codepoint); cachedIndex.has_value())
    {
        ++impl_->metrics.cacheHits;
        output.entry = impl_->entries[*cachedIndex];
        output.cacheHit = true;
        return output;
    }

    ++impl_->metrics.cacheMisses;
    if (impl_->entries.size() >= static_cast<std::size_t>(impl_->config.maxGlyphs))
    {
        output.diagnostic = Diagnostic(
            TextErrorCode::GlyphCacheLimitReached,
            "glyph cache reached its declared maxGlyphs limit",
            0U,
            codepoint);
        return output;
    }

    GlyphRasterResult raster = impl_->face->RasterizeCodepoint(codepoint, impl_->config.pixelHeight);
    if (!raster.Succeeded())
    {
        output.diagnostic = raster.diagnostic;
        return output;
    }
    ++impl_->metrics.rasterizations;
    const GlyphBitmap& glyph = *raster.glyph;

    GlyphAtlasEntry entry{};
    entry.codepoint = codepoint;
    entry.glyphIndex = glyph.glyphIndex;
    entry.width = glyph.width;
    entry.height = glyph.height;
    entry.bearingX = glyph.bearingX;
    entry.bearingY = glyph.bearingY;
    entry.advanceX26_6 = glyph.advanceX26_6;

    std::uint32_t committedNextX = impl_->nextX;
    std::uint32_t committedNextY = impl_->nextY;
    std::uint32_t committedRowHeight = impl_->rowHeight;

    if (glyph.width != 0U && glyph.height != 0U)
    {
        const std::uint64_t paddingTwice = static_cast<std::uint64_t>(impl_->config.padding) * 2ULL;
        const std::uint64_t requiredWidth = static_cast<std::uint64_t>(glyph.width) + paddingTwice;
        const std::uint64_t requiredHeight = static_cast<std::uint64_t>(glyph.height) + paddingTwice;
        if (requiredWidth > static_cast<std::uint64_t>(impl_->config.width) ||
            requiredHeight > static_cast<std::uint64_t>(impl_->config.height))
        {
            output.diagnostic = Diagnostic(
                TextErrorCode::GlyphAtlasFull,
                "glyph plus padding cannot fit within the fixed atlas dimensions",
                0U,
                codepoint);
            return output;
        }

        std::uint64_t candidateX = static_cast<std::uint64_t>(impl_->nextX);
        std::uint64_t candidateY = static_cast<std::uint64_t>(impl_->nextY);
        std::uint64_t candidateRowHeight = static_cast<std::uint64_t>(impl_->rowHeight);
        if (candidateX + requiredWidth > static_cast<std::uint64_t>(impl_->config.width))
        {
            candidateX = 0U;
            candidateY += candidateRowHeight;
            candidateRowHeight = 0U;
        }
        if (candidateY + requiredHeight > static_cast<std::uint64_t>(impl_->config.height))
        {
            output.diagnostic = Diagnostic(
                TextErrorCode::GlyphAtlasFull,
                "fixed glyph atlas has no remaining shelf capacity for the glyph",
                0U,
                codepoint);
            return output;
        }

        entry.x = static_cast<std::uint32_t>(candidateX + static_cast<std::uint64_t>(impl_->config.padding));
        entry.y = static_cast<std::uint32_t>(candidateY + static_cast<std::uint64_t>(impl_->config.padding));
        committedNextX = static_cast<std::uint32_t>(candidateX + requiredWidth);
        committedNextY = static_cast<std::uint32_t>(candidateY);
        committedRowHeight = static_cast<std::uint32_t>(std::max(candidateRowHeight, requiredHeight));
    }

    const std::size_t entryIndex = impl_->entries.size();
    impl_->entries.push_back(entry);
    if (!impl_->InsertLookup(codepoint, entryIndex))
    {
        impl_->entries.pop_back();
        output.diagnostic = Diagnostic(
            TextErrorCode::GlyphCacheLimitReached,
            "preallocated glyph lookup table has no free slot",
            0U,
            codepoint);
        return output;
    }

    if (glyph.width != 0U && glyph.height != 0U)
    {
        const std::size_t atlasWidth = static_cast<std::size_t>(impl_->config.width);
        const std::size_t glyphWidth = static_cast<std::size_t>(glyph.width);
        const std::size_t glyphHeight = static_cast<std::size_t>(glyph.height);
        for (std::size_t row = 0U; row < glyphHeight; ++row)
        {
            const std::size_t destinationOffset =
                (static_cast<std::size_t>(entry.y) + row) * atlasWidth + static_cast<std::size_t>(entry.x);
            const std::size_t sourceOffset = row * glyphWidth;
            std::copy_n(
                glyph.alpha8.data() + sourceOffset,
                glyphWidth,
                impl_->alpha8.data() + destinationOffset);
        }
        impl_->metrics.occupiedBitmapPixels +=
            static_cast<std::uint64_t>(glyph.width) * static_cast<std::uint64_t>(glyph.height);
        impl_->nextX = committedNextX;
        impl_->nextY = committedNextY;
        impl_->rowHeight = committedRowHeight;
    }

    impl_->metrics.glyphCount = impl_->entries.size();
    output.entry = entry;
    output.cacheHit = false;
    return output;
}

GlyphAtlasWarmResult GlyphAtlas::WarmUtf8(const std::string_view text)
{
    GlyphAtlasWarmResult output{};
    std::size_t cursor = 0U;
    while (cursor < text.size())
    {
        const std::size_t codepointOffset = cursor;
        char32_t codepoint = 0;
        Utf8Diagnostic utf8Diagnostic{};
        if (!DecodeNextUtf8(text, cursor, codepoint, utf8Diagnostic))
        {
            output.diagnostic = InvalidUtf8Diagnostic(utf8Diagnostic);
            return output;
        }

        GlyphAtlasResolveResult resolved = ResolveCodepoint(codepoint);
        if (!resolved.Succeeded())
        {
            output.diagnostic = resolved.diagnostic;
            if (output.diagnostic.has_value())
            {
                output.diagnostic->byteOffset = codepointOffset;
            }
            return output;
        }

        ++output.codepointCount;
        if (resolved.cacheHit)
        {
            ++output.cacheHits;
        }
        else
        {
            ++output.uniqueGlyphsAdded;
        }
    }
    return output;
}

GlyphAtlasConfig GlyphAtlas::Config() const noexcept
{
    return impl_ != nullptr ? impl_->config : GlyphAtlasConfig{};
}

GlyphAtlasMetrics GlyphAtlas::Metrics() const noexcept
{
    return impl_ != nullptr ? impl_->metrics : GlyphAtlasMetrics{};
}

std::span<const std::uint8_t> GlyphAtlas::Alpha8() const noexcept
{
    if (impl_ == nullptr)
    {
        return {};
    }
    return std::span<const std::uint8_t>(impl_->alpha8.data(), impl_->alpha8.size());
}

assets::ResourceHandle<assets::FontResource> GlyphAtlas::ResourceHandle() const noexcept
{
    return impl_ != nullptr && impl_->face != nullptr
        ? impl_->face->ResourceHandle()
        : assets::ResourceHandle<assets::FontResource>{};
}

GlyphAtlasPrepareResult PrepareGlyphAtlas(
    assets::ResourceRegistry& registry,
    const assets::ResourceHandle<assets::FontResource> handle,
    const GlyphAtlasConfig config)
{
    GlyphAtlasPrepareResult output{};
    std::size_t atlasPixelCount = 0U;
    std::size_t lookupSlotCount = 0U;
    if (const auto diagnostic = ValidateGlyphAtlasConfig(config, atlasPixelCount, lookupSlotCount); diagnostic.has_value())
    {
        output.diagnostic = *diagnostic;
        return output;
    }

    FontFacePrepareResult preparedFace = PrepareFontFace(registry, handle);
    if (!preparedFace.Succeeded())
    {
        output.diagnostic = preparedFace.diagnostic;
        return output;
    }

    try
    {
        auto impl = std::make_unique<GlyphAtlas::Impl>();
        impl->face = std::move(preparedFace.face);
        impl->config = config;
        impl->alpha8.assign(atlasPixelCount, 0U);
        impl->entries.reserve(static_cast<std::size_t>(config.maxGlyphs));
        impl->lookupSlots.resize(lookupSlotCount);
        impl->metrics.retainedAtlasBytes = impl->alpha8.size();
        impl->metrics.lookupSlotCount = impl->lookupSlots.size();
        output.atlas = std::unique_ptr<GlyphAtlas>(new GlyphAtlas(std::move(impl)));
    }
    catch (const std::bad_alloc&)
    {
        output.diagnostic = Diagnostic(
            TextErrorCode::GlyphAtlasAllocationFailed,
            "failed to allocate bounded glyph atlas storage");
    }
    return output;
}
} // namespace trace2d::text
