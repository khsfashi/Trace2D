#pragma once

#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/render/RenderData.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace trace2d::render
{
using SpriteResolvedView = OrthographicView;

enum class SpriteMaterialPipeline : std::uint8_t
{
    BuiltInSprite = 0,
};

enum class SpriteSamplerCompatibility : std::uint8_t
{
    Nearest = 0,
    Linear = 1,
};

enum class SpriteBlendCompatibility : std::uint8_t
{
    Normal = 0,
    Additive = 1,
    Multiply = 2,
    Screen = 3,
};

enum class SpriteMaskCompatibility : std::uint8_t
{
    None = 0,
};

enum class SpritePrimitiveKind : std::uint8_t
{
    Quad = 0,
};

enum class SpriteResolveError : std::uint8_t
{
    None = 0,
    NullAsset,
    PageIndexOutOfRange,
    RegionIndexOutOfRange,
    RegionPageMismatch,
    InvalidCanonicalIdentity,
    InvalidPageResourceIdentity,
    UnsupportedColorSpace,
    UnsupportedAlphaMode,
    UnsupportedSampling,
    RegionIdNotFound,
    DuplicateRegionId,
    DuplicatePageId,
    UnresolvedSelection,
};

enum class SpriteResolveField : std::uint8_t
{
    None = 0,
    Asset,
    PageIndex,
    RegionIndex,
    RegionPage,
    PageId,
    RegionId,
    PageTextureReference,
    PageSize,
    PageColorSpace,
    PageAlphaMode,
    Sampling,
    Selection,
};

[[nodiscard]] std::string_view ToString(SpriteMaterialPipeline value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteSamplerCompatibility value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteBlendCompatibility value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteMaskCompatibility value) noexcept;
[[nodiscard]] std::string_view ToString(SpritePrimitiveKind value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteResolveError value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteResolveField value) noexcept;

struct SpriteResolveStatus final
{
    SpriteResolveError error{SpriteResolveError::None};
    SpriteResolveField field{SpriteResolveField::None};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == SpriteResolveError::None;
    }

    [[nodiscard]] bool operator==(const SpriteResolveStatus&) const noexcept = default;
};

struct SpritePageResourceKey final
{
    std::string_view textureReference{};
    assets::SpritePixelSize size{};
    assets::SpriteColorSpace colorSpace{assets::SpriteColorSpace::Srgb};
    assets::SpriteAlphaMode alphaMode{assets::SpriteAlphaMode::Straight};

    [[nodiscard]] bool operator==(const SpritePageResourceKey&) const noexcept = default;
};

class ResolvedSpriteRegion final
{
public:
    ResolvedSpriteRegion() = default;

    [[nodiscard]] bool Valid() const noexcept;
    [[nodiscard]] const assets::SpriteAsset* Asset() const noexcept;
    [[nodiscard]] const assets::SpriteAtlasPage* Page() const noexcept;
    [[nodiscard]] const assets::SpriteRegion* Region() const noexcept;
    [[nodiscard]] std::size_t PageIndex() const noexcept;
    [[nodiscard]] std::size_t RegionIndex() const noexcept;
    [[nodiscard]] const SpritePageResourceKey& PageResource() const noexcept;
    [[nodiscard]] SpriteMaterialPipeline MaterialPipeline() const noexcept;
    [[nodiscard]] SpriteSamplerCompatibility Sampler() const noexcept;
    [[nodiscard]] SpriteBlendCompatibility Blend() const noexcept;
    [[nodiscard]] SpriteMaskCompatibility Mask() const noexcept;
    [[nodiscard]] SpritePrimitiveKind Primitive() const noexcept;

private:
    friend SpriteResolveStatus ResolveSpriteRegionByIndices(
        const assets::SpriteAsset* asset,
        std::size_t pageIndex,
        std::size_t regionIndex,
        ResolvedSpriteRegion& outSelection) noexcept;

    const assets::SpriteAsset* asset_{nullptr};
    const assets::SpriteAtlasPage* page_{nullptr};
    const assets::SpriteRegion* region_{nullptr};
    std::size_t pageIndex_{0U};
    std::size_t regionIndex_{0U};
    SpritePageResourceKey pageResource_{};
    SpriteMaterialPipeline materialPipeline_{SpriteMaterialPipeline::BuiltInSprite};
    SpriteSamplerCompatibility sampler_{SpriteSamplerCompatibility::Nearest};
    SpriteBlendCompatibility blend_{SpriteBlendCompatibility::Normal};
    SpriteMaskCompatibility mask_{SpriteMaskCompatibility::None};
    SpritePrimitiveKind primitive_{SpritePrimitiveKind::Quad};
};

struct SpriteRenderContractData final
{
    const assets::SpriteAsset* asset{nullptr};
    const assets::SpriteAtlasPage* page{nullptr};
    const assets::SpriteRegion* region{nullptr};
    std::size_t pageIndex{0U};
    std::size_t regionIndex{0U};
    SpritePageResourceKey pageResource{};
    SpriteMaterialPipeline materialPipeline{SpriteMaterialPipeline::BuiltInSprite};
    SpriteSamplerCompatibility sampler{SpriteSamplerCompatibility::Nearest};
    SpriteBlendCompatibility blend{SpriteBlendCompatibility::Normal};
    SpriteMaskCompatibility mask{SpriteMaskCompatibility::None};
    SpritePrimitiveKind primitive{SpritePrimitiveKind::Quad};

    [[nodiscard]] bool operator==(const SpriteRenderContractData&) const noexcept = default;
};

// Setup-time resolver. Performs bounds and canonical page/region relationship checks.
// Complexity is O(1) plus semantic string comparison; do not call it per draw.
[[nodiscard]] SpriteResolveStatus ResolveSpriteRegionByIndices(
    const assets::SpriteAsset* asset,
    std::size_t pageIndex,
    std::size_t regionIndex,
    ResolvedSpriteRegion& outSelection) noexcept;

// Setup/tooling resolver. Performs linear semantic-ID lookup with no heap allocation.
// Complexity is O(region_count + page_count); the returned selection is the steady-state input.
[[nodiscard]] SpriteResolveStatus ResolveSpriteRegionById(
    const assets::SpriteAsset* asset,
    std::string_view regionId,
    ResolvedSpriteRegion& outSelection) noexcept;

// Steady-state SR0 extraction. O(1), no filesystem, parsing, image decode, semantic lookup,
// formatted diagnostics, renderer/GPU initialization, or required heap allocation.
[[nodiscard]] SpriteResolveStatus ExtractSpriteRenderContract(
    const ResolvedSpriteRegion& selection,
    SpriteRenderContractData& outData) noexcept;

static_assert(std::is_trivially_copyable_v<SpritePageResourceKey>);
static_assert(std::is_trivially_copyable_v<ResolvedSpriteRegion>);
static_assert(std::is_trivially_copyable_v<SpriteRenderContractData>);
} // namespace trace2d::render
