#include <trace2d/tile/TileMap.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <locale>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace trace2d::tile
{
namespace
{
constexpr std::uintmax_t MaximumTileDocumentSourceBytes = 8U * 1024U * 1024U;

struct NormalizedReference final
{
    std::string id{};
    std::filesystem::path resolvedPath{};
};

TileDiagnostic MakeDiagnostic(
    const TileErrorCode code,
    std::string path,
    std::string message,
    const std::string_view reference = {},
    const std::filesystem::path& resolvedPath = {},
    const toml::node* node = nullptr)
{
    TileDiagnostic diagnostic{};
    diagnostic.code = code;
    diagnostic.reference = std::string{reference};
    diagnostic.resolvedPath = resolvedPath.empty() ? std::string{} : resolvedPath.generic_string();
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
    diagnostics.push_back(MakeDiagnostic(code, std::move(path), std::move(message), {}, {}, node));
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
    std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected a string.", node);
        return std::nullopt;
    }
    if (value->empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Value must not be empty.", node);
        return std::nullopt;
    }
    return value;
}

std::optional<std::int64_t> ReadRequiredInteger(
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
    const std::optional<std::int64_t> value = node->value<std::int64_t>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected an integer.", node);
    }
    return value;
}

std::optional<std::int32_t> ReadRequiredInt32(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<TileDiagnostic>& diagnostics)
{
    const std::optional<std::int64_t> value = ReadRequiredInteger(table, key, path, diagnostics);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    if (*value < static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) ||
        *value > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()))
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Integer is outside the int32 range.", table.get(key));
        return std::nullopt;
    }
    return static_cast<std::int32_t>(*value);
}

std::optional<std::uint32_t> ReadRequiredUInt32(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<TileDiagnostic>& diagnostics)
{
    const std::optional<std::int64_t> value = ReadRequiredInteger(table, key, path, diagnostics);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    if (*value < 0 || static_cast<std::uint64_t>(*value) > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Integer is outside the uint32 range.", table.get(key));
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*value);
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
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Required field is missing.");
        return false;
    }
    const toml::array* values = node->as_array();
    if (values == nullptr || values->size() != 2U)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected an array containing exactly two integers.", node);
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
    std::int32_t signedFirst = 0;
    std::int32_t signedSecond = 0;
    if (!ReadInt32Pair(table, key, path, signedFirst, signedSecond, diagnostics))
    {
        return false;
    }
    if (signedFirst < 0 || signedSecond < 0)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Pair values must be non-negative.", table.get(key));
        return false;
    }
    first = static_cast<std::uint32_t>(signedFirst);
    second = static_cast<std::uint32_t>(signedSecond);
    return true;
}

bool ReadRegion(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    TileRegion& region,
    std::vector<TileDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Required field is missing.");
        return false;
    }
    const toml::array* values = node->as_array();
    if (values == nullptr || values->size() != 4U)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected [x, y, width, height].", node);
        return false;
    }
    std::int32_t parsed[4]{};
    for (std::size_t index = 0U; index < 4U; ++index)
    {
        const toml::node* valueNode = values->get(index);
        const std::optional<std::int64_t> value = valueNode == nullptr ? std::nullopt : valueNode->value<std::int64_t>();
        if (!value.has_value() || *value < static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) ||
            *value > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()))
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Region values must fit int32.", node);
            return false;
        }
        parsed[index] = static_cast<std::int32_t>(*value);
    }
    region = TileRegion{parsed[0], parsed[1], parsed[2], parsed[3]};
    return true;
}

bool ReadOptionalBool(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    const bool fallback,
    std::vector<TileDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        return fallback;
    }
    const std::optional<bool> value = node->value<bool>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected a boolean.", node);
        return fallback;
    }
    return *value;
}

std::uint8_t ReadOptionalQuarterTurns(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<TileDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        return 0U;
    }
    const std::optional<std::int64_t> value = node->value<std::int64_t>();
    if (!value.has_value() || *value < 0 || *value > 3)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, std::string{path}, "Expected an integer in [0, 3].", node);
        return 0U;
    }
    return static_cast<std::uint8_t>(*value);
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

bool IsAsciiDrivePrefix(const std::string_view value) noexcept
{
    return value.size() >= 2U && ((value[0] >= 'A' && value[0] <= 'Z') || (value[0] >= 'a' && value[0] <= 'z')) && value[1] == ':';
}

bool IsPortableProjectReference(const std::string_view reference) noexcept
{
    if (reference.empty())
    {
        return false;
    }
    std::string portable{reference};
    std::replace(portable.begin(), portable.end(), '\\', '/');
    if (portable.front() == '/' || IsAsciiDrivePrefix(portable))
    {
        return false;
    }
    std::size_t cursor = 0U;
    while (cursor <= portable.size())
    {
        const std::size_t separator = portable.find('/', cursor);
        const std::size_t end = separator == std::string::npos ? portable.size() : separator;
        const std::string_view component{portable.data() + cursor, end - cursor};
        if (component == ".." || component.find('\0') != std::string_view::npos)
        {
            return false;
        }
        if (separator == std::string::npos)
        {
            break;
        }
        cursor = separator + 1U;
    }
    return true;
}

bool TryNormalizeReference(
    const std::filesystem::path& projectRoot,
    const std::string_view reference,
    NormalizedReference& normalized,
    TileDiagnostic* diagnostic)
{
    if (!IsPortableProjectReference(reference))
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = MakeDiagnostic(
                TileErrorCode::InvalidReference,
                "$reference",
                "Tile document reference must be a non-empty project-relative path without parent traversal.",
                reference);
        }
        return false;
    }

    std::string portable{reference};
    std::replace(portable.begin(), portable.end(), '\\', '/');
    normalized.id.clear();
    normalized.id.reserve(portable.size());
    std::size_t cursor = 0U;
    while (cursor <= portable.size())
    {
        const std::size_t separator = portable.find('/', cursor);
        const std::size_t end = separator == std::string::npos ? portable.size() : separator;
        const std::string_view component{portable.data() + cursor, end - cursor};
        if (!component.empty() && component != ".")
        {
            if (!normalized.id.empty())
            {
                normalized.id.push_back('/');
            }
            normalized.id.append(component);
        }
        if (separator == std::string::npos)
        {
            break;
        }
        cursor = separator + 1U;
    }
    if (normalized.id.empty())
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = MakeDiagnostic(TileErrorCode::InvalidReference, "$reference", "Tile document reference must identify a file below the project root.", reference);
        }
        return false;
    }
    normalized.resolvedPath = (projectRoot / std::filesystem::path{normalized.id}).lexically_normal();
    return true;
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

std::vector<TileDefinition> CanonicalTiles(const TileSetDocument& document)
{
    std::vector<TileDefinition> tiles = document.tiles;
    std::sort(tiles.begin(), tiles.end(), [](const TileDefinition& left, const TileDefinition& right) {
        return left.semanticId < right.semanticId;
    });
    for (TileDefinition& tile : tiles)
    {
        std::sort(tile.tags.begin(), tile.tags.end());
    }
    return tiles;
}

std::vector<TileLayerDocument> CanonicalLayers(const TileMapDocument& document)
{
    std::vector<TileLayerDocument> layers = document.layers;
    std::sort(layers.begin(), layers.end(), [](const TileLayerDocument& left, const TileLayerDocument& right) {
        if (left.order != right.order)
        {
            return left.order < right.order;
        }
        return left.semanticId < right.semanticId;
    });
    for (TileLayerDocument& layer : layers)
    {
        std::sort(layer.cells.begin(), layer.cells.end(), [](const TileCellDocument& left, const TileCellDocument& right) {
            if (left.y != right.y)
            {
                return left.y < right.y;
            }
            if (left.x != right.x)
            {
                return left.x < right.x;
            }
            return left.tileSemanticId < right.tileSemanticId;
        });
    }
    return layers;
}

void ValidateFormatVersion(
    const toml::table& root,
    const std::int64_t expected,
    std::vector<TileDiagnostic>& diagnostics,
    const std::string_view kind)
{
    const toml::node* formatNode = root.get("format_version");
    if (formatNode == nullptr)
    {
        AddDiagnostic(diagnostics, TileErrorCode::UnsupportedFormat, "format_version", "Required field is missing.");
        return;
    }
    const std::optional<std::int64_t> version = formatNode->value<std::int64_t>();
    if (!version.has_value() || *version != expected)
    {
        AddDiagnostic(
            diagnostics,
            TileErrorCode::UnsupportedFormat,
            "format_version",
            "Unsupported " + std::string{kind} + " format version. Expected 1.",
            formatNode);
    }
}

std::optional<toml::table> ParseToml(
    const std::string_view text,
    const std::string_view sourceName,
    std::vector<TileDiagnostic>& diagnostics)
{
    try
    {
        return toml::parse(text, sourceName);
    }
    catch (const toml::parse_error& error)
    {
        TileDiagnostic diagnostic{};
        diagnostic.code = TileErrorCode::ParseError;
        diagnostic.path = "$";
        diagnostic.message = std::string{error.description()};
        diagnostic.line = static_cast<std::size_t>(error.source().begin.line);
        diagnostic.column = static_cast<std::size_t>(error.source().begin.column);
        diagnostics.push_back(std::move(diagnostic));
        return std::nullopt;
    }
}

void AttachSourceContext(
    std::vector<TileDiagnostic>& diagnostics,
    const std::string_view reference,
    const std::filesystem::path& resolvedPath)
{
    for (TileDiagnostic& diagnostic : diagnostics)
    {
        diagnostic.reference = std::string{reference};
        diagnostic.resolvedPath = resolvedPath.generic_string();
    }
}

bool ReadDocumentText(
    const std::filesystem::path& projectRoot,
    const std::string_view reference,
    NormalizedReference& normalized,
    std::string& text,
    std::vector<TileDiagnostic>& diagnostics)
{
    TileDiagnostic referenceDiagnostic{};
    if (!TryNormalizeReference(projectRoot, reference, normalized, &referenceDiagnostic))
    {
        diagnostics.push_back(std::move(referenceDiagnostic));
        return false;
    }
    std::error_code error{};
    if (!std::filesystem::exists(normalized.resolvedPath, error) || error)
    {
        diagnostics.push_back(MakeDiagnostic(TileErrorCode::MissingFile, "$reference", "Tile document file does not exist.", normalized.id, normalized.resolvedPath));
        return false;
    }
    const std::uintmax_t sourceBytes = std::filesystem::file_size(normalized.resolvedPath, error);
    if (error || sourceBytes > MaximumTileDocumentSourceBytes)
    {
        diagnostics.push_back(MakeDiagnostic(
            TileErrorCode::ReadFailure,
            "$reference",
            error ? "Could not determine tile document file size." : "Tile document source exceeds the 8 MiB setup-time limit.",
            normalized.id,
            normalized.resolvedPath));
        return false;
    }
    std::ifstream stream{normalized.resolvedPath, std::ios::binary};
    if (!stream)
    {
        diagnostics.push_back(MakeDiagnostic(TileErrorCode::ReadFailure, "$reference", "Could not open tile document file.", normalized.id, normalized.resolvedPath));
        return false;
    }
    text.assign(std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{});
    if (!stream.good() && !stream.eof())
    {
        diagnostics.push_back(MakeDiagnostic(TileErrorCode::ReadFailure, "$reference", "Could not read tile document file.", normalized.id, normalized.resolvedPath));
        return false;
    }
    return true;
}

std::vector<TileDiagnostic> WriteDocumentText(
    const std::filesystem::path& projectRoot,
    const std::string_view reference,
    const std::string_view canonical)
{
    std::vector<TileDiagnostic> diagnostics{};
    NormalizedReference normalized{};
    TileDiagnostic referenceDiagnostic{};
    if (!TryNormalizeReference(projectRoot, reference, normalized, &referenceDiagnostic))
    {
        diagnostics.push_back(std::move(referenceDiagnostic));
        return diagnostics;
    }
    std::error_code error{};
    const std::filesystem::path parent = normalized.resolvedPath.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, error);
        if (error)
        {
            diagnostics.push_back(MakeDiagnostic(TileErrorCode::WriteFailure, "$reference", "Could not create tile document parent directory.", normalized.id, normalized.resolvedPath));
            return diagnostics;
        }
    }
    const std::filesystem::path temporary = normalized.resolvedPath.string() + ".tmp";
    {
        std::ofstream stream{temporary, std::ios::binary | std::ios::trunc};
        if (!stream)
        {
            diagnostics.push_back(MakeDiagnostic(TileErrorCode::WriteFailure, "$reference", "Could not open temporary tile document for writing.", normalized.id, temporary));
            return diagnostics;
        }
        stream.write(canonical.data(), static_cast<std::streamsize>(canonical.size()));
        stream.flush();
        if (!stream)
        {
            diagnostics.push_back(MakeDiagnostic(TileErrorCode::WriteFailure, "$reference", "Could not write complete canonical tile document.", normalized.id, temporary));
            return diagnostics;
        }
    }
    // #79 owns the final persistence policy. T0 follows the existing authored-input pattern:
    // validate, write a sibling temporary, then replace only at this explicit setup boundary.
    std::filesystem::remove(normalized.resolvedPath, error);
    error.clear();
    std::filesystem::rename(temporary, normalized.resolvedPath, error);
    if (error)
    {
        std::error_code cleanupError{};
        std::filesystem::remove(temporary, cleanupError);
        diagnostics.push_back(MakeDiagnostic(TileErrorCode::WriteFailure, "$reference", "Could not replace canonical tile document.", normalized.id, normalized.resolvedPath));
    }
    return diagnostics;
}
} // namespace

std::string_view ToString(const TileErrorCode code) noexcept
{
    switch (code)
    {
    case TileErrorCode::InvalidReference: return "invalid_reference";
    case TileErrorCode::UnsupportedFormat: return "unsupported_format";
    case TileErrorCode::MissingFile: return "missing_file";
    case TileErrorCode::ReadFailure: return "read_failure";
    case TileErrorCode::ParseError: return "parse_error";
    case TileErrorCode::SchemaError: return "schema_error";
    case TileErrorCode::WriteFailure: return "write_failure";
    }
    return "unknown_tile_error";
}

std::vector<TileDiagnostic> ValidateTileSet(const TileSetDocument& document)
{
    std::vector<TileDiagnostic> diagnostics{};
    if (document.semanticId.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "id", "TileSet id must not be empty.");
    }
    if (!IsPortableProjectReference(document.textureReference))
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "texture", "Texture reference must be project-relative without parent traversal.");
    }
    if (document.sourceWidth <= 0 || document.sourceHeight <= 0)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "source_size", "TileSet source dimensions must be positive.");
    }
    if (document.tiles.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "tiles", "TileSet must define at least one tile.");
    }

    std::unordered_set<std::string_view> tileIds{};
    tileIds.reserve(document.tiles.size());
    for (std::size_t index = 0U; index < document.tiles.size(); ++index)
    {
        const TileDefinition& tile = document.tiles[index];
        const std::string path = "tiles[" + std::to_string(index) + "]";
        if (tile.semanticId.empty())
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".id", "Tile id must not be empty.");
        }
        else if (!tileIds.insert(tile.semanticId).second)
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".id", "Duplicate tile id.");
        }
        const std::int64_t right = static_cast<std::int64_t>(tile.sourceRegion.x) + static_cast<std::int64_t>(tile.sourceRegion.width);
        const std::int64_t bottom = static_cast<std::int64_t>(tile.sourceRegion.y) + static_cast<std::int64_t>(tile.sourceRegion.height);
        if (tile.sourceRegion.x < 0 || tile.sourceRegion.y < 0 || tile.sourceRegion.width <= 0 || tile.sourceRegion.height <= 0 ||
            right > static_cast<std::int64_t>(document.sourceWidth) || bottom > static_cast<std::int64_t>(document.sourceHeight))
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".region", "Tile region must be positive and contained by source_size.");
        }
        std::unordered_set<std::string_view> tags{};
        tags.reserve(tile.tags.size());
        for (std::size_t tagIndex = 0U; tagIndex < tile.tags.size(); ++tagIndex)
        {
            if (tile.tags[tagIndex].empty())
            {
                AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".tags[" + std::to_string(tagIndex) + "]", "Tile tag must not be empty.");
            }
            else if (!tags.insert(tile.tags[tagIndex]).second)
            {
                AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".tags[" + std::to_string(tagIndex) + "]", "Duplicate tile tag.");
            }
        }
    }
    return diagnostics;
}

std::vector<TileDiagnostic> ValidateTileMap(const TileMapDocument& document)
{
    std::vector<TileDiagnostic> diagnostics{};
    if (document.semanticId.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "id", "TileMap id must not be empty.");
    }
    if (document.tileSetSemanticId.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "tile_set", "TileMap tile_set id must not be empty.");
    }
    if (document.cellWidth == 0U || document.cellHeight == 0U)
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "cell_size", "TileMap cell dimensions must be positive.");
    }
    if (document.layers.empty())
    {
        AddDiagnostic(diagnostics, TileErrorCode::SchemaError, "layers", "TileMap must define at least one layer.");
    }

    std::unordered_set<std::string_view> layerIds{};
    layerIds.reserve(document.layers.size());
    for (std::size_t layerIndex = 0U; layerIndex < document.layers.size(); ++layerIndex)
    {
        const TileLayerDocument& layer = document.layers[layerIndex];
        const std::string path = "layers[" + std::to_string(layerIndex) + "]";
        if (layer.semanticId.empty())
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".id", "Layer id must not be empty.");
        }
        else if (!layerIds.insert(layer.semanticId).second)
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".id", "Duplicate layer id.");
        }
        if (layer.width == 0U || layer.height == 0U)
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".size", "Layer dimensions must be positive.");
        }
        const std::uint64_t cellCount = static_cast<std::uint64_t>(layer.width) * static_cast<std::uint64_t>(layer.height);
        if (cellCount > MaximumCompiledCellsPerLayer)
        {
            AddDiagnostic(diagnostics, TileErrorCode::SchemaError, path + ".size", "Layer exceeds the T0 compiled-cell safety bound.");
        }

        std::unordered_set<std::uint64_t> occupiedCoordinates{};
        occupiedCoordinates.reserve(layer.cells.size());
        for (std::size_t cellIndex = 0U; cellIndex < layer.cells.size(); ++cellIndex)
        {
            const TileCellDocument& cell = layer.cells[cellIndex];
            const std::string cellPath = path + ".cells[" + std::to_string(cellIndex) + "]";
            if (cell.x < 0 || cell.y < 0 || static_cast<std::uint32_t>(cell.x) >= layer.width || static_cast<std::uint32_t>(cell.y) >= layer.height)
            {
                AddDiagnostic(diagnostics, TileErrorCode::SchemaError, cellPath, "Cell coordinate is outside the layer-local size.");
                continue;
            }
            if (cell.tileSemanticId.empty())
            {
                AddDiagnostic(diagnostics, TileErrorCode::SchemaError, cellPath + ".tile", "Cell tile id must not be empty.");
            }
            if (cell.transform.quarterTurns > 3U)
            {
                AddDiagnostic(diagnostics, TileErrorCode::SchemaError, cellPath + ".rotation_quarters", "rotation_quarters must be in [0, 3].");
            }
            const std::uint64_t coordinateKey = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell.y)) << 32U) |
                static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell.x));
            if (!occupiedCoordinates.insert(coordinateKey).second)
            {
                AddDiagnostic(diagnostics, TileErrorCode::SchemaError, cellPath, "Duplicate authored cell coordinate.");
            }
        }
    }
    return diagnostics;
}

TileSetLoadResult ParseTileSetToml(const std::string_view text, const std::string_view sourceName)
{
    TileSetLoadResult result{};
    std::optional<toml::table> root = ParseToml(text, sourceName, result.diagnostics);
    if (!root.has_value())
    {
        return result;
    }
    ValidateKnownKeys(*root, "", {"format_version", "id", "texture", "source_size", "tiles"}, result.diagnostics);
    ValidateFormatVersion(*root, TileSetDocument::FormatVersion, result.diagnostics, "TileSet");

    TileSetDocument document{};
    if (const std::optional<std::string> id = ReadRequiredString(*root, "id", "id", result.diagnostics); id.has_value())
    {
        document.semanticId = *id;
    }
    if (const std::optional<std::string> texture = ReadRequiredString(*root, "texture", "texture", result.diagnostics); texture.has_value())
    {
        document.textureReference = *texture;
    }
    ReadInt32Pair(*root, "source_size", "source_size", document.sourceWidth, document.sourceHeight, result.diagnostics);

    const toml::node* tilesNode = root->get("tiles");
    const toml::array* tiles = tilesNode == nullptr ? nullptr : tilesNode->as_array();
    if (tiles == nullptr)
    {
        AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, "tiles", "Expected an array of tile tables.", tilesNode);
    }
    else
    {
        document.tiles.reserve(tiles->size());
        for (std::size_t index = 0U; index < tiles->size(); ++index)
        {
            const toml::node* tileNode = tiles->get(index);
            const toml::table* tileTable = tileNode == nullptr ? nullptr : tileNode->as_table();
            const std::string path = "tiles[" + std::to_string(index) + "]";
            if (tileTable == nullptr)
            {
                AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, path, "Expected a tile table.", tileNode);
                continue;
            }
            ValidateKnownKeys(*tileTable, path, {"id", "region", "tags"}, result.diagnostics);
            TileDefinition tile{};
            if (const std::optional<std::string> id = ReadRequiredString(*tileTable, "id", path + ".id", result.diagnostics); id.has_value())
            {
                tile.semanticId = *id;
            }
            ReadRegion(*tileTable, "region", path + ".region", tile.sourceRegion, result.diagnostics);
            tile.tags = ReadOptionalStringArray(*tileTable, "tags", path + ".tags", result.diagnostics);
            document.tiles.push_back(std::move(tile));
        }
    }

    std::vector<TileDiagnostic> validation = ValidateTileSet(document);
    result.diagnostics.insert(result.diagnostics.end(), std::make_move_iterator(validation.begin()), std::make_move_iterator(validation.end()));
    if (result.diagnostics.empty())
    {
        result.document = std::move(document);
    }
    return result;
}

TileMapLoadResult ParseTileMapToml(const std::string_view text, const std::string_view sourceName)
{
    TileMapLoadResult result{};
    std::optional<toml::table> root = ParseToml(text, sourceName, result.diagnostics);
    if (!root.has_value())
    {
        return result;
    }
    ValidateKnownKeys(*root, "", {"format_version", "id", "tile_set", "cell_size", "layers"}, result.diagnostics);
    ValidateFormatVersion(*root, TileMapDocument::FormatVersion, result.diagnostics, "TileMap");

    TileMapDocument document{};
    if (const std::optional<std::string> id = ReadRequiredString(*root, "id", "id", result.diagnostics); id.has_value())
    {
        document.semanticId = *id;
    }
    if (const std::optional<std::string> tileSet = ReadRequiredString(*root, "tile_set", "tile_set", result.diagnostics); tileSet.has_value())
    {
        document.tileSetSemanticId = *tileSet;
    }
    ReadUInt32Pair(*root, "cell_size", "cell_size", document.cellWidth, document.cellHeight, result.diagnostics);

    const toml::node* layersNode = root->get("layers");
    const toml::array* layers = layersNode == nullptr ? nullptr : layersNode->as_array();
    if (layers == nullptr)
    {
        AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, "layers", "Expected an array of layer tables.", layersNode);
    }
    else
    {
        document.layers.reserve(layers->size());
        for (std::size_t layerIndex = 0U; layerIndex < layers->size(); ++layerIndex)
        {
            const toml::node* layerNode = layers->get(layerIndex);
            const toml::table* layerTable = layerNode == nullptr ? nullptr : layerNode->as_table();
            const std::string path = "layers[" + std::to_string(layerIndex) + "]";
            if (layerTable == nullptr)
            {
                AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, path, "Expected a layer table.", layerNode);
                continue;
            }
            ValidateKnownKeys(*layerTable, path, {"id", "order", "origin", "size", "visible", "cells"}, result.diagnostics);
            TileLayerDocument layer{};
            if (const std::optional<std::string> id = ReadRequiredString(*layerTable, "id", path + ".id", result.diagnostics); id.has_value())
            {
                layer.semanticId = *id;
            }
            if (const std::optional<std::int32_t> order = ReadRequiredInt32(*layerTable, "order", path + ".order", result.diagnostics); order.has_value())
            {
                layer.order = *order;
            }
            ReadInt32Pair(*layerTable, "origin", path + ".origin", layer.originX, layer.originY, result.diagnostics);
            ReadUInt32Pair(*layerTable, "size", path + ".size", layer.width, layer.height, result.diagnostics);
            layer.visible = ReadOptionalBool(*layerTable, "visible", path + ".visible", true, result.diagnostics);

            const toml::node* cellsNode = layerTable->get("cells");
            if (cellsNode != nullptr)
            {
                const toml::array* cells = cellsNode->as_array();
                if (cells == nullptr)
                {
                    AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, path + ".cells", "Expected an array of cell tables.", cellsNode);
                }
                else
                {
                    layer.cells.reserve(cells->size());
                    for (std::size_t cellIndex = 0U; cellIndex < cells->size(); ++cellIndex)
                    {
                        const toml::node* cellNode = cells->get(cellIndex);
                        const toml::table* cellTable = cellNode == nullptr ? nullptr : cellNode->as_table();
                        const std::string cellPath = path + ".cells[" + std::to_string(cellIndex) + "]";
                        if (cellTable == nullptr)
                        {
                            AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, cellPath, "Expected a cell table.", cellNode);
                            continue;
                        }
                        ValidateKnownKeys(*cellTable, cellPath, {"x", "y", "tile", "flip_x", "flip_y", "rotation_quarters"}, result.diagnostics);
                        TileCellDocument cell{};
                        if (const std::optional<std::int32_t> x = ReadRequiredInt32(*cellTable, "x", cellPath + ".x", result.diagnostics); x.has_value())
                        {
                            cell.x = *x;
                        }
                        if (const std::optional<std::int32_t> y = ReadRequiredInt32(*cellTable, "y", cellPath + ".y", result.diagnostics); y.has_value())
                        {
                            cell.y = *y;
                        }
                        if (const std::optional<std::string> tile = ReadRequiredString(*cellTable, "tile", cellPath + ".tile", result.diagnostics); tile.has_value())
                        {
                            cell.tileSemanticId = *tile;
                        }
                        cell.transform.flipX = ReadOptionalBool(*cellTable, "flip_x", cellPath + ".flip_x", false, result.diagnostics);
                        cell.transform.flipY = ReadOptionalBool(*cellTable, "flip_y", cellPath + ".flip_y", false, result.diagnostics);
                        cell.transform.quarterTurns = ReadOptionalQuarterTurns(*cellTable, "rotation_quarters", cellPath + ".rotation_quarters", result.diagnostics);
                        layer.cells.push_back(std::move(cell));
                    }
                }
            }
            document.layers.push_back(std::move(layer));
        }
    }

    std::vector<TileDiagnostic> validation = ValidateTileMap(document);
    result.diagnostics.insert(result.diagnostics.end(), std::make_move_iterator(validation.begin()), std::make_move_iterator(validation.end()));
    if (result.diagnostics.empty())
    {
        result.document = std::move(document);
    }
    return result;
}

std::string SaveTileSetToml(const TileSetDocument& document)
{
    std::ostringstream stream{};
    stream.imbue(std::locale::classic());
    stream << "format_version = " << TileSetDocument::FormatVersion << "\nid = ";
    WriteQuoted(stream, document.semanticId);
    stream << "\ntexture = ";
    WriteQuoted(stream, document.textureReference);
    stream << "\nsource_size = [" << document.sourceWidth << ", " << document.sourceHeight << "]\n";

    for (const TileDefinition& tile : CanonicalTiles(document))
    {
        stream << "\n[[tiles]]\nid = ";
        WriteQuoted(stream, tile.semanticId);
        stream << "\nregion = [" << tile.sourceRegion.x << ", " << tile.sourceRegion.y << ", " << tile.sourceRegion.width << ", " << tile.sourceRegion.height << "]\ntags = [";
        for (std::size_t index = 0U; index < tile.tags.size(); ++index)
        {
            if (index != 0U)
            {
                stream << ", ";
            }
            WriteQuoted(stream, tile.tags[index]);
        }
        stream << "]\n";
    }
    return stream.str();
}

std::string SaveTileMapToml(const TileMapDocument& document)
{
    std::ostringstream stream{};
    stream.imbue(std::locale::classic());
    stream << "format_version = " << TileMapDocument::FormatVersion << "\nid = ";
    WriteQuoted(stream, document.semanticId);
    stream << "\ntile_set = ";
    WriteQuoted(stream, document.tileSetSemanticId);
    stream << "\ncell_size = [" << document.cellWidth << ", " << document.cellHeight << "]\n";

    for (const TileLayerDocument& layer : CanonicalLayers(document))
    {
        stream << "\n[[layers]]\nid = ";
        WriteQuoted(stream, layer.semanticId);
        stream << "\norder = " << layer.order;
        stream << "\norigin = [" << layer.originX << ", " << layer.originY << "]";
        stream << "\nsize = [" << layer.width << ", " << layer.height << "]";
        stream << "\nvisible = " << (layer.visible ? "true" : "false") << "\n";
        for (const TileCellDocument& cell : layer.cells)
        {
            stream << "\n[[layers.cells]]\nx = " << cell.x << "\ny = " << cell.y << "\ntile = ";
            WriteQuoted(stream, cell.tileSemanticId);
            stream << "\nflip_x = " << (cell.transform.flipX ? "true" : "false");
            stream << "\nflip_y = " << (cell.transform.flipY ? "true" : "false");
            stream << "\nrotation_quarters = " << static_cast<unsigned int>(cell.transform.quarterTurns) << "\n";
        }
    }
    return stream.str();
}

TileSetCompileResult CompileTileSet(const TileSetDocument& document)
{
    TileSetCompileResult result{};
    result.diagnostics = ValidateTileSet(document);
    if (!result.diagnostics.empty())
    {
        return result;
    }
    CompiledTileSet compiled{};
    compiled.semanticId = document.semanticId;
    compiled.textureReference = document.textureReference;
    compiled.sourceWidth = document.sourceWidth;
    compiled.sourceHeight = document.sourceHeight;
    const std::vector<TileDefinition> tiles = CanonicalTiles(document);
    compiled.tiles.reserve(tiles.size());
    for (const TileDefinition& tile : tiles)
    {
        compiled.tiles.push_back(CompiledTileDefinition{tile.semanticId, tile.sourceRegion, tile.tags});
    }
    result.tileSet = std::move(compiled);
    return result;
}

TileMapCompileResult CompileTileMap(const CompiledTileSet& tileSet, const TileMapDocument& document)
{
    TileMapCompileResult result{};
    result.diagnostics = ValidateTileMap(document);
    if (document.tileSetSemanticId != tileSet.semanticId)
    {
        AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, "tile_set", "TileMap tile_set does not match the supplied compiled TileSet.");
    }
    if (!result.diagnostics.empty())
    {
        return result;
    }

    std::unordered_map<std::string_view, std::uint32_t> tileIndices{};
    tileIndices.reserve(tileSet.tiles.size());
    for (std::size_t index = 0U; index < tileSet.tiles.size(); ++index)
    {
        if (index > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
        {
            AddDiagnostic(result.diagnostics, TileErrorCode::SchemaError, "tiles", "Compiled TileSet exceeds uint32 tile-index capacity.");
            return result;
        }
        tileIndices.emplace(tileSet.tiles[index].semanticId, static_cast<std::uint32_t>(index));
    }

    CompiledTileMap compiled{};
    compiled.semanticId = document.semanticId;
    compiled.tileSetSemanticId = document.tileSetSemanticId;
    compiled.cellWidth = document.cellWidth;
    compiled.cellHeight = document.cellHeight;
    const std::vector<TileLayerDocument> layers = CanonicalLayers(document);
    compiled.layers.reserve(layers.size());
    for (std::size_t layerIndex = 0U; layerIndex < layers.size(); ++layerIndex)
    {
        const TileLayerDocument& source = layers[layerIndex];
        CompiledTileLayer layer{};
        layer.semanticId = source.semanticId;
        layer.order = source.order;
        layer.originX = source.originX;
        layer.originY = source.originY;
        layer.width = source.width;
        layer.height = source.height;
        layer.visible = source.visible;
        const std::size_t cellCount = static_cast<std::size_t>(static_cast<std::uint64_t>(source.width) * static_cast<std::uint64_t>(source.height));
        layer.cells.resize(cellCount);
        for (std::size_t cellIndex = 0U; cellIndex < source.cells.size(); ++cellIndex)
        {
            const TileCellDocument& cell = source.cells[cellIndex];
            const auto found = tileIndices.find(cell.tileSemanticId);
            if (found == tileIndices.end())
            {
                AddDiagnostic(
                    result.diagnostics,
                    TileErrorCode::SchemaError,
                    "layers[" + std::to_string(layerIndex) + "].cells[" + std::to_string(cellIndex) + "].tile",
                    "Cell references unknown tile id '" + cell.tileSemanticId + "'.");
                continue;
            }
            const std::size_t offset = static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(source.width) + static_cast<std::size_t>(cell.x);
            CompiledTileCell& destination = layer.cells[offset];
            destination.tileIndex = found->second;
            destination.transformBits = static_cast<std::uint8_t>(
                (cell.transform.quarterTurns & 0x03U) |
                (cell.transform.flipX ? 0x04U : 0x00U) |
                (cell.transform.flipY ? 0x08U : 0x00U));
        }
        compiled.layers.push_back(std::move(layer));
    }
    if (!result.diagnostics.empty())
    {
        return result;
    }
    result.tileMap = std::move(compiled);
    return result;
}

std::optional<std::size_t> FindTileIndex(const CompiledTileSet& tileSet, const std::string_view semanticId) noexcept
{
    const auto found = std::find_if(tileSet.tiles.begin(), tileSet.tiles.end(), [semanticId](const CompiledTileDefinition& tile) {
        return tile.semanticId == semanticId;
    });
    if (found == tileSet.tiles.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(tileSet.tiles.begin(), found));
}

std::optional<std::size_t> FindLayerIndex(const CompiledTileMap& tileMap, const std::string_view semanticId) noexcept
{
    const auto found = std::find_if(tileMap.layers.begin(), tileMap.layers.end(), [semanticId](const CompiledTileLayer& layer) {
        return layer.semanticId == semanticId;
    });
    if (found == tileMap.layers.end())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::distance(tileMap.layers.begin(), found));
}

const CompiledTileCell* CellAtWorld(
    const CompiledTileMap& tileMap,
    const std::size_t layerIndex,
    const std::int32_t worldX,
    const std::int32_t worldY) noexcept
{
    if (layerIndex >= tileMap.layers.size())
    {
        return nullptr;
    }
    const CompiledTileLayer& layer = tileMap.layers[layerIndex];
    const std::int64_t localX = static_cast<std::int64_t>(worldX) - static_cast<std::int64_t>(layer.originX);
    const std::int64_t localY = static_cast<std::int64_t>(worldY) - static_cast<std::int64_t>(layer.originY);
    if (localX < 0 || localY < 0 || localX >= static_cast<std::int64_t>(layer.width) || localY >= static_cast<std::int64_t>(layer.height))
    {
        return nullptr;
    }
    const std::size_t offset = static_cast<std::size_t>(localY) * static_cast<std::size_t>(layer.width) + static_cast<std::size_t>(localX);
    if (offset >= layer.cells.size())
    {
        return nullptr;
    }
    return &layer.cells[offset];
}

std::optional<TileCellInspection> InspectCell(
    const CompiledTileSet& tileSet,
    const CompiledTileMap& tileMap,
    const std::string_view layerSemanticId,
    const std::int32_t worldX,
    const std::int32_t worldY) noexcept
{
    const std::optional<std::size_t> layerIndex = FindLayerIndex(tileMap, layerSemanticId);
    if (!layerIndex.has_value())
    {
        return std::nullopt;
    }
    const CompiledTileCell* cell = CellAtWorld(tileMap, *layerIndex, worldX, worldY);
    if (cell == nullptr)
    {
        return std::nullopt;
    }
    const CompiledTileLayer& layer = tileMap.layers[*layerIndex];
    TileCellInspection inspection{};
    inspection.layerIndex = *layerIndex;
    inspection.worldX = worldX;
    inspection.worldY = worldY;
    inspection.layerSemanticId = layer.semanticId;
    inspection.transform = cell->Transform();
    if (cell->Empty())
    {
        return inspection;
    }
    if (cell->tileIndex >= tileSet.tiles.size())
    {
        return std::nullopt;
    }
    const CompiledTileDefinition& tile = tileSet.tiles[cell->tileIndex];
    inspection.occupied = true;
    inspection.tileSemanticId = tile.semanticId;
    inspection.sourceRegion = tile.sourceRegion;
    inspection.tags = tile.tags;
    return inspection;
}

TileDocumentStore::TileDocumentStore(std::filesystem::path projectRoot)
    : projectRoot_{std::move(projectRoot)}
{
}

const std::filesystem::path& TileDocumentStore::ProjectRoot() const noexcept
{
    return projectRoot_;
}

TileSetLoadResult TileDocumentStore::LoadTileSet(const std::string_view projectRelativeReference) const
{
    TileSetLoadResult result{};
    NormalizedReference normalized{};
    std::string text{};
    if (!ReadDocumentText(projectRoot_, projectRelativeReference, normalized, text, result.diagnostics))
    {
        return result;
    }
    result = ParseTileSetToml(text, normalized.id);
    AttachSourceContext(result.diagnostics, normalized.id, normalized.resolvedPath);
    return result;
}

TileMapLoadResult TileDocumentStore::LoadTileMap(const std::string_view projectRelativeReference) const
{
    TileMapLoadResult result{};
    NormalizedReference normalized{};
    std::string text{};
    if (!ReadDocumentText(projectRoot_, projectRelativeReference, normalized, text, result.diagnostics))
    {
        return result;
    }
    result = ParseTileMapToml(text, normalized.id);
    AttachSourceContext(result.diagnostics, normalized.id, normalized.resolvedPath);
    return result;
}

std::vector<TileDiagnostic> TileDocumentStore::SaveTileSet(
    const std::string_view projectRelativeReference,
    const TileSetDocument& document) const
{
    std::vector<TileDiagnostic> diagnostics = ValidateTileSet(document);
    if (!diagnostics.empty())
    {
        return diagnostics;
    }
    return WriteDocumentText(projectRoot_, projectRelativeReference, SaveTileSetToml(document));
}

std::vector<TileDiagnostic> TileDocumentStore::SaveTileMap(
    const std::string_view projectRelativeReference,
    const TileMapDocument& document) const
{
    std::vector<TileDiagnostic> diagnostics = ValidateTileMap(document);
    if (!diagnostics.empty())
    {
        return diagnostics;
    }
    return WriteDocumentText(projectRoot_, projectRelativeReference, SaveTileMapToml(document));
}
} // namespace trace2d::tile
