#pragma once

#include <trace2d/render/SpriteRenderContract.hpp>

#include <cstdint>
#include <string_view>
#include <type_traits>

namespace trace2d::render
{
enum class SpriteAppearanceSampling : std::uint8_t
{
    InheritAsset = 0,
    Nearest = 1,
    Linear = 2,
};

enum class SpriteBlendMode : std::uint8_t
{
    Normal = 0,
    Additive = 1,
    Multiply = 2,
    Screen = 3,
};

enum class SpriteTextureEncoding : std::uint8_t
{
    Srgb = 0,
    Linear = 1,
};

enum class SpriteAppearanceError : std::uint8_t
{
    None = 0,
    UnresolvedSelection,
    InvalidTint,
    InvalidOpacity,
    UnsupportedSampling,
    UnsupportedBlend,
    UnsupportedColorSpace,
    UnsupportedAlphaMode,
    InvalidPageSize,
    InvalidPackedRect,
    SampleBoundsOverflow,
};

enum class SpriteAppearanceField : std::uint8_t
{
    None = 0,
    Selection,
    TintRed,
    TintGreen,
    TintBlue,
    TintAlpha,
    Opacity,
    Sampling,
    Blend,
    PageColorSpace,
    PageAlphaMode,
    PageSize,
    PackedRect,
    SampleBounds,
};

[[nodiscard]] std::string_view ToString(SpriteAppearanceSampling value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteBlendMode value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteTextureEncoding value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteAppearanceError value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteAppearanceField value) noexcept;

struct SpriteLinearRgba final
{
    float red{1.0F};
    float green{1.0F};
    float blue{1.0F};
    float alpha{1.0F};

    [[nodiscard]] bool operator==(const SpriteLinearRgba&) const noexcept = default;
};

struct SpriteAppearance2D final
{
    SpriteLinearRgba tint{};
    float opacity{1.0F};
    SpriteAppearanceSampling sampling{SpriteAppearanceSampling::InheritAsset};
    SpriteBlendMode blend{SpriteBlendMode::Normal};

    [[nodiscard]] bool operator==(const SpriteAppearance2D&) const noexcept = default;
};

struct SpriteSampleBounds final
{
    Float2 minimum{};
    Float2 maximum{};

    [[nodiscard]] bool operator==(const SpriteSampleBounds&) const noexcept = default;
};

struct SpriteAppearanceStatus final
{
    SpriteAppearanceError error{SpriteAppearanceError::None};
    SpriteAppearanceField field{SpriteAppearanceField::None};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == SpriteAppearanceError::None;
    }

    [[nodiscard]] bool operator==(const SpriteAppearanceStatus&) const noexcept = default;
};

// Fixed-size, backend-independent presentation state resolved from canonical Sprite metadata
// plus authoritative runtime appearance intent. Canonical source alpha remains straight; the
// renderer/shader derives premultiplied fragment RGB immediately before fixed-function blending.
struct SpriteAppearanceContractData final
{
    SpriteLinearRgba tint{};
    float opacity{1.0F};
    SpriteSamplerCompatibility sampler{SpriteSamplerCompatibility::Nearest};
    SpriteBlendCompatibility blend{SpriteBlendCompatibility::Normal};
    SpriteTextureEncoding textureEncoding{SpriteTextureEncoding::Srgb};
    assets::SpriteAlphaMode sourceAlphaMode{assets::SpriteAlphaMode::Straight};
    SpriteSampleBounds sampleBounds{};

    [[nodiscard]] bool operator==(const SpriteAppearanceContractData&) const noexcept = default;
};

// O(1), fixed-size caller-owned extraction. Performs no allocation, semantic lookup, filesystem,
// image decode, renderer initialization, or GPU work. SR2 canonical pixel-edge UVs are not changed;
// sampleBounds are a separate texel-center clamp used only at sampling time.
[[nodiscard]] SpriteAppearanceStatus ExtractSpriteAppearanceContract(
    const ResolvedSpriteRegion& selection,
    const SpriteAppearance2D& appearance,
    SpriteAppearanceContractData& outData) noexcept;

// CPU oracle for the exact SR3 fragment boundary. sampledStraightLinear is already decoded into
// linear RGB. The returned RGB is premultiplied by the effective alpha.
[[nodiscard]] SpriteLinearRgba EvaluateSpritePremultipliedFragment(
    const SpriteLinearRgba& sampledStraightLinear,
    const SpriteAppearanceContractData& appearance) noexcept;

// CPU oracle for the four built-in fixed-function blend equations. sourcePremultiplied must use
// the SR3 premultiplied fragment convention. Returns false only for an unknown blend enum.
[[nodiscard]] bool TryEvaluateSpriteBlend(
    const SpriteLinearRgba& sourcePremultiplied,
    const SpriteLinearRgba& destination,
    SpriteBlendCompatibility blend,
    SpriteLinearRgba& outColor) noexcept;

static_assert(std::is_trivially_copyable_v<SpriteLinearRgba>);
static_assert(std::is_trivially_copyable_v<SpriteAppearance2D>);
static_assert(std::is_trivially_copyable_v<SpriteSampleBounds>);
static_assert(std::is_trivially_copyable_v<SpriteAppearanceStatus>);
static_assert(std::is_trivially_copyable_v<SpriteAppearanceContractData>);
} // namespace trace2d::render
