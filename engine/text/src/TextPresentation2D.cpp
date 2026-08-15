#include <trace2d/text/TextPresentation2D.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace trace2d::text
{
namespace
{
constexpr double Fixed26_6Scale = 64.0;

[[nodiscard]] bool IsFiniteUnit(const float value) noexcept
{
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

[[nodiscard]] bool IsValidSampler(const render::SpriteSamplerCompatibility sampler) noexcept
{
    switch (sampler)
    {
    case render::SpriteSamplerCompatibility::Nearest:
    case render::SpriteSamplerCompatibility::Linear:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidTextureHandle(const render::TextureHandle texture) noexcept
{
    return texture.generation != 0U &&
        texture.domain == assets::ResourceTypeDomain::Texture;
}

[[nodiscard]] bool TryAtlasSizes(
    const GlyphAtlasConfig config,
    std::size_t& outPixelCount,
    std::size_t& outRgbaBytes) noexcept
{
    outPixelCount = 0U;
    outRgbaBytes = 0U;
    if (config.width == 0U || config.height == 0U)
    {
        return false;
    }

    const std::size_t width = static_cast<std::size_t>(config.width);
    const std::size_t height = static_cast<std::size_t>(config.height);
    if (height > std::numeric_limits<std::size_t>::max() / width)
    {
        return false;
    }
    outPixelCount = width * height;
    constexpr std::size_t Channels = 4U;
    if (outPixelCount > std::numeric_limits<std::size_t>::max() / Channels)
    {
        outPixelCount = 0U;
        return false;
    }
    outRgbaBytes = outPixelCount * Channels;
    return true;
}

[[nodiscard]] TextPresentationStatus ValidateBindingChain(
    const std::span<const TextFontAtlasRef> fallbackAtlases,
    const std::span<const GlyphAtlasTextureBinding2D> bindings) noexcept
{
    if (fallbackAtlases.empty() || fallbackAtlases.size() != bindings.size())
    {
        return TextPresentationStatus{TextPresentationError::InvalidBindingChain, 0U, 0U};
    }

    for (std::size_t fontSlot = 0U; fontSlot < fallbackAtlases.size(); ++fontSlot)
    {
        const GlyphAtlas* const atlas = fallbackAtlases[fontSlot].atlas;
        const GlyphAtlasTextureBinding2D& binding = bindings[fontSlot];
        if (atlas == nullptr || !atlas->IsPrepared() || binding.atlas != atlas ||
            !IsValidTextureHandle(binding.texture))
        {
            return TextPresentationStatus{TextPresentationError::InvalidBindingChain, 0U, fontSlot};
        }

        const GlyphAtlasConfig atlasConfig = atlas->Config();
        if (binding.width != atlasConfig.width || binding.height != atlasConfig.height)
        {
            return TextPresentationStatus{TextPresentationError::InvalidBindingChain, 0U, fontSlot};
        }
        if (binding.pixelRevision != GlyphAtlasPixelRevision(*atlas))
        {
            return TextPresentationStatus{TextPresentationError::StaleAtlasBinding, 0U, fontSlot};
        }
    }
    return {};
}

[[nodiscard]] TextPresentationStatus ValidatePresentationConfig(
    const TextPresentationConfig2D& config) noexcept
{
    if (!std::isfinite(config.pixelsPerUnit) || config.pixelsPerUnit <= 0.0F)
    {
        return TextPresentationStatus{TextPresentationError::InvalidPixelsPerUnit, 0U, 0U};
    }
    if (!std::isfinite(config.origin.x) || !std::isfinite(config.origin.y))
    {
        return TextPresentationStatus{TextPresentationError::InvalidOrigin, 0U, 0U};
    }
    if (!IsFiniteUnit(config.tint.red) || !IsFiniteUnit(config.tint.green) ||
        !IsFiniteUnit(config.tint.blue) || !IsFiniteUnit(config.tint.alpha))
    {
        return TextPresentationStatus{TextPresentationError::InvalidTint, 0U, 0U};
    }
    if (!IsFiniteUnit(config.opacity))
    {
        return TextPresentationStatus{TextPresentationError::InvalidOpacity, 0U, 0U};
    }
    if (!IsValidSampler(config.sampler))
    {
        return TextPresentationStatus{TextPresentationError::UnsupportedSampler, 0U, 0U};
    }
    return {};
}

[[nodiscard]] TextPresentationStatus BuildGlyphPresentation(
    const PositionedGlyph& glyph,
    const GlyphAtlasTextureBinding2D& binding,
    const TextPresentationConfig2D& config,
    const std::size_t glyphIndex,
    render::SpritePresentationRenderData& outData) noexcept
{
    outData = {};
    const GlyphAtlasEntry& entry = glyph.atlasEntry;
    const std::uint64_t rightPixel =
        static_cast<std::uint64_t>(entry.x) + static_cast<std::uint64_t>(entry.width);
    const std::uint64_t bottomPixel =
        static_cast<std::uint64_t>(entry.y) + static_cast<std::uint64_t>(entry.height);
    if (rightPixel > binding.width || bottomPixel > binding.height)
    {
        return TextPresentationStatus{TextPresentationError::InvalidGlyphRegion, glyphIndex, glyph.fontSlot};
    }

    if (glyphIndex >= static_cast<std::size_t>(render::InvalidSpriteStableOrder) ||
        config.stableOrderBase >
            render::InvalidSpriteStableOrder - 1U - static_cast<std::uint64_t>(glyphIndex))
    {
        return TextPresentationStatus{TextPresentationError::StableOrderOverflow, glyphIndex, glyph.fontSlot};
    }
    const std::uint64_t stableOrder =
        config.stableOrderBase + static_cast<std::uint64_t>(glyphIndex);

    const double leftPixel =
        static_cast<double>(glyph.penX26_6) / Fixed26_6Scale +
        static_cast<double>(entry.bearingX);
    const double topPixel =
        static_cast<double>(glyph.baselineY26_6) / Fixed26_6Scale -
        static_cast<double>(entry.bearingY);
    const double rightLayoutPixel = leftPixel + static_cast<double>(entry.width);
    const double bottomLayoutPixel = topPixel + static_cast<double>(entry.height);
    const double inversePixelsPerUnit = 1.0 / static_cast<double>(config.pixelsPerUnit);
    const double left = static_cast<double>(config.origin.x) + leftPixel * inversePixelsPerUnit;
    const double top = static_cast<double>(config.origin.y) + topPixel * inversePixelsPerUnit;
    const double right = static_cast<double>(config.origin.x) + rightLayoutPixel * inversePixelsPerUnit;
    const double bottom = static_cast<double>(config.origin.y) + bottomLayoutPixel * inversePixelsPerUnit;
    const double maxFloat = static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(left) || !std::isfinite(top) || !std::isfinite(right) ||
        !std::isfinite(bottom) || std::abs(left) > maxFloat || std::abs(top) > maxFloat ||
        std::abs(right) > maxFloat || std::abs(bottom) > maxFloat)
    {
        return TextPresentationStatus{TextPresentationError::GeometryOverflow, glyphIndex, glyph.fontSlot};
    }

    const float inverseWidth = 1.0F / static_cast<float>(binding.width);
    const float inverseHeight = 1.0F / static_cast<float>(binding.height);
    const float u0 = static_cast<float>(entry.x) * inverseWidth;
    const float v0 = static_cast<float>(entry.y) * inverseHeight;
    const float u1 = static_cast<float>(rightPixel) * inverseWidth;
    const float v1 = static_cast<float>(bottomPixel) * inverseHeight;

    render::SpriteDrawQuad quad{};
    quad.topLeft = render::SpriteDrawVertex{{static_cast<float>(left), static_cast<float>(top)}, {u0, v0}};
    quad.topRight = render::SpriteDrawVertex{{static_cast<float>(right), static_cast<float>(top)}, {u1, v0}};
    quad.bottomRight = render::SpriteDrawVertex{{static_cast<float>(right), static_cast<float>(bottom)}, {u1, v1}};
    quad.bottomLeft = render::SpriteDrawVertex{{static_cast<float>(left), static_cast<float>(bottom)}, {u0, v1}};

    render::SpriteAppearanceContractData appearance{};
    appearance.tint = config.tint;
    appearance.opacity = config.opacity;
    appearance.sampler = config.sampler;
    appearance.blend = render::SpriteBlendCompatibility::Normal;
    appearance.textureEncoding = render::SpriteTextureEncoding::Linear;
    appearance.sourceAlphaMode = assets::SpriteAlphaMode::Straight;
    appearance.sampleBounds = render::SpriteSampleBounds{
        render::Float2{
            (static_cast<float>(entry.x) + 0.5F) * inverseWidth,
            (static_cast<float>(entry.y) + 0.5F) * inverseHeight,
        },
        render::Float2{
            (static_cast<float>(rightPixel) - 0.5F) * inverseWidth,
            (static_cast<float>(bottomPixel) - 0.5F) * inverseHeight,
        },
    };

    outData.presentation = render::SpritePresentation2D{quad, appearance};
    outData.texture = binding.texture;
    outData.order = render::SpriteOrder2D{
        config.painterLayer,
        config.painterOrder,
        stableOrder,
        {},
    };
    outData.mask = {};
    outData.geometryKind = render::SpritePresentationGeometryKind::Quad;
    outData.primitivePatches = {};
    outData.pixelPerfectViewport = nullptr;
    outData.materialPipeline = render::BuiltInSpriteMaterialPipelineIdentity;
    return {};
}

[[nodiscard]] TextPresentationStatus RunPresentationPass(
    const TextLayoutRun& layout,
    const std::span<const GlyphAtlasTextureBinding2D> bindings,
    const TextPresentationConfig2D& config,
    const std::span<render::SpritePresentationRenderData> output,
    const bool writeOutput,
    std::size_t& outCount,
    TextPresentationMeasurement2D* const measurement) noexcept
{
    outCount = 0U;
    bool hasPreviousTexture = false;
    render::TextureHandle previousTexture{};
    const std::span<const PositionedGlyph> glyphs = layout.Glyphs();

    for (std::size_t glyphIndex = 0U; glyphIndex < glyphs.size(); ++glyphIndex)
    {
        const PositionedGlyph& glyph = glyphs[glyphIndex];
        if (measurement != nullptr)
        {
            ++measurement->layoutGlyphs;
        }
        if (glyph.fontSlot >= bindings.size())
        {
            return TextPresentationStatus{TextPresentationError::InvalidFontSlot, glyphIndex, glyph.fontSlot};
        }

        const bool widthZero = glyph.atlasEntry.width == 0U;
        const bool heightZero = glyph.atlasEntry.height == 0U;
        if (widthZero != heightZero)
        {
            return TextPresentationStatus{TextPresentationError::InvalidGlyphRegion, glyphIndex, glyph.fontSlot};
        }
        if (widthZero)
        {
            if (measurement != nullptr)
            {
                ++measurement->zeroAreaGlyphs;
            }
            continue;
        }

        render::SpritePresentationRenderData data{};
        const TextPresentationStatus built =
            BuildGlyphPresentation(glyph, bindings[glyph.fontSlot], config, glyphIndex, data);
        if (!built.Succeeded())
        {
            return built;
        }

        if (measurement != nullptr &&
            (!hasPreviousTexture || previousTexture != data.texture))
        {
            ++measurement->contiguousTextureRuns;
        }
        previousTexture = data.texture;
        hasPreviousTexture = true;

        if (outCount == std::numeric_limits<std::size_t>::max())
        {
            return TextPresentationStatus{TextPresentationError::InsufficientOutputCapacity, glyphIndex, glyph.fontSlot};
        }
        if (writeOutput)
        {
            output[outCount] = data;
        }
        ++outCount;
    }

    if (measurement != nullptr)
    {
        measurement->emittedQuads = static_cast<std::uint64_t>(outCount);
    }
    return {};
}
} // namespace

std::string_view ToString(const TextPresentationError value) noexcept
{
    switch (value)
    {
    case TextPresentationError::None:
        return "none";
    case TextPresentationError::InvalidAtlas:
        return "invalid_atlas";
    case TextPresentationError::AtlasSizeOverflow:
        return "atlas_size_overflow";
    case TextPresentationError::InsufficientRgbaCapacity:
        return "insufficient_rgba_capacity";
    case TextPresentationError::InvalidTextureHandle:
        return "invalid_texture_handle";
    case TextPresentationError::TextureSizeMismatch:
        return "texture_size_mismatch";
    case TextPresentationError::TextureColorSpaceMismatch:
        return "texture_color_space_mismatch";
    case TextPresentationError::TextureAlphaModeMismatch:
        return "texture_alpha_mode_mismatch";
    case TextPresentationError::TexturePayloadMismatch:
        return "texture_payload_mismatch";
    case TextPresentationError::InvalidBindingChain:
        return "invalid_binding_chain";
    case TextPresentationError::StaleAtlasBinding:
        return "stale_atlas_binding";
    case TextPresentationError::InvalidPixelsPerUnit:
        return "invalid_pixels_per_unit";
    case TextPresentationError::InvalidOrigin:
        return "invalid_origin";
    case TextPresentationError::InvalidTint:
        return "invalid_tint";
    case TextPresentationError::InvalidOpacity:
        return "invalid_opacity";
    case TextPresentationError::UnsupportedSampler:
        return "unsupported_sampler";
    case TextPresentationError::InvalidFontSlot:
        return "invalid_font_slot";
    case TextPresentationError::InvalidGlyphRegion:
        return "invalid_glyph_region";
    case TextPresentationError::GeometryOverflow:
        return "geometry_overflow";
    case TextPresentationError::StableOrderOverflow:
        return "stable_order_overflow";
    case TextPresentationError::InsufficientOutputCapacity:
        return "insufficient_output_capacity";
    }
    return "unknown";
}

std::uint64_t GlyphAtlasPixelRevision(const GlyphAtlas& atlas) noexcept
{
    return atlas.Metrics().occupiedBitmapPixels;
}

TextPresentationStatus WriteGlyphAtlasRgba8(
    const GlyphAtlas& atlas,
    const std::span<std::uint8_t> output,
    std::size_t& outRequiredBytes) noexcept
{
    outRequiredBytes = 0U;
    if (!atlas.IsPrepared())
    {
        return TextPresentationStatus{TextPresentationError::InvalidAtlas, 0U, 0U};
    }

    std::size_t pixelCount = 0U;
    if (!TryAtlasSizes(atlas.Config(), pixelCount, outRequiredBytes))
    {
        return TextPresentationStatus{TextPresentationError::AtlasSizeOverflow, 0U, 0U};
    }
    const std::span<const std::uint8_t> alpha = atlas.Alpha8();
    if (alpha.size() != pixelCount)
    {
        return TextPresentationStatus{TextPresentationError::InvalidAtlas, 0U, 0U};
    }
    if (output.size() < outRequiredBytes)
    {
        return TextPresentationStatus{TextPresentationError::InsufficientRgbaCapacity, 0U, 0U};
    }

    for (std::size_t pixel = 0U; pixel < pixelCount; ++pixel)
    {
        const std::size_t destination = pixel * 4U;
        output[destination] = 255U;
        output[destination + 1U] = 255U;
        output[destination + 2U] = 255U;
        output[destination + 3U] = alpha[pixel];
    }
    return {};
}

TextPresentationStatus ResolveGlyphAtlasTextureBinding2D(
    const GlyphAtlas& atlas,
    const assets::ResourceRegistry& resources,
    const render::TextureHandle texture,
    GlyphAtlasTextureBinding2D& outBinding) noexcept
{
    outBinding = {};
    if (!atlas.IsPrepared())
    {
        return TextPresentationStatus{TextPresentationError::InvalidAtlas, 0U, 0U};
    }
    if (!IsValidTextureHandle(texture))
    {
        return TextPresentationStatus{TextPresentationError::InvalidTextureHandle, 0U, 0U};
    }

    const assets::TextureResource* const resource = resources.Resolve(texture);
    if (resource == nullptr)
    {
        return TextPresentationStatus{TextPresentationError::InvalidTextureHandle, 0U, 0U};
    }

    const GlyphAtlasConfig config = atlas.Config();
    if (resource->width != config.width || resource->height != config.height)
    {
        return TextPresentationStatus{TextPresentationError::TextureSizeMismatch, 0U, 0U};
    }
    if (resource->colorSpace != assets::TextureResourceColorSpace::Linear)
    {
        return TextPresentationStatus{TextPresentationError::TextureColorSpaceMismatch, 0U, 0U};
    }
    if (resource->alphaMode != assets::TextureResourceAlphaMode::Straight)
    {
        return TextPresentationStatus{TextPresentationError::TextureAlphaModeMismatch, 0U, 0U};
    }

    std::size_t pixelCount = 0U;
    std::size_t expectedBytes = 0U;
    if (!TryAtlasSizes(config, pixelCount, expectedBytes))
    {
        return TextPresentationStatus{TextPresentationError::AtlasSizeOverflow, 0U, 0U};
    }
    const std::span<const std::uint8_t> alpha = atlas.Alpha8();
    if (alpha.size() != pixelCount || resource->canonicalRgba8.size() != expectedBytes)
    {
        return TextPresentationStatus{TextPresentationError::TexturePayloadMismatch, 0U, 0U};
    }

    for (std::size_t pixel = 0U; pixel < pixelCount; ++pixel)
    {
        const std::size_t source = pixel * 4U;
        if (resource->canonicalRgba8[source] != 255U ||
            resource->canonicalRgba8[source + 1U] != 255U ||
            resource->canonicalRgba8[source + 2U] != 255U ||
            resource->canonicalRgba8[source + 3U] != alpha[pixel])
        {
            return TextPresentationStatus{TextPresentationError::TexturePayloadMismatch, 0U, 0U};
        }
    }

    outBinding.atlas = &atlas;
    outBinding.texture = texture;
    outBinding.pixelRevision = GlyphAtlasPixelRevision(atlas);
    outBinding.width = config.width;
    outBinding.height = config.height;
    return {};
}

TextPresentationStatus BuildTextPresentation2D(
    const TextLayoutRun& layout,
    const std::span<const TextFontAtlasRef> fallbackAtlases,
    const std::span<const GlyphAtlasTextureBinding2D> bindings,
    const TextPresentationConfig2D& config,
    const std::span<render::SpritePresentationRenderData> output,
    std::size_t& outRequiredCount,
    TextPresentationMeasurement2D& outMeasurement) noexcept
{
    outRequiredCount = 0U;
    outMeasurement = {};

    if (const TextPresentationStatus chain = ValidateBindingChain(fallbackAtlases, bindings);
        !chain.Succeeded())
    {
        return chain;
    }
    if (const TextPresentationStatus configStatus = ValidatePresentationConfig(config);
        !configStatus.Succeeded())
    {
        return configStatus;
    }

    const TextPresentationStatus measured = RunPresentationPass(
        layout,
        bindings,
        config,
        {},
        false,
        outRequiredCount,
        &outMeasurement);
    if (!measured.Succeeded())
    {
        return measured;
    }
    if (output.size() < outRequiredCount)
    {
        return TextPresentationStatus{TextPresentationError::InsufficientOutputCapacity, 0U, 0U};
    }

    std::size_t written = 0U;
    const TextPresentationStatus built = RunPresentationPass(
        layout,
        bindings,
        config,
        output,
        true,
        written,
        nullptr);
    if (!built.Succeeded())
    {
        return built;
    }
    return {};
}
} // namespace trace2d::text
