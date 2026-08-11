#include <trace2d/render/SpriteAppearance2D.hpp>

#include <cmath>
#include <cstdint>

namespace trace2d::render
{
namespace
{
[[nodiscard]] SpriteAppearanceStatus Success() noexcept
{
    return SpriteAppearanceStatus{};
}

[[nodiscard]] SpriteAppearanceStatus Failure(
    const SpriteAppearanceError error,
    const SpriteAppearanceField field) noexcept
{
    return SpriteAppearanceStatus{error, field};
}

[[nodiscard]] bool IsUnitFinite(const float value) noexcept
{
    return std::isfinite(value) && value >= 0.0F && value <= 1.0F;
}

[[nodiscard]] SpriteAppearanceStatus ValidateTint(const SpriteLinearRgba& tint) noexcept
{
    if (!IsUnitFinite(tint.red))
    {
        return Failure(SpriteAppearanceError::InvalidTint, SpriteAppearanceField::TintRed);
    }
    if (!IsUnitFinite(tint.green))
    {
        return Failure(SpriteAppearanceError::InvalidTint, SpriteAppearanceField::TintGreen);
    }
    if (!IsUnitFinite(tint.blue))
    {
        return Failure(SpriteAppearanceError::InvalidTint, SpriteAppearanceField::TintBlue);
    }
    if (!IsUnitFinite(tint.alpha))
    {
        return Failure(SpriteAppearanceError::InvalidTint, SpriteAppearanceField::TintAlpha);
    }
    return Success();
}

[[nodiscard]] bool TryResolveSampler(
    const SpriteAppearanceSampling requested,
    const SpriteSamplerCompatibility inherited,
    SpriteSamplerCompatibility& outSampler) noexcept
{
    switch (requested)
    {
    case SpriteAppearanceSampling::InheritAsset:
        outSampler = inherited;
        return inherited == SpriteSamplerCompatibility::Nearest ||
            inherited == SpriteSamplerCompatibility::Linear;
    case SpriteAppearanceSampling::Nearest:
        outSampler = SpriteSamplerCompatibility::Nearest;
        return true;
    case SpriteAppearanceSampling::Linear:
        outSampler = SpriteSamplerCompatibility::Linear;
        return true;
    }
    return false;
}

[[nodiscard]] bool TryResolveBlend(
    const SpriteBlendMode requested,
    SpriteBlendCompatibility& outBlend) noexcept
{
    switch (requested)
    {
    case SpriteBlendMode::Normal:
        outBlend = SpriteBlendCompatibility::Normal;
        return true;
    case SpriteBlendMode::Additive:
        outBlend = SpriteBlendCompatibility::Additive;
        return true;
    case SpriteBlendMode::Multiply:
        outBlend = SpriteBlendCompatibility::Multiply;
        return true;
    case SpriteBlendMode::Screen:
        outBlend = SpriteBlendCompatibility::Screen;
        return true;
    }
    return false;
}
} // namespace

std::string_view ToString(const SpriteAppearanceSampling value) noexcept
{
    switch (value)
    {
    case SpriteAppearanceSampling::InheritAsset: return "inherit_asset";
    case SpriteAppearanceSampling::Nearest: return "nearest";
    case SpriteAppearanceSampling::Linear: return "linear";
    }
    return "unknown";
}

std::string_view ToString(const SpriteBlendMode value) noexcept
{
    switch (value)
    {
    case SpriteBlendMode::Normal: return "normal";
    case SpriteBlendMode::Additive: return "additive";
    case SpriteBlendMode::Multiply: return "multiply";
    case SpriteBlendMode::Screen: return "screen";
    }
    return "unknown";
}

std::string_view ToString(const SpriteTextureEncoding value) noexcept
{
    switch (value)
    {
    case SpriteTextureEncoding::Srgb: return "srgb";
    case SpriteTextureEncoding::Linear: return "linear";
    }
    return "unknown";
}

std::string_view ToString(const SpriteAppearanceError value) noexcept
{
    switch (value)
    {
    case SpriteAppearanceError::None: return "none";
    case SpriteAppearanceError::UnresolvedSelection: return "unresolved_selection";
    case SpriteAppearanceError::InvalidTint: return "invalid_tint";
    case SpriteAppearanceError::InvalidOpacity: return "invalid_opacity";
    case SpriteAppearanceError::UnsupportedSampling: return "unsupported_sampling";
    case SpriteAppearanceError::UnsupportedBlend: return "unsupported_blend";
    case SpriteAppearanceError::UnsupportedColorSpace: return "unsupported_color_space";
    case SpriteAppearanceError::UnsupportedAlphaMode: return "unsupported_alpha_mode";
    case SpriteAppearanceError::InvalidPageSize: return "invalid_page_size";
    case SpriteAppearanceError::InvalidPackedRect: return "invalid_packed_rect";
    case SpriteAppearanceError::SampleBoundsOverflow: return "sample_bounds_overflow";
    }
    return "unknown";
}

std::string_view ToString(const SpriteAppearanceField value) noexcept
{
    switch (value)
    {
    case SpriteAppearanceField::None: return "none";
    case SpriteAppearanceField::Selection: return "selection";
    case SpriteAppearanceField::TintRed: return "tint.red";
    case SpriteAppearanceField::TintGreen: return "tint.green";
    case SpriteAppearanceField::TintBlue: return "tint.blue";
    case SpriteAppearanceField::TintAlpha: return "tint.alpha";
    case SpriteAppearanceField::Opacity: return "opacity";
    case SpriteAppearanceField::Sampling: return "sampling";
    case SpriteAppearanceField::Blend: return "blend";
    case SpriteAppearanceField::PageColorSpace: return "page.color_space";
    case SpriteAppearanceField::PageAlphaMode: return "page.alpha_mode";
    case SpriteAppearanceField::PageSize: return "page.size";
    case SpriteAppearanceField::PackedRect: return "region.packed_rect";
    case SpriteAppearanceField::SampleBounds: return "sample_bounds";
    }
    return "unknown";
}

SpriteAppearanceStatus ExtractSpriteAppearanceContract(
    const ResolvedSpriteRegion& selection,
    const SpriteAppearance2D& appearance,
    SpriteAppearanceContractData& outData) noexcept
{
    outData = SpriteAppearanceContractData{};
    if (!selection.Valid() || selection.Page() == nullptr || selection.Region() == nullptr)
    {
        return Failure(
            SpriteAppearanceError::UnresolvedSelection,
            SpriteAppearanceField::Selection);
    }

    const SpriteAppearanceStatus tintStatus = ValidateTint(appearance.tint);
    if (!tintStatus.Succeeded())
    {
        return tintStatus;
    }
    if (!IsUnitFinite(appearance.opacity))
    {
        return Failure(
            SpriteAppearanceError::InvalidOpacity,
            SpriteAppearanceField::Opacity);
    }

    SpriteAppearanceContractData resolved{};
    resolved.tint = appearance.tint;
    resolved.opacity = appearance.opacity;

    if (!TryResolveSampler(appearance.sampling, selection.Sampler(), resolved.sampler))
    {
        return Failure(
            SpriteAppearanceError::UnsupportedSampling,
            SpriteAppearanceField::Sampling);
    }
    if (!TryResolveBlend(appearance.blend, resolved.blend))
    {
        return Failure(
            SpriteAppearanceError::UnsupportedBlend,
            SpriteAppearanceField::Blend);
    }

    const SpritePageResourceKey& pageResource = selection.PageResource();
    switch (pageResource.colorSpace)
    {
    case assets::SpriteColorSpace::Srgb:
        resolved.textureEncoding = SpriteTextureEncoding::Srgb;
        break;
    case assets::SpriteColorSpace::Linear:
        resolved.textureEncoding = SpriteTextureEncoding::Linear;
        break;
    default:
        return Failure(
            SpriteAppearanceError::UnsupportedColorSpace,
            SpriteAppearanceField::PageColorSpace);
    }

    if (pageResource.alphaMode != assets::SpriteAlphaMode::Straight)
    {
        return Failure(
            SpriteAppearanceError::UnsupportedAlphaMode,
            SpriteAppearanceField::PageAlphaMode);
    }
    resolved.sourceAlphaMode = pageResource.alphaMode;

    const std::uint32_t pageWidth = pageResource.size.width;
    const std::uint32_t pageHeight = pageResource.size.height;
    if (pageWidth == 0U || pageHeight == 0U)
    {
        return Failure(
            SpriteAppearanceError::InvalidPageSize,
            SpriteAppearanceField::PageSize);
    }

    const assets::SpritePixelRect& packed = selection.Region()->packedRect;
    if (packed.width == 0U || packed.height == 0U)
    {
        return Failure(
            SpriteAppearanceError::InvalidPackedRect,
            SpriteAppearanceField::PackedRect);
    }

    const std::uint64_t packedRight =
        static_cast<std::uint64_t>(packed.x) + static_cast<std::uint64_t>(packed.width);
    const std::uint64_t packedBottom =
        static_cast<std::uint64_t>(packed.y) + static_cast<std::uint64_t>(packed.height);
    if (packedRight > static_cast<std::uint64_t>(pageWidth) ||
        packedBottom > static_cast<std::uint64_t>(pageHeight))
    {
        return Failure(
            SpriteAppearanceError::InvalidPackedRect,
            SpriteAppearanceField::PackedRect);
    }

    const double width = static_cast<double>(pageWidth);
    const double height = static_cast<double>(pageHeight);
    const double minU = (static_cast<double>(packed.x) + 0.5) / width;
    const double maxU =
        (static_cast<double>(packed.x) + static_cast<double>(packed.width) - 0.5) / width;
    const double minV = (static_cast<double>(packed.y) + 0.5) / height;
    const double maxV =
        (static_cast<double>(packed.y) + static_cast<double>(packed.height) - 0.5) / height;

    resolved.sampleBounds = SpriteSampleBounds{
        Float2{static_cast<float>(minU), static_cast<float>(minV)},
        Float2{static_cast<float>(maxU), static_cast<float>(maxV)},
    };
    if (!std::isfinite(resolved.sampleBounds.minimum.x) ||
        !std::isfinite(resolved.sampleBounds.minimum.y) ||
        !std::isfinite(resolved.sampleBounds.maximum.x) ||
        !std::isfinite(resolved.sampleBounds.maximum.y))
    {
        return Failure(
            SpriteAppearanceError::SampleBoundsOverflow,
            SpriteAppearanceField::SampleBounds);
    }

    outData = resolved;
    return Success();
}

SpriteLinearRgba EvaluateSpritePremultipliedFragment(
    const SpriteLinearRgba& sampledStraightLinear,
    const SpriteAppearanceContractData& appearance) noexcept
{
    const float effectiveAlpha =
        sampledStraightLinear.alpha * appearance.tint.alpha * appearance.opacity;
    return SpriteLinearRgba{
        sampledStraightLinear.red * appearance.tint.red * effectiveAlpha,
        sampledStraightLinear.green * appearance.tint.green * effectiveAlpha,
        sampledStraightLinear.blue * appearance.tint.blue * effectiveAlpha,
        effectiveAlpha,
    };
}

bool TryEvaluateSpriteBlend(
    const SpriteLinearRgba& sourcePremultiplied,
    const SpriteLinearRgba& destination,
    const SpriteBlendCompatibility blend,
    SpriteLinearRgba& outColor) noexcept
{
    const float sourceAlpha = sourcePremultiplied.alpha;
    const float oneMinusSourceAlpha = 1.0F - sourceAlpha;

    SpriteLinearRgba result{};
    result.alpha = sourceAlpha + destination.alpha * oneMinusSourceAlpha;

    switch (blend)
    {
    case SpriteBlendCompatibility::Normal:
        result.red = sourcePremultiplied.red + destination.red * oneMinusSourceAlpha;
        result.green = sourcePremultiplied.green + destination.green * oneMinusSourceAlpha;
        result.blue = sourcePremultiplied.blue + destination.blue * oneMinusSourceAlpha;
        break;
    case SpriteBlendCompatibility::Additive:
        result.red = sourcePremultiplied.red + destination.red;
        result.green = sourcePremultiplied.green + destination.green;
        result.blue = sourcePremultiplied.blue + destination.blue;
        break;
    case SpriteBlendCompatibility::Multiply:
        result.red =
            sourcePremultiplied.red * destination.red + destination.red * oneMinusSourceAlpha;
        result.green =
            sourcePremultiplied.green * destination.green + destination.green * oneMinusSourceAlpha;
        result.blue =
            sourcePremultiplied.blue * destination.blue + destination.blue * oneMinusSourceAlpha;
        break;
    case SpriteBlendCompatibility::Screen:
        result.red =
            sourcePremultiplied.red + destination.red * (1.0F - sourcePremultiplied.red);
        result.green =
            sourcePremultiplied.green + destination.green * (1.0F - sourcePremultiplied.green);
        result.blue =
            sourcePremultiplied.blue + destination.blue * (1.0F - sourcePremultiplied.blue);
        break;
    default:
        outColor = SpriteLinearRgba{};
        return false;
    }

    outColor = result;
    return true;
}
} // namespace trace2d::render
