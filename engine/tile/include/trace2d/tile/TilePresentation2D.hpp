#pragma once

#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/tile/TileMap.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

namespace trace2d::tile
{
enum class TilePresentationError : std::uint8_t
{
    None = 0,
    InvalidTextureHandle,
    TextureIdentityMismatch,
    TextureSizeMismatch,
    UnsupportedTextureAlphaMode,
    InvalidPixelsPerUnit,
    InvalidView,
    TileSetMismatch,
    InvalidLayerStorage,
    TileIndexOutOfRange,
    InvalidTileRegion,
    GeometryOverflow,
    CountOverflow,
    InsufficientCapacity,
};

[[nodiscard]] std::string_view ToString(TilePresentationError value) noexcept;

struct TileTextureBinding2D final
{
    render::TextureHandle texture{render::InvalidTextureHandle};
    render::SpriteTextureEncoding textureEncoding{render::SpriteTextureEncoding::Srgb};
    assets::SpriteAlphaMode sourceAlphaMode{assets::SpriteAlphaMode::Straight};
    std::uint32_t width{0U};
    std::uint32_t height{0U};

    [[nodiscard]] bool operator==(const TileTextureBinding2D&) const noexcept = default;
};

struct TilePresentationConfig2D final
{
    float pixelsPerUnit{16.0F};
    std::int32_t painterLayer{0};
    render::SpriteSamplerCompatibility sampler{render::SpriteSamplerCompatibility::Nearest};

    [[nodiscard]] bool operator==(const TilePresentationConfig2D&) const noexcept = default;
};

struct TilePresentationMeasurement2D final
{
    std::uint64_t totalLayerCells{0U};
    std::uint64_t visibleLayers{0U};
    std::uint64_t candidateCells{0U};
    std::uint64_t occupiedCandidateCells{0U};
    std::uint64_t visibleOccupiedCells{0U};
    std::uint64_t emittedQuads{0U};
    std::uint64_t retainedChunkMetadataBytes{0U};

    [[nodiscard]] bool operator==(const TilePresentationMeasurement2D&) const noexcept = default;
};

struct TilePresentationStatus final
{
    TilePresentationError error{TilePresentationError::None};
    std::size_t layerIndex{0U};
    std::uint32_t localX{0U};
    std::uint32_t localY{0U};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == TilePresentationError::None;
    }

    [[nodiscard]] bool operator==(const TilePresentationStatus&) const noexcept = default;
};

// Setup-time binding. ResourceRegistry identity/metadata checks happen once here; the returned
// value is fixed-size steady-state input and contains no path/string/container ownership.
[[nodiscard]] TilePresentationStatus ResolveTileTextureBinding2D(
    const CompiledTileSet& tileSet,
    const assets::ResourceRegistry& resources,
    render::TextureHandle texture,
    TileTextureBinding2D& outBinding);

// Builds only visible occupied tile cells into the existing production Sprite presentation type.
// The first pass counts and validates without writing. If output is too small, outRequiredCount is
// exact and no partial output is published. With sufficient caller capacity the second pass writes
// canonical layer-order / row-major items without allocation, semantic lookup, filesystem work or
// GPU access. T1 deliberately retains no chunk metadata: viewport range arithmetic bounds cell
// iteration before any cell visit.
[[nodiscard]] TilePresentationStatus BuildTileMapPresentation2D(
    const CompiledTileSet& tileSet,
    const CompiledTileMap& tileMap,
    const TileTextureBinding2D& texture,
    const render::OrthographicView& view,
    const TilePresentationConfig2D& config,
    std::span<render::SpritePresentationRenderData> output,
    std::size_t& outRequiredCount,
    TilePresentationMeasurement2D& outMeasurement) noexcept;

static_assert(std::is_trivially_copyable_v<TileTextureBinding2D>);
static_assert(std::is_trivially_copyable_v<TilePresentationConfig2D>);
static_assert(std::is_trivially_copyable_v<TilePresentationMeasurement2D>);
static_assert(std::is_trivially_copyable_v<TilePresentationStatus>);
} // namespace trace2d::tile
