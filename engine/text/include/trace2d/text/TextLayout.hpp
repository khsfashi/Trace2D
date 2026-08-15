#pragma once

#include <trace2d/text/Text.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace trace2d::text
{
enum class TextWrapMode : std::uint8_t
{
    None = 0,
    GlyphBoundary,
};

enum class TextHorizontalAlignment : std::uint8_t
{
    Left = 0,
    Center,
    Right,
};

enum class TextVerticalAlignment : std::uint8_t
{
    Top = 0,
    Middle,
    Bottom,
};

enum class TextLayoutErrorCode : std::uint8_t
{
    InvalidConfig = 0,
    InvalidUtf8,
    GlyphResolveFailed,
    GlyphCapacityExceeded,
    LineCapacityExceeded,
    MetricOverflow,
    AllocationFailed,
};

[[nodiscard]] std::string_view ToString(TextLayoutErrorCode value) noexcept;

struct TextLayoutDiagnostic final
{
    TextLayoutErrorCode code{TextLayoutErrorCode::InvalidConfig};
    std::string message{};
    std::size_t byteOffset{0};
    char32_t codepoint{0};
    std::optional<TextDiagnostic> textDiagnostic{};
};

struct TextLayoutRunConfig final
{
    std::size_t maxGlyphs{4096U};
    std::size_t maxLines{256U};
};

struct TextLayoutOptions final
{
    // Zero width means unbounded. GlyphBoundary wrapping breaks only between decoded
    // Unicode scalar values; language-specific word breaking remains intentionally deferred.
    std::int64_t boxWidth26_6{0};
    // Zero height means the measured content height.
    std::int64_t boxHeight26_6{0};
    // Zero selects atlas.pixelHeight * 64. Callers that already own exact font metrics
    // may provide the FreeType-derived line height and ascender from F0 measurement.
    std::int64_t lineHeight26_6{0};
    std::int64_t baselineOffset26_6{0};
    TextWrapMode wrapMode{TextWrapMode::GlyphBoundary};
    TextHorizontalAlignment horizontalAlignment{TextHorizontalAlignment::Left};
    TextVerticalAlignment verticalAlignment{TextVerticalAlignment::Top};
};

// Non-owning ordered fallback entry. The caller owns each atlas and the span storage for the
// duration of LayoutUtf8. Earlier entries have strictly higher fallback priority.
struct TextFontAtlasRef final
{
    GlyphAtlas* atlas{nullptr};
};

struct PositionedGlyph final
{
    GlyphAtlasEntry atlasEntry{};
    // Index into the exact fallback span passed to LayoutUtf8. Single-atlas layout always uses 0.
    std::size_t fontSlot{0};
    std::size_t byteOffset{0};
    std::size_t lineIndex{0};
    std::int64_t penX26_6{0};
    std::int64_t baselineY26_6{0};
};

struct TextLayoutLine final
{
    std::size_t firstGlyph{0};
    std::size_t glyphCount{0};
    std::int64_t advanceWidth26_6{0};
    std::int64_t offsetX26_6{0};
    std::int64_t baselineY26_6{0};
};

struct TextLayoutMetrics final
{
    std::size_t glyphCount{0};
    std::size_t lineCount{0};
    std::int64_t contentWidth26_6{0};
    std::int64_t contentHeight26_6{0};
    std::int64_t layoutWidth26_6{0};
    std::int64_t layoutHeight26_6{0};
};

struct TextLayoutResult final
{
    std::optional<TextLayoutMetrics> metrics{};
    std::optional<TextLayoutDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return metrics.has_value() && !diagnostic.has_value();
    }
};

struct TextLayoutRunPrepareResult;

// TextLayoutRun owns two fixed-capacity vector sets: the currently published result and a
// scratch result. LayoutUtf8 writes scratch state and swaps only on success, so a failed layout
// never publishes a partial run. Within declared capacities the vectors do not grow after prepare.
class TextLayoutRun final
{
public:
    TextLayoutRun(const TextLayoutRun&) = delete;
    TextLayoutRun& operator=(const TextLayoutRun&) = delete;
    TextLayoutRun(TextLayoutRun&&) noexcept;
    TextLayoutRun& operator=(TextLayoutRun&&) noexcept;
    ~TextLayoutRun();

    // Source-compatible F2 convenience path. Equivalent to a one-entry fallback chain.
    [[nodiscard]] TextLayoutResult LayoutUtf8(
        GlyphAtlas& atlas,
        std::string_view text,
        TextLayoutOptions options = {});

    // Deterministic F3 path: first supporting prepared atlas wins for each Unicode scalar.
    [[nodiscard]] TextLayoutResult LayoutUtf8(
        std::span<const TextFontAtlasRef> fallbackAtlases,
        std::string_view text,
        TextLayoutOptions options = {});

    [[nodiscard]] TextLayoutRunConfig Config() const noexcept;
    [[nodiscard]] TextLayoutMetrics Metrics() const noexcept;
    [[nodiscard]] std::span<const PositionedGlyph> Glyphs() const noexcept;
    [[nodiscard]] std::span<const TextLayoutLine> Lines() const noexcept;

private:
    struct Impl;
    explicit TextLayoutRun(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_{};

    friend TextLayoutRunPrepareResult PrepareTextLayoutRun(TextLayoutRunConfig config);
};

struct TextLayoutRunPrepareResult final
{
    std::unique_ptr<TextLayoutRun> run{};
    std::optional<TextLayoutDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return run != nullptr && !diagnostic.has_value();
    }
};

[[nodiscard]] TextLayoutRunPrepareResult PrepareTextLayoutRun(TextLayoutRunConfig config = {});
} // namespace trace2d::text
