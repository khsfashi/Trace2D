#include <trace2d/tile/GeneratedTileMap2D.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <locale>
#include <numeric>
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
void AddDiagnostic(
    std::vector<TileDiagnostic>& diagnostics,
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
    diagnostics.push_back(std::move(diagnostic));
}

bool IsKnownKey(const std::string_view key, const std::initializer_list<std::string_view> keys)
{
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

void ValidateKnownKeys(
    const toml::table& table,
    const std::string_view path,
    const std::initializer_list<std::string_view> keys,
    std::vector<TileDiagnostic>& diagnostics)
{
    for (const auto& [key, value] : table)
    {
        const std::string_view name = key.str();
        if (IsKnownKey(name, keys))
        {
            continue;
        }

        std::string fieldPath{path};
        if (!fieldPath.empty())
        {
            fieldPath.push_back('.');
        }
        fieldPath.append(name);
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

bool ReadInt32(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::int32_t& output,
    std::vector<TileDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    const std::optional<std::int64_t> value = node == nullptr ? std::nullopt : node->value<std::int64_t>();
    constexpr std::int64_t Min = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min());
    constexpr std::int64_t Max = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
    if (!value.has_value() || *value < Min || *value > Max)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected an int32 value.", node);
        return false;
    }
    output = static_cast<std::int32_t>(*value);
    return true;
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
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected exactly two int32 values.", node);
        return false;
    }

    constexpr std::int64_t Min = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min());
    constexpr std::int64_t Max = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
    const toml::node* firstNode = values->get(0U);
    const toml::node* secondNode = values->get(1U);
    const std::optional<std::int64_t> firstValue = firstNode == nullptr ? std::nullopt : firstNode->value<std::int64_t>();
    const std::optional<std::int64_t> secondValue = secondNode == nullptr ? std::nullopt : secondNode->value<std::int64_t>();
    if (!firstValue.has_value() || !secondValue.has_value() ||
        *firstValue < Min || *firstValue > Max || *secondValue < Min || *secondValue > Max)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Pair values must fit int32.", node);
        return false;
    }

    first = static_cast<std::int32_t>(*firstValue);
    second = static_cast<std::int32_t>(*secondValue);
    return true;
}

bool ReadUInt32Pair(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::uint32_t& first,
    std::uint32_t& second,
    std::vector<TileDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    const toml::array* values = node == nullptr ? nullptr : node->as_array();
    if (values == nullptr || values->size() != 2U)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected exactly two uint32 values.", node);
        return false;
    }

    constexpr std::int64_t Max = static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max());
    const toml::node* firstNode = values->get(0U);
    const toml::node* secondNode = values->get(1U);
    const std::optional<std::int64_t> firstValue = firstNode == nullptr ? std::nullopt : firstNode->value<std::int64_t>();
    const std::optional<std::int64_t> secondValue = secondNode == nullptr ? std::nullopt : secondNode->value<std::int64_t>();
    if (!firstValue.has_value() || !secondValue.has_value() ||
        *firstValue < 0 || *firstValue > Max || *secondValue < 0 || *secondValue > Max)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Pair values must fit uint32.", node);
        return false;
    }

    first = static_cast<std::uint32_t>(*firstValue);
    second = static_cast<std::uint32_t>(*secondValue);
    return true;
}

void ReadOptionalBool(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    bool& output,
    std::vector<TileDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        return;
    }
    const std::optional<bool> value = node->value<bool>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected a bool value.", node);
        return;
    }
    output = *value;
}

std::vector<std::string> ReadStringArray(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<TileDiagnostic>& diagnostics)
{
    std::vector<std::string> output{};
    const toml::node* node = table.get(key);
    const toml::array* values = node == nullptr ? nullptr : node->as_array();
    if (values == nullptr)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected an array of strings.", node);
        return output;
    }

    output.reserve(values->size());
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
            output.emplace_back();
            continue;
        }
        output.push_back(*value);
    }
    return output;
}

std::vector<std::int32_t> ReadTileIndexArray(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<TileDiagnostic>& diagnostics)
{
    std::vector<std::int32_t> output{};
    const toml::node* node = table.get(key);
    const toml::array* values = node == nullptr ? nullptr : node->as_array();
    if (values == nullptr)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected an array of int32 tile-table indices.", node);
        return output;
    }

    constexpr std::int64_t Min = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min());
    constexpr std::int64_t Max = static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
    output.reserve(values->size());
    for (std::size_t index = 0U; index < values->size(); ++index)
    {
        const toml::node* valueNode = values->get(index);
        const std::optional<std::int64_t> value = valueNode == nullptr ? std::nullopt : valueNode->value<std::int64_t>();
        if (!value.has_value() || *value < Min || *value > Max)
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                std::string{path} + "[" + std::to_string(index) + "]",
                "Expected an int32 tile-table index.",
                valueNode);
            output.push_back(GeneratedEmptyTileTableIndex);
            continue;
        }
        output.push_back(static_cast<std::int32_t>(*value));
    }
    return output;
}

std::vector<std::uint8_t> ReadTransformArray(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<TileDiagnostic>& diagnostics)
{
    std::vector<std::uint8_t> output{};
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        return output;
    }

    const toml::array* values = node->as_array();
    if (values == nullptr)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected an array of transform-bit integers.", node);
        return output;
    }

    output.reserve(values->size());
    for (std::size_t index = 0U; index < values->size(); ++index)
    {
        const toml::node* valueNode = values->get(index);
        const std::optional<std::int64_t> value = valueNode == nullptr ? std::nullopt : valueNode->value<std::int64_t>();
        if (!value.has_value() || *value < 0 || *value > static_cast<std::int64_t>(GeneratedTileTransformMask))
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                std::string{path} + "[" + std::to_string(index) + "]",
                "Transform bits must be in [0, 15].",
                valueNode);
            output.push_back(0U);
            continue;
        }
        output.push_back(static_cast<std::uint8_t>(*value));
    }
    return output;
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

void WriteStringArray(std::ostream& stream, const std::vector<std::string>& values)
{
    stream << '[';
    for (std::size_t index = 0U; index < values.size(); ++index)
    {
        if (index != 0U)
        {
            stream << ", ";
        }
        WriteQuoted(stream, values[index]);
    }
    stream << ']';
}

template <typename T>
void WriteNumericArray(std::ostream& stream, const std::vector<T>& values)
{
    stream << '[';
    for (std::size_t index = 0U; index < values.size(); ++index)
    {
        if (index != 0U)
        {
            stream << ", ";
        }
        stream << static_cast<std::int64_t>(values[index]);
    }
    stream << ']';
}

TileTransform DecodeTransform(const std::uint8_t bits) noexcept
{
    return TileTransform{
        .flipX = (bits & 0x04U) != 0U,
        .flipY = (bits & 0x08U) != 0U,
        .quarterTurns = static_cast<std::uint8_t>(bits & 0x03U),
    };
}

std::vector<TileDiagnostic> ValidateDocumentShape(const GeneratedTileMapDocument& document)
{
    std::vector<TileDiagnostic> diagnostics{};
    if (document.semanticId.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "id", "Generated TileMap id must not be empty.");
    }
    if (document.tileSetSemanticId.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "tile_set", "tile_set must not be empty.");
    }
    if (document.cellWidth == 0U || document.cellHeight == 0U)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "cell_size", "cell_size dimensions must be greater than zero.");
    }
    if (document.layers.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "layers", "At least one generated layer is required.");
    }

    std::vector<std::pair<std::string_view, std::size_t>> tileIds{};
    tileIds.reserve(document.tileTable.size());
    for (std::size_t index = 0U; index < document.tileTable.size(); ++index)
    {
        if (document.tileTable[index].empty())
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                "tile_table[" + std::to_string(index) + "]",
                "Tile-table semantic id must not be empty.");
        }
        tileIds.emplace_back(document.tileTable[index], index);
    }
    std::sort(tileIds.begin(), tileIds.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
    });
    for (std::size_t index = 1U; index < tileIds.size(); ++index)
    {
        if (!tileIds[index].first.empty() && tileIds[index - 1U].first == tileIds[index].first)
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                "tile_table[" + std::to_string(tileIds[index].second) + "]",
                "Duplicate tile-table semantic id.");
        }
    }

    std::vector<std::pair<std::string_view, std::size_t>> layerIds{};
    layerIds.reserve(document.layers.size());
    for (std::size_t layerIndex = 0U; layerIndex < document.layers.size(); ++layerIndex)
    {
        const GeneratedTileLayerDocument& layer = document.layers[layerIndex];
        const std::string path = "layers[" + std::to_string(layerIndex) + "]";
        if (layer.semanticId.empty())
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".id", "Layer id must not be empty.");
        }
        layerIds.emplace_back(layer.semanticId, layerIndex);

        if (layer.width == 0U || layer.height == 0U)
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".size", "Layer dimensions must be greater than zero.");
            continue;
        }

        const std::uint64_t cellCount =
            static_cast<std::uint64_t>(layer.width) * static_cast<std::uint64_t>(layer.height);
        if (cellCount > MaximumCompiledCellsPerLayer)
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                path + ".size",
                "Generated layer exceeds the current bounded dense TileMap cell limit.");
        }
        if (cellCount > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".size", "Layer cell count exceeds addressable size.");
            continue;
        }

        const std::int64_t maxX =
            static_cast<std::int64_t>(layer.originX) + static_cast<std::int64_t>(layer.width) - 1;
        const std::int64_t maxY =
            static_cast<std::int64_t>(layer.originY) + static_cast<std::int64_t>(layer.height) - 1;
        if (maxX > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) ||
            maxY > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()))
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                path + ".origin",
                "Layer world-coordinate extent must fit int32.");
        }

        const std::size_t expectedCount = static_cast<std::size_t>(cellCount);
        if (layer.tileTableIndices.size() != expectedCount)
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                path + ".tiles",
                "Dense tile payload length must equal width * height.");
        }
        if (!layer.transformBits.empty() && layer.transformBits.size() != expectedCount)
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                path + ".transforms",
                "Transform payload must be empty or have exactly width * height entries.");
        }

        const std::size_t checkedTileCount = std::min(layer.tileTableIndices.size(), expectedCount);
        for (std::size_t cellIndex = 0U; cellIndex < checkedTileCount; ++cellIndex)
        {
            const std::int32_t tileIndex = layer.tileTableIndices[cellIndex];
            if (tileIndex < GeneratedEmptyTileTableIndex ||
                (tileIndex >= 0 && static_cast<std::size_t>(tileIndex) >= document.tileTable.size()))
            {
                AddDiagnostic(
                    diagnostics,
                    TileErrorCode::SchemaError,
                    path + ".tiles[" + std::to_string(cellIndex) + "]",
                    "Tile index must be -1 or reference an entry in tile_table.");
            }

            if (cellIndex >= layer.transformBits.size())
            {
                continue;
            }
            const std::uint8_t transform = layer.transformBits[cellIndex];
            if ((transform & static_cast<std::uint8_t>(~GeneratedTileTransformMask)) != 0U)
            {
                AddDiagnostic(
                    diagnostics,
                    TileErrorCode::SchemaError,
                    path + ".transforms[" + std::to_string(cellIndex) + "]",
                    "Transform contains unsupported bits.");
            }
            if (tileIndex == GeneratedEmptyTileTableIndex && transform != 0U)
            {
                AddDiagnostic(
                    diagnostics,
                    TileErrorCode::SchemaError,
                    path + ".transforms[" + std::to_string(cellIndex) + "]",
                    "Empty cells must use identity transform bits.");
            }
        }
    }

    std::sort(layerIds.begin(), layerIds.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
    });
    for (std::size_t index = 1U; index < layerIds.size(); ++index)
    {
        if (!layerIds[index].first.empty() && layerIds[index - 1U].first == layerIds[index].first)
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                "layers[" + std::to_string(layerIds[index].second) + "].id",
                "Duplicate generated layer id.");
        }
    }
    return diagnostics;
}
} // namespace

GeneratedTileMapLoadResult ParseGeneratedTileMapToml(
    const std::string_view text,
    const std::string_view sourceName)
{
    GeneratedTileMapLoadResult result{};
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

    ValidateKnownKeys(
        *root,
        "",
        {"format_version", "id", "tile_set", "cell_size", "tile_table", "layers"},
        result.diagnostics);

    const toml::node* formatNode = root->get("format_version");
    const std::optional<std::int64_t> formatVersion =
        formatNode == nullptr ? std::nullopt : formatNode->value<std::int64_t>();
    if (!formatVersion.has_value() || *formatVersion != GeneratedTileMapDocument::FormatVersion)
    {
        AddDiagnostic(
            result.diagnostics,
            TileErrorCode::UnsupportedFormat,
            "format_version",
            "Unsupported generated TileMap format version. Expected 1.",
            formatNode);
    }

    GeneratedTileMapDocument document{};
    if (const std::optional<std::string> value = ReadRequiredString(*root, "id", "id", result.diagnostics); value.has_value())
    {
        document.semanticId = *value;
    }
    if (const std::optional<std::string> value = ReadRequiredString(*root, "tile_set", "tile_set", result.diagnostics); value.has_value())
    {
        document.tileSetSemanticId = *value;
    }
    ReadUInt32Pair(*root, "cell_size", "cell_size", document.cellWidth, document.cellHeight, result.diagnostics);
    document.tileTable = ReadStringArray(*root, "tile_table", "tile_table", result.diagnostics);

    const toml::node* layersNode = root->get("layers");
    const toml::array* layers = layersNode == nullptr ? nullptr : layersNode->as_array();
    if (layers == nullptr)
    {
        AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, "layers", "Expected an array of generated layer tables.", layersNode);
    }
    else
    {
        document.layers.reserve(layers->size());
        for (std::size_t index = 0U; index < layers->size(); ++index)
        {
            const toml::node* layerNode = layers->get(index);
            const toml::table* layerTable = layerNode == nullptr ? nullptr : layerNode->as_table();
            const std::string path = "layers[" + std::to_string(index) + "]";
            if (layerTable == nullptr)
            {
                AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, path, "Expected a generated layer table.", layerNode);
                continue;
            }

            ValidateKnownKeys(
                *layerTable,
                path,
                {"id", "order", "origin", "size", "visible", "tiles", "transforms"},
                result.diagnostics);

            GeneratedTileLayerDocument layer{};
            if (const std::optional<std::string> value = ReadRequiredString(*layerTable, "id", path + ".id", result.diagnostics); value.has_value())
            {
                layer.semanticId = *value;
            }
            ReadInt32(*layerTable, "order", path + ".order", layer.order, result.diagnostics);
            ReadInt32Pair(*layerTable, "origin", path + ".origin", layer.originX, layer.originY, result.diagnostics);
            ReadUInt32Pair(*layerTable, "size", path + ".size", layer.width, layer.height, result.diagnostics);
            ReadOptionalBool(*layerTable, "visible", path + ".visible", layer.visible, result.diagnostics);
            layer.tileTableIndices = ReadTileIndexArray(*layerTable, "tiles", path + ".tiles", result.diagnostics);
            layer.transformBits = ReadTransformArray(*layerTable, "transforms", path + ".transforms", result.diagnostics);
            document.layers.push_back(std::move(layer));
        }
    }

    std::vector<TileDiagnostic> shapeDiagnostics = ValidateDocumentShape(document);
    result.diagnostics.insert(
        result.diagnostics.end(),
        std::make_move_iterator(shapeDiagnostics.begin()),
        std::make_move_iterator(shapeDiagnostics.end()));
    if (result.diagnostics.empty())
    {
        result.document = std::move(document);
    }
    return result;
}

std::string SaveGeneratedTileMapToml(const GeneratedTileMapDocument& document)
{
    std::vector<std::size_t> tableOrder(document.tileTable.size());
    std::iota(tableOrder.begin(), tableOrder.end(), 0U);
    std::sort(tableOrder.begin(), tableOrder.end(), [&document](const std::size_t left, const std::size_t right) {
        if (document.tileTable[left] != document.tileTable[right])
        {
            return document.tileTable[left] < document.tileTable[right];
        }
        return left < right;
    });

    std::vector<std::string> canonicalTable{};
    canonicalTable.reserve(document.tileTable.size());
    std::vector<std::int32_t> oldToNew(document.tileTable.size(), GeneratedEmptyTileTableIndex);
    for (std::size_t newIndex = 0U; newIndex < tableOrder.size(); ++newIndex)
    {
        const std::size_t oldIndex = tableOrder[newIndex];
        canonicalTable.push_back(document.tileTable[oldIndex]);
        if (newIndex <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
        {
            oldToNew[oldIndex] = static_cast<std::int32_t>(newIndex);
        }
    }

    std::vector<const GeneratedTileLayerDocument*> layers{};
    layers.reserve(document.layers.size());
    for (const GeneratedTileLayerDocument& layer : document.layers)
    {
        layers.push_back(&layer);
    }
    std::sort(layers.begin(), layers.end(), [](const GeneratedTileLayerDocument* left, const GeneratedTileLayerDocument* right) {
        return std::tie(left->order, left->semanticId) < std::tie(right->order, right->semanticId);
    });

    std::ostringstream stream{};
    stream.imbue(std::locale::classic());
    stream << "format_version = 1\nid = ";
    WriteQuoted(stream, document.semanticId);
    stream << "\ntile_set = ";
    WriteQuoted(stream, document.tileSetSemanticId);
    stream << "\ncell_size = [" << document.cellWidth << ", " << document.cellHeight << "]\ntile_table = ";
    WriteStringArray(stream, canonicalTable);
    stream << "\n";

    for (const GeneratedTileLayerDocument* layer : layers)
    {
        std::vector<std::int32_t> remapped = layer->tileTableIndices;
        for (std::int32_t& tileIndex : remapped)
        {
            if (tileIndex >= 0 && static_cast<std::size_t>(tileIndex) < oldToNew.size())
            {
                tileIndex = oldToNew[static_cast<std::size_t>(tileIndex)];
            }
        }

        stream << "\n[[layers]]\nid = ";
        WriteQuoted(stream, layer->semanticId);
        stream << "\norder = " << layer->order;
        stream << "\norigin = [" << layer->originX << ", " << layer->originY << "]";
        stream << "\nsize = [" << layer->width << ", " << layer->height << "]";
        stream << "\nvisible = " << (layer->visible ? "true" : "false");
        stream << "\ntiles = ";
        WriteNumericArray(stream, remapped);
        if (!layer->transformBits.empty())
        {
            stream << "\ntransforms = ";
            WriteNumericArray(stream, layer->transformBits);
        }
        stream << "\n";
    }
    return stream.str();
}

std::vector<TileDiagnostic> ValidateGeneratedTileMap(
    const CompiledTileSet& tileSet,
    const GeneratedTileMapDocument& document)
{
    std::vector<TileDiagnostic> diagnostics = ValidateDocumentShape(document);
    if (document.tileSetSemanticId != tileSet.semanticId)
    {
        AddDiagnostic(
            diagnostics,
            TileErrorCode::SchemaError,
            "tile_set",
            "Generated TileMap tile_set does not match the supplied compiled TileSet identity.");
    }

    for (std::size_t index = 0U; index < document.tileTable.size(); ++index)
    {
        if (!document.tileTable[index].empty() && !FindTileIndex(tileSet, document.tileTable[index]).has_value())
        {
            AddDiagnostic(
                diagnostics,
                TileErrorCode::SchemaError,
                "tile_table[" + std::to_string(index) + "]",
                "Tile table references an unknown compiled tile id.");
        }
    }
    return diagnostics;
}

GeneratedTileMapConversionResult ConvertGeneratedTileMap(
    const CompiledTileSet& tileSet,
    const GeneratedTileMapDocument& document)
{
    GeneratedTileMapConversionResult result{};
    result.diagnostics = ValidateGeneratedTileMap(tileSet, document);
    if (!result.diagnostics.empty())
    {
        return result;
    }

    TileMapDocument output{};
    output.semanticId = document.semanticId;
    output.tileSetSemanticId = document.tileSetSemanticId;
    output.cellWidth = document.cellWidth;
    output.cellHeight = document.cellHeight;
    output.layers.reserve(document.layers.size());
    result.metrics.tileTableEntries = document.tileTable.size();

    for (const GeneratedTileLayerDocument& sourceLayer : document.layers)
    {
        const std::uint64_t denseCellCount =
            static_cast<std::uint64_t>(sourceLayer.width) * static_cast<std::uint64_t>(sourceLayer.height);
        result.metrics.denseCellCount += denseCellCount;
        result.metrics.knownDensePayloadBytes +=
            denseCellCount * static_cast<std::uint64_t>(sizeof(std::int32_t));
        if (!sourceLayer.transformBits.empty())
        {
            result.metrics.knownDensePayloadBytes +=
                denseCellCount * static_cast<std::uint64_t>(sizeof(std::uint8_t));
        }

        const std::size_t occupiedCount = static_cast<std::size_t>(std::count_if(
            sourceLayer.tileTableIndices.begin(),
            sourceLayer.tileTableIndices.end(),
            [](const std::int32_t tileIndex) { return tileIndex != GeneratedEmptyTileTableIndex; }));

        TileLayerDocument outputLayer{};
        outputLayer.semanticId = sourceLayer.semanticId;
        outputLayer.order = sourceLayer.order;
        outputLayer.originX = sourceLayer.originX;
        outputLayer.originY = sourceLayer.originY;
        outputLayer.width = sourceLayer.width;
        outputLayer.height = sourceLayer.height;
        outputLayer.visible = sourceLayer.visible;
        outputLayer.cells.reserve(occupiedCount);
        result.metrics.occupiedCellCount += static_cast<std::uint64_t>(occupiedCount);

        for (std::size_t cellIndex = 0U; cellIndex < sourceLayer.tileTableIndices.size(); ++cellIndex)
        {
            const std::int32_t tileIndex = sourceLayer.tileTableIndices[cellIndex];
            if (tileIndex == GeneratedEmptyTileTableIndex)
            {
                continue;
            }

            const std::size_t localX = cellIndex % static_cast<std::size_t>(sourceLayer.width);
            const std::size_t localY = cellIndex / static_cast<std::size_t>(sourceLayer.width);
            const std::uint8_t transformBits =
                sourceLayer.transformBits.empty() ? 0U : sourceLayer.transformBits[cellIndex];
            outputLayer.cells.push_back(TileCellDocument{
                .x = static_cast<std::int32_t>(localX),
                .y = static_cast<std::int32_t>(localY),
                .tileSemanticId = document.tileTable[static_cast<std::size_t>(tileIndex)],
                .transform = DecodeTransform(transformBits),
            });
        }

        result.metrics.outputCellCapacity += outputLayer.cells.capacity();
        output.layers.push_back(std::move(outputLayer));
    }

    std::sort(output.layers.begin(), output.layers.end(), [](const TileLayerDocument& left, const TileLayerDocument& right) {
        return std::tie(left.order, left.semanticId) < std::tie(right.order, right.semanticId);
    });

    const std::vector<TileDiagnostic> outputDiagnostics = ValidateTileMap(output);
    if (!outputDiagnostics.empty())
    {
        result.diagnostics.insert(result.diagnostics.end(), outputDiagnostics.begin(), outputDiagnostics.end());
        return result;
    }

    result.tileMap = std::move(output);
    return result;
}
} // namespace trace2d::tile
