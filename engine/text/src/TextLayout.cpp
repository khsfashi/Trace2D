#include <trace2d/text/TextLayout.hpp>

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
constexpr std::size_t MaxLayoutGlyphs = 1U << 20U;
constexpr std::size_t MaxLayoutLines = 1U << 16U;

[[nodiscard]] TextLayoutDiagnostic LayoutDiagnostic(
    const TextLayoutErrorCode code,
    std::string message,
    const std::size_t byteOffset = 0U,
    const char32_t codepoint = 0)
{
    TextLayoutDiagnostic output{};
    output.code = code;
    output.message = std::move(message);
    output.byteOffset = byteOffset;
    output.codepoint = codepoint;
    return output;
}

[[nodiscard]] bool DecodeNextUtf8NoAlloc(
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

[[nodiscard]] TextLayoutDiagnostic InvalidUtf8LayoutDiagnostic(const Utf8Diagnostic diagnostic)
{
    TextLayoutDiagnostic output{};
    output.code = TextLayoutErrorCode::InvalidUtf8;
    output.byteOffset = diagnostic.byteOffset;
    output.message = "invalid UTF-8: ";
    output.message.append(ToString(diagnostic.code));
    return output;
}

[[nodiscard]] bool CheckedAdd(
    const std::int64_t left,
    const std::int64_t right,
    std::int64_t& output) noexcept
{
    if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) ||
        (right < 0 && left < std::numeric_limits<std::int64_t>::min() - right))
    {
        return false;
    }
    output = left + right;
    return true;
}

[[nodiscard]] bool CheckedMultiplyNonNegative(
    const std::size_t count,
    const std::int64_t value,
    std::int64_t& output) noexcept
{
    if (value < 0)
    {
        return false;
    }
    if (count == 0U || value == 0)
    {
        output = 0;
        return true;
    }
    const auto max = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    const auto unsignedValue = static_cast<std::uint64_t>(value);
    if (count > max / unsignedValue)
    {
        return false;
    }
    output = static_cast<std::int64_t>(static_cast<std::uint64_t>(count) * unsignedValue);
    return true;
}

[[nodiscard]] std::optional<TextLayoutDiagnostic> ValidateOptions(
    const GlyphAtlasConfig atlasConfig,
    const TextLayoutOptions options,
    std::int64_t& lineHeight26_6,
    std::int64_t& baselineOffset26_6) noexcept
{
    if (options.boxWidth26_6 < 0 || options.boxHeight26_6 < 0 ||
        options.lineHeight26_6 < 0 || options.baselineOffset26_6 < 0)
    {
        return LayoutDiagnostic(
            TextLayoutErrorCode::InvalidConfig,
            "text layout dimensions and metrics must be non-negative");
    }

    const std::int64_t defaultMetric = static_cast<std::int64_t>(atlasConfig.pixelHeight) * 64;
    lineHeight26_6 = options.lineHeight26_6 != 0 ? options.lineHeight26_6 : defaultMetric;
    baselineOffset26_6 = options.baselineOffset26_6 != 0
        ? options.baselineOffset26_6
        : std::min(lineHeight26_6, defaultMetric);
    if (lineHeight26_6 <= 0 || baselineOffset26_6 > lineHeight26_6)
    {
        return LayoutDiagnostic(
            TextLayoutErrorCode::InvalidConfig,
            "line height must be positive and baseline offset must fit within the line box");
    }

    switch (options.wrapMode)
    {
    case TextWrapMode::None:
    case TextWrapMode::GlyphBoundary:
        break;
    default:
        return LayoutDiagnostic(TextLayoutErrorCode::InvalidConfig, "unknown text wrap mode");
    }
    switch (options.horizontalAlignment)
    {
    case TextHorizontalAlignment::Left:
    case TextHorizontalAlignment::Center:
    case TextHorizontalAlignment::Right:
        break;
    default:
        return LayoutDiagnostic(TextLayoutErrorCode::InvalidConfig, "unknown horizontal alignment");
    }
    switch (options.verticalAlignment)
    {
    case TextVerticalAlignment::Top:
    case TextVerticalAlignment::Middle:
    case TextVerticalAlignment::Bottom:
        break;
    default:
        return LayoutDiagnostic(TextLayoutErrorCode::InvalidConfig, "unknown vertical alignment");
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<TextLayoutDiagnostic> ValidateUtf8AndCapacity(
    const std::string_view text,
    const std::size_t maxGlyphs) noexcept
{
    std::size_t glyphCount = 0U;
    std::size_t cursor = 0U;
    while (cursor < text.size())
    {
        char32_t codepoint = 0;
        Utf8Diagnostic diagnostic{};
        if (!DecodeNextUtf8NoAlloc(text, cursor, codepoint, diagnostic))
        {
            return InvalidUtf8LayoutDiagnostic(diagnostic);
        }
        if (codepoint == U'\n' || codepoint == U'\r')
        {
            continue;
        }
        if (glyphCount >= maxGlyphs)
        {
            return LayoutDiagnostic(
                TextLayoutErrorCode::GlyphCapacityExceeded,
                "decoded text exceeds the prepared glyph capacity");
        }
        ++glyphCount;
    }
    return std::nullopt;
}
} // namespace

std::string_view ToString(const TextLayoutErrorCode value) noexcept
{
    switch (value)
    {
    case TextLayoutErrorCode::InvalidConfig:
        return "invalid_config";
    case TextLayoutErrorCode::InvalidUtf8:
        return "invalid_utf8";
    case TextLayoutErrorCode::GlyphResolveFailed:
        return "glyph_resolve_failed";
    case TextLayoutErrorCode::GlyphCapacityExceeded:
        return "glyph_capacity_exceeded";
    case TextLayoutErrorCode::LineCapacityExceeded:
        return "line_capacity_exceeded";
    case TextLayoutErrorCode::MetricOverflow:
        return "metric_overflow";
    case TextLayoutErrorCode::AllocationFailed:
        return "allocation_failed";
    }
    return "unknown";
}

struct TextLayoutRun::Impl final
{
    TextLayoutRunConfig config{};
    std::vector<PositionedGlyph> glyphs{};
    std::vector<TextLayoutLine> lines{};
    std::vector<PositionedGlyph> scratchGlyphs{};
    std::vector<TextLayoutLine> scratchLines{};
    TextLayoutMetrics metrics{};
};

TextLayoutRun::TextLayoutRun(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl))
{
}

TextLayoutRun::TextLayoutRun(TextLayoutRun&&) noexcept = default;
TextLayoutRun& TextLayoutRun::operator=(TextLayoutRun&&) noexcept = default;
TextLayoutRun::~TextLayoutRun() = default;

TextLayoutResult TextLayoutRun::LayoutUtf8(
    GlyphAtlas& atlas,
    const std::string_view text,
    const TextLayoutOptions options)
{
    TextLayoutResult output{};
    if (impl_ == nullptr)
    {
        output.diagnostic = LayoutDiagnostic(TextLayoutErrorCode::InvalidConfig, "text layout run is not prepared");
        return output;
    }

    std::int64_t lineHeight26_6 = 0;
    std::int64_t baselineOffset26_6 = 0;
    if (const auto diagnostic = ValidateOptions(atlas.Config(), options, lineHeight26_6, baselineOffset26_6);
        diagnostic.has_value())
    {
        output.diagnostic = *diagnostic;
        return output;
    }
    if (const auto diagnostic = ValidateUtf8AndCapacity(text, impl_->config.maxGlyphs); diagnostic.has_value())
    {
        output.diagnostic = *diagnostic;
        return output;
    }

    impl_->scratchGlyphs.clear();
    impl_->scratchLines.clear();

    std::size_t lineFirstGlyph = 0U;
    std::int64_t penX26_6 = 0;
    bool pendingFinalEmptyLine = false;

    const auto finalizeLine = [&]() -> std::optional<TextLayoutDiagnostic> {
        if (impl_->scratchLines.size() >= impl_->config.maxLines)
        {
            return LayoutDiagnostic(
                TextLayoutErrorCode::LineCapacityExceeded,
                "layout exceeds the prepared line capacity");
        }
        TextLayoutLine line{};
        line.firstGlyph = lineFirstGlyph;
        line.glyphCount = impl_->scratchGlyphs.size() - lineFirstGlyph;
        line.advanceWidth26_6 = penX26_6;
        impl_->scratchLines.push_back(line);
        lineFirstGlyph = impl_->scratchGlyphs.size();
        penX26_6 = 0;
        return std::nullopt;
    };

    std::size_t cursor = 0U;
    while (cursor < text.size())
    {
        const std::size_t codepointOffset = cursor;
        char32_t codepoint = 0;
        Utf8Diagnostic utf8Diagnostic{};
        if (!DecodeNextUtf8NoAlloc(text, cursor, codepoint, utf8Diagnostic))
        {
            output.diagnostic = InvalidUtf8LayoutDiagnostic(utf8Diagnostic);
            return output;
        }

        if (codepoint == U'\r' || codepoint == U'\n')
        {
            if (codepoint == U'\r' && cursor < text.size())
            {
                const std::size_t savedCursor = cursor;
                char32_t next = 0;
                Utf8Diagnostic nextDiagnostic{};
                if (DecodeNextUtf8NoAlloc(text, cursor, next, nextDiagnostic))
                {
                    if (next != U'\n')
                    {
                        cursor = savedCursor;
                    }
                }
                else
                {
                    output.diagnostic = InvalidUtf8LayoutDiagnostic(nextDiagnostic);
                    return output;
                }
            }

            if (const auto diagnostic = finalizeLine(); diagnostic.has_value())
            {
                output.diagnostic = *diagnostic;
                return output;
            }
            pendingFinalEmptyLine = true;
            continue;
        }

        GlyphAtlasResolveResult resolved = atlas.ResolveCodepoint(codepoint);
        if (!resolved.Succeeded())
        {
            TextLayoutDiagnostic diagnostic = LayoutDiagnostic(
                TextLayoutErrorCode::GlyphResolveFailed,
                "glyph atlas failed to resolve a decoded codepoint",
                codepointOffset,
                codepoint);
            diagnostic.textDiagnostic = resolved.diagnostic;
            output.diagnostic = std::move(diagnostic);
            return output;
        }

        const GlyphAtlasEntry entry = *resolved.entry;
        if (entry.advanceX26_6 < 0)
        {
            output.diagnostic = LayoutDiagnostic(
                TextLayoutErrorCode::MetricOverflow,
                "negative glyph advances are not supported by the deterministic F2 layout path",
                codepointOffset,
                codepoint);
            return output;
        }

        std::int64_t candidateX26_6 = 0;
        if (!CheckedAdd(penX26_6, entry.advanceX26_6, candidateX26_6))
        {
            output.diagnostic = LayoutDiagnostic(
                TextLayoutErrorCode::MetricOverflow,
                "glyph advance accumulation overflowed int64",
                codepointOffset,
                codepoint);
            return output;
        }

        const bool lineHasGlyphs = impl_->scratchGlyphs.size() != lineFirstGlyph;
        if (options.wrapMode == TextWrapMode::GlyphBoundary && options.boxWidth26_6 > 0 &&
            lineHasGlyphs && candidateX26_6 > options.boxWidth26_6)
        {
            if (const auto diagnostic = finalizeLine(); diagnostic.has_value())
            {
                output.diagnostic = *diagnostic;
                return output;
            }
            candidateX26_6 = entry.advanceX26_6;
        }

        PositionedGlyph positioned{};
        positioned.atlasEntry = entry;
        positioned.byteOffset = codepointOffset;
        positioned.lineIndex = impl_->scratchLines.size();
        positioned.penX26_6 = penX26_6;
        impl_->scratchGlyphs.push_back(positioned);
        penX26_6 = candidateX26_6;
        pendingFinalEmptyLine = false;
    }

    if (impl_->scratchGlyphs.size() != lineFirstGlyph || pendingFinalEmptyLine)
    {
        if (const auto diagnostic = finalizeLine(); diagnostic.has_value())
        {
            output.diagnostic = *diagnostic;
            return output;
        }
    }

    TextLayoutMetrics metrics{};
    metrics.glyphCount = impl_->scratchGlyphs.size();
    metrics.lineCount = impl_->scratchLines.size();
    for (const TextLayoutLine& line : impl_->scratchLines)
    {
        metrics.contentWidth26_6 = std::max(metrics.contentWidth26_6, line.advanceWidth26_6);
    }
    if (!CheckedMultiplyNonNegative(metrics.lineCount, lineHeight26_6, metrics.contentHeight26_6))
    {
        output.diagnostic = LayoutDiagnostic(
            TextLayoutErrorCode::MetricOverflow,
            "line count multiplied by line height overflowed int64");
        return output;
    }

    metrics.layoutWidth26_6 = options.boxWidth26_6 > 0 ? options.boxWidth26_6 : metrics.contentWidth26_6;
    metrics.layoutHeight26_6 = options.boxHeight26_6 > 0 ? options.boxHeight26_6 : metrics.contentHeight26_6;

    const std::int64_t verticalSlack = metrics.layoutHeight26_6 > metrics.contentHeight26_6
        ? metrics.layoutHeight26_6 - metrics.contentHeight26_6
        : 0;
    std::int64_t verticalOffset26_6 = 0;
    switch (options.verticalAlignment)
    {
    case TextVerticalAlignment::Top:
        break;
    case TextVerticalAlignment::Middle:
        verticalOffset26_6 = verticalSlack / 2;
        break;
    case TextVerticalAlignment::Bottom:
        verticalOffset26_6 = verticalSlack;
        break;
    }

    for (std::size_t lineIndex = 0U; lineIndex < impl_->scratchLines.size(); ++lineIndex)
    {
        TextLayoutLine& line = impl_->scratchLines[lineIndex];
        const std::int64_t horizontalSlack = metrics.layoutWidth26_6 > line.advanceWidth26_6
            ? metrics.layoutWidth26_6 - line.advanceWidth26_6
            : 0;
        switch (options.horizontalAlignment)
        {
        case TextHorizontalAlignment::Left:
            line.offsetX26_6 = 0;
            break;
        case TextHorizontalAlignment::Center:
            line.offsetX26_6 = horizontalSlack / 2;
            break;
        case TextHorizontalAlignment::Right:
            line.offsetX26_6 = horizontalSlack;
            break;
        }

        std::int64_t lineTop26_6 = 0;
        if (!CheckedMultiplyNonNegative(lineIndex, lineHeight26_6, lineTop26_6) ||
            !CheckedAdd(lineTop26_6, verticalOffset26_6, lineTop26_6) ||
            !CheckedAdd(lineTop26_6, baselineOffset26_6, line.baselineY26_6))
        {
            output.diagnostic = LayoutDiagnostic(
                TextLayoutErrorCode::MetricOverflow,
                "aligned line position overflowed int64");
            return output;
        }

        const std::size_t glyphEnd = line.firstGlyph + line.glyphCount;
        for (std::size_t glyphIndex = line.firstGlyph; glyphIndex < glyphEnd; ++glyphIndex)
        {
            PositionedGlyph& glyph = impl_->scratchGlyphs[glyphIndex];
            if (!CheckedAdd(glyph.penX26_6, line.offsetX26_6, glyph.penX26_6))
            {
                output.diagnostic = LayoutDiagnostic(
                    TextLayoutErrorCode::MetricOverflow,
                    "aligned glyph position overflowed int64",
                    glyph.byteOffset,
                    glyph.atlasEntry.codepoint);
                return output;
            }
            glyph.baselineY26_6 = line.baselineY26_6;
        }
    }

    impl_->glyphs.swap(impl_->scratchGlyphs);
    impl_->lines.swap(impl_->scratchLines);
    impl_->metrics = metrics;
    output.metrics = metrics;
    return output;
}

TextLayoutRunConfig TextLayoutRun::Config() const noexcept
{
    return impl_ != nullptr ? impl_->config : TextLayoutRunConfig{};
}

TextLayoutMetrics TextLayoutRun::Metrics() const noexcept
{
    return impl_ != nullptr ? impl_->metrics : TextLayoutMetrics{};
}

std::span<const PositionedGlyph> TextLayoutRun::Glyphs() const noexcept
{
    if (impl_ == nullptr)
    {
        return {};
    }
    return std::span<const PositionedGlyph>(impl_->glyphs.data(), impl_->glyphs.size());
}

std::span<const TextLayoutLine> TextLayoutRun::Lines() const noexcept
{
    if (impl_ == nullptr)
    {
        return {};
    }
    return std::span<const TextLayoutLine>(impl_->lines.data(), impl_->lines.size());
}

TextLayoutRunPrepareResult PrepareTextLayoutRun(const TextLayoutRunConfig config)
{
    TextLayoutRunPrepareResult output{};
    if (config.maxGlyphs == 0U || config.maxGlyphs > MaxLayoutGlyphs ||
        config.maxLines == 0U || config.maxLines > MaxLayoutLines)
    {
        output.diagnostic = LayoutDiagnostic(
            TextLayoutErrorCode::InvalidConfig,
            "text layout capacities exceed the supported bounded range");
        return output;
    }

    try
    {
        auto impl = std::make_unique<TextLayoutRun::Impl>();
        impl->config = config;
        impl->glyphs.reserve(config.maxGlyphs);
        impl->lines.reserve(config.maxLines);
        impl->scratchGlyphs.reserve(config.maxGlyphs);
        impl->scratchLines.reserve(config.maxLines);
        output.run = std::unique_ptr<TextLayoutRun>(new TextLayoutRun(std::move(impl)));
    }
    catch (const std::bad_alloc&)
    {
        output.diagnostic = LayoutDiagnostic(
            TextLayoutErrorCode::AllocationFailed,
            "failed to allocate bounded text layout storage");
    }
    return output;
}
} // namespace trace2d::text
