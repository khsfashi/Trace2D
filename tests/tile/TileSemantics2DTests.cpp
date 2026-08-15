#include <trace2d/tile/TileSemantics2D.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>
#include <string_view>
#include <type_traits>

namespace trace2d::tile
{
namespace
{
struct TileFixture final
{
    CompiledTileSet tileSet{};
    CompiledTileMap tileMap{};
};

TileFixture MakeFixture()
{
    TileSetDocument tileSetDocument{};
    tileSetDocument.semanticId = "overworld";
    tileSetDocument.textureReference = "textures/overworld.png";
    tileSetDocument.sourceWidth = 48;
    tileSetDocument.sourceHeight = 16;
    tileSetDocument.tiles = {
        TileDefinition{.semanticId = "wall", .sourceRegion = TileRegion{16, 0, 16, 16}, .tags = {"blocking", "solid"}},
        TileDefinition{.semanticId = "grass", .sourceRegion = TileRegion{0, 0, 16, 16}, .tags = {"ground", "walkable"}},
        TileDefinition{.semanticId = "water", .sourceRegion = TileRegion{32, 0, 16, 16}, .tags = {"water"}},
    };
    TileSetCompileResult tileSetResult = CompileTileSet(tileSetDocument);
    EXPECT_TRUE(tileSetResult.Succeeded());

    TileLayerDocument ground{};
    ground.semanticId = "ground";
    ground.order = 0;
    ground.originX = -2;
    ground.originY = 3;
    ground.width = 4U;
    ground.height = 3U;
    ground.visible = true;
    ground.cells = {
        TileCellDocument{.x = 1, .y = 1, .tileSemanticId = "grass"},
        TileCellDocument{.x = 2, .y = 1, .tileSemanticId = "wall"},
        TileCellDocument{.x = 3, .y = 1, .tileSemanticId = "water"},
    };

    TileMapDocument tileMapDocument{};
    tileMapDocument.semanticId = "room_a";
    tileMapDocument.tileSetSemanticId = "overworld";
    tileMapDocument.cellWidth = 16U;
    tileMapDocument.cellHeight = 16U;
    tileMapDocument.layers = {ground};
    TileMapCompileResult tileMapResult = CompileTileMap(*tileSetResult.tileSet, tileMapDocument);
    EXPECT_TRUE(tileMapResult.Succeeded());

    return TileFixture{
        .tileSet = std::move(*tileSetResult.tileSet),
        .tileMap = std::move(*tileMapResult.tileMap),
    };
}

bool HasDiagnosticPath(const std::vector<TileDiagnostic>& diagnostics, const std::string_view path)
{
    return std::any_of(diagnostics.begin(), diagnostics.end(), [path](const TileDiagnostic& diagnostic) {
        return diagnostic.path == path;
    });
}

constexpr std::string_view OverlayToml = R"toml(format_version = 1
id = "room_a_semantics"
tile_set = "overworld"
tile_map = "room_a"

[[rules]]
tile = "wall"
collision = "solid"
navigation = "blocked"
occlusion = "opaque"

[[rules]]
tile = "grass"
navigation = "walkable"

[[markers]]
id = "exit_east"
kind = "transition"
layer = "ground"
cell = [1, 4]
tags = ["east", "room_b"]

[[markers]]
id = "player_spawn"
kind = "spawn.player"
layer = "ground"
cell = [-2, 3]
tags = ["primary", "checkpoint"]
)toml";
} // namespace

static_assert(std::is_trivially_copyable_v<CompiledTileSemanticState>);
static_assert(sizeof(CompiledTileSemanticState) == 4U);

TEST(TileSemantics2DTests, CanonicalParseCompileAndCellHandoffStayResolvedAndCompact)
{
    const TileFixture fixture = MakeFixture();
    const TileSemanticOverlayLoadResult load = ParseTileSemanticOverlayToml(OverlayToml, "room_a.tilemeta.toml");
    ASSERT_TRUE(load.Succeeded());

    const std::string canonical = SaveTileSemanticOverlayToml(*load.document);
    const TileSemanticOverlayLoadResult reparsed = ParseTileSemanticOverlayToml(canonical, "canonical.tilemeta.toml");
    ASSERT_TRUE(reparsed.Succeeded());
    EXPECT_EQ(SaveTileSemanticOverlayToml(*reparsed.document), canonical);

    const TileSemanticOverlayCompileResult compiled = CompileTileSemanticOverlay(
        fixture.tileSet,
        fixture.tileMap,
        *reparsed.document);
    ASSERT_TRUE(compiled.Succeeded());
    ASSERT_EQ(compiled.overlay->tileStates.size(), fixture.tileSet.tiles.size());

    const std::optional<std::size_t> groundIndex = FindLayerIndex(fixture.tileMap, "ground");
    ASSERT_TRUE(groundIndex.has_value());

    const CompiledTileSemanticState* grass = SemanticStateForCell(
        *compiled.overlay,
        fixture.tileMap,
        *groundIndex,
        -1,
        4);
    ASSERT_NE(grass, nullptr);
    EXPECT_EQ(grass->collision, TileCollisionHandoff::None);
    EXPECT_EQ(grass->navigation, TileNavigationHandoff::Walkable);
    EXPECT_EQ(grass->occlusion, TileOcclusionHandoff::None);

    const CompiledTileSemanticState* wall = SemanticStateForCell(
        *compiled.overlay,
        fixture.tileMap,
        *groundIndex,
        0,
        4);
    ASSERT_NE(wall, nullptr);
    EXPECT_EQ(wall->collision, TileCollisionHandoff::Solid);
    EXPECT_EQ(wall->navigation, TileNavigationHandoff::Blocked);
    EXPECT_EQ(wall->occlusion, TileOcclusionHandoff::Opaque);

    const CompiledTileSemanticState* water = SemanticStateForCell(
        *compiled.overlay,
        fixture.tileMap,
        *groundIndex,
        1,
        4);
    ASSERT_NE(water, nullptr);
    EXPECT_EQ(*water, CompiledTileSemanticState{});

    EXPECT_EQ(SemanticStateForCell(*compiled.overlay, fixture.tileMap, *groundIndex, -2, 3), nullptr);
}

TEST(TileSemantics2DTests, SemanticMarkersAreIndependentFromRenderedOccupancy)
{
    const TileFixture fixture = MakeFixture();
    const TileSemanticOverlayLoadResult load = ParseTileSemanticOverlayToml(OverlayToml);
    ASSERT_TRUE(load.Succeeded());
    const TileSemanticOverlayCompileResult compiled = CompileTileSemanticOverlay(fixture.tileSet, fixture.tileMap, *load.document);
    ASSERT_TRUE(compiled.Succeeded());

    const std::optional<TileMarkerInspection> spawn = InspectMarker(*compiled.overlay, fixture.tileMap, "player_spawn");
    ASSERT_TRUE(spawn.has_value());
    EXPECT_EQ(spawn->kind, "spawn.player");
    EXPECT_EQ(spawn->layerSemanticId, "ground");
    EXPECT_EQ(spawn->worldX, -2);
    EXPECT_EQ(spawn->worldY, 3);
    ASSERT_EQ(spawn->tags.size(), 2U);
    EXPECT_EQ(spawn->tags[0], "checkpoint");
    EXPECT_EQ(spawn->tags[1], "primary");

    const CompiledTileCell* renderCell = CellAtWorld(fixture.tileMap, spawn->layerIndex, spawn->worldX, spawn->worldY);
    ASSERT_NE(renderCell, nullptr);
    EXPECT_TRUE(renderCell->Empty());

    ASSERT_EQ(compiled.overlay->markers.size(), 2U);
    EXPECT_EQ(compiled.overlay->markers[0].semanticId, "exit_east");
    EXPECT_EQ(compiled.overlay->markers[1].semanticId, "player_spawn");
    EXPECT_FALSE(InspectMarker(*compiled.overlay, fixture.tileMap, "missing").has_value());
}

TEST(TileSemantics2DTests, RejectsUnknownEnumsAndUnknownFieldsDuringParse)
{
    constexpr std::string_view invalid = R"toml(format_version = 1
id = "bad"
tile_set = "overworld"
tile_map = "room_a"

[[rules]]
tile = "wall"
collision = "capsule"
unknown = true
)toml";

    const TileSemanticOverlayLoadResult load = ParseTileSemanticOverlayToml(invalid, "bad.tilemeta.toml");
    EXPECT_FALSE(load.Succeeded());
    EXPECT_TRUE(HasDiagnosticPath(load.diagnostics, "rules[0].collision"));
    EXPECT_TRUE(HasDiagnosticPath(load.diagnostics, "rules[0].unknown"));
}

TEST(TileSemantics2DTests, RejectsIdentityUnknownReferencesDuplicatesAndOutOfBoundsMarkers)
{
    const TileFixture fixture = MakeFixture();

    TileSemanticOverlayDocument document{};
    document.semanticId = "bad_semantics";
    document.tileSetSemanticId = "wrong_tiles";
    document.tileMapSemanticId = "room_a";
    document.rules = {
        TileSemanticRuleDocument{.tileSemanticId = "unknown", .collision = TileCollisionHandoff::Solid},
        TileSemanticRuleDocument{.tileSemanticId = "unknown", .navigation = TileNavigationHandoff::Blocked},
    };
    document.markers = {
        TileMarkerDocument{.semanticId = "spawn", .kind = "spawn.player", .layerSemanticId = "missing", .worldX = 0, .worldY = 0},
        TileMarkerDocument{.semanticId = "spawn", .kind = "spawn.player", .layerSemanticId = "ground", .worldX = 99, .worldY = 99},
    };

    const std::vector<TileDiagnostic> diagnostics = ValidateTileSemanticOverlay(fixture.tileSet, fixture.tileMap, document);
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "tile_set"));
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "rules[1].tile"));
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "rules[0].tile"));
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "markers[0].layer"));
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "markers[1].id"));
    EXPECT_TRUE(HasDiagnosticPath(diagnostics, "markers[1].cell"));

    const TileSemanticOverlayCompileResult compiled = CompileTileSemanticOverlay(fixture.tileSet, fixture.tileMap, document);
    EXPECT_FALSE(compiled.Succeeded());
    EXPECT_FALSE(compiled.overlay.has_value());
}
} // namespace trace2d::tile
