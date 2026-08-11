#include <trace2d/assets/SpriteAssets.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <numeric>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace trace2d::assets
{
namespace
{
constexpr std::int64_t SpriteAssetFormatVersion = 1;
constexpr std::string_view SpriteAssetSchema = "trace2d.sprite";
constexpr std::string_view SpriteAssetSuffix = ".sprite.toml";
constexpr std::uintmax_t MaximumSpriteAssetSourceBytes = 4U * 1024U * 1024U;

struct NormalizedReference final
{
    std::string id{};
    std::filesystem::path resolvedPath{};
};

std::filesystem::path NormalizeProjectRoot(std::filesystem::path root)
{
    root = root.lexically_normal();
    if (root.empty())
    {
        return std::filesystem::path{"."};
    }
    return root;
}

SpriteAssetDiagnostic MakeDiagnostic(
    const SpriteAssetErrorCode code,
    std::string path,
    std::string message,
    const std::string_view reference = {},
    const std::filesystem::path& resolvedPath = {},
    const toml::node* node = nullptr)
{
    SpriteAssetDiagnostic diagnostic{};
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
    std::vector<SpriteAssetDiagnostic>& diagnostics,
    const SpriteAssetErrorCode code,
    std::string path,
    std::string message,
    const toml::node* node = nullptr,
    const std::string_view reference = {},
    const std::filesystem::path& resolvedPath = {})
{
    diagnostics.push_back(MakeDiagnostic(
        code,
        std::move(path),
        std::move(message),
        reference,
        resolvedPath,
        node));
}

bool IsAsciiDrivePrefix(const std::string_view reference) noexcept
{
    return reference.size() >= 2U && reference[1] == ':' &&
        std::isalpha(static_cast<unsigned char>(reference[0])) != 0;
}

bool TryNormalizeProjectRelativeReference(
    const std::string_view reference,
    std::string& canonical,
    SpriteAssetDiagnostic* diagnostic)
{
    if (reference.empty())
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = MakeDiagnostic(
                SpriteAssetErrorCode::InvalidReference,
                "$reference",
                "Asset reference must not be empty.",
                reference);
        }
        return false;
    }

    std::string portable{reference};
    std::replace(portable.begin(), portable.end(), '\\', '/');
    if (portable.front() == '/' || IsAsciiDrivePrefix(portable))
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = MakeDiagnostic(
                SpriteAssetErrorCode::InvalidReference,
                "$reference",
                "Asset reference must be project-relative, not absolute.",
                reference);
        }
        return false;
    }

    canonical.clear();
    canonical.reserve(portable.size());
    std::size_t cursor = 0U;
    while (cursor <= portable.size())
    {
        const std::size_t separator = portable.find('/', cursor);
        const std::size_t end = separator == std::string::npos ? portable.size() : separator;
        const std::string_view component{portable.data() + cursor, end - cursor};

        if (!component.empty() && component != ".")
        {
            if (component == "..")
            {
                if (diagnostic != nullptr)
                {
                    *diagnostic = MakeDiagnostic(
                        SpriteAssetErrorCode::InvalidReference,
                        "$reference",
                        "Asset reference must not contain '..' traversal components.",
                        reference);
                }
                return false;
            }
            if (component.find('\0') != std::string_view::npos)
            {
                if (diagnostic != nullptr)
                {
                    *diagnostic = MakeDiagnostic(
                        SpriteAssetErrorCode::InvalidReference,
                        "$reference",
                        "Asset reference contains an embedded null character.",
                        reference);
                }
                return false;
            }

            if (!canonical.empty())
            {
                canonical.push_back('/');
            }
            canonical.append(component);
        }

        if (separator == std::string::npos)
        {
            break;
        }
        cursor = separator + 1U;
    }

    if (canonical.empty())
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = MakeDiagnostic(
                SpriteAssetErrorCode::InvalidReference,
                "$reference",
                "Asset reference must identify a file below the project root.",
                reference);
        }
        return false;
    }
    return true;
}

bool TryNormalizeReference(
    const std::filesystem::path& projectRoot,
    const std::string_view reference,
    NormalizedReference& normalized,
    SpriteAssetDiagnostic* diagnostic)
{
    if (!TryNormalizeProjectRelativeReference(reference, normalized.id, diagnostic))
    {
        return false;
    }
    normalized.resolvedPath = (projectRoot / std::filesystem::path{normalized.id}).lexically_normal();
    return true;
}

bool HasSpriteAssetSuffix(const std::string_view reference) noexcept
{
    return reference.size() >= SpriteAssetSuffix.size() && reference.ends_with(SpriteAssetSuffix);
}

bool IsKnownKey(const std::string_view key, const std::initializer_list<std::string_view> knownKeys)
{
    return std::find(knownKeys.begin(), knownKeys.end(), key) != knownKeys.end();
}

void ValidateKnownKeys(
    const toml::table& table,
    const std::string_view path,
    const std::initializer_list<std::string_view> knownKeys,
    std::vector<SpriteAssetDiagnostic>& diagnostics)
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
        AddDiagnostic(
            diagnostics,
            SpriteAssetErrorCode::SchemaError,
            std::move(fieldPath),
            "Unknown field.",
            &value);
    }
}

std::optional<std::string> ReadRequiredString(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<SpriteAssetDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(
            diagnostics,
            SpriteAssetErrorCode::SchemaError,
            std::string{path},
            "Required field is missing.");
        return std::nullopt;
    }

    std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(
            diagnostics,
            SpriteAssetErrorCode::SchemaError,
            std::string{path},
            "Expected a string.",
            node);
        return std::nullopt;
    }
    if (value->empty())
    {
        AddDiagnostic(
            diagnostics,
            SpriteAssetErrorCode::SchemaError,
            std::string{path},
            "Value must not be empty.",
            node);
        return std::nullopt;
    }
    return value;
}

bool ReadUInt32Array(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    const std::size_t expectedCount,
    std::uint32_t* destination,
    std::vector<SpriteAssetDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    const toml::array* array = node == nullptr ? nullptr : node->as_array();
    if (array == nullptr || array->size() != expectedCount)
    {
        AddDiagnostic(
            diagnostics,
            SpriteAssetErrorCode::SchemaError,
            std::string{path},
            "Expected an integer array with exactly " + std::to_string(expectedCount) + " elements.",
            node);
        return false;
    }

    bool valid = true;
    for (std::size_t index = 0U; index < expectedCount; ++index)
    {
        const toml::node* valueNode = array->get(index);
        const std::optional<std::int64_t> value =
            valueNode == nullptr ? std::nullopt : valueNode->value<std::int64_t>();
        if (!value.has_value() || *value < 0 ||
            static_cast<std::uint64_t>(*value) > std::numeric_limits<std::uint32_t>::max())
        {
            AddDiagnostic(
                diagnostics,
                SpriteAssetErrorCode::SchemaError,
                std::string{path} + "[" + std::to_string(index) + "]",
                "Expected a non-negative integer that fits uint32.",
                valueNode);
            valid = false;
            continue;
        }
        destination[index] = static_cast<std::uint32_t>(*value);
    }
    return valid;
}

bool ReadInt64Array(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    const std::size_t expectedCount,
    std::int64_t* destination,
    std::vector<SpriteAssetDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    const toml::array* array = node == nullptr ? nullptr : node->as_array();
    if (array == nullptr || array->size() != expectedCount)
    {
        AddDiagnostic(
            diagnostics,
            SpriteAssetErrorCode::SchemaError,
            std::string{path},
            "Expected an integer array with exactly " + std::to_string(expectedCount) + " elements.",
            node);
        return false;
    }

    bool valid = true;
    for (std::size_t index = 0U; index < expectedCount; ++index)
    {
        const toml::node* valueNode = array->get(index);
        const std::optional<std::int64_t> value =
            valueNode == nullptr ? std::nullopt : valueNode->value<std::int64_t>();
        if (!value.has_value())
        {
            AddDiagnostic(
                diagnostics,
                SpriteAssetErrorCode::SchemaError,
                std::string{path} + "[" + std::to_string(index) + "]",
                "Expected a signed 64-bit integer.",
                valueNode);
            valid = false;
            continue;
        }
        destination[index] = *value;
    }
    return valid;
}

std::uint64_t Magnitude(const std::int64_t value) noexcept
{
    if (value >= 0)
    {
        return static_cast<std::uint64_t>(value);
    }
    return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

void ReducePivot(SpriteRationalPivot& pivot) noexcept
{
    if (pivot.denominator <= 0)
    {
        return;
    }

    const std::uint64_t denominator = static_cast<std::uint64_t>(pivot.denominator);
    const std::uint64_t divisor = std::gcd(
        std::gcd(Magnitude(pivot.xNumerator), Magnitude(pivot.yNumerator)),
        denominator);
    if (divisor <= 1U)
    {
        return;
    }

    const std::int64_t signedDivisor = static_cast<std::int64_t>(divisor);
    pivot.xNumerator /= signedDivisor;
    pivot.yNumerator /= signedDivisor;
    pivot.denominator /= signedDivisor;
}

bool FitsWithin(
    const std::uint32_t offset,
    const std::uint32_t extent,
    const std::uint32_t limit) noexcept
{
    return static_cast<std::uint64_t>(offset) + static_cast<std::uint64_t>(extent) <=
        static_cast<std::uint64_t>(limit);
}

std::string EscapeTomlString(const std::string_view value)
{
    static constexpr char HexDigits[] = "0123456789ABCDEF";
    std::string escaped{};
    escaped.reserve(value.size() + 2U);
    escaped.push_back('"');
    for (const char rawCharacter : value)
    {
        const unsigned char character = static_cast<unsigned char>(rawCharacter);
        switch (character)
        {
        case '"': escaped.append("\\\""); break;
        case '\\': escaped.append("\\\\"); break;
        case '\b': escaped.append("\\b"); break;
        case '\t': escaped.append("\\t"); break;
        case '\n': escaped.append("\\n"); break;
        case '\f': escaped.append("\\f"); break;
        case '\r': escaped.append("\\r"); break;
        default:
            if (character < 0x20U || character == 0x7FU)
            {
                escaped.append("\\u00");
                escaped.push_back(HexDigits[(character >> 4U) & 0x0FU]);
                escaped.push_back(HexDigits[character & 0x0FU]);
            }
            else
            {
                escaped.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    escaped.push_back('"');
    return escaped;
}

void WriteSize(std::ostringstream& output, const SpritePixelSize value)
{
    output << '[' << value.width << ", " << value.height << ']';
}

void WriteOffset(std::ostringstream& output, const SpritePixelOffset value)
{
    output << '[' << value.x << ", " << value.y << ']';
}

void WriteRect(std::ostringstream& output, const SpritePixelRect value)
{
    output << '[' << value.x << ", " << value.y << ", " << value.width << ", " << value.height << ']';
}

void CopySourceContext(
    std::vector<SpriteAssetDiagnostic>& diagnostics,
    const std::string_view reference,
    const std::filesystem::path& resolvedPath)
{
    for (SpriteAssetDiagnostic& diagnostic : diagnostics)
    {
        if (diagnostic.reference.empty())
        {
            diagnostic.reference = std::string{reference};
        }
        if (diagnostic.resolvedPath.empty() && !resolvedPath.empty())
        {
            diagnostic.resolvedPath = resolvedPath.generic_string();
        }
    }
}
} // namespace

std::string_view ToString(const SpriteColorSpace value) noexcept
{
    switch (value)
    {
    case SpriteColorSpace::Srgb: return "srgb";
    case SpriteColorSpace::Linear: return "linear";
    }
    return "unknown";
}

std::string_view ToString(const SpriteAlphaMode value) noexcept
{
    switch (value)
    {
    case SpriteAlphaMode::Straight: return "straight";
    }
    return "unknown";
}

std::string_view ToString(const SpriteSampling value) noexcept
{
    switch (value)
    {
    case SpriteSampling::Nearest: return "nearest";
    case SpriteSampling::Linear: return "linear";
    }
    return "unknown";
}

std::string_view ToString(const SpritePackedRotation value) noexcept
{
    switch (value)
    {
    case SpritePackedRotation::None: return "none";
    case SpritePackedRotation::Cw90: return "cw90";
    }
    return "unknown";
}

std::string_view ToString(const SpriteAssetErrorCode code) noexcept
{
    switch (code)
    {
    case SpriteAssetErrorCode::InvalidReference: return "invalid_reference";
    case SpriteAssetErrorCode::UnsupportedFormat: return "unsupported_format";
    case SpriteAssetErrorCode::MissingFile: return "missing_file";
    case SpriteAssetErrorCode::ReadFailure: return "read_failure";
    case SpriteAssetErrorCode::ParseError: return "parse_error";
    case SpriteAssetErrorCode::SchemaError: return "schema_error";
    case SpriteAssetErrorCode::TextureValidationError: return "texture_validation_error";
    }
    return "unknown";
}

SpriteAssetLoadResult ParseSpriteAssetToml(
    const std::string_view text,
    const std::string_view canonicalAssetId,
    const std::string_view sourceName)
{
    SpriteAssetLoadResult result{};

    std::string normalizedAssetId{};
    SpriteAssetDiagnostic normalizationDiagnostic{};
    if (!TryNormalizeProjectRelativeReference(
            canonicalAssetId,
            normalizedAssetId,
            &normalizationDiagnostic))
    {
        result.diagnostics.push_back(std::move(normalizationDiagnostic));
        return result;
    }
    if (!HasSpriteAssetSuffix(normalizedAssetId))
    {
        result.diagnostics.push_back(MakeDiagnostic(
            SpriteAssetErrorCode::UnsupportedFormat,
            "$reference",
            "Canonical Sprite assets must use the '.sprite.toml' suffix.",
            normalizedAssetId));
        return result;
    }

    toml::table root{};
    try
    {
        root = toml::parse(text, sourceName);
    }
    catch (const toml::parse_error& error)
    {
        SpriteAssetDiagnostic diagnostic{};
        diagnostic.code = SpriteAssetErrorCode::ParseError;
        diagnostic.reference = normalizedAssetId;
        diagnostic.path = "$";
        diagnostic.message = std::string{error.description()};
        diagnostic.line = static_cast<std::size_t>(error.source().begin.line);
        diagnostic.column = static_cast<std::size_t>(error.source().begin.column);
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }

    ValidateKnownKeys(root, "", {"schema", "version", "sampling", "pages", "regions"}, result.diagnostics);

    const toml::node* schemaNode = root.get("schema");
    const std::optional<std::string> schema =
        schemaNode == nullptr ? std::nullopt : schemaNode->value<std::string>();
    if (!schema.has_value() || *schema != SpriteAssetSchema)
    {
        AddDiagnostic(
            result.diagnostics,
            SpriteAssetErrorCode::SchemaError,
            "schema",
            "Expected schema = 'trace2d.sprite'.",
            schemaNode);
    }

    const toml::node* versionNode = root.get("version");
    const std::optional<std::int64_t> version =
        versionNode == nullptr ? std::nullopt : versionNode->value<std::int64_t>();
    if (!version.has_value() || *version != SpriteAssetFormatVersion)
    {
        AddDiagnostic(
            result.diagnostics,
            SpriteAssetErrorCode::SchemaError,
            "version",
            "Expected integer version = 1.",
            versionNode);
    }

    auto asset = std::make_shared<SpriteAsset>();
    asset->id = normalizedAssetId;
    asset->schemaVersion = 1U;

    const toml::node* samplingNode = root.get("sampling");
    const std::optional<std::string> sampling =
        samplingNode == nullptr ? std::nullopt : samplingNode->value<std::string>();
    if (!sampling.has_value())
    {
        AddDiagnostic(
            result.diagnostics,
            SpriteAssetErrorCode::SchemaError,
            "sampling",
            "Expected sampling = 'nearest' or 'linear'.",
            samplingNode);
    }
    else if (*sampling == "nearest")
    {
        asset->sampling = SpriteSampling::Nearest;
    }
    else if (*sampling == "linear")
    {
        asset->sampling = SpriteSampling::Linear;
    }
    else
    {
        AddDiagnostic(
            result.diagnostics,
            SpriteAssetErrorCode::SchemaError,
            "sampling",
            "Supported sampling values are 'nearest' and 'linear'.",
            samplingNode);
    }

    std::unordered_set<std::string> pageIds{};
    const toml::node* pagesNode = root.get("pages");
    const toml::array* pages = pagesNode == nullptr ? nullptr : pagesNode->as_array();
    if (pages == nullptr || pages->empty())
    {
        AddDiagnostic(
            result.diagnostics,
            SpriteAssetErrorCode::SchemaError,
            "pages",
            "At least one Sprite atlas page is required.",
            pagesNode);
    }
    else
    {
        asset->pages.reserve(pages->size());
        pageIds.reserve(pages->size());
        for (std::size_t index = 0U; index < pages->size(); ++index)
        {
            const toml::node* pageNode = pages->get(index);
            const toml::table* pageTable = pageNode == nullptr ? nullptr : pageNode->as_table();
            const std::string path = "pages[" + std::to_string(index) + "]";
            if (pageTable == nullptr)
            {
                AddDiagnostic(
                    result.diagnostics,
                    SpriteAssetErrorCode::SchemaError,
                    path,
                    "Expected a page table.",
                    pageNode);
                continue;
            }

            ValidateKnownKeys(
                *pageTable,
                path,
                {"id", "texture", "size", "color_space", "alpha_mode"},
                result.diagnostics);

            SpriteAtlasPage page{};
            const std::optional<std::string> id =
                ReadRequiredString(*pageTable, "id", path + ".id", result.diagnostics);
            const std::optional<std::string> texture =
                ReadRequiredString(*pageTable, "texture", path + ".texture", result.diagnostics);
            std::uint32_t sizeValues[2]{};
            const bool sizeValid = ReadUInt32Array(
                *pageTable,
                "size",
                path + ".size",
                2U,
                sizeValues,
                result.diagnostics);
            const std::optional<std::string> colorSpace =
                ReadRequiredString(*pageTable, "color_space", path + ".color_space", result.diagnostics);
            const std::optional<std::string> alphaMode =
                ReadRequiredString(*pageTable, "alpha_mode", path + ".alpha_mode", result.diagnostics);

            if (id.has_value())
            {
                page.id = *id;
                if (!pageIds.emplace(page.id).second)
                {
                    AddDiagnostic(
                        result.diagnostics,
                        SpriteAssetErrorCode::SchemaError,
                        path + ".id",
                        "Duplicate page id.",
                        pageTable->get("id"));
                }
            }

            if (texture.has_value())
            {
                SpriteAssetDiagnostic textureDiagnostic{};
                if (!TryNormalizeProjectRelativeReference(
                        *texture,
                        page.textureReference,
                        &textureDiagnostic))
                {
                    textureDiagnostic.path = path + ".texture";
                    textureDiagnostic.reference = normalizedAssetId;
                    if (const toml::node* textureNode = pageTable->get("texture"); textureNode != nullptr)
                    {
                        textureDiagnostic.line = static_cast<std::size_t>(textureNode->source().begin.line);
                        textureDiagnostic.column = static_cast<std::size_t>(textureNode->source().begin.column);
                    }
                    result.diagnostics.push_back(std::move(textureDiagnostic));
                }
            }

            if (sizeValid)
            {
                page.size = SpritePixelSize{sizeValues[0], sizeValues[1]};
                if (page.size.width == 0U || page.size.height == 0U)
                {
                    AddDiagnostic(
                        result.diagnostics,
                        SpriteAssetErrorCode::SchemaError,
                        path + ".size",
                        "Page width and height must both be greater than zero.",
                        pageTable->get("size"));
                }
            }

            if (colorSpace.has_value())
            {
                if (*colorSpace == "srgb") page.colorSpace = SpriteColorSpace::Srgb;
                else if (*colorSpace == "linear") page.colorSpace = SpriteColorSpace::Linear;
                else
                {
                    AddDiagnostic(
                        result.diagnostics,
                        SpriteAssetErrorCode::SchemaError,
                        path + ".color_space",
                        "Supported color_space values are 'srgb' and 'linear'.",
                        pageTable->get("color_space"));
                }
            }

            if (alphaMode.has_value() && *alphaMode != "straight")
            {
                AddDiagnostic(
                    result.diagnostics,
                    SpriteAssetErrorCode::SchemaError,
                    path + ".alpha_mode",
                    "Sprite schema v1 supports alpha_mode = 'straight' only.",
                    pageTable->get("alpha_mode"));
            }

            asset->pages.push_back(std::move(page));
        }
    }

    std::unordered_set<std::string> regionIds{};
    const toml::node* regionsNode = root.get("regions");
    const toml::array* regions = regionsNode == nullptr ? nullptr : regionsNode->as_array();
    if (regions == nullptr || regions->empty())
    {
        AddDiagnostic(
            result.diagnostics,
            SpriteAssetErrorCode::SchemaError,
            "regions",
            "At least one Sprite region is required.",
            regionsNode);
    }
    else
    {
        asset->regions.reserve(regions->size());
        regionIds.reserve(regions->size());
        for (std::size_t index = 0U; index < regions->size(); ++index)
        {
            const toml::node* regionNode = regions->get(index);
            const toml::table* regionTable = regionNode == nullptr ? nullptr : regionNode->as_table();
            const std::string path = "regions[" + std::to_string(index) + "]";
            if (regionTable == nullptr)
            {
                AddDiagnostic(
                    result.diagnostics,
                    SpriteAssetErrorCode::SchemaError,
                    path,
                    "Expected a region table.",
                    regionNode);
                continue;
            }

            ValidateKnownKeys(
                *regionTable,
                path,
                {"id", "page", "source_size", "trim_offset", "trim_size", "packed_rect", "pivot", "packed_rotation"},
                result.diagnostics);

            SpriteRegion region{};
            const std::optional<std::string> id =
                ReadRequiredString(*regionTable, "id", path + ".id", result.diagnostics);
            const std::optional<std::string> page =
                ReadRequiredString(*regionTable, "page", path + ".page", result.diagnostics);
            std::uint32_t sourceSize[2]{};
            std::uint32_t trimOffset[2]{};
            std::uint32_t trimSize[2]{};
            std::uint32_t packedRect[4]{};
            std::int64_t pivot[3]{};
            const bool sourceValid = ReadUInt32Array(
                *regionTable, "source_size", path + ".source_size", 2U, sourceSize, result.diagnostics);
            const bool trimOffsetValid = ReadUInt32Array(
                *regionTable, "trim_offset", path + ".trim_offset", 2U, trimOffset, result.diagnostics);
            const bool trimSizeValid = ReadUInt32Array(
                *regionTable, "trim_size", path + ".trim_size", 2U, trimSize, result.diagnostics);
            const bool packedValid = ReadUInt32Array(
                *regionTable, "packed_rect", path + ".packed_rect", 4U, packedRect, result.diagnostics);
            const bool pivotValid = ReadInt64Array(
                *regionTable, "pivot", path + ".pivot", 3U, pivot, result.diagnostics);
            const std::optional<std::string> packedRotation =
                ReadRequiredString(
                    *regionTable,
                    "packed_rotation",
                    path + ".packed_rotation",
                    result.diagnostics);

            if (id.has_value())
            {
                region.id = *id;
                if (!regionIds.emplace(region.id).second)
                {
                    AddDiagnostic(
                        result.diagnostics,
                        SpriteAssetErrorCode::SchemaError,
                        path + ".id",
                        "Duplicate region id.",
                        regionTable->get("id"));
                }
            }
            if (page.has_value()) region.pageId = *page;
            if (sourceValid) region.sourceSize = SpritePixelSize{sourceSize[0], sourceSize[1]};
            if (trimOffsetValid) region.trimOffset = SpritePixelOffset{trimOffset[0], trimOffset[1]};
            if (trimSizeValid) region.trimSize = SpritePixelSize{trimSize[0], trimSize[1]};
            if (packedValid)
            {
                region.packedRect = SpritePixelRect{
                    packedRect[0], packedRect[1], packedRect[2], packedRect[3]};
            }
            if (pivotValid)
            {
                region.pivot = SpriteRationalPivot{pivot[0], pivot[1], pivot[2]};
                if (region.pivot.denominator <= 0)
                {
                    AddDiagnostic(
                        result.diagnostics,
                        SpriteAssetErrorCode::SchemaError,
                        path + ".pivot[2]",
                        "Pivot denominator must be greater than zero.",
                        regionTable->get("pivot"));
                }
                else
                {
                    ReducePivot(region.pivot);
                }
            }

            if (packedRotation.has_value())
            {
                if (*packedRotation == "none") region.packedRotation = SpritePackedRotation::None;
                else if (*packedRotation == "cw90") region.packedRotation = SpritePackedRotation::Cw90;
                else
                {
                    AddDiagnostic(
                        result.diagnostics,
                        SpriteAssetErrorCode::SchemaError,
                        path + ".packed_rotation",
                        "Supported packed_rotation values are 'none' and 'cw90'.",
                        regionTable->get("packed_rotation"));
                }
            }

            asset->regions.push_back(std::move(region));
        }
    }

    std::unordered_map<std::string, const SpriteAtlasPage*> pagesById{};
    pagesById.reserve(asset->pages.size());
    for (const SpriteAtlasPage& page : asset->pages)
    {
        if (!page.id.empty())
        {
            pagesById.emplace(page.id, &page);
        }
    }

    for (std::size_t index = 0U; index < asset->regions.size(); ++index)
    {
        const SpriteRegion& region = asset->regions[index];
        const std::string path = "regions[" + std::to_string(index) + "]";
        const auto pageIt = pagesById.find(region.pageId);
        if (region.pageId.empty() || pageIt == pagesById.end())
        {
            AddDiagnostic(
                result.diagnostics,
                SpriteAssetErrorCode::SchemaError,
                path + ".page",
                "Region references an unknown page id.");
            continue;
        }

        if (region.sourceSize.width == 0U || region.sourceSize.height == 0U)
        {
            AddDiagnostic(
                result.diagnostics,
                SpriteAssetErrorCode::SchemaError,
                path + ".source_size",
                "source_size width and height must both be greater than zero.");
        }
        if (region.trimSize.width == 0U || region.trimSize.height == 0U)
        {
            AddDiagnostic(
                result.diagnostics,
                SpriteAssetErrorCode::SchemaError,
                path + ".trim_size",
                "trim_size width and height must both be greater than zero.");
        }
        if (region.packedRect.width == 0U || region.packedRect.height == 0U)
        {
            AddDiagnostic(
                result.diagnostics,
                SpriteAssetErrorCode::SchemaError,
                path + ".packed_rect",
                "packed_rect width and height must both be greater than zero.");
        }

        if (!FitsWithin(region.trimOffset.x, region.trimSize.width, region.sourceSize.width) ||
            !FitsWithin(region.trimOffset.y, region.trimSize.height, region.sourceSize.height))
        {
            AddDiagnostic(
                result.diagnostics,
                SpriteAssetErrorCode::SchemaError,
                path + ".trim_offset",
                "Trim rectangle must fit inside source_size.");
        }

        const SpriteAtlasPage& page = *pageIt->second;
        if (!FitsWithin(region.packedRect.x, region.packedRect.width, page.size.width) ||
            !FitsWithin(region.packedRect.y, region.packedRect.height, page.size.height))
        {
            AddDiagnostic(
                result.diagnostics,
                SpriteAssetErrorCode::SchemaError,
                path + ".packed_rect",
                "packed_rect must fit inside the referenced page size.");
        }

        const bool packedDimensionsMatch =
            region.packedRotation == SpritePackedRotation::None
                ? region.packedRect.width == region.trimSize.width &&
                    region.packedRect.height == region.trimSize.height
                : region.packedRect.width == region.trimSize.height &&
                    region.packedRect.height == region.trimSize.width;
        if (!packedDimensionsMatch)
        {
            AddDiagnostic(
                result.diagnostics,
                SpriteAssetErrorCode::SchemaError,
                path + ".packed_rect",
                region.packedRotation == SpritePackedRotation::None
                    ? "Unrotated packed extent must match trim_size."
                    : "cw90 packed extent must equal swapped trim_size dimensions.");
        }
    }

    CopySourceContext(result.diagnostics, normalizedAssetId, {});
    if (result.diagnostics.empty())
    {
        result.asset = std::move(asset);
    }
    return result;
}

std::string SaveSpriteAssetToml(const SpriteAsset& asset)
{
    std::ostringstream output{};
    output << "schema = " << EscapeTomlString(SpriteAssetSchema) << '\n';
    output << "version = " << asset.schemaVersion << '\n';
    output << "sampling = " << EscapeTomlString(ToString(asset.sampling)) << "\n\n";

    for (const SpriteAtlasPage& page : asset.pages)
    {
        output << "[[pages]]\n";
        output << "id = " << EscapeTomlString(page.id) << '\n';
        output << "texture = " << EscapeTomlString(page.textureReference) << '\n';
        output << "size = ";
        WriteSize(output, page.size);
        output << '\n';
        output << "color_space = " << EscapeTomlString(ToString(page.colorSpace)) << '\n';
        output << "alpha_mode = " << EscapeTomlString(ToString(page.alphaMode)) << "\n\n";
    }

    for (const SpriteRegion& region : asset.regions)
    {
        output << "[[regions]]\n";
        output << "id = " << EscapeTomlString(region.id) << '\n';
        output << "page = " << EscapeTomlString(region.pageId) << '\n';
        output << "source_size = ";
        WriteSize(output, region.sourceSize);
        output << '\n';
        output << "trim_offset = ";
        WriteOffset(output, region.trimOffset);
        output << '\n';
        output << "trim_size = ";
        WriteSize(output, region.trimSize);
        output << '\n';
        output << "packed_rect = ";
        WriteRect(output, region.packedRect);
        output << '\n';
        output << "pivot = [" << region.pivot.xNumerator << ", " << region.pivot.yNumerator
               << ", " << region.pivot.denominator << "]\n";
        output << "packed_rotation = " << EscapeTomlString(ToString(region.packedRotation)) << "\n\n";
    }

    return output.str();
}

SpriteAssetCache::SpriteAssetCache(std::filesystem::path projectRoot)
    : projectRoot_{NormalizeProjectRoot(std::move(projectRoot))},
      textureCache_{projectRoot_}
{
}

SpriteAssetLoadResult SpriteAssetCache::Load(const std::string_view projectRelativeReference)
{
    ++requests_;

    NormalizedReference normalized{};
    SpriteAssetDiagnostic normalizationDiagnostic{};
    if (!TryNormalizeReference(
            projectRoot_,
            projectRelativeReference,
            normalized,
            &normalizationDiagnostic))
    {
        ++failedImports_;
        SpriteAssetLoadResult result{};
        result.diagnostics.push_back(std::move(normalizationDiagnostic));
        return result;
    }
    if (!HasSpriteAssetSuffix(normalized.id))
    {
        ++failedImports_;
        SpriteAssetLoadResult result{};
        result.diagnostics.push_back(MakeDiagnostic(
            SpriteAssetErrorCode::UnsupportedFormat,
            "$reference",
            "Canonical Sprite assets must use the '.sprite.toml' suffix.",
            normalized.id,
            normalized.resolvedPath));
        return result;
    }

    const auto cached = cache_.find(normalized.id);
    if (cached != cache_.end())
    {
        ++cacheHits_;
        SpriteAssetLoadResult result{};
        result.asset = cached->second;
        return result;
    }
    ++cacheMisses_;

    std::error_code filesystemError{};
    const bool exists = std::filesystem::exists(normalized.resolvedPath, filesystemError);
    if (filesystemError)
    {
        ++failedImports_;
        SpriteAssetLoadResult result{};
        result.diagnostics.push_back(MakeDiagnostic(
            SpriteAssetErrorCode::ReadFailure,
            "$reference",
            "Unable to inspect Sprite asset file: " + filesystemError.message(),
            normalized.id,
            normalized.resolvedPath));
        return result;
    }
    if (!exists)
    {
        ++failedImports_;
        SpriteAssetLoadResult result{};
        result.diagnostics.push_back(MakeDiagnostic(
            SpriteAssetErrorCode::MissingFile,
            "$reference",
            "Sprite asset file does not exist.",
            normalized.id,
            normalized.resolvedPath));
        return result;
    }
    if (!std::filesystem::is_regular_file(normalized.resolvedPath, filesystemError) || filesystemError)
    {
        ++failedImports_;
        SpriteAssetLoadResult result{};
        result.diagnostics.push_back(MakeDiagnostic(
            SpriteAssetErrorCode::ReadFailure,
            "$reference",
            "Sprite asset path must resolve to a regular file.",
            normalized.id,
            normalized.resolvedPath));
        return result;
    }

    const std::uintmax_t fileSize = std::filesystem::file_size(normalized.resolvedPath, filesystemError);
    if (filesystemError || fileSize > MaximumSpriteAssetSourceBytes ||
        fileSize > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()))
    {
        ++failedImports_;
        SpriteAssetLoadResult result{};
        result.diagnostics.push_back(MakeDiagnostic(
            SpriteAssetErrorCode::ReadFailure,
            "$reference",
            filesystemError
                ? "Unable to determine Sprite asset size: " + filesystemError.message()
                : "Sprite asset source exceeds the 4 MiB S1 text-import limit.",
            normalized.id,
            normalized.resolvedPath));
        return result;
    }

    std::string text(static_cast<std::size_t>(fileSize), '\0');
    std::ifstream input{normalized.resolvedPath, std::ios::binary};
    if (!input)
    {
        ++failedImports_;
        SpriteAssetLoadResult result{};
        result.diagnostics.push_back(MakeDiagnostic(
            SpriteAssetErrorCode::ReadFailure,
            "$reference",
            "Unable to open Sprite asset file for reading.",
            normalized.id,
            normalized.resolvedPath));
        return result;
    }
    if (!text.empty())
    {
        input.read(text.data(), static_cast<std::streamsize>(text.size()));
    }
    if (!input && !input.eof())
    {
        ++failedImports_;
        SpriteAssetLoadResult result{};
        result.diagnostics.push_back(MakeDiagnostic(
            SpriteAssetErrorCode::ReadFailure,
            "$reference",
            "Sprite asset file could not be read completely.",
            normalized.id,
            normalized.resolvedPath));
        return result;
    }

    SpriteAssetLoadResult result = ParseSpriteAssetToml(
        text,
        normalized.id,
        normalized.resolvedPath.generic_string());
    CopySourceContext(result.diagnostics, normalized.id, normalized.resolvedPath);
    if (!result.Succeeded())
    {
        ++failedImports_;
        return result;
    }

    for (std::size_t index = 0U; index < result.asset->pages.size(); ++index)
    {
        const SpriteAtlasPage& page = result.asset->pages[index];
        TextureAssetLoadResult textureResult = textureCache_.Load(page.textureReference);
        if (!textureResult.Succeeded())
        {
            const std::string path = "pages[" + std::to_string(index) + "].texture";
            std::string message{"Referenced texture failed deterministic import validation."};
            if (textureResult.diagnostic.has_value())
            {
                message.append(" ");
                message.append(textureResult.diagnostic->message);
            }
            result.diagnostics.push_back(MakeDiagnostic(
                SpriteAssetErrorCode::TextureValidationError,
                path,
                std::move(message),
                normalized.id,
                normalized.resolvedPath));
            continue;
        }

        if (textureResult.asset->width != page.size.width ||
            textureResult.asset->height != page.size.height)
        {
            result.diagnostics.push_back(MakeDiagnostic(
                SpriteAssetErrorCode::TextureValidationError,
                "pages[" + std::to_string(index) + "].size",
                "Canonical page size does not match decoded texture dimensions.",
                normalized.id,
                normalized.resolvedPath));
        }
    }

    if (!result.diagnostics.empty())
    {
        result.asset.reset();
        ++failedImports_;
        return result;
    }

    cache_.emplace(normalized.id, result.asset);
    ++successfulImports_;
    return result;
}

bool SpriteAssetCache::Invalidate(const std::string_view projectRelativeReference)
{
    NormalizedReference normalized{};
    if (!TryNormalizeReference(projectRoot_, projectRelativeReference, normalized, nullptr))
    {
        return false;
    }

    const auto cached = cache_.find(normalized.id);
    if (cached == cache_.end())
    {
        return false;
    }

    for (const SpriteAtlasPage& page : cached->second->pages)
    {
        static_cast<void>(textureCache_.Invalidate(page.textureReference));
    }
    cache_.erase(cached);
    return true;
}

void SpriteAssetCache::Clear() noexcept
{
    cache_.clear();
    textureCache_.Clear();
}

const std::filesystem::path& SpriteAssetCache::ProjectRoot() const noexcept
{
    return projectRoot_;
}

SpriteAssetCacheMetrics SpriteAssetCache::Metrics() const noexcept
{
    SpriteAssetCacheMetrics metrics{};
    metrics.requests = requests_;
    metrics.cacheHits = cacheHits_;
    metrics.cacheMisses = cacheMisses_;
    metrics.successfulImports = successfulImports_;
    metrics.failedImports = failedImports_;
    metrics.cachedAssets = cache_.size();
    return metrics;
}

TextureAssetCacheMetrics SpriteAssetCache::TextureMetrics() const noexcept
{
    return textureCache_.Metrics();
}
} // namespace trace2d::assets
