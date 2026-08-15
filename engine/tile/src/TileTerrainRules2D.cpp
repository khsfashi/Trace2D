#include <trace2d/tile/TileTerrainRules2D.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <limits>
#include <locale>
#include <optional>
#include <ostream>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

namespace trace2d::tile
{
namespace
{
TileDiagnostic MakeDiagnostic(
    const TileErrorCode code,
    std::string path,
    std::string message,
    const toml::node* node = nullptr)
{
    TileDiagnostic diagnostic{};
    diagnostic.code = code;
    diagnostic.path = std::move(path);
    diagnostic.message = std::move(message);
    if (node != nullptr)
    {
        const toml::source_position position = node->source().begin;
        diagnostic.line = static_cast<std::size_t>(position.line);
        diagnostic.column = static_cast<std::size_t>(position.column);
    }
    return diagnostic;
}

void AddDiagnostic(
    std::vector<TileDiagnostic>& diagnostics,
    const TileErrorCode code,
    std::string path,
    std::string message,
    const toml::node* node = nullptr)
{
    diagnostics.push_back(MakeDiagnostic(code, std::move(path), std::move(message), node));
}

bool IsKnownKey(const std::string_view key, const std::initializer_list<std::string_view> knownKeys)
{
    return std::find(knownKeys.begin(), knownKeys.end(), key) != knownKeys.end();
}

void ValidateKnownKeys(
    const toml::table& table,
    const std::string_view path,
    const std::initializer_list<std::string_view> knownKeys,
    std::vector<TileDiagnostic>& diagnostics)
{
    for (const auto& [key, value] : table)
    {
        const std::string_view keyView = key.str();
        if (IsKnownKey(keyView, knownKeys))
        {
            continue;
        }
        std::string fieldPath{path};
        if (!fieldPath.empty())
        {
            fieldPath.push_back('.');
        }
        fieldPath.append(keyView);
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::move(fieldPath), "Unknown field.", &value);
    }
}

std::optional<std::string> ReadRequiredString(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<TileDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }
    const std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value() || value->empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected a non-empty string.", node);
        return std::nullopt;
    }
    return value;
}

bool ReadInt32Pair(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::int32_t& first,
    std::int32_t& second,
    std::vector<TileDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    const toml::array* values = node == nullptr ? nullptr : node->as_array();
    if (values == nullptr || values->size() != 2U)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected an array containing exactly two int32 values.", node);
        return false;
    }

    const toml::node* firstNode = values->get(0U);
    const toml::node* secondNode = values->get(1U);
    const std::optional<std::int64_t> firstValue = firstNode == nullptr ? std::nullopt : firstNode->value<std::int64_t>();
    const std::optional<std::int64_t> secondValue = secondNode == nullptr ? std::nullopt : secondNode->value<std::int64_t>();
    if (!firstValue.has_value() || !secondValue.has_value() ||
        *firstValue < static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) ||
        *firstValue > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) ||
        *secondValue < static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) ||
        *secondValue > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()))
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Cell coordinates must fit int32.", node);
        return false;
    }

    first = static_cast<std::int32_t>(*firstValue);
    second = static_cast<std::int32_t>(*secondValue);
    return true;
}

std::optional<TerrainNeighbor> ParseNeighbor(const std::string_view value) noexcept
{
    if (value == "north")
    {
        return TerrainNeighbor::North;
    }
    if (value == "east")
    {
        return TerrainNeighbor::East;
    }
    if (value == "south")
    {
        return TerrainNeighbor::South;
    }
    if (value == "west")
    {
        return TerrainNeighbor::West;
    }
    return std::nullopt;
}

std::uint8_t ReadNeighborMask(
    const toml::table& table,
    const std::string_view path,
    std::vector<TileDiagnostic>& diagnostics)
{
    const toml::node* node = table.get("neighbors");
    const toml::array* values = node == nullptr ? nullptr : node->as_array();
    if (values == nullptr)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected an array of cardinal neighbor names.", node);
        return 0U;
    }

    std::uint8_t mask = 0U;
    for (std::size_t index = 0U; index < values->size(); ++index)
    {
        const toml::node* valueNode = values->get(index);
        const std::optional<std::string> value = valueNode == nullptr ? std::nullopt : valueNode->value<std::string>();
        if (!value.has_value())
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                std::string{path} + "[" + std::to_string(index) + "]",
                "Expected a cardinal neighbor string.",
                valueNode);
            continue;
        }
        const std::optional<TerrainNeighbor> neighbor = ParseNeighbor(*value);
        if (!neighbor.has_value())
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                std::string{path} + "[" + std::to_string(index) + "]",
                "Neighbor must be 'north', 'east', 'south', or 'west'.",
                valueNode);
            continue;
        }
        const std::uint8_t bit = static_cast<std::uint8_t>(*neighbor);
        if ((mask & bit) != 0U)
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                std::string{path} + "[" + std::to_string(index) + "]",
                "Duplicate cardinal neighbor.",
                valueNode);
            continue;
        }
        mask = static_cast<std::uint8_t>(mask | bit);
    }
    return mask;
}

void WriteQuoted(std::ostream& stream, const std::string_view value)
{
    stream << '"';
    for (const char character : value)
    {
        switch (character)
        {
        case '\\': stream << "\\\\"; break;
        case '"': stream << "\\\""; break;
        case '\n': stream << "\\n"; break;
        case '\r': stream << "\\r"; break;
        case '\t': stream << "\\t"; break;
        default: stream << character; break;
        }
    }
    stream << '"';
}

void WriteNeighborMask(std::ostream& stream, const std::uint8_t mask)
{
    constexpr std::array<std::pair<TerrainNeighbor, std::string_view>, 4U> Neighbors{{
        {TerrainNeighbor::North, "north"},
        {TerrainNeighbor::East, "east"},
        {TerrainNeighbor::South, "south"},
        {TerrainNeighbor::West, "west"},
    }};

    stream << '[';
    bool wroteValue = false;
    for (const auto& [neighbor, name] : Neighbors)
    {
        if ((mask & static_cast<std::uint8_t>(neighbor)) == 0U)
        {
            continue;
        }
        if (wroteValue)
        {
            stream << ", ";
        }
        WriteQuoted(stream, name);
        wroteValue = true;
    }
    stream << ']';
}

const TerrainDefinitionDocument* FindTerrain(
    const TileTerrainRuleDocument& document,
    const std::string_view semanticId) noexcept
{
    const auto iterator = std::find_if(
        document.terrains.begin(),
        document.terrains.end(),
        [semanticId](const TerrainDefinitionDocument& terrain) {
            return terrain.semanticId == semanticId;
        });
    return iterator == document.terrains.end() ? nullptr : &*iterator;
}

const TileLayerDocument* FindLayer(
    const TileMapDocument& tileMap,
    const std::string_view semanticId) noexcept
{
    const auto iterator = std::find_if(
        tileMap.layers.begin(),
        tileMap.layers.end(),
        [semanticId](const TileLayerDocument& layer) {
            return layer.semanticId == semanticId;
        });
    return iterator == tileMap.layers.end() ? nullptr : &*iterator;
}

std::size_t FindLayerIndexDocument(
    const TileMapDocument& tileMap,
    const std::string_view semanticId) noexcept
{
    const auto iterator = std::find_if(
        tileMap.layers.begin(),
        tileMap.layers.end(),
        [semanticId](const TileLayerDocument& layer) {
            return layer.semanticId == semanticId;
        });
    return iterator == tileMap.layers.end()
        ? tileMap.layers.size()
        : static_cast<std::size_t>(std::distance(tileMap.layers.begin(), iterator));
}

std::uint64_t CellKey(const std::int32_t x, const std::int32_t y) noexcept
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) << 32U) |
        static_cast<std::uint32_t>(x);
}

std::vector<TileDiagnostic> ValidateDocumentShape(const TileTerrainRuleDocument& document)
{
    std::vector<TileDiagnostic> diagnostics{};
    if (document.semanticId.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "id", "TileTerrainRule id must not be empty.");
    }
    if (document.tileSetSemanticId.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "tile_set", "tile_set must not be empty.");
    }
    if (document.tileMapSemanticId.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "tile_map", "tile_map must not be empty.");
    }
    if (document.layerSemanticId.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "layer", "layer must not be empty.");
    }
    if (document.terrains.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "terrains", "At least one terrain definition is required.");
    }
    if (document.cells.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "cells", "At least one terrain paint cell is required.");
    }

    std::vector<std::pair<std::string_view, std::size_t>> terrainIds{};
    terrainIds.reserve(document.terrains.size());
    for (std::size_t index = 0U; index < document.terrains.size(); ++index)
    {
        const TerrainDefinitionDocument& terrain = document.terrains[index];
        const std::string path = "terrains[" + std::to_string(index) + "]";
        if (terrain.semanticId.empty())
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".id", "Terrain id must not be empty.");
        }
        if (terrain.fallbackTileSemanticId.empty())
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".fallback_tile", "Terrain fallback tile must not be empty.");
        }
        terrainIds.emplace_back(terrain.semanticId, index);
    }
    std::sort(terrainIds.begin(), terrainIds.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
    });
    for (std::size_t index = 1U; index < terrainIds.size(); ++index)
    {
        if (!terrainIds[index].first.empty() && terrainIds[index - 1U].first == terrainIds[index].first)
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                "terrains[" + std::to_string(terrainIds[index].second) + "].id",
                "Duplicate terrain id.");
        }
    }

    struct RuleKey final
    {
        std::string_view terrain{};
        std::uint8_t mask{0U};
        std::size_t index{0U};
    };
    std::vector<RuleKey> ruleKeys{};
    ruleKeys.reserve(document.rules.size());
    for (std::size_t index = 0U; index < document.rules.size(); ++index)
    {
        const TerrainVariantRuleDocument& rule = document.rules[index];
        const std::string path = "rules[" + std::to_string(index) + "]";
        if (rule.terrainSemanticId.empty())
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".terrain", "Rule terrain id must not be empty.");
        }
        if ((rule.neighborMask & static_cast<std::uint8_t>(~TerrainNeighborMaskAll)) != 0U)
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".neighbors", "Rule neighbor mask contains unsupported bits.");
        }
        if (rule.tileSemanticId.empty())
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".tile", "Rule tile id must not be empty.");
        }
        ruleKeys.push_back(RuleKey{rule.terrainSemanticId, rule.neighborMask, index});
    }
    std::sort(ruleKeys.begin(), ruleKeys.end(), [](const RuleKey& left, const RuleKey& right) {
        return std::tie(left.terrain, left.mask) < std::tie(right.terrain, right.mask);
    });
    for (std::size_t index = 1U; index < ruleKeys.size(); ++index)
    {
        if (ruleKeys[index - 1U].terrain == ruleKeys[index].terrain &&
            ruleKeys[index - 1U].mask == ruleKeys[index].mask)
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                "rules[" + std::to_string(ruleKeys[index].index) + "].neighbors",
                "Duplicate terrain rule for the same exact cardinal neighbor mask.");
        }
    }

    std::vector<std::pair<std::uint64_t, std::size_t>> cellKeys{};
    cellKeys.reserve(document.cells.size());
    for (std::size_t index = 0U; index < document.cells.size(); ++index)
    {
        const TerrainPaintCellDocument& cell = document.cells[index];
        if (cell.terrainSemanticId.empty())
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                "cells[" + std::to_string(index) + "].terrain",
                "Paint cell terrain id must not be empty.");
        }
        cellKeys.emplace_back(CellKey(cell.x, cell.y), index);
    }
    std::sort(cellKeys.begin(), cellKeys.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
    });
    for (std::size_t index = 1U; index < cellKeys.size(); ++index)
    {
        if (cellKeys[index - 1U].first == cellKeys[index].first)
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                "cells[" + std::to_string(cellKeys[index].second) + "].cell",
                "Duplicate terrain paint coordinate.");
        }
    }
    return diagnostics;
}

struct ResolvedTerrain final
{
    const TerrainDefinitionDocument* definition{nullptr};
    std::array<const TerrainVariantRuleDocument*, 16U> rules{};
};

std::size_t FindResolvedTerrainIndex(
    const std::vector<ResolvedTerrain>& terrains,
    const std::string_view semanticId) noexcept
{
    const auto iterator = std::lower_bound(
        terrains.begin(),
        terrains.end(),
        semanticId,
        [](const ResolvedTerrain& terrain, const std::string_view id) {
            return terrain.definition->semanticId < id;
        });
    if (iterator == terrains.end() || iterator->definition->semanticId != semanticId)
    {
        return terrains.size();
    }
    return static_cast<std::size_t>(std::distance(terrains.begin(), iterator));
}
} // namespace

std::string_view ToString(const TerrainNeighbor value) noexcept
{
    switch (value)
    {
    case TerrainNeighbor::North: return "north";
    case TerrainNeighbor::East: return "east";
    case TerrainNeighbor::South: return "south";
    case TerrainNeighbor::West: return "west";
    }
    return "north";
}

TileTerrainRuleLoadResult ParseTileTerrainRulesToml(
    const std::string_view text,
    const std::string_view sourceName)
{
    TileTerrainRuleLoadResult result{};
    std::optional<toml::table> root{};
    try
    {
        root = toml::parse(text, sourceName);
    }
    catch (const toml::parse_error& error)
    {
        TileDiagnostic diagnostic{};
        diagnostic.code = TileErrorCode::ParseError;
        diagnostic.path = "$";
        diagnostic.message = std::string{error.description()};
        diagnostic.line = static_cast<std::size_t>(error.source().begin.line);
        diagnostic.column = static_cast<std::size_t>(error.source().begin.column);
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }

    ValidateKnownKeys(*root, "", {"format_version", "id", "tile_set", "tile_map", "layer", "terrains", "rules", "cells"}, result.diagnostics);
    const toml::node* formatNode = root->get("format_version");
    const std::optional<std::int64_t> formatVersion = formatNode == nullptr ? std::nullopt : formatNode->value<std::int64_t>();
    if (!formatVersion.has_value() || *formatVersion != TileTerrainRuleDocument::FormatVersion)
    {
        AddDiagnostic(result.diagnostics, TileErrorCode::UnsupportedFormat, "format_version", "Unsupported TileTerrainRule format version. Expected 1.", formatNode);
    }

    TileTerrainRuleDocument document{};
    if (const std::optional<std::string> value = ReadRequiredString(*root, "id", "id", result.diagnostics); value.has_value())
    {
        document.semanticId = *value;
    }
    if (const std::optional<std::string> value = ReadRequiredString(*root, "tile_set", "tile_set", result.diagnostics); value.has_value())
    {
        document.tileSetSemanticId = *value;
    }
    if (const std::optional<std::string> value = ReadRequiredString(*root, "tile_map", "tile_map", result.diagnostics); value.has_value())
    {
        document.tileMapSemanticId = *value;
    }
    if (const std::optional<std::string> value = ReadRequiredString(*root, "layer", "layer", result.diagnostics); value.has_value())
    {
        document.layerSemanticId = *value;
    }

    if (const toml::node* terrainsNode = root->get("terrains"); terrainsNode != nullptr)
    {
        const toml::array* terrains = terrainsNode->as_array();
        if (terrains == nullptr)
        {
            AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, "terrains", "Expected an array of terrain tables.", terrainsNode);
        }
        else
        {
            document.terrains.reserve(terrains->size());
            for (std::size_t index = 0U; index < terrains->size(); ++index)
            {
                const toml::node* terrainNode = terrains->get(index);
                const toml::table* terrainTable = terrainNode == nullptr ? nullptr : terrainNode->as_table();
                const std::string path = "terrains[" + std::to_string(index) + "]";
                if (terrainTable == nullptr)
                {
                    AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, path, "Expected a terrain table.", terrainNode);
                    continue;
                }
                ValidateKnownKeys(*terrainTable, path, {"id", "fallback_tile"}, result.diagnostics);
                TerrainDefinitionDocument terrain{};
                if (const std::optional<std::string> value = ReadRequiredString(*terrainTable, "id", path + ".id", result.diagnostics); value.has_value())
                {
                    terrain.semanticId = *value;
                }
                if (const std::optional<std::string> value = ReadRequiredString(*terrainTable, "fallback_tile", path + ".fallback_tile", result.diagnostics); value.has_value())
                {
                    terrain.fallbackTileSemanticId = *value;
                }
                document.terrains.push_back(std::move(terrain));
            }
        }
    }

    if (const toml::node* rulesNode = root->get("rules"); rulesNode != nullptr)
    {
        const toml::array* rules = rulesNode->as_array();
        if (rules == nullptr)
        {
            AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, "rules", "Expected an array of terrain rule tables.", rulesNode);
        }
        else
        {
            document.rules.reserve(rules->size());
            for (std::size_t index = 0U; index < rules->size(); ++index)
            {
                const toml::node* ruleNode = rules->get(index);
                const toml::table* ruleTable = ruleNode == nullptr ? nullptr : ruleNode->as_table();
                const std::string path = "rules[" + std::to_string(index) + "]";
                if (ruleTable == nullptr)
                {
                    AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, path, "Expected a terrain rule table.", ruleNode);
                    continue;
                }
                ValidateKnownKeys(*ruleTable, path, {"terrain", "neighbors", "tile"}, result.diagnostics);
                TerrainVariantRuleDocument rule{};
                if (const std::optional<std::string> value = ReadRequiredString(*ruleTable, "terrain", path + ".terrain", result.diagnostics); value.has_value())
                {
                    rule.terrainSemanticId = *value;
                }
                rule.neighborMask = ReadNeighborMask(*ruleTable, path + ".neighbors", result.diagnostics);
                if (const std::optional<std::string> value = ReadRequiredString(*ruleTable, "tile", path + ".tile", result.diagnostics); value.has_value())
                {
                    rule.tileSemanticId = *value;
                }
                document.rules.push_back(std::move(rule));
            }
        }
    }

    if (const toml::node* cellsNode = root->get("cells"); cellsNode != nullptr)
    {
        const toml::array* cells = cellsNode->as_array();
        if (cells == nullptr)
        {
            AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, "cells", "Expected an array of terrain paint cell tables.", cellsNode);
        }
        else
        {
            document.cells.reserve(cells->size());
            for (std::size_t index = 0U; index < cells->size(); ++index)
            {
                const toml::node* cellNode = cells->get(index);
                const toml::table* cellTable = cellNode == nullptr ? nullptr : cellNode->as_table();
                const std::string path = "cells[" + std::to_string(index) + "]";
                if (cellTable == nullptr)
                {
                    AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, path, "Expected a terrain paint cell table.", cellNode);
                    continue;
                }
                ValidateKnownKeys(*cellTable, path, {"cell", "terrain"}, result.diagnostics);
                TerrainPaintCellDocument cell{};
                ReadInt32Pair(*cellTable, "cell", path + ".cell", cell.x, cell.y, result.diagnostics);
                if (const std::optional<std::string> value = ReadRequiredString(*cellTable, "terrain", path + ".terrain", result.diagnostics); value.has_value())
                {
                    cell.terrainSemanticId = *value;
                }
                document.cells.push_back(std::move(cell));
            }
        }
    }

    std::vector<TileDiagnostic> validation = ValidateDocumentShape(document);
    result.diagnostics.insert(result.diagnostics.end(), std::make_move_iterator(validation.begin()), std::make_move_iterator(validation.end()));
    if (result.diagnostics.empty())
    {
        result.document = std::move(document);
    }
    return result;
}

std::string SaveTileTerrainRulesToml(const TileTerrainRuleDocument& document)
{
    std::vector<TerrainDefinitionDocument> terrains = document.terrains;
    std::sort(terrains.begin(), terrains.end(), [](const TerrainDefinitionDocument& left, const TerrainDefinitionDocument& right) {
        return left.semanticId < right.semanticId;
    });

    std::vector<TerrainVariantRuleDocument> rules = document.rules;
    std::sort(rules.begin(), rules.end(), [](const TerrainVariantRuleDocument& left, const TerrainVariantRuleDocument& right) {
        return std::tie(left.terrainSemanticId, left.neighborMask, left.tileSemanticId) <
            std::tie(right.terrainSemanticId, right.neighborMask, right.tileSemanticId);
    });

    std::vector<TerrainPaintCellDocument> cells = document.cells;
    std::sort(cells.begin(), cells.end(), [](const TerrainPaintCellDocument& left, const TerrainPaintCellDocument& right) {
        return std::tie(left.y, left.x, left.terrainSemanticId) < std::tie(right.y, right.x, right.terrainSemanticId);
    });

    std::ostringstream stream{};
    stream.imbue(std::locale::classic());
    stream << "format_version = 1\nid = ";
    WriteQuoted(stream, document.semanticId);
    stream << "\ntile_set = ";
    WriteQuoted(stream, document.tileSetSemanticId);
    stream << "\ntile_map = ";
    WriteQuoted(stream, document.tileMapSemanticId);
    stream << "\nlayer = ";
    WriteQuoted(stream, document.layerSemanticId);
    stream << "\n";

    for (const TerrainDefinitionDocument& terrain : terrains)
    {
        stream << "\n[[terrains]]\nid = ";
        WriteQuoted(stream, terrain.semanticId);
        stream << "\nfallback_tile = ";
        WriteQuoted(stream, terrain.fallbackTileSemanticId);
        stream << "\n";
    }
    for (const TerrainVariantRuleDocument& rule : rules)
    {
        stream << "\n[[rules]]\nterrain = ";
        WriteQuoted(stream, rule.terrainSemanticId);
        stream << "\nneighbors = ";
        WriteNeighborMask(stream, rule.neighborMask);
        stream << "\ntile = ";
        WriteQuoted(stream, rule.tileSemanticId);
        stream << "\n";
    }
    for (const TerrainPaintCellDocument& cell : cells)
    {
        stream << "\n[[cells]]\ncell = [" << cell.x << ", " << cell.y << "]\nterrain = ";
        WriteQuoted(stream, cell.terrainSemanticId);
        stream << "\n";
    }
    return stream.str();
}

std::vector<TileDiagnostic> ValidateTileTerrainRules(
    const CompiledTileSet& tileSet,
    const TileMapDocument& tileMap,
    const TileTerrainRuleDocument& document)
{
    std::vector<TileDiagnostic> diagnostics = ValidateDocumentShape(document);
    const std::vector<TileDiagnostic> tileMapDiagnostics = ValidateTileMap(tileMap);
    diagnostics.insert(diagnostics.end(), tileMapDiagnostics.begin(), tileMapDiagnostics.end());

    if (document.tileSetSemanticId != tileSet.semanticId)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "tile_set", "TileTerrainRule tile_set does not match the compiled TileSet identity.");
    }
    if (tileMap.tileSetSemanticId != tileSet.semanticId)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "tile_map", "TileMap does not reference the supplied TileSet identity.");
    }
    if (document.tileMapSemanticId != tileMap.semanticId)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "tile_map", "TileTerrainRule tile_map does not match the TileMap identity.");
    }

    const TileLayerDocument* layer = FindLayer(tileMap, document.layerSemanticId);
    if (layer == nullptr)
    {
        if (!document.layerSemanticId.empty())
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "layer", "Terrain rules reference an unknown TileMap layer id.");
        }
    }
    else
    {
        const std::uint64_t layerCellCount = static_cast<std::uint64_t>(layer->width) * static_cast<std::uint64_t>(layer->height);
        if (layerCellCount > MaximumCompiledCellsPerLayer)
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "layer", "Terrain preprocessing requires the current bounded dense TileMap layer limit.");
        }
    }

    for (std::size_t index = 0U; index < document.terrains.size(); ++index)
    {
        const TerrainDefinitionDocument& terrain = document.terrains[index];
        if (!terrain.fallbackTileSemanticId.empty() && !FindTileIndex(tileSet, terrain.fallbackTileSemanticId).has_value())
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                "terrains[" + std::to_string(index) + "].fallback_tile",
                "Terrain fallback references an unknown tile id.");
        }
    }

    for (std::size_t index = 0U; index < document.rules.size(); ++index)
    {
        const TerrainVariantRuleDocument& rule = document.rules[index];
        if (!rule.terrainSemanticId.empty() && FindTerrain(document, rule.terrainSemanticId) == nullptr)
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                "rules[" + std::to_string(index) + "].terrain",
                "Rule references an unknown terrain id.");
        }
        if (!rule.tileSemanticId.empty() && !FindTileIndex(tileSet, rule.tileSemanticId).has_value())
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                "rules[" + std::to_string(index) + "].tile",
                "Rule references an unknown output tile id.");
        }
    }

    if (layer != nullptr)
    {
        for (std::size_t index = 0U; index < document.cells.size(); ++index)
        {
            const TerrainPaintCellDocument& cell = document.cells[index];
            if (!cell.terrainSemanticId.empty() && FindTerrain(document, cell.terrainSemanticId) == nullptr)
            {
                AddDiagnostic(
                    diagnostics,
                    TileErrorCode::SchemaError,
                    "cells[" + std::to_string(index) + "].terrain",
                    "Paint cell references an unknown terrain id.");
            }
            if (cell.x < 0 || cell.y < 0 ||
                static_cast<std::uint64_t>(cell.x) >= static_cast<std::uint64_t>(layer->width) ||
                static_cast<std::uint64_t>(cell.y) >= static_cast<std::uint64_t>(layer->height))
            {
                AddDiagnostic(
                    diagnostics,
                    TileErrorCode::SchemaError,
                    "cells[" + std::to_string(index) + "].cell",
                    "Terrain paint cell lies outside the target layer's local bounds.");
            }
        }
    }
    return diagnostics;
}

TileTerrainCompileResult CompileTileTerrainRules(
    const CompiledTileSet& tileSet,
    const TileMapDocument& tileMap,
    const TileTerrainRuleDocument& document)
{
    TileTerrainCompileResult result{};
    result.diagnostics = ValidateTileTerrainRules(tileSet, tileMap, document);
    if (!result.diagnostics.empty())
    {
        return result;
    }

    const std::size_t layerIndex = FindLayerIndexDocument(tileMap, document.layerSemanticId);
    if (layerIndex >= tileMap.layers.size())
    {
        return result;
    }
    const TileLayerDocument& sourceLayer = tileMap.layers[layerIndex];

    std::vector<ResolvedTerrain> terrains{};
    terrains.reserve(document.terrains.size());
    for (const TerrainDefinitionDocument& terrain : document.terrains)
    {
        ResolvedTerrain resolved{};
        resolved.definition = &terrain;
        terrains.push_back(resolved);
    }
    std::sort(terrains.begin(), terrains.end(), [](const ResolvedTerrain& left, const ResolvedTerrain& right) {
        return left.definition->semanticId < right.definition->semanticId;
    });
    for (const TerrainVariantRuleDocument& rule : document.rules)
    {
        const std::size_t terrainIndex = FindResolvedTerrainIndex(terrains, rule.terrainSemanticId);
        if (terrainIndex < terrains.size())
        {
            terrains[terrainIndex].rules[rule.neighborMask] = &rule;
        }
    }

    const std::size_t denseCellCount = static_cast<std::size_t>(
        static_cast<std::uint64_t>(sourceLayer.width) * static_cast<std::uint64_t>(sourceLayer.height));
    constexpr std::uint32_t EmptyTerrainIndex = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> terrainAtCell(denseCellCount, EmptyTerrainIndex);

    for (const TerrainPaintCellDocument& cell : document.cells)
    {
        const std::size_t terrainIndex = FindResolvedTerrainIndex(terrains, cell.terrainSemanticId);
        const std::size_t cellIndex = static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(sourceLayer.width) +
            static_cast<std::size_t>(cell.x);
        terrainAtCell[cellIndex] = static_cast<std::uint32_t>(terrainIndex);
    }

    TileMapDocument output = tileMap;
    TileLayerDocument& outputLayer = output.layers[layerIndex];
    std::vector<TileCellDocument> outputCells{};
    outputCells.reserve(sourceLayer.cells.size() + document.cells.size());

    for (const TileCellDocument& cell : sourceLayer.cells)
    {
        if (cell.x >= 0 && cell.y >= 0 &&
            static_cast<std::uint64_t>(cell.x) < static_cast<std::uint64_t>(sourceLayer.width) &&
            static_cast<std::uint64_t>(cell.y) < static_cast<std::uint64_t>(sourceLayer.height))
        {
            const std::size_t cellIndex = static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(sourceLayer.width) +
                static_cast<std::size_t>(cell.x);
            if (terrainAtCell[cellIndex] != EmptyTerrainIndex)
            {
                continue;
            }
        }
        outputCells.push_back(cell);
    }

    for (const TerrainPaintCellDocument& cell : document.cells)
    {
        const std::size_t cellIndex = static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(sourceLayer.width) +
            static_cast<std::size_t>(cell.x);
        const std::uint32_t terrainIndex = terrainAtCell[cellIndex];
        std::uint8_t mask = 0U;

        const auto sameTerrain = [&](const std::int32_t neighborX, const std::int32_t neighborY) noexcept {
            if (neighborX < 0 || neighborY < 0 ||
                static_cast<std::uint64_t>(neighborX) >= static_cast<std::uint64_t>(sourceLayer.width) ||
                static_cast<std::uint64_t>(neighborY) >= static_cast<std::uint64_t>(sourceLayer.height))
            {
                return false;
            }
            const std::size_t neighborIndex = static_cast<std::size_t>(neighborY) * static_cast<std::size_t>(sourceLayer.width) +
                static_cast<std::size_t>(neighborX);
            return terrainAtCell[neighborIndex] == terrainIndex;
        };

        if (sameTerrain(cell.x, cell.y - 1))
        {
            mask = static_cast<std::uint8_t>(mask | static_cast<std::uint8_t>(TerrainNeighbor::North));
        }
        if (sameTerrain(cell.x + 1, cell.y))
        {
            mask = static_cast<std::uint8_t>(mask | static_cast<std::uint8_t>(TerrainNeighbor::East));
        }
        if (sameTerrain(cell.x, cell.y + 1))
        {
            mask = static_cast<std::uint8_t>(mask | static_cast<std::uint8_t>(TerrainNeighbor::South));
        }
        if (sameTerrain(cell.x - 1, cell.y))
        {
            mask = static_cast<std::uint8_t>(mask | static_cast<std::uint8_t>(TerrainNeighbor::West));
        }

        const ResolvedTerrain& terrain = terrains[terrainIndex];
        const TerrainVariantRuleDocument* rule = terrain.rules[mask];
        const std::string& outputTile = rule == nullptr
            ? terrain.definition->fallbackTileSemanticId
            : rule->tileSemanticId;
        outputCells.push_back(TileCellDocument{
            .x = cell.x,
            .y = cell.y,
            .tileSemanticId = outputTile,
            .transform = {},
        });
    }

    std::sort(outputCells.begin(), outputCells.end(), [](const TileCellDocument& left, const TileCellDocument& right) {
        return std::tie(left.y, left.x, left.tileSemanticId) < std::tie(right.y, right.x, right.tileSemanticId);
    });
    outputLayer.cells = std::move(outputCells);
    result.tileMap = std::move(output);
    return result;
}
} // namespace trace2d::tile
