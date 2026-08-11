#include <trace2d/render/SpriteRenderContract.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string_view>
#include <type_traits>

namespace trace2d::render
{
namespace
{
template <typename T>
concept HasTextureHandleMember = requires(const T& value)
{
    value.texture;
};

template <typename T>
concept HasNormalizedUvMember = requires(const T& value)
{
    value.uv;
};

static_assert(!HasTextureHandleMember<assets::SpriteAsset>);
static_assert(!HasNormalizedUvMember<assets::SpriteAsset>);
static_assert(std::is_same_v<SpriteResolvedView, OrthographicView>);
static_assert(std::is_same_v<decltype(SpritePageResourceKey::textureReference), std::string_view>);

assets::SpriteAsset MakeSpriteAsset()
{
    assets::SpriteAsset asset{};
    asset.id = "sprites/player.sprite.toml";
    asset.schemaVersion = 1U;
    asset.sampling = assets::SpriteSampling::Nearest;
    asset.pages = {
        assets::SpriteAtlasPage{
            "page_a",
            "textures/player-a.png",
            assets::SpritePixelSize{64U, 32U},
            assets::SpriteColorSpace::Srgb,
            assets::SpriteAlphaMode::Straight,
        },
        assets::SpriteAtlasPage{
            "page_b",
            "textures/player-b.png",
            assets::SpritePixelSize{128U, 64U},
            assets::SpriteColorSpace::Linear,
            assets::SpriteAlphaMode::Straight,
        },
    };
    asset.regions = {
        assets::SpriteRegion{
            "idle_0",
            "page_a",
            assets::SpritePixelSize{32U, 32U},
            assets::SpritePixelOffset{2U, 1U},
            assets::SpritePixelSize{28U, 30U},
            assets::SpritePixelRect{0U, 0U, 28U, 30U},
            assets::SpriteRationalPivot{16, 28, 1},
            assets::SpritePackedRotation::None,
        },
        assets::SpriteRegion{
            "run_0",
            "page_b",
            assets::SpritePixelSize{20U, 30U},
            assets::SpritePixelOffset{0U, 0U},
            assets::SpritePixelSize{20U, 30U},
            assets::SpritePixelRect{4U, 8U, 30U, 20U},
            assets::SpriteRationalPivot{10, 25, 1},
            assets::SpritePackedRotation::Cw90,
        },
    };
    return asset;
}

TEST(SpriteRenderContractTests, PreResolvedExtractionForwardsCanonicalTruthWithoutGpuState)
{
    const assets::SpriteAsset asset = MakeSpriteAsset();
    ResolvedSpriteRegion selection{};
    const SpriteResolveStatus resolved = ResolveSpriteRegionByIndices(&asset, 1U, 1U, selection);
    ASSERT_TRUE(resolved.Succeeded());
    ASSERT_TRUE(selection.Valid());

    SpriteRenderContractData data{};
    const SpriteResolveStatus extracted = ExtractSpriteRenderContract(selection, data);
    ASSERT_TRUE(extracted.Succeeded());

    EXPECT_EQ(data.asset, &asset);
    EXPECT_EQ(data.page, &asset.pages[1]);
    EXPECT_EQ(data.region, &asset.regions[1]);
    EXPECT_EQ(data.pageIndex, 1U);
    EXPECT_EQ(data.regionIndex, 1U);
    EXPECT_EQ(data.pageResource.textureReference, "textures/player-b.png");
    EXPECT_EQ(data.pageResource.size, (assets::SpritePixelSize{128U, 64U}));
    EXPECT_EQ(data.pageResource.colorSpace, assets::SpriteColorSpace::Linear);
    EXPECT_EQ(data.pageResource.alphaMode, assets::SpriteAlphaMode::Straight);

    // Exact S1 metadata remains referenced, not normalized or rewritten by SR0.
    EXPECT_EQ(data.region->sourceSize, (assets::SpritePixelSize{20U, 30U}));
    EXPECT_EQ(data.region->trimSize, (assets::SpritePixelSize{20U, 30U}));
    EXPECT_EQ(data.region->packedRect, (assets::SpritePixelRect{4U, 8U, 30U, 20U}));
    EXPECT_EQ(data.region->pivot, (assets::SpriteRationalPivot{10, 25, 1}));
    EXPECT_EQ(data.region->packedRotation, assets::SpritePackedRotation::Cw90);
}

TEST(SpriteRenderContractTests, BuiltInCompatibilityDefaultsAreFiniteAndStable)
{
    assets::SpriteAsset asset = MakeSpriteAsset();
    asset.sampling = assets::SpriteSampling::Linear;

    ResolvedSpriteRegion selection{};
    ASSERT_TRUE(ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection).Succeeded());

    SpriteRenderContractData data{};
    ASSERT_TRUE(ExtractSpriteRenderContract(selection, data).Succeeded());
    EXPECT_EQ(data.materialPipeline, SpriteMaterialPipeline::BuiltInSprite);
    EXPECT_EQ(data.sampler, SpriteSamplerCompatibility::Linear);
    EXPECT_EQ(data.blend, SpriteBlendCompatibility::Normal);
    EXPECT_EQ(data.mask, SpriteMaskCompatibility::None);
    EXPECT_EQ(data.primitive, SpritePrimitiveKind::Quad);
    EXPECT_EQ(ToString(data.materialPipeline), "built_in_sprite");
    EXPECT_EQ(ToString(data.sampler), "linear");
    EXPECT_EQ(ToString(data.blend), "normal");
    EXPECT_EQ(ToString(SpriteBlendCompatibility::Additive), "additive");
    EXPECT_EQ(ToString(SpriteBlendCompatibility::Multiply), "multiply");
    EXPECT_EQ(ToString(SpriteBlendCompatibility::Screen), "screen");
    EXPECT_EQ(ToString(data.mask), "none");
    EXPECT_EQ(ToString(data.primitive), "quad");
}

TEST(SpriteRenderContractTests, InvalidIndicesAndRegionPageMismatchFailDuringSetup)
{
    assets::SpriteAsset asset = MakeSpriteAsset();
    ResolvedSpriteRegion selection{};

    EXPECT_EQ(
        ResolveSpriteRegionByIndices(nullptr, 0U, 0U, selection),
        (SpriteResolveStatus{SpriteResolveError::NullAsset, SpriteResolveField::Asset}));
    EXPECT_EQ(
        ResolveSpriteRegionByIndices(&asset, 99U, 0U, selection),
        (SpriteResolveStatus{SpriteResolveError::PageIndexOutOfRange, SpriteResolveField::PageIndex}));
    EXPECT_EQ(
        ResolveSpriteRegionByIndices(&asset, 0U, 99U, selection),
        (SpriteResolveStatus{SpriteResolveError::RegionIndexOutOfRange, SpriteResolveField::RegionIndex}));
    EXPECT_EQ(
        ResolveSpriteRegionByIndices(&asset, 1U, 0U, selection),
        (SpriteResolveStatus{SpriteResolveError::RegionPageMismatch, SpriteResolveField::RegionPage}));
    EXPECT_FALSE(selection.Valid());
}

TEST(SpriteRenderContractTests, InvalidCanonicalPageCompatibilityFailsBeforeExtraction)
{
    assets::SpriteAsset asset = MakeSpriteAsset();
    ResolvedSpriteRegion selection{};

    asset.pages[0].textureReference.clear();
    EXPECT_EQ(
        ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection),
        (SpriteResolveStatus{
            SpriteResolveError::InvalidPageResourceIdentity,
            SpriteResolveField::PageTextureReference}));

    asset = MakeSpriteAsset();
    asset.pages[0].size.width = 0U;
    EXPECT_EQ(
        ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection),
        (SpriteResolveStatus{
            SpriteResolveError::InvalidPageResourceIdentity,
            SpriteResolveField::PageSize}));

    asset = MakeSpriteAsset();
    asset.pages[0].colorSpace = static_cast<assets::SpriteColorSpace>(255U);
    EXPECT_EQ(
        ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection),
        (SpriteResolveStatus{
            SpriteResolveError::UnsupportedColorSpace,
            SpriteResolveField::PageColorSpace}));

    asset = MakeSpriteAsset();
    asset.pages[0].alphaMode = static_cast<assets::SpriteAlphaMode>(255U);
    EXPECT_EQ(
        ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection),
        (SpriteResolveStatus{
            SpriteResolveError::UnsupportedAlphaMode,
            SpriteResolveField::PageAlphaMode}));

    asset = MakeSpriteAsset();
    asset.sampling = static_cast<assets::SpriteSampling>(255U);
    EXPECT_EQ(
        ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection),
        (SpriteResolveStatus{
            SpriteResolveError::UnsupportedSampling,
            SpriteResolveField::Sampling}));
}

TEST(SpriteRenderContractTests, SemanticIdResolutionIsDeterministicSetupOnlyWork)
{
    assets::SpriteAsset asset = MakeSpriteAsset();
    ResolvedSpriteRegion selection{};

    ASSERT_TRUE(ResolveSpriteRegionById(&asset, "run_0", selection).Succeeded());
    EXPECT_EQ(selection.PageIndex(), 1U);
    EXPECT_EQ(selection.RegionIndex(), 1U);
    EXPECT_EQ(selection.Page(), &asset.pages[1]);
    EXPECT_EQ(selection.Region(), &asset.regions[1]);

    EXPECT_EQ(
        ResolveSpriteRegionById(&asset, "missing", selection),
        (SpriteResolveStatus{SpriteResolveError::RegionIdNotFound, SpriteResolveField::RegionId}));

    asset = MakeSpriteAsset();
    asset.regions.push_back(asset.regions[1]);
    EXPECT_EQ(
        ResolveSpriteRegionById(&asset, "run_0", selection),
        (SpriteResolveStatus{SpriteResolveError::DuplicateRegionId, SpriteResolveField::RegionId}));

    asset = MakeSpriteAsset();
    asset.pages.push_back(asset.pages[1]);
    EXPECT_EQ(
        ResolveSpriteRegionById(&asset, "run_0", selection),
        (SpriteResolveStatus{SpriteResolveError::DuplicatePageId, SpriteResolveField::PageId}));
}

TEST(SpriteRenderContractTests, ExtractionRejectsDefaultSelectionAndReusesCallerOwnedOutput)
{
    SpriteRenderContractData data{};
    const ResolvedSpriteRegion unresolved{};
    EXPECT_EQ(
        ExtractSpriteRenderContract(unresolved, data),
        (SpriteResolveStatus{SpriteResolveError::UnresolvedSelection, SpriteResolveField::Selection}));

    const assets::SpriteAsset asset = MakeSpriteAsset();
    ResolvedSpriteRegion selection{};
    ASSERT_TRUE(ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection).Succeeded());

    const assets::SpriteAtlasPage* expectedPage = &asset.pages[0];
    const assets::SpriteRegion* expectedRegion = &asset.regions[0];
    for (std::size_t iteration = 0U; iteration < 10000U; ++iteration)
    {
        ASSERT_TRUE(ExtractSpriteRenderContract(selection, data).Succeeded());
        ASSERT_EQ(data.page, expectedPage);
        ASSERT_EQ(data.region, expectedRegion);
        ASSERT_EQ(data.pageResource.textureReference.data(), asset.pages[0].textureReference.data());
    }
}
} // namespace
} // namespace trace2d::render