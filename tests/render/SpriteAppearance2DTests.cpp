#include <trace2d/render/SpriteAppearance2D.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>

namespace trace2d::render
{
namespace
{
assets::SpriteAsset MakeSpriteAsset(
    const assets::SpriteSampling sampling = assets::SpriteSampling::Nearest,
    const assets::SpriteColorSpace colorSpace = assets::SpriteColorSpace::Srgb,
    const assets::SpritePackedRotation rotation = assets::SpritePackedRotation::None)
{
    assets::SpriteAsset asset{};
    asset.id = "sprites/test.sprite.toml";
    asset.sampling = sampling;
    asset.pages = {
        assets::SpriteAtlasPage{
            "page",
            "textures/test.png",
            assets::SpritePixelSize{64U, 32U},
            colorSpace,
            assets::SpriteAlphaMode::Straight,
        },
    };
    asset.regions = {
        assets::SpriteRegion{
            "region",
            "page",
            assets::SpritePixelSize{16U, 8U},
            assets::SpritePixelOffset{0U, 0U},
            assets::SpritePixelSize{16U, 8U},
            rotation == assets::SpritePackedRotation::None
                ? assets::SpritePixelRect{4U, 6U, 16U, 8U}
                : assets::SpritePixelRect{4U, 6U, 8U, 16U},
            assets::SpriteRationalPivot{0, 0, 1},
            rotation,
        },
    };
    return asset;
}

ResolvedSpriteRegion ResolveSingle(const assets::SpriteAsset& asset)
{
    ResolvedSpriteRegion selection{};
    EXPECT_TRUE(ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection).Succeeded());
    return selection;
}

TEST(SpriteAppearance2DTests, DefaultsPreserveWhiteTintOpacityAndAssetSampling)
{
    const assets::SpriteAsset asset = MakeSpriteAsset(assets::SpriteSampling::Linear);
    const ResolvedSpriteRegion selection = ResolveSingle(asset);

    SpriteAppearanceContractData data{};
    ASSERT_TRUE(ExtractSpriteAppearanceContract(selection, SpriteAppearance2D{}, data).Succeeded());

    EXPECT_EQ(data.tint, (SpriteLinearRgba{}));
    EXPECT_FLOAT_EQ(data.opacity, 1.0F);
    EXPECT_EQ(data.sampler, SpriteSamplerCompatibility::Linear);
    EXPECT_EQ(data.blend, SpriteBlendCompatibility::Normal);
    EXPECT_EQ(data.textureEncoding, SpriteTextureEncoding::Srgb);
    EXPECT_EQ(data.sourceAlphaMode, assets::SpriteAlphaMode::Straight);
}

TEST(SpriteAppearance2DTests, ExplicitSamplingOverridesAssetWithoutMutation)
{
    const assets::SpriteAsset asset = MakeSpriteAsset(assets::SpriteSampling::Nearest);
    const ResolvedSpriteRegion selection = ResolveSingle(asset);

    SpriteAppearance2D appearance{};
    appearance.sampling = SpriteAppearanceSampling::Linear;
    SpriteAppearanceContractData data{};
    ASSERT_TRUE(ExtractSpriteAppearanceContract(selection, appearance, data).Succeeded());

    EXPECT_EQ(data.sampler, SpriteSamplerCompatibility::Linear);
    EXPECT_EQ(asset.sampling, assets::SpriteSampling::Nearest);
    EXPECT_EQ(selection.Sampler(), SpriteSamplerCompatibility::Nearest);
}

TEST(SpriteAppearance2DTests, InvalidTintAndOpacityFailStructurally)
{
    const assets::SpriteAsset asset = MakeSpriteAsset();
    const ResolvedSpriteRegion selection = ResolveSingle(asset);
    SpriteAppearanceContractData data{};

    SpriteAppearance2D appearance{};
    appearance.tint.red = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(
        ExtractSpriteAppearanceContract(selection, appearance, data),
        (SpriteAppearanceStatus{SpriteAppearanceError::InvalidTint, SpriteAppearanceField::TintRed}));

    appearance = SpriteAppearance2D{};
    appearance.tint.green = 1.01F;
    EXPECT_EQ(
        ExtractSpriteAppearanceContract(selection, appearance, data),
        (SpriteAppearanceStatus{SpriteAppearanceError::InvalidTint, SpriteAppearanceField::TintGreen}));

    appearance = SpriteAppearance2D{};
    appearance.opacity = -0.01F;
    EXPECT_EQ(
        ExtractSpriteAppearanceContract(selection, appearance, data),
        (SpriteAppearanceStatus{SpriteAppearanceError::InvalidOpacity, SpriteAppearanceField::Opacity}));

    appearance.opacity = std::numeric_limits<float>::infinity();
    EXPECT_EQ(
        ExtractSpriteAppearanceContract(selection, appearance, data),
        (SpriteAppearanceStatus{SpriteAppearanceError::InvalidOpacity, SpriteAppearanceField::Opacity}));
}

TEST(SpriteAppearance2DTests, UnknownFiniteEnumsFailWithoutPartialOutput)
{
    const assets::SpriteAsset asset = MakeSpriteAsset();
    const ResolvedSpriteRegion selection = ResolveSingle(asset);
    SpriteAppearanceContractData data{};
    data.opacity = 0.25F;

    SpriteAppearance2D appearance{};
    appearance.sampling = static_cast<SpriteAppearanceSampling>(255U);
    EXPECT_EQ(
        ExtractSpriteAppearanceContract(selection, appearance, data),
        (SpriteAppearanceStatus{
            SpriteAppearanceError::UnsupportedSampling,
            SpriteAppearanceField::Sampling}));
    EXPECT_EQ(data, SpriteAppearanceContractData{});

    appearance = SpriteAppearance2D{};
    appearance.blend = static_cast<SpriteBlendMode>(255U);
    EXPECT_EQ(
        ExtractSpriteAppearanceContract(selection, appearance, data),
        (SpriteAppearanceStatus{
            SpriteAppearanceError::UnsupportedBlend,
            SpriteAppearanceField::Blend}));
    EXPECT_EQ(data, SpriteAppearanceContractData{});
}

TEST(SpriteAppearance2DTests, ColorSpaceMapsToDistinctTextureEncoding)
{
    const assets::SpriteAsset srgbAsset = MakeSpriteAsset(
        assets::SpriteSampling::Nearest, assets::SpriteColorSpace::Srgb);
    const assets::SpriteAsset linearAsset = MakeSpriteAsset(
        assets::SpriteSampling::Nearest, assets::SpriteColorSpace::Linear);

    SpriteAppearanceContractData srgb{};
    SpriteAppearanceContractData linear{};
    ASSERT_TRUE(ExtractSpriteAppearanceContract(
        ResolveSingle(srgbAsset), SpriteAppearance2D{}, srgb).Succeeded());
    ASSERT_TRUE(ExtractSpriteAppearanceContract(
        ResolveSingle(linearAsset), SpriteAppearance2D{}, linear).Succeeded());

    EXPECT_EQ(srgb.textureEncoding, SpriteTextureEncoding::Srgb);
    EXPECT_EQ(linear.textureEncoding, SpriteTextureEncoding::Linear);
    EXPECT_EQ(srgb.sourceAlphaMode, assets::SpriteAlphaMode::Straight);
    EXPECT_EQ(linear.sourceAlphaMode, assets::SpriteAlphaMode::Straight);
}

TEST(SpriteAppearance2DTests, AtlasSafeBoundsUseTexelCentersWithoutChangingCanonicalUvs)
{
    const assets::SpriteAsset asset = MakeSpriteAsset();
    const ResolvedSpriteRegion selection = ResolveSingle(asset);

    SpriteAppearanceContractData data{};
    ASSERT_TRUE(ExtractSpriteAppearanceContract(selection, SpriteAppearance2D{}, data).Succeeded());

    EXPECT_FLOAT_EQ(data.sampleBounds.minimum.x, 4.5F / 64.0F);
    EXPECT_FLOAT_EQ(data.sampleBounds.maximum.x, 19.5F / 64.0F);
    EXPECT_FLOAT_EQ(data.sampleBounds.minimum.y, 6.5F / 32.0F);
    EXPECT_FLOAT_EQ(data.sampleBounds.maximum.y, 13.5F / 32.0F);
}

TEST(SpriteAppearance2DTests, OnePixelExtentCollapsesToSingleTexelCenter)
{
    assets::SpriteAsset asset = MakeSpriteAsset();
    asset.regions[0].packedRect = assets::SpritePixelRect{7U, 9U, 1U, 1U};
    asset.regions[0].trimSize = assets::SpritePixelSize{1U, 1U};
    asset.regions[0].sourceSize = assets::SpritePixelSize{1U, 1U};
    const ResolvedSpriteRegion selection = ResolveSingle(asset);

    SpriteAppearanceContractData data{};
    ASSERT_TRUE(ExtractSpriteAppearanceContract(selection, SpriteAppearance2D{}, data).Succeeded());
    EXPECT_FLOAT_EQ(data.sampleBounds.minimum.x, data.sampleBounds.maximum.x);
    EXPECT_FLOAT_EQ(data.sampleBounds.minimum.y, data.sampleBounds.maximum.y);
    EXPECT_FLOAT_EQ(data.sampleBounds.minimum.x, 7.5F / 64.0F);
    EXPECT_FLOAT_EQ(data.sampleBounds.minimum.y, 9.5F / 32.0F);
}

TEST(SpriteAppearance2DTests, Cw90UsesSamePackedRegionSampleBounds)
{
    assets::SpriteAsset asset = MakeSpriteAsset(
        assets::SpriteSampling::Nearest,
        assets::SpriteColorSpace::Srgb,
        assets::SpritePackedRotation::Cw90);
    const ResolvedSpriteRegion selection = ResolveSingle(asset);

    SpriteAppearanceContractData data{};
    ASSERT_TRUE(ExtractSpriteAppearanceContract(selection, SpriteAppearance2D{}, data).Succeeded());
    EXPECT_FLOAT_EQ(data.sampleBounds.minimum.x, 4.5F / 64.0F);
    EXPECT_FLOAT_EQ(data.sampleBounds.maximum.x, 11.5F / 64.0F);
    EXPECT_FLOAT_EQ(data.sampleBounds.minimum.y, 6.5F / 32.0F);
    EXPECT_FLOAT_EQ(data.sampleBounds.maximum.y, 21.5F / 32.0F);
}

TEST(SpriteAppearance2DTests, CorruptedPackedBoundsFailAtAppearanceBoundary)
{
    assets::SpriteAsset asset = MakeSpriteAsset();
    const ResolvedSpriteRegion selection = ResolveSingle(asset);
    asset.regions[0].packedRect.x = 63U;
    asset.regions[0].packedRect.width = 2U;

    SpriteAppearanceContractData data{};
    EXPECT_EQ(
        ExtractSpriteAppearanceContract(selection, SpriteAppearance2D{}, data),
        (SpriteAppearanceStatus{
            SpriteAppearanceError::InvalidPackedRect,
            SpriteAppearanceField::PackedRect}));
}

TEST(SpriteAppearance2DTests, PremultipliedFragmentUsesLinearTintAndEffectiveAlphaExactly)
{
    SpriteAppearanceContractData appearance{};
    appearance.tint = SpriteLinearRgba{0.5F, 0.25F, 1.0F, 0.5F};
    appearance.opacity = 0.25F;

    const SpriteLinearRgba result = EvaluateSpritePremultipliedFragment(
        SpriteLinearRgba{0.8F, 0.4F, 0.2F, 0.5F}, appearance);

    const float expectedAlpha = 0.5F * 0.5F * 0.25F;
    EXPECT_FLOAT_EQ(result.alpha, expectedAlpha);
    EXPECT_FLOAT_EQ(result.red, 0.8F * 0.5F * expectedAlpha);
    EXPECT_FLOAT_EQ(result.green, 0.4F * 0.25F * expectedAlpha);
    EXPECT_FLOAT_EQ(result.blue, 0.2F * 1.0F * expectedAlpha);
}

TEST(SpriteAppearance2DTests, BlendEquationsMatchFrozenPremultipliedContract)
{
    const SpriteLinearRgba source{0.2F, 0.1F, 0.05F, 0.25F};
    const SpriteLinearRgba destination{0.4F, 0.5F, 0.6F, 0.75F};
    SpriteLinearRgba result{};

    ASSERT_TRUE(TryEvaluateSpriteBlend(
        source, destination, SpriteBlendCompatibility::Normal, result));
    EXPECT_FLOAT_EQ(result.red, 0.2F + 0.4F * 0.75F);
    EXPECT_FLOAT_EQ(result.green, 0.1F + 0.5F * 0.75F);
    EXPECT_FLOAT_EQ(result.blue, 0.05F + 0.6F * 0.75F);
    EXPECT_FLOAT_EQ(result.alpha, 0.25F + 0.75F * 0.75F);

    ASSERT_TRUE(TryEvaluateSpriteBlend(
        source, destination, SpriteBlendCompatibility::Additive, result));
    EXPECT_FLOAT_EQ(result.red, 0.2F + 0.4F);
    EXPECT_FLOAT_EQ(result.green, 0.1F + 0.5F);
    EXPECT_FLOAT_EQ(result.blue, 0.05F + 0.6F);
    EXPECT_FLOAT_EQ(result.alpha, 0.25F + 0.75F * 0.75F);

    ASSERT_TRUE(TryEvaluateSpriteBlend(
        source, destination, SpriteBlendCompatibility::Multiply, result));
    EXPECT_FLOAT_EQ(result.red, 0.2F * 0.4F + 0.4F * 0.75F);
    EXPECT_FLOAT_EQ(result.green, 0.1F * 0.5F + 0.5F * 0.75F);
    EXPECT_FLOAT_EQ(result.blue, 0.05F * 0.6F + 0.6F * 0.75F);
    EXPECT_FLOAT_EQ(result.alpha, 0.25F + 0.75F * 0.75F);

    ASSERT_TRUE(TryEvaluateSpriteBlend(
        source, destination, SpriteBlendCompatibility::Screen, result));
    EXPECT_FLOAT_EQ(result.red, 0.2F + 0.4F * (1.0F - 0.2F));
    EXPECT_FLOAT_EQ(result.green, 0.1F + 0.5F * (1.0F - 0.1F));
    EXPECT_FLOAT_EQ(result.blue, 0.05F + 0.6F * (1.0F - 0.05F));
    EXPECT_FLOAT_EQ(result.alpha, 0.25F + 0.75F * 0.75F);
}

TEST(SpriteAppearance2DTests, OpacityZeroIsColorIdentityForNormalMultiplyAndScreen)
{
    SpriteAppearanceContractData appearance{};
    appearance.opacity = 0.0F;
    const SpriteLinearRgba source = EvaluateSpritePremultipliedFragment(
        SpriteLinearRgba{0.9F, 0.2F, 0.7F, 0.8F}, appearance);
    EXPECT_EQ(source, (SpriteLinearRgba{0.0F, 0.0F, 0.0F, 0.0F}));

    const SpriteLinearRgba destination{0.2F, 0.4F, 0.6F, 0.8F};
    for (const SpriteBlendCompatibility blend : {
             SpriteBlendCompatibility::Normal,
             SpriteBlendCompatibility::Multiply,
             SpriteBlendCompatibility::Screen})
    {
        SpriteLinearRgba result{};
        ASSERT_TRUE(TryEvaluateSpriteBlend(source, destination, blend, result));
        EXPECT_EQ(result, destination);
    }
}

TEST(SpriteAppearance2DTests, RepeatedExtractionReusesCallerOwnedFixedOutput)
{
    const assets::SpriteAsset asset = MakeSpriteAsset();
    const ResolvedSpriteRegion selection = ResolveSingle(asset);
    SpriteAppearanceContractData data{};

    for (std::size_t index = 0U; index < 10000U; ++index)
    {
        ASSERT_TRUE(ExtractSpriteAppearanceContract(selection, SpriteAppearance2D{}, data).Succeeded());
        ASSERT_EQ(data.blend, SpriteBlendCompatibility::Normal);
        ASSERT_EQ(data.sampler, SpriteSamplerCompatibility::Nearest);
    }
}
} // namespace
} // namespace trace2d::render
