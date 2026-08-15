#pragma once

#include <trace2d/render/Renderer.hpp>
#include <trace2d/text/TextLayout.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

namespace trace2d::text
{
enum class TextPresentationError : std::uint8_t
{
    None = 0,
    InvalidAtlas,
    AtlasSizeOverflow,
    InsufficientRgbaCapacity,
    InvalidTextureHandle,
    TextureSizeMismatch,
    TextureColorSpaceMismatch,
    TextureAlphaModeMismatch,
    TexturePayloadMismatch,
    InvalidBindingChain,
    StaleAtlasBinding,
    InvalidPixelsPerUnit,
    InvalidOrigin,
    InvalidTint,
    InvalidOpacity,
    UnsupportedSampler,
    InvalidFontSlot,
    InvalidGlyphRegion,
    GeometryOverflow,
    StableOrderOverflow,
    InsufficientOutputCapacity,
};

[[nodiscard]] std::string_view ToString(TextPresentationError value) noexcept;

struct GlyphAtlasTextureBinding2D final
{
    const GlyphAtlas* atlas{nullptr};
    render::TextureHandle texture{render::InvalidTextureHandle};
    std::uint64_t pixelRevision{0U};
    std::uint32_t width{0U};
    std::uint32_t height{0U};

    [[nodiscard]] bool operator==(const GlyphAtlasTextureBinding2D&) const noexcept = default;
};

struct TextPresentationConfig2D final
{
    render::Float2 origin{};
    float pixelsPerUnit{32.0F};
    std::int32_t painterLayer{0};
    std::int32_t painterOrder{0};
    std::uint64_t stableOrderBase{0U};
    render::SpriteLinearRgba tint{};
    float opacity{1.0F};
    render::SpriteSamplerCompatibility sampler{render::SpriteSamplerCompatibility::Linear};

    [[nodiscard]] bool operator==(const TextPresentationConfig2D&) const noexcept = default;
};

struct TextPresentationMeasurement2D final
{
    std::uint64_t layoutGlyphs{0U};
    std::uint64_t zeroAreaGlyphs{0U};
    std::uint64_t emittedQuads{0U};
    std::uint64_t contiguousTextureRuns{0U};

    [[nodiscard]] bool operator==(const TextPresentationMeasurement2D&) const noexcept = default;
};

struct TextPresentationStatus final
{
    TextPresentationError error{TextPresentationError::None};
    std::size_t glyphIndex{0U};
    std::size_t fontSlot{0U};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == TextPresentationError::None;
    }

    [[nodiscard]] bool operator==(const TextPresentationStatus&) const noexcept = default;
};

// F1's committed non-empty bitmap-pixel count is monotonic and changes only when atlas pixels need
// a new GPU copy. Reuse it as the presentation pixel revision instead of a second mutation counter.
[[nodiscard]] std::uint64_t GlyphAtlasPixelRevision(const GlyphAtlas& atlas) noexcept;

// Explicit atlas-sync operation. Writes white RGB plus glyph coverage alpha for the existing Sprite
// straight-alpha pipeline. Allocation-free; insufficient capacity writes nothing.
[[nodiscard]] TextPresentationStatus WriteGlyphAtlasRgba8(
    const GlyphAtlas& atlas,
    std::span<std::uint8_t> output,
    std::size_t& outRequiredBytes) noexcept;

// Setup-time validation for an ordinary canonical TextureResource. Call before optionally releasing
// its CPU payload; steady presentation uses only the returned fixed-size binding.
[[nodiscard]] TextPresentationStatus ResolveGlyphAtlasTextureBinding2D(
    const GlyphAtlas& atlas,
    const assets::ResourceRegistry& resources,
    render::TextureHandle texture,
    GlyphAtlasTextureBinding2D& outBinding) noexcept;

// The fallbackAtlases span must be the same ordered chain used for the published layout. Bindings
// use the same slot order. A validation/count pass completes before any output is written.
[[nodiscard]] TextPresentationStatus BuildTextPresentation2D(
    const TextLayoutRun& layout,
    std::span<const TextFontAtlasRef> fallbackAtlases,
    std::span<const GlyphAtlasTextureBinding2D> bindings,
    const TextPresentationConfig2D& config,
    std::span<render::SpritePresentationRenderData> output,
    std::size_t& outRequiredCount,
    TextPresentationMeasurement2D& outMeasurement) noexcept;

static_assert(std::is_trivially_copyable_v<GlyphAtlasTextureBinding2D>);
static_assert(std::is_trivially_copyable_v<TextPresentationConfig2D>);
static_assert(std::is_trivially_copyable_v<TextPresentationMeasurement2D>);
static_assert(std::is_trivially_copyable_v<TextPresentationStatus>);
} // namespace trace2d::text
