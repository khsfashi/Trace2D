#include <trace2d/tile/TileSemantics2D.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <ostream>
#include <sstream>
#include <unordered_set>
#include <utility>

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

std::vector<std::string> ReadOptionalStringArray(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<TileDiagnostic>& diagnostics)
{
    std::vector<std::string> result{};
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        return result;
    }
    const toml::array* values = node->as_array();
    if (values == nullptr)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected an array of strings.", node);
        return result;
    }

    result.reserve(values->size());
    for (std::size_t index = 0U; index < values->size(); ++index)
    {
        const toml::node* valueNode = values->get(index);
        const std::optional<std::string> value = valueNode == nullptr ? std::nullopt : valueNode->value<std::string>();
        if (!value.has_value() || value->empty())
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                std::string{path} + "[" + std::to_string(index) + "]",
                "Expected a non-empty string.",
                valueNode);
            continue;
        }
        result.push_back(*value);
    }
    return result;
}

TileCollisionHandoff ReadCollision(
    const toml::table& table,
    const std::string_view path,
    std::vector<TileDiagnostic>& diagnostics)
{
    const toml::node* node = table.get("collision");
    if (node == nullptr)
    {
        return TileCollisionHandoff::None;
    }
    const std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected collision string.", node);
        return TileCollisionHandoff::None;
    }
    if (*value == "none")
    {
        return TileCollisionHandoff::None;
    }
    if (*value == "solid")
    {
        return TileCollisionHandoff::Solid;
    }
    AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "collision must be 'none' or 'solid'.", node);
    return TileCollisionHandoff::None;
}

TileNavigationHandoff ReadNavigation(
    const toml::table& table,
    const std::string_view path,
    std::vector<TileDiagnostic>& diagnostics)
{
    const toml::node* node = table.get("navigation");
    if (node == nullptr)
    {
        return TileNavigationHandoff::None;
    }
    const std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected navigation string.", node);
        return TileNavigationHandoff::None;
    }
    if (*value == "none")
    {
        return TileNavigationHandoff::None;
    }
    if (*value == "walkable")
    {
        return TileNavigationHandoff::Walkable;
    }
    if (*value == "blocked")
    {
        return TileNavigationHandoff::Blocked;
    }
    AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "navigation must be 'none', 'walkable', or 'blocked'.", node);
    return TileNavigationHandoff::None;
}

TileOcclusionHandoff ReadOcclusion(
    const toml::table& table,
    const std::string_view path,
    std::vector<TileDiagnostic>& diagnostics)
{
    const toml::node* node = table.get("occlusion");
    if (node == nullptr)
    {
        return TileOcclusionHandoff::None;
    }
    const std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected occlusion string.", node);
        return TileOcclusionHandoff::None;
    }
    if (*value == "none")
    {
        return TileOcclusionHandoff::None;
    }
    if (*value == "opaque")
    {
        return TileOcclusionHandoff::Opaque;
    }
    AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "occlusion must be 'none' or 'opaque'.", node);
    return TileOcclusionHandoff::None;
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

void WriteTags(std::ostream& stream, const std::vector<std::string>& tags)
{
    stream << '[';
    for (std::size_t index = 0U; index < tags.size(); ++index)
    {
        if (index != 0U)
        {
            stream << ", ";
        }
        WriteQuoted(stream, tags[index]);
    }
    stream << ']';
}

std::vector<TileDiagnostic> ValidateDocumentShape(const TileSemanticOverlayDocument& document)
{
    std::vector<TileDiagnostic> diagnostics{};
    if (document.semanticId.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "id", "TileSemanticOverlay id must not be empty.");
    }
    if (document.tileSetSemanticId.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "tile_set", "tile_set must not be empty.");
    }
    if (document.tileMapSemanticId.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "tile_map", "tile_map must not be empty.");
    }

    std::unordered_set<std::string_view> ruleTiles{};
    ruleTiles.reserve(document.rules.size());
    for (std::size_t index = 0U; index < document.rules.size(); ++index)
    {
        const TileSemanticRuleDocument& rule = document.rules[index];
        const std::string path = "rules[" + std::to_string(index) + "]";
        if (rule.tileSemanticId.empty())
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".tile", "Rule tile id must not be empty.");
        }
        else if (!ruleTiles.insert(rule.tileSemanticId).second)
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".tile", "Duplicate semantic rule for tile.");
        }
        if (rule.collision == TileCollisionHandoff::None &&
            rule.navigation == TileNavigationHandoff::None &&
            rule.occlusion == TileOcclusionHandoff::None)
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path, "Semantic rule must change at least one handoff field from 'none'.");
        }
    }

    std::unordered_set<std::string_view> markerIds{};
    markerIds.reserve(document.markers.size());
    for (std::size_t index = 0U; index < document.markers.size(); ++index)
    {
        const TileMarkerDocument& marker = document.markers[index];
        const std::string path = "markers[" + std::to_string(index) + "]";
        if (marker.semanticId.empty())
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".id", "Marker id must not be empty.");
        }
        else if (!markerIds.insert(marker.semanticId).second)
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".id", "Duplicate marker id.");
        }
        if (marker.kind.empty())
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".kind", "Marker kind must not be empty.");
        }
        if (marker.layerSemanticId.empty())
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".layer", "Marker layer id must not be empty.");
        }
        std::unordered_set<std::string_view> tags{};
        tags.reserve(marker.tags.size());
        for (std::size_t tagIndex = 0U; tagIndex < marker.tags.size(); ++tagIndex)
        {
            if (marker.tags[tagIndex].empty())
            {
                AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".tags[" + std::to_string(tagIndex) + "]", "Marker tag must not be empty.");
            }
            else if (!tags.insert(marker.tags[tagIndex]).second)
            {
                AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".tags[" + std::to_string(tagIndex) + "]", "Duplicate marker tag.");
            }
        }
    }
    return diagnostics;
}
} // namespace

std::string_view ToString(const TileCollisionHandoff value) noexcept
{
    switch (value)
    {
    case TileCollisionHandoff::None: return "none";
    case TileCollisionHandoff::Solid: return "solid";
    }
    return "none";
}

std::string_view ToString(const TileNavigationHandoff value) noexcept
{
    switch (value)
    {
    case TileNavigationHandoff::None: return "none";
    case TileNavigationHandoff::Walkable: return "walkable";
    case TileNavigationHandoff::Blocked: return "blocked";
    }
    return "none";
}

std::string_view ToString(const TileOcclusionHandoff value) noexcept
{
    switch (value)
    {
    case TileOcclusionHandoff::None: return "none";
    case TileOcclusionHandoff::Opaque: return "opaque";
    }
    return "none";
}

TileSemanticOverlayLoadResult ParseTileSemanticOverlayToml(
    const std::string_view text,
    const std::string_view sourceName)
{
    TileSemanticOverlayLoadResult result{};
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

    ValidateKnownKeys(*root, "", {"format_version", "id", "tile_set", "tile_map", "rules", "markers"}, result.diagnostics);
    const toml::node* formatNode = root->get("format_version");
    const std::optional<std::int64_t> formatVersion = formatNode == nullptr ? std::nullopt : formatNode->value<std::int64_t>();
    if (!formatVersion.has_value() || *formatVersion != TileSemanticOverlayDocument::FormatVersion)
    {
        AddDiagnostic(result.diagnostics, TileErrorCode::UnsupportedFormat, "format_version", "Unsupported TileSemanticOverlay format version. Expected 1.", formatNode);
    }

    TileSemanticOverlayDocument document{};
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

    if (const toml::node* rulesNode = root->get("rules"); rulesNode != nullptr)
    {
        const toml::array* rules = rulesNode->as_array();
        if (rules == nullptr)
        {
            AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, "rules", "Expected an array of rule tables.", rulesNode);
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
                    AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, path, "Expected a rule table.", ruleNode);
                    continue;
                }
                ValidateKnownKeys(*ruleTable, path, {"tile", "collision", "navigation", "occlusion"}, result.diagnostics);
                TileSemanticRuleDocument rule{};
                if (const std::optional<std::string> value = ReadRequiredString(*ruleTable, "tile", path + ".tile", result.diagnostics); value.has_value())
                {
                    rule.tileSemanticId = *value;
                }
                rule.collision = ReadCollision(*ruleTable, path + ".collision", result.diagnostics);
                rule.navigation = ReadNavigation(*ruleTable, path + ".navigation", result.diagnostics);
                rule.occlusion = ReadOcclusion(*ruleTable, path + ".occlusion", result.diagnostics);
                document.rules.push_back(std::move(rule));
            }
        }
    }

    if (const toml::node* markersNode = root->get("markers"); markersNode != nullptr)
    {
        const toml::array* markers = markersNode->as_array();
        if (markers == nullptr)
        {
            AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, "markers", "Expected an array of marker tables.", markersNode);
        }
        else
        {
            document.markers.reserve(markers->size());
            for (std::size_t index = 0U; index < markers->size(); ++index)
            {
                const toml::node* markerNode = markers->get(index);
                const toml::table* markerTable = markerNode == nullptr ? nullptr : markerNode->as_table();
                const std::string path = "markers[" + std::to_string(index) + "]";
                if (markerTable == nullptr)
                {
                    AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, path, "Expected a marker table.", markerNode);
                    continue;
                }
                ValidateKnownKeys(*markerTable, path, {"id", "kind", "layer", "cell", "tags"}, result.diagnostics);
                TileMarkerDocument marker{};
                if (const std::optional<std::string> value = ReadRequiredString(*markerTable, "id", path + ".id", result.diagnostics); value.has_value())
                {
                    marker.semanticId = *value;
                }
                if (const std::optional<std::string> value = ReadRequiredString(*markerTable, "kind", path + ".kind", result.diagnostics); value.has_value())
                {
                    marker.kind = *value;
                }
                if (const std::optional<std::string> value = ReadRequiredString(*markerTable, "layer", path + ".layer", result.diagnostics); value.has_value())
                {
                    marker.layerSemanticId = *value;
                }
                ReadInt32Pair(*markerTable, "cell", path + ".cell", marker.worldX, marker.worldY, result.diagnostics);
                marker.tags = ReadOptionalStringArray(*markerTable, "tags", path + ".tags", result.diagnostics);
                document.markers.push_back(std::move(marker));
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

std::string SaveTileSemanticOverlayToml(const TileSemanticOverlayDocument& document)
{
    std::vector<TileSemanticRuleDocument> rules = document.rules;
    std::sort(rules.begin(), rules.end(), [](const TileSemanticRuleDocument& left, const TileSemanticRuleDocument& right) {
        return left.tileSemanticId < right.tileSemanticId;
    });

    std::vector<TileMarkerDocument> markers = document.markers;
    std::sort(markers.begin(), markers.end(), [](const TileMarkerDocument& left, const TileMarkerDocument& right) {
        return left.semanticId < right.semanticId;
    });
    for (TileMarkerDocument& marker : markers)
    {
        std::sort(marker.tags.begin(), marker.tags.end());
    }

    std::ostringstream stream{};
    stream.imbue(std::locale::classic());
    stream << "format_version = 1\n";
    stream << "id = ";
    WriteQuoted(stream, document.semanticId);
    stream << "\ntile_set = ";
    WriteQuoted(stream, document.tileSetSemanticId);
    stream << "\ntile_map = ";
    WriteQuoted(stream, document.tileMapSemanticId);
    stream << "\n";

    for (const TileSemanticRuleDocument& rule : rules)
    {
        stream << "\n[[rules]]\ntile = ";
        WriteQuoted(stream, rule.tileSemanticId);
        stream << "\ncollision = ";
        WriteQuoted(stream, ToString(rule.collision));
        stream << "\nnavigation = ";
        WriteQuoted(stream, ToString(rule.navigation));
        stream << "\nocclusion = ";
        WriteQuoted(stream, ToString(rule.occlusion));
        stream << "\n";
    }

    for (const TileMarkerDocument& marker : markers)
    {
        stream << "\n[[markers]]\nid = ";
        WriteQuoted(stream, marker.semanticId);
        stream << "\nkind = ";
        WriteQuoted(stream, marker.kind);
        stream << "\nlayer = ";
        WriteQuoted(stream, marker.layerSemanticId);
        stream << "\ncell = [" << marker.worldX << ", " << marker.worldY << "]\ntags = ";
        WriteTags(stream, marker.tags);
        stream << "\n";
    }
    return stream.str();
}

std::vector<TileDiagnostic> ValidateTileSemanticOverlay(
    const CompiledTileSet& tileSet,
    const CompiledTileMap& tileMap,
    const TileSemanticOverlayDocument& document)
{
    std::vector<TileDiagnostic> diagnostics = ValidateDocumentShape(document);
    if (document.tileSetSemanticId != tileSet.semanticId)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "tile_set", "TileSemanticOverlay tile_set does not match the compiled TileSet identity.");
    }
    if (document.tileMapSemanticId != tileMap.semanticId)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "tile_map", "TileSemanticOverlay tile_map does not match the compiled TileMap identity.");
    }
    if (tileMap.tileSetSemanticId != tileSet.semanticId)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "tile_map", "Compiled TileMap does not reference the supplied TileSet identity.");
    }

    for (std::size_t index = 0U; index < document.rules.size(); ++index)
    {
        const TileSemanticRuleDocument& rule = document.rules[index];
        if (!rule.tileSemanticId.empty() && !FindTileIndex(tileSet, rule.tileSemanticId).has_value())
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                "rules[" + std::to_string(index) + "].tile",
                "Semantic rule references an unknown tile id.");
        }
    }

    for (std::size_t index = 0U; index < document.markers.size(); ++index)
    {
        const TileMarkerDocument& marker = document.markers[index];
        const std::optional<std::size_t> layerIndex = marker.layerSemanticId.empty()
            ? std::nullopt
            : FindLayerIndex(tileMap, marker.layerSemanticId);
        if (!layerIndex.has_value())
        {
            if (!marker.layerSemanticId.empty())
            {
                AddDiagnostic(
                    diagnostics,
                    TileErrorCode::SchemaError,
                    "markers[" + std::to_string(index) + "].layer",
                    "Marker references an unknown layer id.");
            }
            continue;
        }

        const CompiledTileLayer& layer = tileMap.layers[*layerIndex];
        const std::int64_t localX = static_cast<std::int64_t>(marker.worldX) - static_cast<std::int64_t>(layer.originX);
        const std::int64_t localY = static_cast<std::int64_t>(marker.worldY) - static_cast<std::int64_t>(layer.originY);
        if (localX < 0 || localY < 0 ||
            static_cast<std::uint64_t>(localX) >= static_cast<std::uint64_t>(layer.width) ||
            static_cast<std::uint64_t>(localY) >= static_cast<std::uint64_t>(layer.height))
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                "markers[" + std::to_string(index) + "].cell",
                "Marker world cell lies outside its referenced layer bounds.");
        }
    }
    return diagnostics;
}

TileSemanticOverlayCompileResult CompileTileSemanticOverlay(
    const CompiledTileSet& tileSet,
    const CompiledTileMap& tileMap,
    const TileSemanticOverlayDocument& document)
{
    TileSemanticOverlayCompileResult result{};
    result.diagnostics = ValidateTileSemanticOverlay(tileSet, tileMap, document);
    if (!result.diagnostics.empty())
    {
        return result;
    }

    CompiledTileSemanticOverlay overlay{};
    overlay.semanticId = document.semanticId;
    overlay.tileSetSemanticId = document.tileSetSemanticId;
    overlay.tileMapSemanticId = document.tileMapSemanticId;
    overlay.tileStates.resize(tileSet.tiles.size());

    for (const TileSemanticRuleDocument& rule : document.rules)
    {
        const std::optional<std::size_t> tileIndex = FindTileIndex(tileSet, rule.tileSemanticId);
        if (!tileIndex.has_value())
        {
            continue;
        }
        overlay.tileStates[*tileIndex] = CompiledTileSemanticState{
            .collision = rule.collision,
            .navigation = rule.navigation,
            .occlusion = rule.occlusion,
            .reserved = 0U,
        };
    }

    std::vector<const TileMarkerDocument*> markerOrder{};
    markerOrder.reserve(document.markers.size());
    for (const TileMarkerDocument& marker : document.markers)
    {
        markerOrder.push_back(&marker);
    }
    std::sort(markerOrder.begin(), markerOrder.end(), [](const TileMarkerDocument* left, const TileMarkerDocument* right) {
        return left->semanticId < right->semanticId;
    });

    overlay.markers.reserve(markerOrder.size());
    for (const TileMarkerDocument* marker : markerOrder)
    {
        const std::optional<std::size_t> layerIndex = FindLayerIndex(tileMap, marker->layerSemanticId);
        if (!layerIndex.has_value())
        {
            continue;
        }
        CompiledTileMarker compiled{};
        compiled.semanticId = marker->semanticId;
        compiled.kind = marker->kind;
        compiled.layerIndex = static_cast<std::uint32_t>(*layerIndex);
        compiled.worldX = marker->worldX;
        compiled.worldY = marker->worldY;
        compiled.tags = marker->tags;
        std::sort(compiled.tags.begin(), compiled.tags.end());
        overlay.markers.push_back(std::move(compiled));
    }

    result.overlay = std::move(overlay);
    return result;
}

const CompiledTileSemanticState* SemanticStateForTile(
    const CompiledTileSemanticOverlay& overlay,
    const std::uint32_t tileIndex) noexcept
{
    if (tileIndex == EmptyTileIndex || static_cast<std::size_t>(tileIndex) >= overlay.tileStates.size())
    {
        return nullptr;
    }
    return &overlay.tileStates[tileIndex];
}

const CompiledTileSemanticState* SemanticStateForCell(
    const CompiledTileSemanticOverlay& overlay,
    const CompiledTileMap& tileMap,
    const std::size_t layerIndex,
    const std::int32_t worldX,
    const std::int32_t worldY) noexcept
{
    const CompiledTileCell* cell = CellAtWorld(tileMap, layerIndex, worldX, worldY);
    if (cell == nullptr || cell->Empty())
    {
        return nullptr;
    }
    return SemanticStateForTile(overlay, cell->tileIndex);
}

std::optional<std::size_t> FindMarkerIndex(
    const CompiledTileSemanticOverlay& overlay,
    const std::string_view semanticId) noexcept
{
    const auto iterator = std::lower_bound(
        overlay.markers.begin(),
        overlay.markers.end(),
        semanticId,
        [](const CompiledTileMarker& marker, const std::string_view id) {
            return marker.semanticId < id;
        });
    if (iterator == overlay.markers.end() || iterator->semanticId != semanticId)
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(overlay.markers.begin(), iterator));
}

std::optional<TileMarkerInspection> InspectMarker(
    const CompiledTileSemanticOverlay& overlay,
    const CompiledTileMap& tileMap,
    const std::string_view semanticId) noexcept
{
    const std::optional<std::size_t> markerIndex = FindMarkerIndex(overlay, semanticId);
    if (!markerIndex.has_value())
    {
        return std::nullopt;
    }
    const CompiledTileMarker& marker = overlay.markers[*markerIndex];
    const std::size_t layerIndex = static_cast<std::size_t>(marker.layerIndex);
    if (layerIndex >= tileMap.layers.size())
    {
        return std::nullopt;
    }
    return TileMarkerInspection{
        .markerIndex = *markerIndex,
        .semanticId = marker.semanticId,
        .kind = marker.kind,
        .layerIndex = layerIndex,
        .layerSemanticId = tileMap.layers[layerIndex].semanticId,
        .worldX = marker.worldX,
        .worldY = marker.worldY,
        .tags = marker.tags,
    };
}
} // namespace trace2d::tile
