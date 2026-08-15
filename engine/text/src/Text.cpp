#include <trace2d/text/Text.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

namespace trace2d::text
{
namespace
{
constexpr std::uint32_t MaxPixelHeight = 4096U;
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
    if (codepoint > static_cast<char32_t>(0x10FFFFU) ||
        (codepoint >= static_cast<char32_t>(0xD800U) && codepoint <= static_cast<char32_t>(0xDFFFU)))
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
} // namespace trace2d::text
