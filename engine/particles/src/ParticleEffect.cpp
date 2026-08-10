#include <trace2d/particles/ParticleEffect.hpp>

#include <trace2d/scene/SceneText.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace trace2d::particles
{
namespace
{
constexpr std::int64_t ParticleEffectFormatVersion = 1;
constexpr std::uintmax_t MaximumParticleEffectSourceBytes = 1024U * 1024U;
constexpr std::string_view ParticleEffectSuffix = ".trace2d.particle.toml";

struct NormalizedReference final
{
    std::string id{};
    std::filesystem::path resolvedPath{};
};

ParticleEffectDiagnostic MakeDiagnostic(
    const ParticleEffectErrorCode code,
    std::string path,
    std::string message,
    const std::string_view reference = {},
    const std::filesystem::path& resolvedPath = {},
    const toml::node* node = nullptr)
{
    ParticleEffectDiagnostic diagnostic{};
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
    std::vector<ParticleEffectDiagnostic>& diagnostics,
    const ParticleEffectErrorCode code,
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
    ParticleEffectDiagnostic* diagnostic)
{
    if (reference.empty())
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = MakeDiagnostic(
                ParticleEffectErrorCode::InvalidReference,
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
                ParticleEffectErrorCode::InvalidReference,
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
                        ParticleEffectErrorCode::InvalidReference,
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
                        ParticleEffectErrorCode::InvalidReference,
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
                ParticleEffectErrorCode::InvalidReference,
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
    ParticleEffectDiagnostic* diagnostic)
{
    if (!TryNormalizeProjectRelativeReference(reference, normalized.id, diagnostic))
    {
        return false;
    }
    normalized.resolvedPath = (projectRoot / std::filesystem::path{normalized.id}).lexically_normal();
    return true;
}

bool IsKnownKey(const std::string_view key, const std::initializer_list<std::string_view> knownKeys)
{
    return std::find(knownKeys.begin(), knownKeys.end(), key) != knownKeys.end();
}

void ValidateKnownKeys(
    const toml::table& table,
    const std::string_view path,
    const std::initializer_list<std::string_view> knownKeys,
    std::vector<ParticleEffectDiagnostic>& diagnostics)
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
            ParticleEffectErrorCode::SchemaError,
            std::move(fieldPath),
            "Unknown field.",
            &value);
    }
}

const toml::table* ReadRequiredTable(
    const toml::table& parent,
    const std::string_view key,
    const std::string_view path,
    std::vector<ParticleEffectDiagnostic>& diagnostics)
{
    const toml::node* node = parent.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(
            diagnostics,
            ParticleEffectErrorCode::SchemaError,
            std::string{path},
            "Required table is missing.");
        return nullptr;
    }
    const toml::table* table = node->as_table();
    if (table == nullptr)
    {
        AddDiagnostic(
            diagnostics,
            ParticleEffectErrorCode::SchemaError,
            std::string{path},
            "Expected a table.",
            node);
    }
    return table;
}

std::optional<std::string> ReadRequiredString(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<ParticleEffectDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }
    std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Expected a string.", node);
        return std::nullopt;
    }
    if (value->empty())
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Value must not be empty.", node);
        return std::nullopt;
    }
    return value;
}

std::optional<bool> ReadRequiredBool(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<ParticleEffectDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }
    const std::optional<bool> value = node->value<bool>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Expected a boolean.", node);
    }
    return value;
}

std::optional<std::uint64_t> ReadRequiredUInt64(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<ParticleEffectDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }
    const std::optional<std::int64_t> value = node->value<std::int64_t>();
    if (!value.has_value() || *value < 0)
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Expected a non-negative integer.", node);
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(*value);
}

std::optional<std::uint32_t> ReadRequiredUInt32(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<ParticleEffectDiagnostic>& diagnostics)
{
    const std::optional<std::uint64_t> value = ReadRequiredUInt64(table, key, path, diagnostics);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    if (*value > std::numeric_limits<std::uint32_t>::max())
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Integer exceeds uint32 range.", table.get(key));
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*value);
}

std::optional<float> ReadFiniteFloatNode(
    const toml::node& node,
    const std::string_view path,
    std::vector<ParticleEffectDiagnostic>& diagnostics)
{
    if (!node.is_number())
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Expected a number.", &node);
        return std::nullopt;
    }
    const std::optional<double> value = node.value<double>();
    const double maximum = static_cast<double>(std::numeric_limits<float>::max());
    if (!value.has_value() || !std::isfinite(*value) || *value < -maximum || *value > maximum)
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Number must be finite and fit in a 32-bit float.", &node);
        return std::nullopt;
    }
    return static_cast<float>(*value);
}

std::optional<float> ReadRequiredFloat(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<ParticleEffectDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }
    return ReadFiniteFloatNode(*node, path, diagnostics);
}

bool ReadFloatPair(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    ParticleFloatRange& destination,
    std::vector<ParticleEffectDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    const toml::array* array = node == nullptr ? nullptr : node->as_array();
    if (array == nullptr || array->size() != 2U)
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Expected an array with exactly two finite numbers.", node);
        return false;
    }
    const toml::node* minNode = array->get(0U);
    const toml::node* maxNode = array->get(1U);
    if (minNode == nullptr || maxNode == nullptr)
    {
        return false;
    }
    const std::optional<float> minValue = ReadFiniteFloatNode(*minNode, std::string{path} + "[0]", diagnostics);
    const std::optional<float> maxValue = ReadFiniteFloatNode(*maxNode, std::string{path} + "[1]", diagnostics);
    if (!minValue.has_value() || !maxValue.has_value())
    {
        return false;
    }
    if (*minValue > *maxValue)
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Range minimum must be <= maximum.", node);
        return false;
    }
    destination = ParticleFloatRange{*minValue, *maxValue};
    return true;
}

bool ReadUIntPair(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    ParticleUIntRange& destination,
    std::vector<ParticleEffectDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    const toml::array* array = node == nullptr ? nullptr : node->as_array();
    if (array == nullptr || array->size() != 2U)
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Expected an array with exactly two non-negative integers.", node);
        return false;
    }
    const toml::node* minNode = array->get(0U);
    const toml::node* maxNode = array->get(1U);
    if (minNode == nullptr || maxNode == nullptr)
    {
        return false;
    }
    const std::optional<std::int64_t> minValue = minNode->value<std::int64_t>();
    const std::optional<std::int64_t> maxValue = maxNode->value<std::int64_t>();
    if (!minValue.has_value() || !maxValue.has_value() || *minValue < 0 || *maxValue < 0 ||
        static_cast<std::uint64_t>(*minValue) > std::numeric_limits<std::uint32_t>::max() ||
        static_cast<std::uint64_t>(*maxValue) > std::numeric_limits<std::uint32_t>::max())
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Range values must fit uint32.", node);
        return false;
    }
    if (*minValue > *maxValue)
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Range minimum must be <= maximum.", node);
        return false;
    }
    destination = ParticleUIntRange{static_cast<std::uint32_t>(*minValue), static_cast<std::uint32_t>(*maxValue)};
    return true;
}

bool ReadVec2(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    ParticleVec2& destination,
    std::vector<ParticleEffectDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    const toml::array* array = node == nullptr ? nullptr : node->as_array();
    if (array == nullptr || array->size() != 2U)
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Expected an array with exactly two finite numbers.", node);
        return false;
    }
    const toml::node* xNode = array->get(0U);
    const toml::node* yNode = array->get(1U);
    if (xNode == nullptr || yNode == nullptr)
    {
        return false;
    }
    const std::optional<float> x = ReadFiniteFloatNode(*xNode, std::string{path} + "[0]", diagnostics);
    const std::optional<float> y = ReadFiniteFloatNode(*yNode, std::string{path} + "[1]", diagnostics);
    if (!x.has_value() || !y.has_value())
    {
        return false;
    }
    destination = ParticleVec2{*x, *y};
    return true;
}

bool ReadColor(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    ParticleColor& destination,
    std::vector<ParticleEffectDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    const toml::array* array = node == nullptr ? nullptr : node->as_array();
    if (array == nullptr || array->size() != 4U)
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path}, "Expected an RGBA array with exactly four numbers in [0, 1].", node);
        return false;
    }

    float values[4]{};
    for (std::size_t index = 0; index < 4U; ++index)
    {
        const toml::node* valueNode = array->get(index);
        if (valueNode == nullptr)
        {
            return false;
        }
        const std::optional<float> value = ReadFiniteFloatNode(*valueNode, std::string{path} + "[" + std::to_string(index) + "]", diagnostics);
        if (!value.has_value())
        {
            return false;
        }
        if (*value < 0.0F || *value > 1.0F)
        {
            AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, std::string{path} + "[" + std::to_string(index) + "]", "Color channels must be in [0, 1].", valueNode);
            return false;
        }
        values[index] = *value;
    }
    destination = ParticleColor{values[0], values[1], values[2], values[3]};
    return true;
}

bool IsOrderedColorRange(const ParticleColor& minValue, const ParticleColor& maxValue) noexcept
{
    return minValue.r <= maxValue.r && minValue.g <= maxValue.g &&
        minValue.b <= maxValue.b && minValue.a <= maxValue.a;
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

std::string FormatFloat(const float value)
{
    std::ostringstream stream{};
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
    std::string text = stream.str();
    if (text.find_first_of(".eE") == std::string::npos)
    {
        text.append(".0");
    }
    return text;
}

void WriteVec2(std::ostringstream& output, const ParticleVec2 value)
{
    output << '[' << FormatFloat(value.x) << ", " << FormatFloat(value.y) << ']';
}

void WriteRange(std::ostringstream& output, const ParticleFloatRange range)
{
    output << '[' << FormatFloat(range.minValue) << ", " << FormatFloat(range.maxValue) << ']';
}

void WriteColor(std::ostringstream& output, const ParticleColor color)
{
    output << '[' << FormatFloat(color.r) << ", " << FormatFloat(color.g) << ", "
           << FormatFloat(color.b) << ", " << FormatFloat(color.a) << ']';
}

std::string_view BackendName(const ParticleEffectBackend backend) noexcept
{
    return backend == ParticleEffectBackend::Gpu ? "gpu" : "cpu";
}

std::string_view BlendName(const ParticleBlendMode blendMode) noexcept
{
    return blendMode == ParticleBlendMode::Additive ? "additive" : "alpha";
}

std::string_view SimulationSpaceName(const ParticleSimulationSpace space) noexcept
{
    return space == ParticleSimulationSpace::World ? "world" : "local";
}

std::string_view ShapeName(const ParticleSpawnShapeType shape) noexcept
{
    switch (shape)
    {
    case ParticleSpawnShapeType::Point: return "point";
    case ParticleSpawnShapeType::Box: return "box";
    case ParticleSpawnShapeType::Circle: return "circle";
    }
    return "point";
}

bool ValidateBurstBudget(
    const ParticleReferenceDefinition& definition,
    const std::vector<ParticleBurst>& bursts,
    const ParticleReferenceLimits& limits,
    std::vector<ParticleEffectDiagnostic>& diagnostics)
{
    if (bursts.size() > limits.maxBursts)
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::CapacityExceedsLimit, "bursts", "Burst count exceeds configured maxBursts safety budget.");
        return false;
    }
    if (definition.periodicCount > limits.maxSpawnAttemptsPerFrame)
    {
        AddDiagnostic(diagnostics, ParticleEffectErrorCode::CapacityExceedsLimit, "emission.count", "Periodic count exceeds configured maxSpawnAttemptsPerFrame safety budget.");
        return false;
    }

    ParticleFrameIndex previousFrame = 0;
    bool hasPrevious = false;
    std::size_t index = 0U;
    while (index < bursts.size())
    {
        const ParticleFrameIndex frame = bursts[index].frame;
        if (hasPrevious && frame < previousFrame)
        {
            AddDiagnostic(diagnostics, ParticleEffectErrorCode::SchemaError, "bursts[" + std::to_string(index) + "].frame", "Burst frames must be authored in non-decreasing order.");
            return false;
        }
        previousFrame = frame;
        hasPrevious = true;

        std::uint64_t attempts = 0U;
        while (index < bursts.size() && bursts[index].frame == frame)
        {
            attempts += bursts[index].count;
            ++index;
        }
        if (definition.periodicCount != 0U && definition.periodicEveryFrames != 0U &&
            frame >= definition.periodicStartFrame &&
            ((frame - definition.periodicStartFrame) % definition.periodicEveryFrames) == 0U)
        {
            attempts += definition.periodicCount;
        }
        if (attempts > limits.maxSpawnAttemptsPerFrame)
        {
            AddDiagnostic(diagnostics, ParticleEffectErrorCode::CapacityExceedsLimit, "bursts", "Combined burst and periodic attempts exceed configured maxSpawnAttemptsPerFrame safety budget.");
            return false;
        }
    }
    return true;
}
} // namespace

std::string_view ToString(const ParticleEffectErrorCode code) noexcept
{
    switch (code)
    {
    case ParticleEffectErrorCode::InvalidReference: return "invalid_reference";
    case ParticleEffectErrorCode::UnsupportedFormat: return "unsupported_format";
    case ParticleEffectErrorCode::MissingFile: return "missing_file";
    case ParticleEffectErrorCode::ReadFailure: return "read_failure";
    case ParticleEffectErrorCode::ParseError: return "parse_error";
    case ParticleEffectErrorCode::SchemaError: return "schema_error";
    case ParticleEffectErrorCode::CapacityExceedsLimit: return "capacity_exceeds_limit";
    }
    return "unknown";
}

ParticleEffectLoadResult ParseParticleEffectToml(
    const std::string_view text,
    const std::string_view canonicalAssetId,
    const ParticleReferenceLimits& limits,
    const std::string_view sourceName)
{
    ParticleEffectLoadResult result{};
    std::string normalizedAssetId{};
    ParticleEffectDiagnostic assetIdDiagnostic{};
    if (!TryNormalizeProjectRelativeReference(canonicalAssetId, normalizedAssetId, &assetIdDiagnostic))
    {
        result.diagnostics.push_back(std::move(assetIdDiagnostic));
        return result;
    }

    toml::table root{};
    try
    {
        root = toml::parse(text, sourceName);
    }
    catch (const toml::parse_error& error)
    {
        ParticleEffectDiagnostic diagnostic{};
        diagnostic.code = ParticleEffectErrorCode::ParseError;
        diagnostic.reference = normalizedAssetId;
        diagnostic.path = "$";
        diagnostic.message = std::string{error.description()};
        diagnostic.line = static_cast<std::size_t>(error.source().begin.line);
        diagnostic.column = static_cast<std::size_t>(error.source().begin.column);
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }

    ValidateKnownKeys(root, "", {"format_version", "effect", "emission", "spawn", "lifetime", "motion", "scale", "rotation", "color", "presentation", "bursts"}, result.diagnostics);

    const toml::node* versionNode = root.get("format_version");
    const std::optional<std::int64_t> version = versionNode == nullptr ? std::nullopt : versionNode->value<std::int64_t>();
    if (!version.has_value() || *version != ParticleEffectFormatVersion)
    {
        AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "format_version", "Expected integer format_version = 1.", versionNode);
    }

    auto asset = std::make_shared<ParticleEffectAsset>();
    asset->id = normalizedAssetId;

    if (const toml::table* table = ReadRequiredTable(root, "effect", "effect", result.diagnostics); table != nullptr)
    {
        ValidateKnownKeys(*table, "effect", {"id", "backend", "max_particles", "duration_frames", "loop", "play_on_load", "simulation_space"}, result.diagnostics);
        const std::optional<std::string> id = ReadRequiredString(*table, "id", "effect.id", result.diagnostics);
        const std::optional<std::string> backend = ReadRequiredString(*table, "backend", "effect.backend", result.diagnostics);
        const std::optional<std::uint32_t> maxParticles = ReadRequiredUInt32(*table, "max_particles", "effect.max_particles", result.diagnostics);
        const std::optional<std::uint32_t> duration = ReadRequiredUInt32(*table, "duration_frames", "effect.duration_frames", result.diagnostics);
        const std::optional<bool> loop = ReadRequiredBool(*table, "loop", "effect.loop", result.diagnostics);
        const std::optional<bool> playOnLoad = ReadRequiredBool(*table, "play_on_load", "effect.play_on_load", result.diagnostics);
        const std::optional<std::string> simulationSpace = ReadRequiredString(*table, "simulation_space", "effect.simulation_space", result.diagnostics);

        if (id.has_value()) asset->semanticId = *id;
        if (backend.has_value())
        {
            if (*backend == "cpu") asset->backend = ParticleEffectBackend::Cpu;
            else if (*backend == "gpu") asset->backend = ParticleEffectBackend::Gpu;
            else AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "effect.backend", "Supported backend values are 'cpu' and reserved 'gpu'.", table->get("backend"));
        }
        if (maxParticles.has_value())
        {
            asset->definition.maxParticles = *maxParticles;
            if (*maxParticles == 0U)
            {
                AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "effect.max_particles", "max_particles must be greater than zero.", table->get("max_particles"));
            }
            else if (*maxParticles > limits.maxParticlesPerEmitter)
            {
                AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::CapacityExceedsLimit, "effect.max_particles", "max_particles exceeds configured maxParticlesPerEmitter safety budget.", table->get("max_particles"));
            }
        }
        if (duration.has_value())
        {
            asset->lifecycle.durationFrames = *duration;
            if (*duration == 0U)
            {
                AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "effect.duration_frames", "duration_frames must be greater than zero.", table->get("duration_frames"));
            }
        }
        if (loop.has_value()) asset->lifecycle.loop = *loop;
        if (playOnLoad.has_value()) asset->lifecycle.playOnLoad = *playOnLoad;
        if (simulationSpace.has_value())
        {
            if (*simulationSpace == "local") asset->definition.simulationSpace = ParticleSimulationSpace::Local;
            else if (*simulationSpace == "world") asset->definition.simulationSpace = ParticleSimulationSpace::World;
            else AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "effect.simulation_space", "Supported values are 'local' and 'world'.", table->get("simulation_space"));
        }
    }

    if (const toml::table* table = ReadRequiredTable(root, "emission", "emission", result.diagnostics); table != nullptr)
    {
        ValidateKnownKeys(*table, "emission", {"start_frame", "count", "every_frames"}, result.diagnostics);
        const std::optional<std::uint64_t> startFrame = ReadRequiredUInt64(*table, "start_frame", "emission.start_frame", result.diagnostics);
        const std::optional<std::uint32_t> count = ReadRequiredUInt32(*table, "count", "emission.count", result.diagnostics);
        const std::optional<std::uint32_t> every = ReadRequiredUInt32(*table, "every_frames", "emission.every_frames", result.diagnostics);
        if (startFrame.has_value()) asset->definition.periodicStartFrame = static_cast<ParticleFrameIndex>(*startFrame);
        if (count.has_value()) asset->definition.periodicCount = *count;
        if (every.has_value()) asset->definition.periodicEveryFrames = *every;
        if (count.has_value() && every.has_value() && *count != 0U && *every == 0U)
        {
            AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "emission.every_frames", "every_frames must be > 0 when periodic count is non-zero.", table->get("every_frames"));
        }
    }

    if (const toml::table* table = ReadRequiredTable(root, "spawn", "spawn", result.diagnostics); table != nullptr)
    {
        ValidateKnownKeys(*table, "spawn", {"shape", "offset", "box_half_extents", "circle_radius"}, result.diagnostics);
        const std::optional<std::string> shape = ReadRequiredString(*table, "shape", "spawn.shape", result.diagnostics);
        ReadVec2(*table, "offset", "spawn.offset", asset->definition.spawnShape.offset, result.diagnostics);
        ReadVec2(*table, "box_half_extents", "spawn.box_half_extents", asset->definition.spawnShape.boxHalfExtents, result.diagnostics);
        const std::optional<float> radius = ReadRequiredFloat(*table, "circle_radius", "spawn.circle_radius", result.diagnostics);
        if (radius.has_value()) asset->definition.spawnShape.circleRadius = *radius;
        if (shape.has_value())
        {
            if (*shape == "point") asset->definition.spawnShape.type = ParticleSpawnShapeType::Point;
            else if (*shape == "box") asset->definition.spawnShape.type = ParticleSpawnShapeType::Box;
            else if (*shape == "circle") asset->definition.spawnShape.type = ParticleSpawnShapeType::Circle;
            else AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "spawn.shape", "Supported shapes are 'point', 'box', and 'circle'.", table->get("shape"));
        }
        const ParticleVec2 extents = asset->definition.spawnShape.boxHalfExtents;
        const float circleRadius = asset->definition.spawnShape.circleRadius;
        if (extents.x < 0.0F || extents.y < 0.0F || circleRadius < 0.0F)
        {
            AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "spawn", "Spawn extents and radius must be non-negative.", table->get("shape"));
        }
        if (shape.has_value() && *shape == "point" && (extents.x != 0.0F || extents.y != 0.0F || circleRadius != 0.0F))
        {
            AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "spawn", "Point spawn requires zero box_half_extents and circle_radius.", table->get("shape"));
        }
        if (shape.has_value() && *shape == "box" && circleRadius != 0.0F)
        {
            AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "spawn.circle_radius", "Box spawn requires circle_radius = 0.", table->get("circle_radius"));
        }
        if (shape.has_value() && *shape == "circle" && (extents.x != 0.0F || extents.y != 0.0F))
        {
            AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "spawn.box_half_extents", "Circle spawn requires box_half_extents = [0, 0].", table->get("box_half_extents"));
        }
    }

    if (const toml::table* table = ReadRequiredTable(root, "lifetime", "lifetime", result.diagnostics); table != nullptr)
    {
        ValidateKnownKeys(*table, "lifetime", {"frames"}, result.diagnostics);
        ReadUIntPair(*table, "frames", "lifetime.frames", asset->definition.lifetimeFrames, result.diagnostics);
        if (asset->definition.lifetimeFrames.minValue == 0U)
        {
            AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "lifetime.frames", "Lifetime minimum must be greater than zero.", table->get("frames"));
        }
    }

    if (const toml::table* table = ReadRequiredTable(root, "motion", "motion", result.diagnostics); table != nullptr)
    {
        ValidateKnownKeys(*table, "motion", {"speed", "angle_radians", "acceleration"}, result.diagnostics);
        ReadFloatPair(*table, "speed", "motion.speed", asset->definition.speed, result.diagnostics);
        ReadFloatPair(*table, "angle_radians", "motion.angle_radians", asset->definition.angleRadians, result.diagnostics);
        ReadVec2(*table, "acceleration", "motion.acceleration", asset->definition.acceleration, result.diagnostics);
    }

    if (const toml::table* table = ReadRequiredTable(root, "scale", "scale", result.diagnostics); table != nullptr)
    {
        ValidateKnownKeys(*table, "scale", {"initial", "end_multiplier"}, result.diagnostics);
        ReadFloatPair(*table, "initial", "scale.initial", asset->definition.initialSize, result.diagnostics);
        const std::optional<float> endMultiplier = ReadRequiredFloat(*table, "end_multiplier", "scale.end_multiplier", result.diagnostics);
        if (endMultiplier.has_value()) asset->definition.endSizeMultiplier = *endMultiplier;
        if (asset->definition.initialSize.minValue < 0.0F || asset->definition.endSizeMultiplier < 0.0F)
        {
            AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "scale", "Scale values must be non-negative.", table->get("initial"));
        }
    }

    if (const toml::table* table = ReadRequiredTable(root, "rotation", "rotation", result.diagnostics); table != nullptr)
    {
        ValidateKnownKeys(*table, "rotation", {"initial_radians", "angular_velocity_radians_per_frame"}, result.diagnostics);
        ReadFloatPair(*table, "initial_radians", "rotation.initial_radians", asset->definition.rotationRadians, result.diagnostics);
        ReadFloatPair(*table, "angular_velocity_radians_per_frame", "rotation.angular_velocity_radians_per_frame", asset->definition.angularVelocityRadiansPerFrame, result.diagnostics);
    }

    if (const toml::table* table = ReadRequiredTable(root, "color", "color", result.diagnostics); table != nullptr)
    {
        ValidateKnownKeys(*table, "color", {"initial_min", "initial_max", "end"}, result.diagnostics);
        ReadColor(*table, "initial_min", "color.initial_min", asset->definition.initialColor.minValue, result.diagnostics);
        ReadColor(*table, "initial_max", "color.initial_max", asset->definition.initialColor.maxValue, result.diagnostics);
        ReadColor(*table, "end", "color.end", asset->definition.endColor, result.diagnostics);
        if (!IsOrderedColorRange(asset->definition.initialColor.minValue, asset->definition.initialColor.maxValue))
        {
            AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "color", "Every initial_min channel must be <= initial_max.", table->get("initial_min"));
        }
    }

    if (const toml::table* table = ReadRequiredTable(root, "presentation", "presentation", result.diagnostics); table != nullptr)
    {
        ValidateKnownKeys(*table, "presentation", {"sprites", "blend"}, result.diagnostics);
        const std::optional<std::string> blend = ReadRequiredString(*table, "blend", "presentation.blend", result.diagnostics);
        if (blend.has_value())
        {
            if (*blend == "alpha") asset->blendMode = ParticleBlendMode::Alpha;
            else if (*blend == "additive") asset->blendMode = ParticleBlendMode::Additive;
            else AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "presentation.blend", "Supported blend modes are 'alpha' and 'additive'.", table->get("blend"));
        }

        const toml::node* spritesNode = table->get("sprites");
        const toml::array* sprites = spritesNode == nullptr ? nullptr : spritesNode->as_array();
        if (sprites == nullptr || sprites->empty())
        {
            AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "presentation.sprites", "At least one project-relative sprite reference is required.", spritesNode);
        }
        else if (sprites->size() > std::numeric_limits<std::uint32_t>::max())
        {
            AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::CapacityExceedsLimit, "presentation.sprites", "Sprite reference count exceeds uint32 range.", spritesNode);
        }
        else
        {
            asset->spriteReferences.reserve(sprites->size());
            for (std::size_t index = 0; index < sprites->size(); ++index)
            {
                const toml::node* spriteNode = sprites->get(index);
                const std::optional<std::string> sprite = spriteNode == nullptr ? std::nullopt : spriteNode->value<std::string>();
                if (!sprite.has_value() || sprite->empty())
                {
                    AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "presentation.sprites[" + std::to_string(index) + "]", "Expected a non-empty project-relative string.", spriteNode);
                    continue;
                }
                std::string normalized{};
                ParticleEffectDiagnostic normalizationDiagnostic{};
                if (!TryNormalizeProjectRelativeReference(*sprite, normalized, &normalizationDiagnostic))
                {
                    normalizationDiagnostic.path = "presentation.sprites[" + std::to_string(index) + "]";
                    normalizationDiagnostic.line = spriteNode == nullptr ? 0U : static_cast<std::size_t>(spriteNode->source().begin.line);
                    normalizationDiagnostic.column = spriteNode == nullptr ? 0U : static_cast<std::size_t>(spriteNode->source().begin.column);
                    result.diagnostics.push_back(std::move(normalizationDiagnostic));
                    continue;
                }
                asset->spriteReferences.push_back(std::move(normalized));
            }
            asset->definition.spriteChoiceCount = static_cast<std::uint32_t>(sprites->size());
        }
    }

    if (const toml::node* burstsNode = root.get("bursts"); burstsNode != nullptr)
    {
        const toml::array* bursts = burstsNode->as_array();
        if (bursts == nullptr)
        {
            AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "bursts", "Expected an array of burst tables.", burstsNode);
        }
        else
        {
            asset->bursts.reserve(bursts->size());
            for (std::size_t index = 0; index < bursts->size(); ++index)
            {
                const toml::node* burstNode = bursts->get(index);
                const toml::table* burst = burstNode == nullptr ? nullptr : burstNode->as_table();
                const std::string path = "bursts[" + std::to_string(index) + "]";
                if (burst == nullptr)
                {
                    AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, path, "Expected a burst table.", burstNode);
                    continue;
                }
                ValidateKnownKeys(*burst, path, {"frame", "count"}, result.diagnostics);
                const std::optional<std::uint64_t> frame = ReadRequiredUInt64(*burst, "frame", path + ".frame", result.diagnostics);
                const std::optional<std::uint32_t> count = ReadRequiredUInt32(*burst, "count", path + ".count", result.diagnostics);
                if (frame.has_value() && count.has_value())
                {
                    asset->bursts.push_back(ParticleBurst{static_cast<ParticleFrameIndex>(*frame), *count});
                }
            }
        }
    }

    ValidateBurstBudget(asset->definition, asset->bursts, limits, result.diagnostics);

    if (!result.diagnostics.empty())
    {
        return result;
    }

    result.asset = std::shared_ptr<const ParticleEffectAsset>{std::move(asset)};
    return result;
}

std::string SaveParticleEffectToml(const ParticleEffectAsset& asset)
{
    std::ostringstream output{};
    output.imbue(std::locale::classic());
    output << "format_version = " << ParticleEffectFormatVersion << "\n\n";
    output << "[effect]\n";
    output << "id = " << EscapeTomlString(asset.semanticId) << "\n";
    output << "backend = " << EscapeTomlString(BackendName(asset.backend)) << "\n";
    output << "max_particles = " << asset.definition.maxParticles << "\n";
    output << "duration_frames = " << asset.lifecycle.durationFrames << "\n";
    output << "loop = " << (asset.lifecycle.loop ? "true" : "false") << "\n";
    output << "play_on_load = " << (asset.lifecycle.playOnLoad ? "true" : "false") << "\n";
    output << "simulation_space = " << EscapeTomlString(SimulationSpaceName(asset.definition.simulationSpace)) << "\n\n";

    output << "[emission]\n";
    output << "start_frame = " << asset.definition.periodicStartFrame << "\n";
    output << "count = " << asset.definition.periodicCount << "\n";
    output << "every_frames = " << asset.definition.periodicEveryFrames << "\n\n";

    output << "[spawn]\n";
    output << "shape = " << EscapeTomlString(ShapeName(asset.definition.spawnShape.type)) << "\n";
    output << "offset = "; WriteVec2(output, asset.definition.spawnShape.offset); output << "\n";
    output << "box_half_extents = "; WriteVec2(output, asset.definition.spawnShape.boxHalfExtents); output << "\n";
    output << "circle_radius = " << FormatFloat(asset.definition.spawnShape.circleRadius) << "\n\n";

    output << "[lifetime]\nframes = [" << asset.definition.lifetimeFrames.minValue << ", " << asset.definition.lifetimeFrames.maxValue << "]\n\n";
    output << "[motion]\nspeed = "; WriteRange(output, asset.definition.speed); output << "\nangle_radians = "; WriteRange(output, asset.definition.angleRadians); output << "\nacceleration = "; WriteVec2(output, asset.definition.acceleration); output << "\n\n";
    output << "[scale]\ninitial = "; WriteRange(output, asset.definition.initialSize); output << "\nend_multiplier = " << FormatFloat(asset.definition.endSizeMultiplier) << "\n\n";
    output << "[rotation]\ninitial_radians = "; WriteRange(output, asset.definition.rotationRadians); output << "\nangular_velocity_radians_per_frame = "; WriteRange(output, asset.definition.angularVelocityRadiansPerFrame); output << "\n\n";
    output << "[color]\ninitial_min = "; WriteColor(output, asset.definition.initialColor.minValue); output << "\ninitial_max = "; WriteColor(output, asset.definition.initialColor.maxValue); output << "\nend = "; WriteColor(output, asset.definition.endColor); output << "\n\n";
    output << "[presentation]\nblend = " << EscapeTomlString(BlendName(asset.blendMode)) << "\nsprites = [";
    for (std::size_t index = 0; index < asset.spriteReferences.size(); ++index)
    {
        if (index != 0U) output << ", ";
        output << EscapeTomlString(asset.spriteReferences[index]);
    }
    output << "]\n";
    for (const ParticleBurst& burst : asset.bursts)
    {
        output << "\n[[bursts]]\nframe = " << burst.frame << "\ncount = " << burst.count << "\n";
    }
    return output.str();
}

ParticleEffectCache::ParticleEffectCache(std::filesystem::path projectRoot, const ParticleReferenceLimits limits)
    : projectRoot_{std::move(projectRoot).lexically_normal()}
    , limits_{limits}
{
    if (projectRoot_.empty()) projectRoot_ = std::filesystem::path{"."};
}

ParticleEffectLoadResult ParticleEffectCache::Load(const std::string_view projectRelativeReference)
{
    ++requests_;
    NormalizedReference normalized{};
    ParticleEffectDiagnostic diagnostic{};
    if (!TryNormalizeReference(projectRoot_, projectRelativeReference, normalized, &diagnostic))
    {
        ++failedImports_;
        ParticleEffectLoadResult result{};
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }

    const auto cached = cache_.find(normalized.id);
    if (cached != cache_.end())
    {
        ++cacheHits_;
        ParticleEffectLoadResult result{};
        result.asset = cached->second;
        return result;
    }
    ++cacheMisses_;

    if (!std::string_view{normalized.id}.ends_with(ParticleEffectSuffix))
    {
        ++failedImports_;
        ParticleEffectLoadResult result{};
        result.diagnostics.push_back(MakeDiagnostic(ParticleEffectErrorCode::UnsupportedFormat, "$reference", "Particle effect files must end with .trace2d.particle.toml.", projectRelativeReference, normalized.resolvedPath));
        return result;
    }

    std::error_code error{};
    if (!std::filesystem::exists(normalized.resolvedPath, error))
    {
        ++failedImports_;
        ParticleEffectLoadResult result{};
        result.diagnostics.push_back(MakeDiagnostic(error ? ParticleEffectErrorCode::ReadFailure : ParticleEffectErrorCode::MissingFile, "$reference", error ? "Unable to inspect particle effect source: " + error.message() : "Particle effect source file does not exist.", projectRelativeReference, normalized.resolvedPath));
        return result;
    }
    if (!std::filesystem::is_regular_file(normalized.resolvedPath, error) || error)
    {
        ++failedImports_;
        ParticleEffectLoadResult result{};
        result.diagnostics.push_back(MakeDiagnostic(ParticleEffectErrorCode::ReadFailure, "$reference", "Particle effect source must resolve to a regular file.", projectRelativeReference, normalized.resolvedPath));
        return result;
    }
    const std::uintmax_t fileSize = std::filesystem::file_size(normalized.resolvedPath, error);
    if (error || fileSize > MaximumParticleEffectSourceBytes)
    {
        ++failedImports_;
        ParticleEffectLoadResult result{};
        result.diagnostics.push_back(MakeDiagnostic(ParticleEffectErrorCode::ReadFailure, "$reference", error ? "Unable to determine particle effect source size: " + error.message() : "Particle effect source exceeds the 1 MiB authoring safety limit.", projectRelativeReference, normalized.resolvedPath));
        return result;
    }

    std::ifstream input{normalized.resolvedPath, std::ios::binary};
    if (!input)
    {
        ++failedImports_;
        ParticleEffectLoadResult result{};
        result.diagnostics.push_back(MakeDiagnostic(ParticleEffectErrorCode::ReadFailure, "$reference", "Unable to open particle effect source for reading.", projectRelativeReference, normalized.resolvedPath));
        return result;
    }
    std::string text(static_cast<std::size_t>(fileSize), '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (!input && !text.empty())
    {
        ++failedImports_;
        ParticleEffectLoadResult result{};
        result.diagnostics.push_back(MakeDiagnostic(ParticleEffectErrorCode::ReadFailure, "$reference", "Particle effect source could not be read completely.", projectRelativeReference, normalized.resolvedPath));
        return result;
    }

    ParticleEffectLoadResult result = ParseParticleEffectToml(text, normalized.id, limits_, normalized.resolvedPath.generic_string());
    if (!result.Succeeded())
    {
        ++failedImports_;
        for (ParticleEffectDiagnostic& item : result.diagnostics)
        {
            if (item.reference.empty()) item.reference = normalized.id;
            if (item.resolvedPath.empty()) item.resolvedPath = normalized.resolvedPath.generic_string();
        }
        return result;
    }

    cache_.emplace(normalized.id, result.asset);
    ++successfulImports_;
    return result;
}

bool ParticleEffectCache::Invalidate(const std::string_view projectRelativeReference)
{
    NormalizedReference normalized{};
    if (!TryNormalizeReference(projectRoot_, projectRelativeReference, normalized, nullptr)) return false;
    return cache_.erase(normalized.id) != 0U;
}

void ParticleEffectCache::Clear() noexcept { cache_.clear(); }
const std::filesystem::path& ParticleEffectCache::ProjectRoot() const noexcept { return projectRoot_; }
const ParticleReferenceLimits& ParticleEffectCache::Limits() const noexcept { return limits_; }
ParticleEffectCacheMetrics ParticleEffectCache::Metrics() const noexcept
{
    return ParticleEffectCacheMetrics{requests_, cacheHits_, cacheMisses_, successfulImports_, failedImports_, cache_.size()};
}

ParticleSceneLoadResult LoadParticleSceneToml(const std::string_view text, const std::string_view sourceName)
{
    ParticleSceneLoadResult result{};
    toml::table root{};
    try
    {
        root = toml::parse(text, sourceName);
    }
    catch (const toml::parse_error& error)
    {
        ParticleEffectDiagnostic diagnostic{};
        diagnostic.code = ParticleEffectErrorCode::ParseError;
        diagnostic.path = "$";
        diagnostic.message = std::string{error.description()};
        diagnostic.line = static_cast<std::size_t>(error.source().begin.line);
        diagnostic.column = static_cast<std::size_t>(error.source().begin.column);
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }

    if (const toml::node* emittersNode = root.get("particle_emitters"); emittersNode != nullptr)
    {
        const toml::array* emitters = emittersNode->as_array();
        if (emitters == nullptr)
        {
            AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "particle_emitters", "Expected an array of ParticleEmitter2D reference tables.", emittersNode);
        }
        else
        {
            std::unordered_set<std::string> entityIds{};
            std::unordered_set<ParticleEmitterStableId> stableIds{};
            result.emitters.reserve(emitters->size());
            for (std::size_t index = 0; index < emitters->size(); ++index)
            {
                const toml::node* emitterNode = emitters->get(index);
                const toml::table* emitter = emitterNode == nullptr ? nullptr : emitterNode->as_table();
                const std::string path = "particle_emitters[" + std::to_string(index) + "]";
                if (emitter == nullptr)
                {
                    AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, path, "Expected a ParticleEmitter2D reference table.", emitterNode);
                    continue;
                }
                ValidateKnownKeys(*emitter, path, {"entity", "effect", "stable_id"}, result.diagnostics);
                const std::optional<std::string> entity = ReadRequiredString(*emitter, "entity", path + ".entity", result.diagnostics);
                const std::optional<std::string> effect = ReadRequiredString(*emitter, "effect", path + ".effect", result.diagnostics);
                const std::optional<std::uint64_t> stableId = ReadRequiredUInt64(*emitter, "stable_id", path + ".stable_id", result.diagnostics);
                if (!entity.has_value() || !effect.has_value() || !stableId.has_value()) continue;

                std::string canonicalEffect{};
                ParticleEffectDiagnostic normalizationDiagnostic{};
                if (!TryNormalizeProjectRelativeReference(*effect, canonicalEffect, &normalizationDiagnostic))
                {
                    normalizationDiagnostic.path = path + ".effect";
                    normalizationDiagnostic.line = static_cast<std::size_t>(emitter->get("effect")->source().begin.line);
                    normalizationDiagnostic.column = static_cast<std::size_t>(emitter->get("effect")->source().begin.column);
                    result.diagnostics.push_back(std::move(normalizationDiagnostic));
                    continue;
                }
                if (!entityIds.emplace(*entity).second)
                {
                    AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, path + ".entity", "Only one ParticleEmitter2D reference is allowed per entity in V1.", emitter->get("entity"));
                    continue;
                }
                const ParticleEmitterStableId typedStableId = static_cast<ParticleEmitterStableId>(*stableId);
                if (!stableIds.emplace(typedStableId).second)
                {
                    AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, path + ".stable_id", "ParticleEmitter2D stable_id values must be unique within a scene.", emitter->get("stable_id"));
                    continue;
                }
                result.emitters.push_back(ParticleEmitter2DSceneReference{*entity, std::move(canonicalEffect), typedStableId});
            }
        }
    }

    root.erase("particle_emitters");
    std::ostringstream sceneSource{};
    sceneSource.imbue(std::locale::classic());
    sceneSource << root;
    scene::SceneLoadResult sceneResult = scene::LoadSceneToml(sceneSource.str(), sourceName);
    for (const scene::SceneTextDiagnostic& sceneDiagnostic : sceneResult.diagnostics)
    {
        ParticleEffectDiagnostic diagnostic{};
        diagnostic.code = ParticleEffectErrorCode::SchemaError;
        diagnostic.path = sceneDiagnostic.path;
        diagnostic.message = sceneDiagnostic.message;
        diagnostic.line = sceneDiagnostic.line;
        diagnostic.column = sceneDiagnostic.column;
        result.diagnostics.push_back(std::move(diagnostic));
    }

    if (sceneResult.scene.has_value())
    {
        for (const ParticleEmitter2DSceneReference& emitter : result.emitters)
        {
            if (!sceneResult.scene->FindBySemanticId(emitter.entityId).has_value())
            {
                AddDiagnostic(result.diagnostics, ParticleEffectErrorCode::SchemaError, "particle_emitters", "ParticleEmitter2D references missing entity '" + emitter.entityId + "'.");
            }
        }
    }

    if (result.diagnostics.empty() && sceneResult.scene.has_value())
    {
        result.scene = std::move(sceneResult.scene);
    }
    return result;
}

ParticleEmitter2DPrepareResult ParticleEmitter2D::Prepare(
    std::shared_ptr<const ParticleEffectAsset> effect,
    const std::uint64_t globalSeed,
    const ParticleEmitterStableId stableId,
    const ParticleReferenceLimits& limits) noexcept
{
    if (effect == nullptr)
    {
        return ParticleEmitter2DPrepareResult{ParticleEmitter2DError::MissingEffect, ParticleReferenceError::None};
    }
    if (effect->backend != ParticleEffectBackend::Cpu)
    {
        return ParticleEmitter2DPrepareResult{ParticleEmitter2DError::BackendUnavailable, ParticleReferenceError::None};
    }

    ParticleReferenceDefinition definition = effect->definition;
    definition.globalSeed = globalSeed;
    definition.emitterStableId = stableId;

    ParticleReferenceEmitter candidate{};
    const ParticleReferencePrepareResult prepare = candidate.Prepare(definition, effect->bursts, limits);
    if (!prepare.Ok())
    {
        return ParticleEmitter2DPrepareResult{ParticleEmitter2DError::ReferencePrepareFailed, prepare.error};
    }

    effect_ = std::move(effect);
    reference_ = std::move(candidate);
    cycleFrame_ = 0U;
    completedLoops_ = 0U;
    playing_ = effect_->lifecycle.playOnLoad;
    resetBeforeNextStep_ = false;
    return {};
}

void ParticleEmitter2D::Reset() noexcept
{
    reference_.Reset();
    cycleFrame_ = 0U;
    completedLoops_ = 0U;
    resetBeforeNextStep_ = false;
    playing_ = effect_ != nullptr && effect_->lifecycle.playOnLoad;
}

void ParticleEmitter2D::Play() noexcept
{
    if (effect_ == nullptr) return;
    if (!effect_->lifecycle.loop && cycleFrame_ >= effect_->lifecycle.durationFrames)
    {
        reference_.Reset();
        cycleFrame_ = 0U;
        resetBeforeNextStep_ = false;
    }
    playing_ = true;
}

void ParticleEmitter2D::Restart() noexcept
{
    if (effect_ == nullptr) return;
    reference_.Reset();
    cycleFrame_ = 0U;
    completedLoops_ = 0U;
    resetBeforeNextStep_ = false;
    playing_ = true;
}

void ParticleEmitter2D::Stop() noexcept { playing_ = false; }

bool ParticleEmitter2D::Step() noexcept
{
    if (effect_ == nullptr || !reference_.IsPrepared()) return false;
    if (!playing_) return true;

    if (resetBeforeNextStep_)
    {
        reference_.Reset();
        cycleFrame_ = 0U;
        resetBeforeNextStep_ = false;
    }
    if (!reference_.Step()) return false;

    ++cycleFrame_;
    if (cycleFrame_ >= effect_->lifecycle.durationFrames)
    {
        if (effect_->lifecycle.loop)
        {
            ++completedLoops_;
            resetBeforeNextStep_ = true;
        }
        else
        {
            playing_ = false;
        }
    }
    return true;
}

bool ParticleEmitter2D::IsPrepared() const noexcept { return effect_ != nullptr && reference_.IsPrepared(); }
bool ParticleEmitter2D::IsPlaying() const noexcept { return playing_; }
std::uint32_t ParticleEmitter2D::CycleFrame() const noexcept { return cycleFrame_; }
std::uint64_t ParticleEmitter2D::CompletedLoops() const noexcept { return completedLoops_; }
const std::shared_ptr<const ParticleEffectAsset>& ParticleEmitter2D::Effect() const noexcept { return effect_; }
ParticleReferenceEmitter& ParticleEmitter2D::Reference() noexcept { return reference_; }
const ParticleReferenceEmitter& ParticleEmitter2D::Reference() const noexcept { return reference_; }
} // namespace trace2d::particles
