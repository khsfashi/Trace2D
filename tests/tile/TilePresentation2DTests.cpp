#include <trace2d/tile/TilePresentation2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace trace2d::tile
{
namespace
{
CompiledTileSet MakeTileSet()
{
    CompiledTileSet set{};
    set.semanticId = "overworld";
    set.textureReference = "textures/tiles.png";
    set.sourceWidth = 32;
    set.sourceHeight = 16;
    set.tiles = {
        CompiledTileDefinition{"grass", TileRegion{0, 0, 16, 16}, {"ground"}},
        CompiledTileDefinition{"wall", TileRegion{16, 0, 16, 16}, {"solid"}},
    };
    return set;
}

TileTextureBinding2D PublishBinding(const CompiledTileSet& set, assets::ResourceRegistry& resources)
{
    assets::TextureResource texture{};
    texture.width = 32U;
    texture.height = 16U;
    texture.colorSpace = assets::TextureResourceColorSpace::Srgb;
    texture.alphaMode = assets::TextureResourceAlphaMode::Straight;
    texture.canonicalRgba8.resize(32U * 16U * 4U, 255U);
    const auto published = resources.PublishTexture("textures/tiles.png", std::move(texture));
    EXPECT_TRUE(published.Succeeded());

    TileTextureBinding2D binding{};
    EXPECT_TRUE(ResolveTileTextureBinding2D(set, resources, published.handle, binding).Succeeded());
    return binding;
}

CompiledTileMap MakeSmallMap()
{
    CompiledTileMap map{};
    map.semanticId = "room";
    map.tileSetSemanticId = "overworld";
    map.cellWidth = 16U;
    map.cellHeight = 16U;

    CompiledTileLayer ground{};
    ground.semanticId = "ground";
    ground.order = 0;
    ground.originX = 0;
    ground.originY = 0;
    ground.width = 3U;
    ground.height = 2U;
    ground.visible = true;
    ground.cells.resize(6U);
    ground.cells[0] = CompiledTileCell{0U, 0U, 0U, 0U, 0U};
    ground.cells[1] = CompiledTileCell{1U, 0x05U, 0U, 0U, 0U}; // flip X + quarter turn 1
    ground.cells[2] = CompiledTileCell{0U, 0U, 0U, 0U, 0U};
    ground.cells[3] = CompiledTileCell{0U, 0U, 0U, 0U, 0U};
    ground.cells[4] = CompiledTileCell{1U, 0x0AU, 0U, 0U, 0U}; // flip Y + quarter turn 2
    ground.cells[5] = CompiledTileCell{0U, 0U, 0U, 0U, 0U};

    CompiledTileLayer hidden = ground;
    hidden.semanticId = "hidden";
    hidden.order = 1;
    hidden.visible = false;
    map.layers = {ground, hidden};
    return map;
}
} // namespace

TEST(TilePresentation2DTests, ResolvesCanonicalTextureAndRejectsIdentityOrAlphaMismatch)
{
    const CompiledTileSet set = MakeTileSet();
    assets::ResourceRegistry resources{"."};
    const TileTextureBinding2D binding = PublishBinding(set, resources);
    EXPECT_EQ(binding.width, 32U);
    EXPECT_EQ(binding.height, 16U);
    EXPECT_EQ(binding.textureEncoding, render::SpriteTextureEncoding::Srgb);

    assets::TextureResource other{};
    other.width = 32U;
    other.height = 16U;
    other.canonicalRgba8.resize(32U * 16U * 4U, 255U);
    const auto otherPublished = resources.PublishTexture("textures/other.png", std::move(other));
    ASSERT_TRUE(otherPublished.Succeeded());
    TileTextureBinding2D rejected{};
    EXPECT_EQ(
        ResolveTileTextureBinding2D(set, resources, otherPublished.handle, rejected).error,
        TilePresentationError::TextureIdentityMismatch);

    assets::ResourceRegistry premultipliedResources{"."};
    assets::TextureResource premultiplied{};
    premultiplied.width = 32U;
    premultiplied.height = 16U;
    premultiplied.alphaMode = assets::TextureResourceAlphaMode::Premultiplied;
    premultiplied.canonicalRgba8.resize(32U * 16U * 4U, 255U);
    const auto premultipliedPublished =
        premultipliedResources.PublishTexture("textures/tiles.png", std::move(premultiplied));
    ASSERT_TRUE(premultipliedPublished.Succeeded());
    EXPECT_EQ(
        ResolveTileTextureBinding2D(set, premultipliedResources, premultipliedPublished.handle, rejected).error,
        TilePresentationError::UnsupportedTextureAlphaMode);
}

TEST(TilePresentation2DTests, BuildsCanonicalQuadUvTransformSampleBoundsAndPainterOrder)
{
    const CompiledTileSet set = MakeTileSet();
    CompiledTileMap map = MakeSmallMap();
    assets::ResourceRegistry resources{"."};
    const TileTextureBinding2D binding = PublishBinding(set, resources);
    const render::OrthographicView view{{1.5F, 1.0F}, {2.0F, 1.5F}, {0.5F, 0.5F}};

    std::array<render::SpritePresentationRenderData, 8U> output{};
    std::size_t count = 0U;
    TilePresentationMeasurement2D measurement{};
    const TilePresentationStatus status = BuildTileMapPresentation2D(
        set,
        map,
        binding,
        view,
        TilePresentationConfig2D{16.0F, 4, render::SpriteSamplerCompatibility::Nearest},
        output,
        count,
        measurement);
    ASSERT_TRUE(status.Succeeded());
    ASSERT_EQ(count, 6U);
    EXPECT_EQ(measurement.visibleLayers, 1U);
    EXPECT_EQ(measurement.emittedQuads, 6U);
    EXPECT_EQ(measurement.retainedChunkMetadataBytes, 0U);

    const auto& first = output[0];
    EXPECT_EQ(first.presentation.quad.topLeft.position, (render::Float2{0.0F, 0.0F}));
    EXPECT_EQ(first.presentation.quad.bottomRight.position, (render::Float2{1.0F, 1.0F}));
    EXPECT_EQ(first.presentation.quad.topLeft.uv, (render::Float2{0.0F, 0.0F}));
    EXPECT_EQ(first.presentation.quad.bottomRight.uv, (render::Float2{0.5F, 1.0F}));
    EXPECT_EQ(first.presentation.appearance.sampleBounds.minimum, (render::Float2{0.015625F, 0.03125F}));
    EXPECT_EQ(first.presentation.appearance.sampleBounds.maximum, (render::Float2{0.484375F, 0.96875F}));
    EXPECT_EQ(first.order.layer, 4);
    EXPECT_EQ(first.order.order, 0);
    EXPECT_EQ(first.order.stableOrder, 0U);

    const auto& transformed = output[1];
    EXPECT_EQ(transformed.presentation.quad.topLeft.uv, (render::Float2{1.0F, 1.0F}));
    EXPECT_EQ(transformed.presentation.quad.topRight.uv, (render::Float2{1.0F, 0.0F}));
    EXPECT_EQ(transformed.presentation.quad.bottomRight.uv, (render::Float2{0.5F, 0.0F}));
    EXPECT_EQ(transformed.presentation.quad.bottomLeft.uv, (render::Float2{0.5F, 1.0F}));
}

TEST(TilePresentation2DTests, CapacityFailureIsTransactionalAndReportsExactRequiredCount)
{
    const CompiledTileSet set = MakeTileSet();
    const CompiledTileMap map = MakeSmallMap();
    assets::ResourceRegistry resources{"."};
    const TileTextureBinding2D binding = PublishBinding(set, resources);
    const render::OrthographicView view{{1.5F, 1.0F}, {2.0F, 1.5F}, {0.5F, 0.5F}};

    render::SpritePresentationRenderData sentinel{};
    sentinel.order.layer = 12345;
    std::array<render::SpritePresentationRenderData, 2U> output{sentinel, sentinel};
    std::size_t required = 0U;
    TilePresentationMeasurement2D measurement{};
    const TilePresentationStatus status = BuildTileMapPresentation2D(
        set,
        map,
        binding,
        view,
        {},
        output,
        required,
        measurement);
    EXPECT_EQ(status.error, TilePresentationError::InsufficientCapacity);
    EXPECT_EQ(required, 6U);
    EXPECT_EQ(output[0].order.layer, 12345);
    EXPECT_EQ(output[1].order.layer, 12345);
}

TEST(TilePresentation2DTests, RepresentativeBoundedMapScansVisibleWindowAndBatchesOneAtlasRun)
{
    const CompiledTileSet set = MakeTileSet();
    assets::ResourceRegistry resources{"."};
    const TileTextureBinding2D binding = PublishBinding(set, resources);

    CompiledTileMap map{};
    map.semanticId = "representative_1024";
    map.tileSetSemanticId = "overworld";
    map.cellWidth = 16U;
    map.cellHeight = 16U;
    CompiledTileLayer layer{};
    layer.semanticId = "ground";
    layer.order = 0;
    layer.width = 1024U;
    layer.height = 1024U;
    layer.visible = true;
    layer.cells.resize(1024U * 1024U, CompiledTileCell{0U, 0U, 0U, 0U, 0U});
    map.layers.push_back(std::move(layer));

    const render::OrthographicView view{{512.0F, 512.0F}, {10.0F, 6.0F}, {0.1F, 1.0F / 6.0F}};
    std::array<render::SpritePresentationRenderData, 512U> output{};
    std::size_t count = 0U;
    TilePresentationMeasurement2D measurement{};
    const TilePresentationStatus status = BuildTileMapPresentation2D(
        set,
        map,
        binding,
        view,
        {},
        output,
        count,
        measurement);
    ASSERT_TRUE(status.Succeeded());
    EXPECT_EQ(measurement.totalLayerCells, 1024ULL * 1024ULL);
    EXPECT_LT(measurement.candidateCells, 400U);
    EXPECT_LT(measurement.candidateCells * 1000ULL, measurement.totalLayerCells);
    EXPECT_EQ(measurement.retainedChunkMetadataBytes, 0U);
    ASSERT_GT(count, 0U);

    std::vector<render::SpriteBatchItem2D> batchItems;
    batchItems.reserve(count);
    for (std::size_t index = 0U; index < count; ++index)
    {
        batchItems.push_back(render::SpriteBatchItem2D{
            render::SpriteBatchCompatibility2D{
                output[index].texture,
                output[index].materialPipeline,
                output[index].presentation.appearance.sampler,
                output[index].presentation.appearance.blend,
                output[index].mask,
            },
            1U,
            true,
        });
    }
    const render::SpritePresentationBatchMeasurement2D batching =
        render::MeasureContiguousSpritePresentationBatches(batchItems);
    EXPECT_EQ(batching.visibleQuads, count);
    EXPECT_EQ(batching.contiguousRuns, 1U);
}
} // namespace trace2d::tile
