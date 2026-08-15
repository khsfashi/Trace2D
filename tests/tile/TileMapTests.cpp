#include <trace2d/tile/TileMap.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <type_traits>

namespace trace2d::tile
{
namespace
{
constexpr std::string_view TileSetToml = R"toml(format_version = 1
id = "overworld"
texture = "textures/overworld.png"
source_size = [64, 32]

[[tiles]]
id = "wall"
region = [16, 0, 16, 16]
tags = ["solid", "blocking"]

[[tiles]]
id = "grass"
region = [0, 0, 16, 16]
tags = ["walkable", "ground"]
)toml";

constexpr std::string_view TileMapToml = R"toml(format_version = 1
id = "room_a"
tile_set = "overworld"
cell_size = [16, 16]

[[layers]]
id = "decor"
order = 10
origin = [0, 0]
size = [2, 2]
visible = true

[[layers.cells]]
x = 1
y = 0
tile = "wall"
flip_x = true
rotation_quarters = 1

[[layers]]
id = "ground"
order = 0
origin = [-2, 3]
size = [4, 3]
visible = true

[[layers.cells]]
x = 1
y = 1
tile = "grass"

[[layers.cells]]
x = 2
y = 1
tile = "wall"
flip_y = true
rotation_quarters = 2
)toml";

std::filesystem::path TempProject(const std::string_view name)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / ("trace2d_tile_" + std::string{name});
    std::error_code error{};
    std::filesystem::remove_all(root, error);
    return root;
}
} // namespace

static_assert(std::is_trivially_copyable_v<CompiledTileCell>);
static_assert(sizeof(CompiledTileCell) == 8U);

TEST(TileMapTests, ParsesCanonicalizesCompilesAndInspectsWithoutPerCellStrings)
{
    const TileSetLoadResult tileSetLoad = ParseTileSetToml(TileSetToml, "overworld.tileset.toml");
    ASSERT_TRUE(tileSetLoad.Succeeded());
    const TileMapLoadResult tileMapLoad = ParseTileMapToml(TileMapToml, "room.tilemap.toml");
    ASSERT_TRUE(tileMapLoad.Succeeded());

    const std::string canonicalTileSet = SaveTileSetToml(*tileSetLoad.document);
    const std::string canonicalTileMap = SaveTileMapToml(*tileMapLoad.document);
    const TileSetLoadResult reparsedTileSet = ParseTileSetToml(canonicalTileSet, "canonical.tileset.toml");
    const TileMapLoadResult reparsedTileMap = ParseTileMapToml(canonicalTileMap, "canonical.tilemap.toml");
    ASSERT_TRUE(reparsedTileSet.Succeeded());
    ASSERT_TRUE(reparsedTileMap.Succeeded());
    EXPECT_EQ(SaveTileSetToml(*reparsedTileSet.document), canonicalTileSet);
    EXPECT_EQ(SaveTileMapToml(*reparsedTileMap.document), canonicalTileMap);

    const TileSetCompileResult compiledTileSetResult = CompileTileSet(*reparsedTileSet.document);
    ASSERT_TRUE(compiledTileSetResult.Succeeded());
    ASSERT_EQ(compiledTileSetResult.tileSet->tiles.size(), 2U);
    EXPECT_EQ(compiledTileSetResult.tileSet->tiles[0].semanticId, "grass");
    EXPECT_EQ(compiledTileSetResult.tileSet->tiles[1].semanticId, "wall");

    const TileMapCompileResult compiledMapResult = CompileTileMap(*compiledTileSetResult.tileSet, *reparsedTileMap.document);
    ASSERT_TRUE(compiledMapResult.Succeeded());
    ASSERT_EQ(compiledMapResult.tileMap->layers.size(), 2U);
    EXPECT_EQ(compiledMapResult.tileMap->layers[0].semanticId, "ground");
    EXPECT_EQ(compiledMapResult.tileMap->layers[1].semanticId, "decor");

    const std::optional<std::size_t> groundIndex = FindLayerIndex(*compiledMapResult.tileMap, "ground");
    ASSERT_TRUE(groundIndex.has_value());
    const CompiledTileCell* grassCell = CellAtWorld(*compiledMapResult.tileMap, *groundIndex, -1, 4);
    ASSERT_NE(grassCell, nullptr);
    EXPECT_FALSE(grassCell->Empty());
    EXPECT_EQ(grassCell->tileIndex, 0U);
    EXPECT_EQ(grassCell->Transform(), TileTransform{});

    const std::optional<TileCellInspection> grassInspection = InspectCell(
        *compiledTileSetResult.tileSet,
        *compiledMapResult.tileMap,
        "ground",
        -1,
        4);
    ASSERT_TRUE(grassInspection.has_value());
    EXPECT_TRUE(grassInspection->occupied);
    EXPECT_EQ(grassInspection->tileSemanticId, "grass");
    EXPECT_EQ(grassInspection->sourceRegion, (TileRegion{0, 0, 16, 16}));
    ASSERT_EQ(grassInspection->tags.size(), 2U);
    EXPECT_EQ(grassInspection->tags[0], "ground");
    EXPECT_EQ(grassInspection->tags[1], "walkable");

    const std::optional<TileCellInspection> emptyInspection = InspectCell(
        *compiledTileSetResult.tileSet,
        *compiledMapResult.tileMap,
        "ground",
        -2,
        3);
    ASSERT_TRUE(emptyInspection.has_value());
    EXPECT_FALSE(emptyInspection->occupied);
    EXPECT_TRUE(emptyInspection->tileSemanticId.empty());

    const std::optional<TileCellInspection> decorInspection = InspectCell(
        *compiledTileSetResult.tileSet,
        *compiledMapResult.tileMap,
        "decor",
        1,
        0);
    ASSERT_TRUE(decorInspection.has_value());
    EXPECT_TRUE(decorInspection->occupied);
    EXPECT_EQ(decorInspection->tileSemanticId, "wall");
    EXPECT_EQ(decorInspection->transform, (TileTransform{true, false, 1U}));
}

TEST(TileMapTests, RejectsInvalidGeometryDuplicatesAndUnknownTileReferences)
{
    TileSetDocument tileSet{};
    tileSet.semanticId = "set";
    tileSet.textureReference = "textures/set.png";
    tileSet.sourceWidth = 16;
    tileSet.sourceHeight = 16;
    tileSet.tiles = {
        TileDefinition{"same", TileRegion{0, 0, 16, 16}, {}},
        TileDefinition{"same", TileRegion{8, 8, 16, 16}, {}},
    };
    EXPECT_FALSE(ValidateTileSet(tileSet).empty());

    tileSet.tiles = {TileDefinition{"known", TileRegion{0, 0, 16, 16}, {"ground"}}};
    const TileSetCompileResult compiledTileSet = CompileTileSet(tileSet);
    ASSERT_TRUE(compiledTileSet.Succeeded());

    TileMapDocument tileMap{};
    tileMap.semanticId = "map";
    tileMap.tileSetSemanticId = "set";
    tileMap.cellWidth = 16U;
    tileMap.cellHeight = 16U;
    TileLayerDocument layer{};
    layer.semanticId = "ground";
    layer.width = 2U;
    layer.height = 2U;
    layer.cells = {
        TileCellDocument{0, 0, "missing", TileTransform{}},
        TileCellDocument{0, 0, "known", TileTransform{}},
        TileCellDocument{2, 0, "known", TileTransform{}},
    };
    tileMap.layers.push_back(layer);

    const std::vector<TileDiagnostic> structural = ValidateTileMap(tileMap);
    ASSERT_FALSE(structural.empty());

    tileMap.layers[0].cells = {TileCellDocument{1, 1, "missing", TileTransform{false, false, 3U}}};
    EXPECT_TRUE(ValidateTileMap(tileMap).empty());
    const TileMapCompileResult compileResult = CompileTileMap(*compiledTileSet.tileSet, tileMap);
    ASSERT_FALSE(compileResult.Succeeded());
    ASSERT_FALSE(compileResult.diagnostics.empty());
    EXPECT_EQ(compileResult.diagnostics.front().code, TileErrorCode::SchemaError);
}

TEST(TileMapTests, StrictParsingRejectsUnknownFieldsAndInvalidTransform)
{
    constexpr std::string_view invalid = R"toml(format_version = 1
id = "map"
tile_set = "set"
cell_size = [16, 16]
unknown = 5

[[layers]]
id = "ground"
order = 0
origin = [0, 0]
size = [1, 1]

[[layers.cells]]
x = 0
y = 0
tile = "grass"
rotation_quarters = 4
)toml";

    const TileMapLoadResult result = ParseTileMapToml(invalid, "invalid.tilemap.toml");
    EXPECT_FALSE(result.Succeeded());
    ASSERT_GE(result.diagnostics.size(), 2U);
}

TEST(TileMapTests, ProjectStoreRejectsTraversalAndRoundTripsCanonicalDocuments)
{
    const TileSetLoadResult tileSetLoad = ParseTileSetToml(TileSetToml);
    const TileMapLoadResult tileMapLoad = ParseTileMapToml(TileMapToml);
    ASSERT_TRUE(tileSetLoad.Succeeded());
    ASSERT_TRUE(tileMapLoad.Succeeded());

    const std::filesystem::path root = TempProject("store");
    TileDocumentStore store{root};
    EXPECT_FALSE(store.SaveTileSet("../outside.tileset.toml", *tileSetLoad.document).empty());
    EXPECT_FALSE(store.SaveTileMap("C:/outside.tilemap.toml", *tileMapLoad.document).empty());

    EXPECT_TRUE(store.SaveTileSet("content/world.tileset.toml", *tileSetLoad.document).empty());
    EXPECT_TRUE(store.SaveTileMap("content/room.tilemap.toml", *tileMapLoad.document).empty());

    const TileSetLoadResult loadedSet = store.LoadTileSet("content/world.tileset.toml");
    const TileMapLoadResult loadedMap = store.LoadTileMap("content/room.tilemap.toml");
    ASSERT_TRUE(loadedSet.Succeeded());
    ASSERT_TRUE(loadedMap.Succeeded());
    EXPECT_EQ(SaveTileSetToml(*loadedSet.document), SaveTileSetToml(*tileSetLoad.document));
    EXPECT_EQ(SaveTileMapToml(*loadedMap.document), SaveTileMapToml(*tileMapLoad.document));

    std::error_code error{};
    std::filesystem::remove_all(root, error);
}

TEST(TileMapTests, CompiledSafetyBoundRejectsUnboundedDenseAllocation)
{
    TileMapDocument map{};
    map.semanticId = "too_large";
    map.tileSetSemanticId = "set";
    map.cellWidth = 16U;
    map.cellHeight = 16U;
    TileLayerDocument layer{};
    layer.semanticId = "ground";
    layer.width = 4096U;
    layer.height = 4096U;
    map.layers.push_back(layer);

    const std::vector<TileDiagnostic> diagnostics = ValidateTileMap(map);
    ASSERT_FALSE(diagnostics.empty());
    EXPECT_EQ(diagnostics.front().code, TileErrorCode::SchemaError);
}
} // namespace trace2d::tile
