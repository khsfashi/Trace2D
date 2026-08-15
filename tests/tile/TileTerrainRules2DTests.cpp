#include <trace2d/tile/TileTerrainRules2D.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string_view>

namespace trace2d::tile
{
namespace
{
struct TerrainFixture final
{
    CompiledTileSet tileSet{};
    TileMapDocument tileMap{};
};

TerrainFixture MakeFixture()
{
    TileSetDocument tileSetDocument{};
    tileSetDocument.semanticId = "overworld";
    tileSetDocument.textureReference = "textures/overworld.png";
    tileSetDocument.sourceWidth = 96;
    tileSetDocument.sourceHeight = 16;
    tileSetDocument.tiles = {
        TileDefinition{.semanticId = "grass_fallback", .sourceRegion = TileRegion{0, 0, 16, 16}},
        TileDefinition{.semanticId = "grass_n", .sourceRegion = TileRegion{16, 0, 16, 16}},
        TileDefinition{.semanticId = "grass_s", .sourceRegion = TileRegion{32, 0, 16, 16}},
        TileDefinition{.semanticId = "grass_ns", .sourceRegion = TileRegion{48, 0, 16, 16}},
        TileDefinition{.semanticId = "stone", .sourceRegion = TileRegion{64, 0, 16, 16}},
        TileDefinition{.semanticId = "water", .sourceRegion = TileRegion{80, 0, 16, 16}},
    };
    TileSetCompileResult tileSetResult = CompileTileSet(tileSetDocument);
    EXPECT_TRUE(tileSetResult.Succeeded());

    TileLayerDocument layer{};
    layer.semanticId = "ground";
    layer.order = 0;
    layer.width = 4U;
    layer.height = 3U;
    layer.cells = {
        TileCellDocument{.x = 1, .y = 1, .tileSemanticId = "stone", .transform = TileTransform{.flipX = true}},
        TileCellDocument{.x = 0, .y = 2, .tileSemanticId = "water", .transform = TileTransform{.flipY = true}},
    };

    TileMapDocument tileMap{};
    tileMap.semanticId = "room_a";
    tileMap.tileSetSemanticId = "overworld";
    tileMap.cellWidth = 16U;
    tileMap.cellHeight = 16U;
    tileMap.layers = {layer};

    return TerrainFixture{
        .tileSet = std::move(*tileSetResult.tileSet),
        .tileMap = std::move(tileMap),
    };
}

constexpr std::string_view TerrainToml = R"toml(format_version = 1
id = "room_a_terrain"
tile_set = "overworld"
tile_map = "room_a"
layer = "ground"

[[terrains]]
id = "grass"
fallback_tile = "grass_fallback"

[[rules]]
terrain = "grass"
neighbors = ["north", "south"]
tile = "grass_ns"

[[rules]]
terrain = "grass"
neighbors = ["south"]
tile = "grass_s"

[[rules]]
terrain = "grass"
neighbors = ["north"]
tile = "grass_n"

[[cells]]
cell = [1, 2]
terrain = "grass"

[[cells]]
cell = [3, 2]
terrain = "grass"

[[cells]]
cell = [1, 0]
terrain = "grass"

[[cells]]
cell = [1, 1]
terrain = "grass"
)toml";

bool HasDiagnosticPath(const std::vector<TileDiagnostic>& diagnostics, const std::string_view path)
{
    return std::any_of(diagnostics.begin(), diagnostics.end(), [path](const TileDiagnostic& diagnostic) {
        return diagnostic.path == path;
    });
}

const TileCellDocument* FindCell(const TileLayerDocument& layer, const std::int32_t x, const std::int32_t y)
{
    const auto iterator = std::find_if(layer.cells.begin(), layer.cells.end(), [x, y](const TileCellDocument& cell) {
        return cell.x == x && cell.y == y;
    });
    return iterator == layer.cells.end() ? nullptr : &*iterator;
}
} // namespace

TEST(TileTerrainRules2DTests, CanonicalParseAndSaveAreStable)
{
    const TileTerrainRuleLoadResult load = ParseTileTerrainRulesToml(TerrainToml, "room_a.terrain.toml");
    ASSERT_TRUE(load.Succeeded());

    const std::string canonical = SaveTileTerrainRulesToml(*load.document);
    const TileTerrainRuleLoadResult reparsed = ParseTileTerrainRulesToml(canonical, "canonical.terrain.toml");
    ASSERT_TRUE(reparsed.Succeeded());
    EXPECT_EQ(SaveTileTerrainRulesToml(*reparsed.document), canonical);

    ASSERT_EQ(reparsed.document->rules.size(), 3U);
    EXPECT_EQ(reparsed.document->rules[0].neighborMask, static_cast<std::uint8_t>(TerrainNeighbor::North));
    EXPECT_EQ(reparsed.document->rules[1].neighborMask, static_cast<std::uint8_t>(TerrainNeighbor::South));
    EXPECT_EQ(
        reparsed.document->rules[2].neighborMask,
        static_cast<std::uint8_t>(TerrainNeighbor::North) | static_cast<std::uint8_t>(TerrainNeighbor::South));
}

TEST(TileTerrainRules2DTests, CardinalRulesCompileToOrdinaryTileMapAndPreserveUnpaintedCells)
{
    const TerrainFixture fixture = MakeFixture();
    const TileTerrainRuleLoadResult load = ParseTileTerrainRulesToml(TerrainToml);
    ASSERT_TRUE(load.Succeeded());

    const TileTerrainCompileResult terrainResult = CompileTileTerrainRules(fixture.tileSet, fixture.tileMap, *load.document);
    ASSERT_TRUE(terrainResult.Succeeded());
    ASSERT_EQ(terrainResult.tileMap->layers.size(), 1U);

    const TileLayerDocument& outputLayer = terrainResult.tileMap->layers[0];
    ASSERT_EQ(outputLayer.cells.size(), 5U);

    const TileCellDocument* top = FindCell(outputLayer, 1, 0);
    const TileCellDocument* middle = FindCell(outputLayer, 1, 1);
    const TileCellDocument* bottom = FindCell(outputLayer, 1, 2);
    const TileCellDocument* isolated = FindCell(outputLayer, 3, 2);
    const TileCellDocument* preserved = FindCell(outputLayer, 0, 2);
    ASSERT_NE(top, nullptr);
    ASSERT_NE(middle, nullptr);
    ASSERT_NE(bottom, nullptr);
    ASSERT_NE(isolated, nullptr);
    ASSERT_NE(preserved, nullptr);

    EXPECT_EQ(top->tileSemanticId, "grass_s");
    EXPECT_EQ(middle->tileSemanticId, "grass_ns");
    EXPECT_EQ(bottom->tileSemanticId, "grass_n");
    EXPECT_EQ(isolated->tileSemanticId, "grass_fallback");
    EXPECT_EQ(middle->transform, TileTransform{});
    EXPECT_EQ(preserved->tileSemanticId, "water");
    EXPECT_TRUE(preserved->transform.flipY);

    const TileMapCompileResult compiled = CompileTileMap(fixture.tileSet, *terrainResult.tileMap);
    ASSERT_TRUE(compiled.Succeeded());
    const std::optional<TileCellInspection> inspection = InspectCell(fixture.tileSet, *compiled.tileMap, "ground", 1, 1);
    ASSERT_TRUE(inspection.has_value());
    EXPECT_TRUE(inspection->occupied);
    EXPECT_EQ(inspection->tileSemanticId, "grass_ns");
}

TEST(TileTerrainRules2DTests, CompileResultDoesNotDependOnPaintOrRuleInputOrder)
{
    const TerrainFixture fixture = MakeFixture();
    const TileTerrainRuleLoadResult load = ParseTileTerrainRulesToml(TerrainToml);
    ASSERT_TRUE(load.Succeeded());

    TileTerrainRuleDocument reversed = *load.document;
    std::reverse(reversed.rules.begin(), reversed.rules.end());
    std::reverse(reversed.cells.begin(), reversed.cells.end());

    const TileTerrainCompileResult first = CompileTileTerrainRules(fixture.tileSet, fixture.tileMap, *load.document);
    const TileTerrainCompileResult second = CompileTileTerrainRules(fixture.tileSet, fixture.tileMap, reversed);
    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(second.Succeeded());
    EXPECT_EQ(*first.tileMap, *second.tileMap);
}

TEST(TileTerrainRules2DTests, RejectsUnknownReferencesDuplicateMasksDuplicateCellsAndBounds)
{
    const TerrainFixture fixture = MakeFixture();

    TileTerrainRuleDocument document{};
    document.semanticId = "bad";
    document.tileSetSemanticId = "overworld";
    document.tileMapSemanticId = "room_a";
    document.layerSemanticId = "ground";
    document.terrains = {
        TerrainDefinitionDocument{.semanticId = "grass", .fallbackTileSemanticId = "missing_tile"},
    };
    document.rules = {
        TerrainVariantRuleDocument{.terrainSemanticId = "grass", .neighborMask = 1U, .tileSemanticId = "missing_output"},
        TerrainVariantRuleDocument{.terrainSemanticId = "grass", .neighborMask = 1U, .tileSemanticId = "grass_n"},
        TerrainVariantRuleDocument{.terrainSemanticId = "missing_terrain", .neighborMask = 2U, .tileSemanticId = "grass_n"},
    };
    document.cells = {
        TerrainPaintCellDocument{.x = 0, .y = 0, .terrainSemanticId = "grass"},
        TerrainPaintCellDocument{.x = 0, .y = 0, .terrainSemanticId = "grass"},
        TerrainPaintCellDocument{.x = 99, .y = 0, .terrainSemanticId = "missing_terrain"},
    };

    const std::vector<TileDiagnostic> diagnostics = ValidateTileTerrainRules(fixture.tileSet, fixture.tileMap, document);
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "terrains[0].fallback_tile"));
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "rules[1].neighbors"));
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "rules[0].tile"));
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "rules[2].terrain"));
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "cells[1].cell"));
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "cells[2].cell"));
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "cells[2].terrain"));

    const TileTerrainCompileResult compiled = CompileTileTerrainRules(fixture.tileSet, fixture.tileMap, document);
    EXPECT_FALSE(compiled.Succeeded());
    EXPECT_FALSE(compiled.tileMap.has_value());
}

TEST(TileTerrainRules2DTests, RejectsUnknownFieldsAndDuplicateNeighborNamesDuringParse)
{
    constexpr std::string_view invalid = R"toml(format_version = 1
id = "bad"
tile_set = "overworld"
tile_map = "room_a"
layer = "ground"

[[terrains]]
id = "grass"
fallback_tile = "grass_fallback"

[[rules]]
terrain = "grass"
neighbors = ["north", "north"]
tile = "grass_n"
unknown = true

[[cells]]
cell = [0, 0]
terrain = "grass"
)toml";

    const TileTerrainRuleLoadResult load = ParseTileTerrainRulesToml(invalid, "bad.terrain.toml");
    EXPECT_FALSE(load.Succeeded());
    EXPECT_TRUE(HasDiagnosticPath(load.diagnostics, "rules[0].neighbors[1]"));
    EXPECT_TRUE(HasDiagnosticPath(load.diagnostics, "rules[0].unknown"));
}
} // namespace trace2d::tile
