#include <trace2d/tile/GeneratedTileMap2D.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::tile
{
namespace
{
CompiledTileSet MakeTileSet()
{
    TileSetDocument document{};
    document.semanticId = "overworld";
    document.textureReference = "textures/overworld.png";
    document.sourceWidth = 48;
    document.sourceHeight = 16;
    document.tiles = {
        TileDefinition{.semanticId = "grass", .sourceRegion = TileRegion{0, 0, 16, 16}},
        TileDefinition{.semanticId = "stone", .sourceRegion = TileRegion{16, 0, 16, 16}},
        TileDefinition{.semanticId = "water", .sourceRegion = TileRegion{32, 0, 16, 16}},
    };

    TileSetCompileResult result = CompileTileSet(document);
    EXPECT_TRUE(result.Succeeded());
    return std::move(*result.tileSet);
}

constexpr std::string_view GeneratedToml = R"toml(format_version = 1
id = "generated_room"
tile_set = "overworld"
cell_size = [16, 16]
tile_table = ["stone", "grass", "water"]

[[layers]]
id = "decor"
order = 4
origin = [10, -2]
size = [3, 2]
visible = false
tiles = [-1, 2, -1, 1, -1, -1]

[[layers]]
id = "ground"
order = 0
origin = [0, 0]
size = [3, 2]
visible = true
tiles = [1, 0, -1, 2, 1, 0]
transforms = [0, 4, 0, 9, 2, 0]
)toml";

bool HasDiagnosticPath(const std::vector<TileDiagnostic>& diagnostics, const std::string_view path)
{
    return std::any_of(diagnostics.begin(), diagnostics.end(), [path](const TileDiagnostic& diagnostic) {
        return diagnostic.path == path;
    });
}
} // namespace

TEST(GeneratedTileMap2DTests, CanonicalSaveReordersTileTableAndLayersDeterministically)
{
    const GeneratedTileMapLoadResult load = ParseGeneratedTileMapToml(GeneratedToml, "generated.tilemap.toml");
    ASSERT_TRUE(load.Succeeded());

    const std::string canonical = SaveGeneratedTileMapToml(*load.document);
    const GeneratedTileMapLoadResult reparsed = ParseGeneratedTileMapToml(canonical, "canonical.tilemap.toml");
    ASSERT_TRUE(reparsed.Succeeded());
    EXPECT_EQ(SaveGeneratedTileMapToml(*reparsed.document), canonical);

    ASSERT_EQ(reparsed.document->tileTable.size(), 3U);
    EXPECT_EQ(reparsed.document->tileTable[0], "grass");
    EXPECT_EQ(reparsed.document->tileTable[1], "stone");
    EXPECT_EQ(reparsed.document->tileTable[2], "water");
    ASSERT_EQ(reparsed.document->layers.size(), 2U);
    EXPECT_EQ(reparsed.document->layers[0].semanticId, "ground");
    EXPECT_EQ(reparsed.document->layers[1].semanticId, "decor");

    ASSERT_EQ(reparsed.document->layers[0].tileTableIndices.size(), 6U);
    EXPECT_EQ(reparsed.document->layers[0].tileTableIndices[0], 0);
    EXPECT_EQ(reparsed.document->layers[0].tileTableIndices[1], 1);
    EXPECT_EQ(reparsed.document->layers[0].tileTableIndices[2], GeneratedEmptyTileTableIndex);
    EXPECT_EQ(reparsed.document->layers[0].tileTableIndices[3], 2);
}

TEST(GeneratedTileMap2DTests, ConversionIsIndependentOfTileTableInputOrder)
{
    const CompiledTileSet tileSet = MakeTileSet();
    const GeneratedTileMapLoadResult load = ParseGeneratedTileMapToml(GeneratedToml);
    ASSERT_TRUE(load.Succeeded());

    GeneratedTileMapDocument reordered = *load.document;
    reordered.tileTable = {"water", "stone", "grass"};
    for (GeneratedTileLayerDocument& layer : reordered.layers)
    {
        for (std::int32_t& index : layer.tileTableIndices)
        {
            if (index == GeneratedEmptyTileTableIndex)
            {
                continue;
            }
            // Old [stone, grass, water] -> new [water, stone, grass].
            constexpr std::int32_t Remap[] = {1, 2, 0};
            index = Remap[index];
        }
    }
    std::reverse(reordered.layers.begin(), reordered.layers.end());

    const GeneratedTileMapConversionResult first = ConvertGeneratedTileMap(tileSet, *load.document);
    const GeneratedTileMapConversionResult second = ConvertGeneratedTileMap(tileSet, reordered);
    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(second.Succeeded());
    EXPECT_EQ(*first.tileMap, *second.tileMap);
}

TEST(GeneratedTileMap2DTests, ConvertedMapUsesExistingCompileAndSemanticInspectionPath)
{
    const CompiledTileSet tileSet = MakeTileSet();
    const GeneratedTileMapLoadResult load = ParseGeneratedTileMapToml(GeneratedToml);
    ASSERT_TRUE(load.Succeeded());

    const GeneratedTileMapConversionResult converted = ConvertGeneratedTileMap(tileSet, *load.document);
    ASSERT_TRUE(converted.Succeeded());
    EXPECT_EQ(converted.metrics.denseCellCount, 12U);
    EXPECT_EQ(converted.metrics.occupiedCellCount, 7U);
    EXPECT_EQ(converted.metrics.knownDensePayloadBytes, 54U);
    EXPECT_EQ(converted.metrics.tileTableEntries, 3U);
    EXPECT_GE(converted.metrics.outputCellCapacity, 7U);

    const TileMapCompileResult compiled = CompileTileMap(tileSet, *converted.tileMap);
    ASSERT_TRUE(compiled.Succeeded());

    const std::optional<TileCellInspection> stone = InspectCell(tileSet, *compiled.tileMap, "ground", 1, 0);
    ASSERT_TRUE(stone.has_value());
    EXPECT_TRUE(stone->occupied);
    EXPECT_EQ(stone->tileSemanticId, "stone");
    EXPECT_TRUE(stone->transform.flipX);
    EXPECT_FALSE(stone->transform.flipY);
    EXPECT_EQ(stone->transform.quarterTurns, 0U);

    const std::optional<TileCellInspection> water = InspectCell(tileSet, *compiled.tileMap, "ground", 0, 1);
    ASSERT_TRUE(water.has_value());
    EXPECT_EQ(water->tileSemanticId, "water");
    EXPECT_FALSE(water->transform.flipX);
    EXPECT_TRUE(water->transform.flipY);
    EXPECT_EQ(water->transform.quarterTurns, 1U);
}

TEST(GeneratedTileMap2DTests, InvalidGeneratedStateFailsTransactionally)
{
    const CompiledTileSet tileSet = MakeTileSet();
    const GeneratedTileMapLoadResult load = ParseGeneratedTileMapToml(GeneratedToml);
    ASSERT_TRUE(load.Succeeded());

    GeneratedTileMapDocument invalid = *load.document;
    invalid.tileTable.push_back("missing_tile");
    invalid.layers[0].tileTableIndices.pop_back();
    invalid.layers[1].tileTableIndices[2] = 99;
    invalid.layers[1].transformBits[2] = 0x10U;

    const std::vector<TileDiagnostic> diagnostics = ValidateGeneratedTileMap(tileSet, invalid);
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "tile_table[3]"));
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "layers[0].tiles"));
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "layers[1].tiles[2]"));
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "layers[1].transforms[2]"));

    const GeneratedTileMapConversionResult converted = ConvertGeneratedTileMap(tileSet, invalid);
    EXPECT_FALSE(converted.Succeeded());
    EXPECT_FALSE(converted.tileMap.has_value());
}

TEST(GeneratedTileMap2DTests, ParserRejectsUnknownFieldsBadCardinalityAndInvalidTransforms)
{
    constexpr std::string_view invalid = R"toml(format_version = 1
id = "bad"
tile_set = "overworld"
cell_size = [16, 16]
tile_table = ["grass"]
unknown = true

[[layers]]
id = "ground"
order = 0
origin = [0, 0]
size = [2, 2]
tiles = [0, -1, 0]
transforms = [0, 0, 16, 0]
)toml";

    const GeneratedTileMapLoadResult load = ParseGeneratedTileMapToml(invalid, "bad.generated.toml");
    EXPECT_FALSE(load.Succeeded());
    EXPECT_TRUE(HasDiagnosticPath(load.diagnostics, "unknown"));
    EXPECT_TRUE(HasDiagnosticPath(load.diagnostics, "layers[0].tiles"));
    EXPECT_TRUE(HasDiagnosticPath(load.diagnostics, "layers[0].transforms[2]"));
}

TEST(GeneratedTileMap2DTests, RepresentativeGeneratedMapKeepsExistingBoundedDenseRuntimeContract)
{
    const CompiledTileSet tileSet = MakeTileSet();
    constexpr std::uint32_t Width = 1024U;
    constexpr std::uint32_t Height = 1024U;
    constexpr std::size_t CellCount = static_cast<std::size_t>(Width) * static_cast<std::size_t>(Height);

    GeneratedTileMapDocument document{};
    document.semanticId = "generated_large";
    document.tileSetSemanticId = "overworld";
    document.cellWidth = 16U;
    document.cellHeight = 16U;
    document.tileTable = {"grass"};

    GeneratedTileLayerDocument layer{};
    layer.semanticId = "ground";
    layer.width = Width;
    layer.height = Height;
    layer.tileTableIndices.assign(CellCount, GeneratedEmptyTileTableIndex);
    for (std::size_t index = 0U; index < CellCount; index += 256U)
    {
        layer.tileTableIndices[index] = 0;
    }
    document.layers.push_back(std::move(layer));

    const GeneratedTileMapConversionResult converted = ConvertGeneratedTileMap(tileSet, document);
    ASSERT_TRUE(converted.Succeeded());
    EXPECT_EQ(converted.metrics.denseCellCount, CellCount);
    EXPECT_EQ(converted.metrics.occupiedCellCount, CellCount / 256U);
    EXPECT_EQ(
        converted.metrics.knownDensePayloadBytes,
        static_cast<std::uint64_t>(CellCount) * sizeof(std::int32_t));

    const TileMapCompileResult compiled = CompileTileMap(tileSet, *converted.tileMap);
    ASSERT_TRUE(compiled.Succeeded());
    ASSERT_EQ(compiled.tileMap->layers.size(), 1U);
    EXPECT_EQ(compiled.tileMap->layers[0].cells.size(), CellCount);
    EXPECT_EQ(sizeof(CompiledTileCell), 8U);
}
} // namespace trace2d::tile
