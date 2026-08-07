#include <trace2d/scene/SceneText.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace trace2d::scene
{
namespace
{
constexpr std::int64_t SceneFormatVersion = 1;

void AddDiagnostic(
    std::vector<SceneTextDiagnostic>& diagnostics,
    std::string path,
    std::string message,
    const toml::node* node = nullptr)
{
    SceneTextDiagnostic diagnostic{};
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

bool IsKnownKey(
    const std::string_view key,
    const std::initializer_list<std::string_view> knownKeys)
{
    return std::find(knownKeys.begin(), knownKeys.end(), key) != knownKeys.end();
}

void ValidateKnownKeys(
    const toml::table& table,
    const std::string_view path,
    const std::initializer_list<std::string_view> knownKeys,
    std::vector<SceneTextDiagnostic>& diagnostics)
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
        AddDiagnostic(diagnostics, std::move(fieldPath), "Unknown field.", &value);
    }
}

std::optional<std::string> ReadRequiredString(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<SceneTextDiagnostic>& diagnostics,
    const bool requireNonEmpty)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a string.", node);
        return std::nullopt;
    }

    if (requireNonEmpty && value->empty())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Value must not be empty.", node);
        return std::nullopt;
    }

    return value;
}

std::optional<std::string> ReadOptionalString(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<SceneTextDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        return std::string{};
    }

    std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a string.", node);
    }
    return value;
}

std::optional<float> ReadFiniteFloat(
    const toml::node& node,
    const std::string_view path,
    std::vector<SceneTextDiagnostic>& diagnostics)
{
    const std::optional<double> value = node.value<double>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a number.", &node);
        return std::nullopt;
    }

    const double maximum = static_cast<double>(std::numeric_limits<float>::max());
    if (!std::isfinite(*value) || *value < -maximum || *value > maximum)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Number must be finite and fit in a 32-bit float.", &node);
        return std::nullopt;
    }

    return static_cast<float>(*value);
}

bool ReadVector2(
    const toml::node& node,
    const std::string_view path,
    Vector2& destination,
    std::vector<SceneTextDiagnostic>& diagnostics)
{
    const toml::array* array = node.as_array();
    if (array == nullptr || array->size() != 2U)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected an array with exactly two numbers.", &node);
        return false;
    }

    const toml::node* xNode = array->get(0U);
    const toml::node* yNode = array->get(1U);
    if (xNode == nullptr || yNode == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected an array with exactly two numbers.", &node);
        return false;
    }

    const std::optional<float> x = ReadFiniteFloat(*xNode, std::string{path} + "[0]", diagnostics);
    const std::optional<float> y = ReadFiniteFloat(*yNode, std::string{path} + "[1]", diagnostics);
    if (!x.has_value() || !y.has_value())
    {
        return false;
    }

    destination = Vector2{*x, *y};
    return true;
}

void ReadTransform(
    const toml::table& table,
    const std::string_view path,
    Transform2D& transform,
    std::vector<SceneTextDiagnostic>& diagnostics)
{
    ValidateKnownKeys(
        table,
        path,
        {"position", "rotation_radians", "scale"},
        diagnostics);

    if (const toml::node* positionNode = table.get("position"); positionNode != nullptr)
    {
        ReadVector2(*positionNode, std::string{path} + ".position", transform.position, diagnostics);
    }

    if (const toml::node* rotationNode = table.get("rotation_radians"); rotationNode != nullptr)
    {
        const std::optional<float> rotation =
            ReadFiniteFloat(*rotationNode, std::string{path} + ".rotation_radians", diagnostics);
        if (rotation.has_value())
        {
            transform.rotationRadians = *rotation;
        }
    }

    if (const toml::node* scaleNode = table.get("scale"); scaleNode != nullptr)
    {
        ReadVector2(*scaleNode, std::string{path} + ".scale", transform.scale, diagnostics);
    }
}

std::vector<std::string> ReadTags(
    const toml::table& table,
    const std::string_view path,
    std::vector<SceneTextDiagnostic>& diagnostics)
{
    std::vector<std::string> tags{};
    const toml::node* node = table.get("tags");
    if (node == nullptr)
    {
        return tags;
    }

    const toml::array* array = node->as_array();
    if (array == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected an array of strings.", node);
        return tags;
    }

    tags.reserve(array->size());
    for (std::size_t index = 0; index < array->size(); ++index)
    {
        const toml::node* tagNode = array->get(index);
        if (tagNode == nullptr)
        {
            continue;
        }

        const std::optional<std::string> tag = tagNode->value<std::string>();
        if (!tag.has_value())
        {
            AddDiagnostic(
                diagnostics,
                std::string{path} + "[" + std::to_string(index) + "]",
                "Expected a string tag.",
                tagNode);
            continue;
        }

        if (tag->empty())
        {
            AddDiagnostic(
                diagnostics,
                std::string{path} + "[" + std::to_string(index) + "]",
                "Tag must not be empty.",
                tagNode);
            continue;
        }

        tags.push_back(*tag);
    }

    return tags;
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
        case '"':
            escaped.append("\\\"");
            break;
        case '\\':
            escaped.append("\\\\");
            break;
        case '\b':
            escaped.append("\\b");
            break;
        case '\t':
            escaped.append("\\t");
            break;
        case '\n':
            escaped.append("\\n");
            break;
        case '\f':
            escaped.append("\\f");
            break;
        case '\r':
            escaped.append("\\r");
            break;
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

bool IsFiniteTransform(const Transform2D& transform) noexcept
{
    return std::isfinite(transform.position.x) &&
           std::isfinite(transform.position.y) &&
           std::isfinite(transform.rotationRadians) &&
           std::isfinite(transform.scale.x) &&
           std::isfinite(transform.scale.y);
}
} // namespace

SceneLoadResult LoadSceneToml(const std::string_view text, const std::string_view sourceName)
{
    SceneLoadResult result{};
    toml::table root{};

    try
    {
        root = toml::parse(text, sourceName);
    }
    catch (const toml::parse_error& error)
    {
        SceneTextDiagnostic diagnostic{};
        diagnostic.path = "$";
        diagnostic.message = std::string{error.description()};
        diagnostic.line = static_cast<std::size_t>(error.source().begin.line);
        diagnostic.column = static_cast<std::size_t>(error.source().begin.column);
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }

    ValidateKnownKeys(root, "", {"format_version", "scene", "entities"}, result.diagnostics);

    const toml::node* formatNode = root.get("format_version");
    if (formatNode == nullptr)
    {
        AddDiagnostic(result.diagnostics, "format_version", "Required field is missing.");
    }
    else
    {
        const std::optional<std::int64_t> version = formatNode->value<std::int64_t>();
        if (!version.has_value())
        {
            AddDiagnostic(result.diagnostics, "format_version", "Expected an integer.", formatNode);
        }
        else if (*version != SceneFormatVersion)
        {
            AddDiagnostic(
                result.diagnostics,
                "format_version",
                "Unsupported scene format version. Expected 1.",
                formatNode);
        }
    }

    SceneMetadata metadata{};
    const toml::node* sceneNode = root.get("scene");
    if (sceneNode == nullptr)
    {
        AddDiagnostic(result.diagnostics, "scene", "Required table is missing.");
    }
    else if (const toml::table* sceneTable = sceneNode->as_table(); sceneTable != nullptr)
    {
        ValidateKnownKeys(*sceneTable, "scene", {"id", "name"}, result.diagnostics);

        const std::optional<std::string> semanticId =
            ReadRequiredString(*sceneTable, "id", "scene.id", result.diagnostics, true);
        const std::optional<std::string> name =
            ReadOptionalString(*sceneTable, "name", "scene.name", result.diagnostics);

        if (semanticId.has_value())
        {
            metadata.semanticId = *semanticId;
        }
        if (name.has_value())
        {
            metadata.name = *name;
        }
    }
    else
    {
        AddDiagnostic(result.diagnostics, "scene", "Expected a table.", sceneNode);
    }

    Scene scene{std::move(metadata)};
    const toml::node* entitiesNode = root.get("entities");
    if (entitiesNode != nullptr)
    {
        const toml::array* entities = entitiesNode->as_array();
        if (entities == nullptr)
        {
            AddDiagnostic(result.diagnostics, "entities", "Expected an array of entity tables.", entitiesNode);
        }
        else
        {
            for (std::size_t index = 0; index < entities->size(); ++index)
            {
                const toml::node* entityNode = entities->get(index);
                const std::string entityPath = "entities[" + std::to_string(index) + "]";
                if (entityNode == nullptr)
                {
                    continue;
                }

                const toml::table* entityTable = entityNode->as_table();
                if (entityTable == nullptr)
                {
                    AddDiagnostic(result.diagnostics, entityPath, "Expected an entity table.", entityNode);
                    continue;
                }

                ValidateKnownKeys(
                    *entityTable,
                    entityPath,
                    {"id", "name", "tags", "transform"},
                    result.diagnostics);

                EntityDescriptor descriptor{};
                const std::optional<std::string> semanticId = ReadRequiredString(
                    *entityTable,
                    "id",
                    entityPath + ".id",
                    result.diagnostics,
                    true);
                const std::optional<std::string> name = ReadOptionalString(
                    *entityTable,
                    "name",
                    entityPath + ".name",
                    result.diagnostics);

                if (semanticId.has_value())
                {
                    descriptor.semanticId = *semanticId;
                }
                if (name.has_value())
                {
                    descriptor.name = *name;
                }

                descriptor.tags = ReadTags(*entityTable, entityPath + ".tags", result.diagnostics);

                if (const toml::node* transformNode = entityTable->get("transform"); transformNode != nullptr)
                {
                    if (const toml::table* transformTable = transformNode->as_table(); transformTable != nullptr)
                    {
                        ReadTransform(
                            *transformTable,
                            entityPath + ".transform",
                            descriptor.transform,
                            result.diagnostics);
                    }
                    else
                    {
                        AddDiagnostic(
                            result.diagnostics,
                            entityPath + ".transform",
                            "Expected a table.",
                            transformNode);
                    }
                }

                if (!semanticId.has_value())
                {
                    continue;
                }

                try
                {
                    static_cast<void>(scene.CreateEntity(std::move(descriptor)));
                }
                catch (const std::invalid_argument&)
                {
                    AddDiagnostic(
                        result.diagnostics,
                        entityPath + ".id",
                        "Duplicate entity semantic ID '" + *semanticId + "'.",
                        entityTable->get("id"));
                }
            }
        }
    }

    if (result.diagnostics.empty())
    {
        result.scene = std::move(scene);
    }

    return result;
}

SceneSaveResult SaveSceneToml(const Scene& scene)
{
    SceneSaveResult result{};
    if (scene.Metadata().semanticId.empty())
    {
        AddDiagnostic(result.diagnostics, "scene.id", "Scene semantic ID must not be empty.");
    }

    std::vector<const Entity*> entities{};
    entities.reserve(scene.EntityCount());

    scene.ForEachEntity(
        [&result, &entities](const EntityId id, const Entity& entity)
        {
            if (entity.SemanticId().empty())
            {
                AddDiagnostic(
                    result.diagnostics,
                    "entities[index=" + std::to_string(id.index) + "].id",
                    "Runtime-only entity without a semantic ID cannot be serialized as authored scene data.");
                return;
            }

            if (!IsFiniteTransform(entity.Transform()))
            {
                AddDiagnostic(
                    result.diagnostics,
                    "entities[id=" + std::string{entity.SemanticId()} + "].transform",
                    "Transform values must be finite before serialization.");
                return;
            }

            entities.push_back(&entity);
        });

    if (!result.diagnostics.empty())
    {
        return result;
    }

    std::sort(
        entities.begin(),
        entities.end(),
        [](const Entity* left, const Entity* right)
        {
            return left->SemanticId() < right->SemanticId();
        });

    std::ostringstream output{};
    output.imbue(std::locale::classic());
    output << "format_version = " << SceneFormatVersion << "\n\n";
    output << "[scene]\n";
    output << "id = " << EscapeTomlString(scene.Metadata().semanticId) << "\n";
    output << "name = " << EscapeTomlString(scene.Metadata().name) << "\n";

    for (const Entity* entity : entities)
    {
        const Transform2D& transform = entity->Transform();
        output << "\n[[entities]]\n";
        output << "id = " << EscapeTomlString(entity->SemanticId()) << "\n";
        output << "name = " << EscapeTomlString(entity->Name()) << "\n";
        output << "tags = [";

        const std::vector<std::string>& tags = entity->Tags();
        for (std::size_t index = 0; index < tags.size(); ++index)
        {
            if (index != 0U)
            {
                output << ", ";
            }
            output << EscapeTomlString(tags[index]);
        }
        output << "]\n\n";

        output << "[entities.transform]\n";
        output << "position = [" << FormatFloat(transform.position.x) << ", "
               << FormatFloat(transform.position.y) << "]\n";
        output << "rotation_radians = " << FormatFloat(transform.rotationRadians) << "\n";
        output << "scale = [" << FormatFloat(transform.scale.x) << ", "
               << FormatFloat(transform.scale.y) << "]\n";
    }

    result.text = output.str();
    return result;
}
} // namespace trace2d::scene
