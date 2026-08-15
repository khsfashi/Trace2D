#include <trace2d/tile/GeneratedTileMap2D.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>
#include <vector>

namespace trace2d::tile
{
namespace
{
CompiledTileSet MakeValidationTileSet()
{
    TileSetDocument document{};
    document.semanticId = "overworld";
    document.textureReference = "textures/overworld.png";
    document.sourceWidth = 16;
    document.sourceHeight = 16;
    document.tiles = {
        TileDefinition{.semanticId = "grass", .sourceRegion = TileRegion{0, 0, 16, 16}},
    };

    TileSetCompileResult result = CompileTileSet(document);
    EXPECT_TRUE(result.Succeeded());
    return std::move(*result.tileSet);
}

bool HasValidationPath(const std::vector<TileDiagnostic>& diagnostics, const std::string_view path)
{
    return std::any_of(diagnostics.begin(), diagnostics.end(), [path](const TileDiagnostic& diagnostic) {
        return diagnostic.path == path;
    });
}
} // namespace

TEST(GeneratedTileMap2DValidationTests, ShortTransformPayloadFailsSafelyAndTransactionally)
{
    const CompiledTileSet tileSet = MakeValidationTileSet();

    GeneratedTileMapDocument document{};
    document.semanticId = "malformed";
    document.tileSetSemanticId = "overworld";
    document.cellWidth = 16U;
    document.cellHeight = 16U;
    document.tileTable = {"grass"};

    GeneratedTileLayerDocument layer{};
    layer.semanticId = "ground";
    layer.width = 2U;
    layer.height = 2U;
    layer.tileTableIndices = {0, GeneratedEmptyTileTableIndex, 0, 0};
    layer.transformBits = {0U};
    document.layers.push_back(std::move(layer));

    const std::vector<TileDiagnostic> diagnostics = ValidateGeneratedTileMap(tileSet, document);
    EXPECT_TRUE(HasValidationPath(diagnostics, "layers[0].transforms"));

    const GeneratedTileMapConversionResult converted = ConvertGeneratedTileMap(tileSet, document);
    EXPECT_FALSE(converted.Succeeded());
    EXPECT_FALSE(converted.tileMap.has_value());
}
} // namespace trace2d::tile
