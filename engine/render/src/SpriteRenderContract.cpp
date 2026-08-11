#include <trace2d/render/SpriteRenderContract.hpp>

namespace trace2d::render
{
namespace
{
[[nodiscard]] SpriteResolveStatus Success() noexcept
{
    return SpriteResolveStatus{};
}

[[nodiscard]] SpriteResolveStatus Failure(
    const SpriteResolveError error,
    const SpriteResolveField field) noexcept
{
    return SpriteResolveStatus{error, field};
}

[[nodiscard]] SpriteResolveStatus ValidatePageCompatibility(
    const assets::SpriteAsset& asset,
    const assets::SpriteAtlasPage& page,
    SpriteSamplerCompatibility& outSampler) noexcept
{
    if (page.id.empty())
    {
        return Failure(SpriteResolveError::InvalidCanonicalIdentity, SpriteResolveField::PageId);
    }
    if (page.textureReference.empty())
    {
        return Failure(
            SpriteResolveError::InvalidPageResourceIdentity,
            SpriteResolveField::PageTextureReference);
    }
    if (page.size.width == 0U || page.size.height == 0U)
    {
        return Failure(
            SpriteResolveError::InvalidPageResourceIdentity,
            SpriteResolveField::PageSize);
    }

    switch (page.colorSpace)
    {
    case assets::SpriteColorSpace::Srgb:
    case assets::SpriteColorSpace::Linear:
        break;
    default:
        return Failure(
            SpriteResolveError::UnsupportedColorSpace,
            SpriteResolveField::PageColorSpace);
    }

    switch (page.alphaMode)
    {
    case assets::SpriteAlphaMode::Straight:
        break;
    default:
        return Failure(
            SpriteResolveError::UnsupportedAlphaMode,
            SpriteResolveField::PageAlphaMode);
    }

    switch (asset.sampling)
    {
    case assets::SpriteSampling::Nearest:
        outSampler = SpriteSamplerCompatibility::Nearest;
        break;
    case assets::SpriteSampling::Linear:
        outSampler = SpriteSamplerCompatibility::Linear;
        break;
    default:
        return Failure(
            SpriteResolveError::UnsupportedSampling,
            SpriteResolveField::Sampling);
    }

    return Success();
}
} // namespace

std::string_view ToString(const SpriteMaterialPipeline value) noexcept
{
    switch (value)
    {
    case SpriteMaterialPipeline::BuiltInSprite: return "built_in_sprite";
    }
    return "unknown";
}

std::string_view ToString(const SpriteSamplerCompatibility value) noexcept
{
    switch (value)
    {
    case SpriteSamplerCompatibility::Nearest: return "nearest";
    case SpriteSamplerCompatibility::Linear: return "linear";
    }
    return "unknown";
}

std::string_view ToString(const SpriteBlendCompatibility value) noexcept
{
    switch (value)
    {
    case SpriteBlendCompatibility::Normal: return "normal";
    case SpriteBlendCompatibility::Additive: return "additive";
    case SpriteBlendCompatibility::Multiply: return "multiply";
    case SpriteBlendCompatibility::Screen: return "screen";
    }
    return "unknown";
}

std::string_view ToString(const SpriteMaskCompatibility value) noexcept
{
    switch (value)
    {
    case SpriteMaskCompatibility::None: return "none";
    }
    return "unknown";
}

std::string_view ToString(const SpritePrimitiveKind value) noexcept
{
    switch (value)
    {
    case SpritePrimitiveKind::Quad: return "quad";
    }
    return "unknown";
}

std::string_view ToString(const SpriteResolveError value) noexcept
{
    switch (value)
    {
    case SpriteResolveError::None: return "none";
    case SpriteResolveError::NullAsset: return "null_asset";
    case SpriteResolveError::PageIndexOutOfRange: return "page_index_out_of_range";
    case SpriteResolveError::RegionIndexOutOfRange: return "region_index_out_of_range";
    case SpriteResolveError::RegionPageMismatch: return "region_page_mismatch";
    case SpriteResolveError::InvalidCanonicalIdentity: return "invalid_canonical_identity";
    case SpriteResolveError::InvalidPageResourceIdentity: return "invalid_page_resource_identity";
    case SpriteResolveError::UnsupportedColorSpace: return "unsupported_color_space";
    case SpriteResolveError::UnsupportedAlphaMode: return "unsupported_alpha_mode";
    case SpriteResolveError::UnsupportedSampling: return "unsupported_sampling";
    case SpriteResolveError::RegionIdNotFound: return "region_id_not_found";
    case SpriteResolveError::DuplicateRegionId: return "duplicate_region_id";
    case SpriteResolveError::DuplicatePageId: return "duplicate_page_id";
    case SpriteResolveError::UnresolvedSelection: return "unresolved_selection";
    }
    return "unknown";
}

std::string_view ToString(const SpriteResolveField value) noexcept
{
    switch (value)
    {
    case SpriteResolveField::None: return "none";
    case SpriteResolveField::Asset: return "asset";
    case SpriteResolveField::PageIndex: return "page_index";
    case SpriteResolveField::RegionIndex: return "region_index";
    case SpriteResolveField::RegionPage: return "region.page";
    case SpriteResolveField::PageId: return "page.id";
    case SpriteResolveField::RegionId: return "region.id";
    case SpriteResolveField::PageTextureReference: return "page.texture";
    case SpriteResolveField::PageSize: return "page.size";
    case SpriteResolveField::PageColorSpace: return "page.color_space";
    case SpriteResolveField::PageAlphaMode: return "page.alpha_mode";
    case SpriteResolveField::Sampling: return "sampling";
    case SpriteResolveField::Selection: return "selection";
    }
    return "unknown";
}

bool ResolvedSpriteRegion::Valid() const noexcept
{
    return asset_ != nullptr && page_ != nullptr && region_ != nullptr &&
        !pageResource_.textureReference.empty();
}

const assets::SpriteAsset* ResolvedSpriteRegion::Asset() const noexcept
{
    return asset_;
}

const assets::SpriteAtlasPage* ResolvedSpriteRegion::Page() const noexcept
{
    return page_;
}

const assets::SpriteRegion* ResolvedSpriteRegion::Region() const noexcept
{
    return region_;
}

std::size_t ResolvedSpriteRegion::PageIndex() const noexcept
{
    return pageIndex_;
}

std::size_t ResolvedSpriteRegion::RegionIndex() const noexcept
{
    return regionIndex_;
}

const SpritePageResourceKey& ResolvedSpriteRegion::PageResource() const noexcept
{
    return pageResource_;
}

SpriteMaterialPipeline ResolvedSpriteRegion::MaterialPipeline() const noexcept
{
    return materialPipeline_;
}

SpriteSamplerCompatibility ResolvedSpriteRegion::Sampler() const noexcept
{
    return sampler_;
}

SpriteBlendCompatibility ResolvedSpriteRegion::Blend() const noexcept
{
    return blend_;
}

SpriteMaskCompatibility ResolvedSpriteRegion::Mask() const noexcept
{
    return mask_;
}

SpritePrimitiveKind ResolvedSpriteRegion::Primitive() const noexcept
{
    return primitive_;
}

SpriteResolveStatus ResolveSpriteRegionByIndices(
    const assets::SpriteAsset* const asset,
    const std::size_t pageIndex,
    const std::size_t regionIndex,
    ResolvedSpriteRegion& outSelection) noexcept
{
    outSelection = ResolvedSpriteRegion{};

    if (asset == nullptr)
    {
        return Failure(SpriteResolveError::NullAsset, SpriteResolveField::Asset);
    }
    if (pageIndex >= asset->pages.size())
    {
        return Failure(
            SpriteResolveError::PageIndexOutOfRange,
            SpriteResolveField::PageIndex);
    }
    if (regionIndex >= asset->regions.size())
    {
        return Failure(
            SpriteResolveError::RegionIndexOutOfRange,
            SpriteResolveField::RegionIndex);
    }

    const assets::SpriteAtlasPage& page = asset->pages[pageIndex];
    const assets::SpriteRegion& region = asset->regions[regionIndex];
    if (region.id.empty())
    {
        return Failure(
            SpriteResolveError::InvalidCanonicalIdentity,
            SpriteResolveField::RegionId);
    }
    if (region.pageId != page.id)
    {
        return Failure(
            SpriteResolveError::RegionPageMismatch,
            SpriteResolveField::RegionPage);
    }

    SpriteSamplerCompatibility sampler = SpriteSamplerCompatibility::Nearest;
    const SpriteResolveStatus compatibility = ValidatePageCompatibility(*asset, page, sampler);
    if (!compatibility.Succeeded())
    {
        return compatibility;
    }

    outSelection.asset_ = asset;
    outSelection.page_ = &page;
    outSelection.region_ = &region;
    outSelection.pageIndex_ = pageIndex;
    outSelection.regionIndex_ = regionIndex;
    outSelection.pageResource_ = SpritePageResourceKey{
        page.textureReference,
        page.size,
        page.colorSpace,
        page.alphaMode,
    };
    outSelection.materialPipeline_ = SpriteMaterialPipeline::BuiltInSprite;
    outSelection.sampler_ = sampler;
    outSelection.blend_ = SpriteBlendCompatibility::Normal;
    outSelection.mask_ = SpriteMaskCompatibility::None;
    outSelection.primitive_ = SpritePrimitiveKind::Quad;
    return Success();
}

SpriteResolveStatus ResolveSpriteRegionById(
    const assets::SpriteAsset* const asset,
    const std::string_view regionId,
    ResolvedSpriteRegion& outSelection) noexcept
{
    outSelection = ResolvedSpriteRegion{};
    if (asset == nullptr)
    {
        return Failure(SpriteResolveError::NullAsset, SpriteResolveField::Asset);
    }

    std::size_t regionIndex = 0U;
    bool foundRegion = false;
    for (std::size_t index = 0U; index < asset->regions.size(); ++index)
    {
        if (asset->regions[index].id != regionId)
        {
            continue;
        }
        if (foundRegion)
        {
            return Failure(
                SpriteResolveError::DuplicateRegionId,
                SpriteResolveField::RegionId);
        }
        foundRegion = true;
        regionIndex = index;
    }
    if (!foundRegion)
    {
        return Failure(
            SpriteResolveError::RegionIdNotFound,
            SpriteResolveField::RegionId);
    }

    const std::string_view pageId = asset->regions[regionIndex].pageId;
    std::size_t pageIndex = 0U;
    bool foundPage = false;
    for (std::size_t index = 0U; index < asset->pages.size(); ++index)
    {
        if (asset->pages[index].id != pageId)
        {
            continue;
        }
        if (foundPage)
        {
            return Failure(
                SpriteResolveError::DuplicatePageId,
                SpriteResolveField::PageId);
        }
        foundPage = true;
        pageIndex = index;
    }
    if (!foundPage)
    {
        return Failure(
            SpriteResolveError::RegionPageMismatch,
            SpriteResolveField::RegionPage);
    }

    return ResolveSpriteRegionByIndices(asset, pageIndex, regionIndex, outSelection);
}

SpriteResolveStatus ExtractSpriteRenderContract(
    const ResolvedSpriteRegion& selection,
    SpriteRenderContractData& outData) noexcept
{
    outData = SpriteRenderContractData{};
    if (!selection.Valid())
    {
        return Failure(
            SpriteResolveError::UnresolvedSelection,
            SpriteResolveField::Selection);
    }

    outData.asset = selection.Asset();
    outData.page = selection.Page();
    outData.region = selection.Region();
    outData.pageIndex = selection.PageIndex();
    outData.regionIndex = selection.RegionIndex();
    outData.pageResource = selection.PageResource();
    outData.materialPipeline = selection.MaterialPipeline();
    outData.sampler = selection.Sampler();
    outData.blend = selection.Blend();
    outData.mask = selection.Mask();
    outData.primitive = selection.Primitive();
    return Success();
}
} // namespace trace2d::render
