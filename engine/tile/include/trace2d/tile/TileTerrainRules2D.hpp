#pragma once

#include <trace2d/tile/TileMap.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::tile
{
enum class TerrainNeighbor : std::uint8_t
{
    North = 0x01U,
    East = 0x02U,
    South = 0x04U,
    West = 0x08U,
};

inline constexpr std::uint8_t TerrainNeighborMaskAll = 0x0FU;

struct TerrainDefinitionDocument final
{
    std::string semanticId{};
    std::string fallbackTileSemanticId{};

    [[nodiscard]] bool operator==(const TerrainDefinitionDocument&) const noexcept = default;
};

struct TerrainVariantRuleDocument final
{
    std::string terrainSemanticId{};
    std::uint8_t neighborMask{0U};
    std::string tileSemanticId{};

    [[nodiscard]] bool operator==(const TerrainVariantRuleDocument&) const noexcept = default;
};

struct TerrainPaintCellDocument final
{
    std::int32_t x{0};
    std::int32_t y{0};
    std::string terrainSemanticId{};

    [[nodiscard]] bool operator==(const TerrainPaintCellDocument&) const noexcept = default;
};

struct TileTerrainRuleDocument final
{
    static constexpr std::int64_t FormatVersion = 1;

    std::string semanticId{};
    std::string tileSetSemanticId{};
    std::string tileMapSemanticId{};
    std::string layerSemanticId{};
    std::vector<TerrainDefinitionDocument> terrains{};
    std::vector<TerrainVariantRuleDocument> rules{};
    std::vector<TerrainPaintCellDocument> cells{};

    [[nodiscard]] bool operator==(const TileTerrainRuleDocument&) const noexcept = default;
};

struct TileTerrainRuleLoadResult final
{
    std::optional<TileTerrainRuleDocument> document{};
    std::vector<TileDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return document.has_value() && diagnostics.empty();
    }
};

struct TileTerrainCompileResult final
{
    std::optional<TileMapDocument> tileMap{};
    std::vector<TileDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return tileMap.has_value() && diagnostics.empty();
    }
};

[[nodiscard]] std::string_view ToString(TerrainNeighbor value) noexcept;
[[nodiscard]] TileTerrainRuleLoadResult ParseTileTerrainRulesToml(
    std::string_view text,
    std::string_view sourceName = {});
[[nodiscard]] std::string SaveTileTerrainRulesToml(const TileTerrainRuleDocument& document);

[[nodiscard]] std::vector<TileDiagnostic> ValidateTileTerrainRules(
    const CompiledTileSet& tileSet,
    const TileMapDocument& tileMap,
    const TileTerrainRuleDocument& document);

// Terrain painting is setup/offline preprocessing. The result is ordinary TileMap
// authoring data and therefore does not add a second runtime cell representation.
[[nodiscard]] TileTerrainCompileResult CompileTileTerrainRules(
    const CompiledTileSet& tileSet,
    const TileMapDocument& tileMap,
    const TileTerrainRuleDocument& document);
} // namespace trace2d::tile
