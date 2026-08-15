#include <trace2d/tile/TilePresentation2D.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace trace2d::tile
{
namespace
{
struct CandidateRange final
{
    std::uint32_t minX{0U};
    std::uint32_t maxX{0U};
    std::uint32_t minY{0U};
    std::uint32_t maxY{0U};
    bool valid{false};
};

[[nodiscard]] bool IsFinite(const render::Float2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

[[nodiscard]] bool IsValidView(const render::OrthographicView& view) noexcept
{
    return IsFinite(view.center) && IsFinite(view.halfExtents) && IsFinite(view.clipScale) &&
           view.halfExtents.x > 0.0F && view.halfExtents.y > 0.0F &&
           view.clipScale.x > 0.0F && view.clipScale.y > 0.0F;
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

[[nodiscard]] bool CheckedAdd(std::uint64_t& value, const std::uint64_t amount) noexcept
{
    if (amount > std::numeric_limits<std::uint64_t>::max() - value)
    {
        return false;
    }
    value += amount;
    return true;
}

[[nodiscard]] bool TryLayerCellCount(const CompiledTileLayer& layer, std::uint64_t& outCount) noexcept
{
    outCount = static_cast<std::uint64_t>(layer.width) * static_cast<std::uint64_t>(layer.height);
    return outCount == layer.cells.size();
}

[[nodiscard]] bool IntersectsInclusive(
    const double left,
    const double top,
    const double right,
    const double bottom,
    const double viewLeft,
    const double viewTop,
    const double viewRight,
    const double viewBottom) noexcept
{
    return right >= viewLeft && left <= viewRight && bottom >= viewTop && top <= viewBottom;
}

[[nodiscard]] std::uint32_t ClampIndex(const double value, const std::uint32_t maximum) noexcept
{
    if (value <= 0.0)
    {
        return 0U;
    }
    const double maximumValue = static_cast<double>(maximum);
    if (value >= maximumValue)
    {
        return maximum;
    }
    return static_cast<std::uint32_t>(value);
}

[[nodiscard]] CandidateRange BuildCandidateRange(
    const CompiledTileLayer& layer,
    const double cellWorldWidth,
    const double cellWorldHeight,
    const render::OrthographicView& view) noexcept
{
    CandidateRange range{};
    if (layer.width == 0U || layer.height == 0U)
    {
        return range;
    }

    const double viewLeft = static_cast<double>(view.center.x - view.halfExtents.x);
    const double viewRight = static_cast<double>(view.center.x + view.halfExtents.x);
    const double viewTop = static_cast<double>(view.center.y - view.halfExtents.y);
    const double viewBottom = static_cast<double>(view.center.y + view.halfExtents.y);

    const double layerLeft = static_cast<double>(layer.originX) * cellWorldWidth;
    const double layerTop = static_cast<double>(layer.originY) * cellWorldHeight;
    const double layerRight =
        (static_cast<double>(layer.originX) + static_cast<double>(layer.width)) * cellWorldWidth;
    const double layerBottom =
        (static_cast<double>(layer.originY) + static_cast<double>(layer.height)) * cellWorldHeight;

    if (!IntersectsInclusive(
            layerLeft,
            layerTop,
            layerRight,
            layerBottom,
            viewLeft,
            viewTop,
            viewRight,
            viewBottom))
    {
        return range;
    }

    // Expand by one cell around the floor-derived viewport window. Exact quad visibility below
    // removes the conservative edge cells while protecting against floating-point boundary loss.
    const double minLocalX =
        std::floor(viewLeft / cellWorldWidth) - static_cast<double>(layer.originX) - 1.0;
    const double maxLocalX =
        std::floor(viewRight / cellWorldWidth) - static_cast<double>(layer.originX) + 1.0;
    const double minLocalY =
        std::floor(viewTop / cellWorldHeight) - static_cast<double>(layer.originY) - 1.0;
    const double maxLocalY =
        std::floor(viewBottom / cellWorldHeight) - static_cast<double>(layer.originY) + 1.0;

    const std::uint32_t maxX = layer.width - 1U;
    const std::uint32_t maxY = layer.height - 1U;
    range.minX = ClampIndex(minLocalX, maxX);
    range.maxX = ClampIndex(maxLocalX, maxX);
    range.minY = ClampIndex(minLocalY, maxY);
    range.maxY = ClampIndex(maxLocalY, maxY);
    range.valid = range.minX <= range.maxX && range.minY <= range.maxY;
    return range;
}

[[nodiscard]] render::Float2 SourceCornerForDestination(
    const float destinationX,
    const float destinationY,
    const TileTransform transform) noexcept
{
    float x = destinationX;
    float y = destinationY;

    switch (transform.quarterTurns)
    {
    case 0U:
        break;
    case 1U:
    {
        const float previousX = x;
        x = y;
        y = 1.0F - previousX;
        break;
    }
    case 2U:
        x = 1.0F - x;
        y = 1.0F - y;
        break;
    case 3U:
    {
        const float previousX = x;
        x = 1.0F - y;
        y = previousX;
        break;
    }
    default:
        break;
    }

    if (transform.flipX)
    {
        x = 1.0F - x;
    }
    if (transform.flipY)
    {
        y = 1.0F - y;
    }
    return render::Float2{x, y};
}

[[nodiscard]] TilePresentationStatus BuildTileSprite(
    const CompiledTileSet& tileSet,
    const CompiledTileMap& tileMap,
    const TileTextureBinding2D& texture,
    const TilePresentationConfig2D& config,
    const std::size_t layerIndex,
    const CompiledTileLayer& layer,
    const std::uint32_t localX,
    const std::uint32_t localY,
    const CompiledTileCell& cell,
    render::SpritePresentationRenderData& outData) noexcept
{
    outData = {};
    if (cell.tileIndex >= tileSet.tiles.size())
    {
        return TilePresentationStatus{TilePresentationError::TileIndexOutOfRange, layerIndex, localX, localY};
    }

    const TileRegion& region = tileSet.tiles[cell.tileIndex].sourceRegion;
    const std::int64_t regionRight = static_cast<std::int64_t>(region.x) + region.width;
    const std::int64_t regionBottom = static_cast<std::int64_t>(region.y) + region.height;
    if (region.x < 0 || region.y < 0 || region.width <= 0 || region.height <= 0 ||
        regionRight > texture.width || regionBottom > texture.height)
    {
        return TilePresentationStatus{TilePresentationError::InvalidTileRegion, layerIndex, localX, localY};
    }

    const double cellWorldWidth = static_cast<double>(tileMap.cellWidth) / config.pixelsPerUnit;
    const double cellWorldHeight = static_cast<double>(tileMap.cellHeight) / config.pixelsPerUnit;
    const double worldCellX = static_cast<double>(layer.originX) + localX;
    const double worldCellY = static_cast<double>(layer.originY) + localY;
    const double left = worldCellX * cellWorldWidth;
    const double top = worldCellY * cellWorldHeight;
    const double right = left + cellWorldWidth;
    const double bottom = top + cellWorldHeight;
    if (!std::isfinite(left) || !std::isfinite(top) || !std::isfinite(right) || !std::isfinite(bottom) ||
        std::abs(left) > std::numeric_limits<float>::max() ||
        std::abs(top) > std::numeric_limits<float>::max() ||
        std::abs(right) > std::numeric_limits<float>::max() ||
        std::abs(bottom) > std::numeric_limits<float>::max())
    {
        return TilePresentationStatus{TilePresentationError::GeometryOverflow, layerIndex, localX, localY};
    }

    const float u0 = static_cast<float>(region.x) / static_cast<float>(texture.width);
    const float v0 = static_cast<float>(region.y) / static_cast<float>(texture.height);
    const float u1 = static_cast<float>(regionRight) / static_cast<float>(texture.width);
    const float v1 = static_cast<float>(regionBottom) / static_cast<float>(texture.height);
    const TileTransform transform = cell.Transform();

    const auto uv = [u0, v0, u1, v1, transform](const float x, const float y) noexcept {
        const render::Float2 source = SourceCornerForDestination(x, y, transform);
        return render::Float2{
            u0 + ((u1 - u0) * source.x),
            v0 + ((v1 - v0) * source.y),
        };
    };

    render::SpriteDrawQuad quad{};
    quad.topLeft = render::SpriteDrawVertex{{static_cast<float>(left), static_cast<float>(top)}, uv(0.0F, 0.0F)};
    quad.topRight = render::SpriteDrawVertex{{static_cast<float>(right), static_cast<float>(top)}, uv(1.0F, 0.0F)};
    quad.bottomRight = render::SpriteDrawVertex{{static_cast<float>(right), static_cast<float>(bottom)}, uv(1.0F, 1.0F)};
    quad.bottomLeft = render::SpriteDrawVertex{{static_cast<float>(left), static_cast<float>(bottom)}, uv(0.0F, 1.0F)};

    const float inverseWidth = 1.0F / static_cast<float>(texture.width);
    const float inverseHeight = 1.0F / static_cast<float>(texture.height);
    render::SpriteAppearanceContractData appearance{};
    appearance.sampler = config.sampler;
    appearance.blend = render::SpriteBlendCompatibility::Normal;
    appearance.textureEncoding = texture.textureEncoding;
    appearance.sourceAlphaMode = texture.sourceAlphaMode;
    appearance.sampleBounds = render::SpriteSampleBounds{
        render::Float2{
            (static_cast<float>(region.x) + 0.5F) * inverseWidth,
            (static_cast<float>(region.y) + 0.5F) * inverseHeight,
        },
        render::Float2{
            (static_cast<float>(regionRight) - 0.5F) * inverseWidth,
            (static_cast<float>(regionBottom) - 0.5F) * inverseHeight,
        },
    };

    if (layerIndex > std::numeric_limits<std::uint32_t>::max())
    {
        return TilePresentationStatus{TilePresentationError::CountOverflow, layerIndex, localX, localY};
    }
    const std::uint64_t localIndex =
        (static_cast<std::uint64_t>(localY) * layer.width) + localX;
    const std::uint64_t stableOrder =
        (static_cast<std::uint64_t>(static_cast<std::uint32_t>(layerIndex)) << 32U) | localIndex;

    outData.presentation = render::SpritePresentation2D{quad, appearance};
    outData.texture = texture.texture;
    outData.order = render::SpriteOrder2D{
        config.painterLayer,
        layer.order,
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

[[nodiscard]] TilePresentationStatus RunPresentationPass(
    const CompiledTileSet& tileSet,
    const CompiledTileMap& tileMap,
    const TileTextureBinding2D& texture,
    const render::OrthographicView& view,
    const TilePresentationConfig2D& config,
    const std::span<render::SpritePresentationRenderData> output,
    const bool writeOutput,
    std::size_t& outCount,
    TilePresentationMeasurement2D& measurement) noexcept
{
    outCount = 0U;
    measurement = {};
    const double cellWorldWidth = static_cast<double>(tileMap.cellWidth) / config.pixelsPerUnit;
    const double cellWorldHeight = static_cast<double>(tileMap.cellHeight) / config.pixelsPerUnit;

    for (std::size_t layerIndex = 0U; layerIndex < tileMap.layers.size(); ++layerIndex)
    {
        const CompiledTileLayer& layer = tileMap.layers[layerIndex];
        std::uint64_t layerCellCount = 0U;
        if (!TryLayerCellCount(layer, layerCellCount))
        {
            return TilePresentationStatus{TilePresentationError::InvalidLayerStorage, layerIndex, 0U, 0U};
        }
        if (!CheckedAdd(measurement.totalLayerCells, layerCellCount))
        {
            return TilePresentationStatus{TilePresentationError::CountOverflow, layerIndex, 0U, 0U};
        }
        if (!layer.visible)
        {
            continue;
        }

        const CandidateRange range = BuildCandidateRange(layer, cellWorldWidth, cellWorldHeight, view);
        if (!range.valid)
        {
            continue;
        }
        if (!CheckedAdd(measurement.visibleLayers, 1U))
        {
            return TilePresentationStatus{TilePresentationError::CountOverflow, layerIndex, 0U, 0U};
        }

        for (std::uint32_t y = range.minY; y <= range.maxY; ++y)
        {
            const std::uint64_t rowOffset = static_cast<std::uint64_t>(y) * layer.width;
            for (std::uint32_t x = range.minX; x <= range.maxX; ++x)
            {
                if (!CheckedAdd(measurement.candidateCells, 1U))
                {
                    return TilePresentationStatus{TilePresentationError::CountOverflow, layerIndex, x, y};
                }
                const std::size_t cellIndex = static_cast<std::size_t>(rowOffset + x);
                const CompiledTileCell& cell = layer.cells[cellIndex];
                if (cell.Empty())
                {
                    continue;
                }
                if (!CheckedAdd(measurement.occupiedCandidateCells, 1U))
                {
                    return TilePresentationStatus{TilePresentationError::CountOverflow, layerIndex, x, y};
                }

                render::SpritePresentationRenderData data{};
                const TilePresentationStatus build =
                    BuildTileSprite(tileSet, tileMap, texture, config, layerIndex, layer, x, y, cell, data);
                if (!build.Succeeded())
                {
                    return build;
                }
                if (!render::IsSpritePresentationQuadVisible(view, data.presentation.quad))
                {
                    continue;
                }
                if (!CheckedAdd(measurement.visibleOccupiedCells, 1U))
                {
                    return TilePresentationStatus{TilePresentationError::CountOverflow, layerIndex, x, y};
                }
                if (outCount == std::numeric_limits<std::size_t>::max())
                {
                    return TilePresentationStatus{TilePresentationError::CountOverflow, layerIndex, x, y};
                }
                if (writeOutput)
                {
                    output[outCount] = data;
                }
                ++outCount;
            }
        }
    }

    measurement.emittedQuads = outCount;
    measurement.retainedChunkMetadataBytes = 0U;
    return {};
}
} // namespace

std::string_view ToString(const TilePresentationError value) noexcept
{
    switch (value)
    {
    case TilePresentationError::None:
        return "none";
    case TilePresentationError::InvalidTextureHandle:
        return "invalid_texture_handle";
    case TilePresentationError::TextureIdentityMismatch:
        return "texture_identity_mismatch";
    case TilePresentationError::TextureSizeMismatch:
        return "texture_size_mismatch";
    case TilePresentationError::UnsupportedTextureAlphaMode:
        return "unsupported_texture_alpha_mode";
    case TilePresentationError::InvalidPixelsPerUnit:
        return "invalid_pixels_per_unit";
    case TilePresentationError::InvalidView:
        return "invalid_view";
    case TilePresentationError::TileSetMismatch:
        return "tile_set_mismatch";
    case TilePresentationError::InvalidLayerStorage:
        return "invalid_layer_storage";
    case TilePresentationError::TileIndexOutOfRange:
        return "tile_index_out_of_range";
    case TilePresentationError::InvalidTileRegion:
        return "invalid_tile_region";
    case TilePresentationError::GeometryOverflow:
        return "geometry_overflow";
    case TilePresentationError::CountOverflow:
        return "count_overflow";
    case TilePresentationError::InsufficientCapacity:
        return "insufficient_capacity";
    }
    return "unknown";
}

TilePresentationStatus ResolveTileTextureBinding2D(
    const CompiledTileSet& tileSet,
    const assets::ResourceRegistry& resources,
    const render::TextureHandle texture,
    TileTextureBinding2D& outBinding)
{
    outBinding = {};
    const assets::TextureResource* resource = resources.Resolve(texture);
    const std::optional<assets::ResourceSnapshot> snapshot = resources.Inspect(texture.Untyped());
    if (resource == nullptr || !snapshot.has_value())
    {
        return TilePresentationStatus{TilePresentationError::InvalidTextureHandle};
    }
    if (snapshot->identity.domain != assets::ResourceTypeDomain::Texture ||
        snapshot->identity.canonicalReference != tileSet.textureReference)
    {
        return TilePresentationStatus{TilePresentationError::TextureIdentityMismatch};
    }
    if (tileSet.sourceWidth <= 0 || tileSet.sourceHeight <= 0 ||
        resource->width != static_cast<std::uint32_t>(tileSet.sourceWidth) ||
        resource->height != static_cast<std::uint32_t>(tileSet.sourceHeight))
    {
        return TilePresentationStatus{TilePresentationError::TextureSizeMismatch};
    }
    if (resource->alphaMode != assets::TextureResourceAlphaMode::Straight)
    {
        return TilePresentationStatus{TilePresentationError::UnsupportedTextureAlphaMode};
    }

    render::SpriteTextureEncoding encoding = render::SpriteTextureEncoding::Srgb;
    switch (resource->colorSpace)
    {
    case assets::TextureResourceColorSpace::Srgb:
        encoding = render::SpriteTextureEncoding::Srgb;
        break;
    case assets::TextureResourceColorSpace::Linear:
        encoding = render::SpriteTextureEncoding::Linear;
        break;
    }

    outBinding = TileTextureBinding2D{
        texture,
        encoding,
        assets::SpriteAlphaMode::Straight,
        resource->width,
        resource->height,
    };
    return {};
}

TilePresentationStatus BuildTileMapPresentation2D(
    const CompiledTileSet& tileSet,
    const CompiledTileMap& tileMap,
    const TileTextureBinding2D& texture,
    const render::OrthographicView& view,
    const TilePresentationConfig2D& config,
    const std::span<render::SpritePresentationRenderData> output,
    std::size_t& outRequiredCount,
    TilePresentationMeasurement2D& outMeasurement) noexcept
{
    outRequiredCount = 0U;
    outMeasurement = {};
    if (!std::isfinite(config.pixelsPerUnit) || config.pixelsPerUnit <= 0.0F ||
        tileMap.cellWidth == 0U || tileMap.cellHeight == 0U || !IsValidSampler(config.sampler))
    {
        return TilePresentationStatus{TilePresentationError::InvalidPixelsPerUnit};
    }
    if (!IsValidView(view))
    {
        return TilePresentationStatus{TilePresentationError::InvalidView};
    }
    if (tileMap.tileSetSemanticId != tileSet.semanticId)
    {
        return TilePresentationStatus{TilePresentationError::TileSetMismatch};
    }
    if (texture.texture == render::InvalidTextureHandle || texture.texture.generation == 0U ||
        texture.texture.domain != assets::ResourceTypeDomain::Texture || texture.width == 0U || texture.height == 0U ||
        texture.width != static_cast<std::uint32_t>(tileSet.sourceWidth) ||
        texture.height != static_cast<std::uint32_t>(tileSet.sourceHeight))
    {
        return TilePresentationStatus{TilePresentationError::InvalidTextureHandle};
    }

    TilePresentationMeasurement2D measured{};
    std::size_t required = 0U;
    const TilePresentationStatus measureStatus = RunPresentationPass(
        tileSet,
        tileMap,
        texture,
        view,
        config,
        output,
        false,
        required,
        measured);
    outRequiredCount = required;
    outMeasurement = measured;
    if (!measureStatus.Succeeded())
    {
        return measureStatus;
    }
    if (output.size() < required)
    {
        return TilePresentationStatus{TilePresentationError::InsufficientCapacity};
    }

    TilePresentationMeasurement2D emitted{};
    std::size_t emittedCount = 0U;
    const TilePresentationStatus emitStatus = RunPresentationPass(
        tileSet,
        tileMap,
        texture,
        view,
        config,
        output,
        true,
        emittedCount,
        emitted);
    outRequiredCount = emittedCount;
    outMeasurement = emitted;
    return emitStatus;
}
} // namespace trace2d::tile
