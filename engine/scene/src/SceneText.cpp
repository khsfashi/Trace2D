#include <trace2d/scene/SceneText.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <locale>
#include <optional>
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
constexpr std::int64_t CurrentSceneFormatVersion = 2;
constexpr std::int64_t MinimumSceneFormatVersion = 1;

struct PendingEntity final
{
    const toml::table* table{nullptr};
    std::string path{};
    std::string parentSemanticId{};
    EntityId id{};
};

struct SaveEntity final
{
    EntityId id{};
    const Entity* entity{nullptr};
    std::vector<AuthoredComponentSnapshot> components{};
};

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

bool IsKnownKey(const std::string_view key, const std::initializer_list<std::string_view> knownKeys)
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
        if (IsKnownKey(keyView, knownKeys)) continue;
        std::string fieldPath{path};
        if (!fieldPath.empty()) fieldPath.push_back('.');
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
    std::vector<SceneTextDiagnostic>& diagnostics,
    const bool requireNonEmpty = false)
{
    const toml::node* node = table.get(key);
    if (node == nullptr) return std::string{};
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

std::optional<float> ReadFiniteFloat(
    const toml::node& node,
    const std::string_view path,
    std::vector<SceneTextDiagnostic>& diagnostics)
{
    if (!node.is_number())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a number.", &node);
        return std::nullopt;
    }
    const std::optional<double> value = node.value<double>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Number cannot be represented as a double.", &node);
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
    if (xNode == nullptr || yNode == nullptr) return false;
    const std::optional<float> x = ReadFiniteFloat(*xNode, std::string{path} + "[0]", diagnostics);
    const std::optional<float> y = ReadFiniteFloat(*yNode, std::string{path} + "[1]", diagnostics);
    if (!x.has_value() || !y.has_value()) return false;
    destination = Vector2{*x, *y};
    return true;
}

void ReadTransform(
    const toml::table& table,
    const std::string_view path,
    Transform2D& transform,
    std::vector<SceneTextDiagnostic>& diagnostics)
{
    ValidateKnownKeys(table, path, {"position", "rotation_radians", "scale"}, diagnostics);
    if (const toml::node* positionNode = table.get("position"); positionNode != nullptr)
        ReadVector2(*positionNode, std::string{path} + ".position", transform.position, diagnostics);
    if (const toml::node* rotationNode = table.get("rotation_radians"); rotationNode != nullptr)
    {
        const std::optional<float> rotation = ReadFiniteFloat(*rotationNode, std::string{path} + ".rotation_radians", diagnostics);
        if (rotation.has_value()) transform.rotationRadians = *rotation;
    }
    if (const toml::node* scaleNode = table.get("scale"); scaleNode != nullptr)
        ReadVector2(*scaleNode, std::string{path} + ".scale", transform.scale, diagnostics);
}

std::vector<std::string> ReadTags(
    const toml::table& table,
    const std::string_view path,
    std::vector<SceneTextDiagnostic>& diagnostics)
{
    std::vector<std::string> tags{};
    const toml::node* node = table.get("tags");
    if (node == nullptr) return tags;
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
        if (tagNode == nullptr) continue;
        const std::optional<std::string> tag = tagNode->value<std::string>();
        if (!tag.has_value())
        {
            AddDiagnostic(diagnostics, std::string{path} + "[" + std::to_string(index) + "]", "Expected a string tag.", tagNode);
            continue;
        }
        if (tag->empty())
        {
            AddDiagnostic(diagnostics, std::string{path} + "[" + std::to_string(index) + "]", "Tag must not be empty.", tagNode);
            continue;
        }
        tags.push_back(*tag);
    }
    return tags;
}

std::optional<SemanticValue> ReadSemanticValue(
    const toml::node& node,
    const std::string_view path,
    std::vector<SceneTextDiagnostic>& diagnostics)
{
    SemanticValue value{};
    if (node.is_boolean())
    {
        const std::optional<bool> boolean = node.value<bool>();
        if (!boolean.has_value())
        {
            AddDiagnostic(diagnostics, std::string{path}, "Boolean value could not be read.", &node);
            return std::nullopt;
        }
        value.kind = SemanticValueKind::Boolean;
        value.booleanValue = *boolean;
        return value;
    }
    if (node.is_integer())
    {
        const std::optional<std::int64_t> integer = node.value<std::int64_t>();
        if (!integer.has_value())
        {
            AddDiagnostic(diagnostics, std::string{path}, "Integer value is not representable as int64.", &node);
            return std::nullopt;
        }
        value.kind = SemanticValueKind::SignedInteger;
        value.signedIntegerValue = *integer;
        return value;
    }
    if (node.is_floating_point())
    {
        const std::optional<double> number = node.value<double>();
        if (!number.has_value() || !std::isfinite(*number))
        {
            AddDiagnostic(diagnostics, std::string{path}, "Floating value must be finite.", &node);
            return std::nullopt;
        }
        value.kind = SemanticValueKind::Float;
        value.floatValue = *number;
        return value;
    }
    if (node.is_string())
    {
        const std::optional<std::string> text = node.value<std::string>();
        if (!text.has_value())
        {
            AddDiagnostic(diagnostics, std::string{path}, "String value could not be read.", &node);
            return std::nullopt;
        }
        value.kind = SemanticValueKind::Text;
        value.textValue = *text;
        return value;
    }
    if (const toml::array* array = node.as_array(); array != nullptr)
    {
        if (array->size() != 2U && array->size() != 4U)
        {
            AddDiagnostic(diagnostics, std::string{path}, "Semantic numeric arrays must contain exactly 2 or 4 values.", &node);
            return std::nullopt;
        }
        for (std::size_t index = 0; index < array->size(); ++index)
        {
            const toml::node* element = array->get(index);
            if (element == nullptr || !element->is_number())
            {
                AddDiagnostic(diagnostics, std::string{path} + "[" + std::to_string(index) + "]", "Expected a number.", element);
                return std::nullopt;
            }
            const std::optional<double> number = element->value<double>();
            if (!number.has_value() || !std::isfinite(*number))
            {
                AddDiagnostic(diagnostics, std::string{path} + "[" + std::to_string(index) + "]", "Number must be finite.", element);
                return std::nullopt;
            }
            value.vectorValue[index] = *number;
        }
        value.kind = array->size() == 2U ? SemanticValueKind::Float2 : SemanticValueKind::Float4;
        return value;
    }
    AddDiagnostic(diagnostics, std::string{path}, "Unsupported component field type. Expected bool, integer, float, text, float2, or float4.", &node);
    return std::nullopt;
}

ComponentAuthoringObject ReadComponentData(
    const toml::table& table,
    const std::string_view path,
    std::vector<SceneTextDiagnostic>& diagnostics)
{
    ComponentAuthoringObject object{};
    object.fields.reserve(table.size());
    for (const auto& [key, node] : table)
    {
        const std::string fieldName{key.str()};
        const std::string fieldPath = std::string{path} + "." + fieldName;
        const std::optional<SemanticValue> value = ReadSemanticValue(node, fieldPath, diagnostics);
        if (value.has_value()) object.fields.push_back(ComponentAuthoringField{fieldName, *value});
    }
    std::sort(object.fields.begin(), object.fields.end(), [](const auto& left, const auto& right) { return left.name < right.name; });
    return object;
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
            else escaped.push_back(static_cast<char>(character));
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
    if (text.find_first_of(".eE") == std::string::npos) text.append(".0");
    return text;
}

std::string FormatDouble(const double value)
{
    std::ostringstream stream{};
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    std::string text = stream.str();
    if (text.find_first_of(".eE") == std::string::npos) text.append(".0");
    return text;
}

bool IsFiniteTransform(const Transform2D& transform) noexcept
{
    return std::isfinite(transform.position.x) && std::isfinite(transform.position.y) &&
           std::isfinite(transform.rotationRadians) && std::isfinite(transform.scale.x) &&
           std::isfinite(transform.scale.y);
}

std::optional<std::string> SerializeSemanticValue(const SemanticValue& value)
{
    switch (value.kind)
    {
    case SemanticValueKind::Boolean:
        return value.booleanValue ? "true" : "false";
    case SemanticValueKind::SignedInteger:
        return std::to_string(value.signedIntegerValue);
    case SemanticValueKind::UnsignedInteger:
        if (value.unsignedIntegerValue > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return std::nullopt;
        return std::to_string(value.unsignedIntegerValue);
    case SemanticValueKind::Float:
        if (!std::isfinite(value.floatValue)) return std::nullopt;
        return FormatDouble(value.floatValue);
    case SemanticValueKind::Text:
    case SemanticValueKind::EntityReference:
    case SemanticValueKind::ResourceReference:
    case SemanticValueKind::EnumName:
        return EscapeTomlString(value.textValue);
    case SemanticValueKind::Float2:
        if (!std::isfinite(value.vectorValue[0]) || !std::isfinite(value.vectorValue[1])) return std::nullopt;
        return "[" + FormatDouble(value.vectorValue[0]) + ", " + FormatDouble(value.vectorValue[1]) + "]";
    case SemanticValueKind::Float4:
        for (double element : value.vectorValue) if (!std::isfinite(element)) return std::nullopt;
        return "[" + FormatDouble(value.vectorValue[0]) + ", " + FormatDouble(value.vectorValue[1]) + ", " +
               FormatDouble(value.vectorValue[2]) + ", " + FormatDouble(value.vectorValue[3]) + "]";
    }
    return std::nullopt;
}

SceneLoadResult LoadSceneTomlImpl(
    const std::string_view text,
    const ComponentRegistry* registry,
    const std::string_view sourceName)
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

    std::int64_t formatVersion = 0;
    const toml::node* formatNode = root.get("format_version");
    if (formatNode == nullptr)
        AddDiagnostic(result.diagnostics, "format_version", "Required field is missing.");
    else if (!formatNode->is_integer())
        AddDiagnostic(result.diagnostics, "format_version", "Expected an integer.", formatNode);
    else
    {
        const std::optional<std::int64_t> version = formatNode->value<std::int64_t>();
        if (!version.has_value()) AddDiagnostic(result.diagnostics, "format_version", "Integer value is not representable.", formatNode);
        else if (*version < MinimumSceneFormatVersion || *version > CurrentSceneFormatVersion)
            AddDiagnostic(result.diagnostics, "format_version", "Unsupported scene format version. Expected 1 or 2.", formatNode);
        else formatVersion = *version;
    }

    if (registry != nullptr && !registry->IsFrozen())
    {
        AddDiagnostic(result.diagnostics, "$", "Component registry must be frozen before authored scene loading.");
        return result;
    }

    SceneMetadata metadata{};
    const toml::node* sceneNode = root.get("scene");
    if (sceneNode == nullptr)
        AddDiagnostic(result.diagnostics, "scene", "Required table is missing.");
    else if (const toml::table* sceneTable = sceneNode->as_table(); sceneTable != nullptr)
    {
        ValidateKnownKeys(*sceneTable, "scene", {"id", "name"}, result.diagnostics);
        const std::optional<std::string> semanticId = ReadRequiredString(*sceneTable, "id", "scene.id", result.diagnostics, true);
        const std::optional<std::string> name = ReadOptionalString(*sceneTable, "name", "scene.name", result.diagnostics);
        if (semanticId.has_value()) metadata.semanticId = *semanticId;
        if (name.has_value()) metadata.name = *name;
    }
    else AddDiagnostic(result.diagnostics, "scene", "Expected a table.", sceneNode);

    Scene scene = registry == nullptr ? Scene{std::move(metadata)} : Scene{*registry, std::move(metadata)};
    std::vector<PendingEntity> pending{};
    const toml::node* entitiesNode = root.get("entities");
    if (entitiesNode != nullptr)
    {
        const toml::array* entities = entitiesNode->as_array();
        if (entities == nullptr)
            AddDiagnostic(result.diagnostics, "entities", "Expected an array of entity tables.", entitiesNode);
        else
        {
            pending.reserve(entities->size());
            for (std::size_t index = 0; index < entities->size(); ++index)
            {
                const toml::node* entityNode = entities->get(index);
                const std::string entityPath = "entities[" + std::to_string(index) + "]";
                if (entityNode == nullptr) continue;
                const toml::table* entityTable = entityNode->as_table();
                if (entityTable == nullptr)
                {
                    AddDiagnostic(result.diagnostics, entityPath, "Expected an entity table.", entityNode);
                    continue;
                }
                if (formatVersion >= 2)
                    ValidateKnownKeys(*entityTable, entityPath, {"id", "name", "tags", "parent", "transform", "components"}, result.diagnostics);
                else
                    ValidateKnownKeys(*entityTable, entityPath, {"id", "name", "tags", "transform"}, result.diagnostics);

                EntityDescriptor descriptor{};
                const std::optional<std::string> semanticId = ReadRequiredString(*entityTable, "id", entityPath + ".id", result.diagnostics, true);
                const std::optional<std::string> name = ReadOptionalString(*entityTable, "name", entityPath + ".name", result.diagnostics);
                if (semanticId.has_value()) descriptor.semanticId = *semanticId;
                if (name.has_value()) descriptor.name = *name;
                descriptor.tags = ReadTags(*entityTable, entityPath + ".tags", result.diagnostics);
                if (const toml::node* transformNode = entityTable->get("transform"); transformNode != nullptr)
                {
                    if (const toml::table* transformTable = transformNode->as_table(); transformTable != nullptr)
                        ReadTransform(*transformTable, entityPath + ".transform", descriptor.transform, result.diagnostics);
                    else AddDiagnostic(result.diagnostics, entityPath + ".transform", "Expected a table.", transformNode);
                }
                if (!semanticId.has_value()) continue;

                PendingEntity entry{};
                entry.table = entityTable;
                entry.path = entityPath;
                if (formatVersion >= 2)
                {
                    const std::optional<std::string> parent = ReadOptionalString(*entityTable, "parent", entityPath + ".parent", result.diagnostics, true);
                    if (parent.has_value()) entry.parentSemanticId = *parent;
                }
                try
                {
                    entry.id = scene.CreateEntity(std::move(descriptor));
                    pending.push_back(std::move(entry));
                }
                catch (const std::invalid_argument&)
                {
                    AddDiagnostic(result.diagnostics, entityPath + ".id", "Duplicate entity semantic ID '" + *semanticId + "'.", entityTable->get("id"));
                }
            }
        }
    }

    if (formatVersion >= 2)
    {
        for (const PendingEntity& entry : pending)
        {
            const toml::node* componentsNode = entry.table->get("components");
            if (componentsNode == nullptr) continue;
            const toml::array* components = componentsNode->as_array();
            if (components == nullptr)
            {
                AddDiagnostic(result.diagnostics, entry.path + ".components", "Expected an array of component tables.", componentsNode);
                continue;
            }
            if (registry == nullptr)
            {
                AddDiagnostic(result.diagnostics, entry.path + ".components", "Authored components require a frozen ComponentRegistry.", componentsNode);
                continue;
            }

            for (std::size_t componentIndex = 0; componentIndex < components->size(); ++componentIndex)
            {
                const toml::node* componentNode = components->get(componentIndex);
                const std::string componentPath = entry.path + ".components[" + std::to_string(componentIndex) + "]";
                if (componentNode == nullptr) continue;
                const toml::table* componentTable = componentNode->as_table();
                if (componentTable == nullptr)
                {
                    AddDiagnostic(result.diagnostics, componentPath, "Expected a component table.", componentNode);
                    continue;
                }
                ValidateKnownKeys(*componentTable, componentPath, {"type", "version", "data"}, result.diagnostics);
                const std::optional<std::string> typeId = ReadRequiredString(*componentTable, "type", componentPath + ".type", result.diagnostics, true);

                std::optional<std::uint32_t> schemaVersion{};
                const toml::node* versionNode = componentTable->get("version");
                if (versionNode == nullptr) AddDiagnostic(result.diagnostics, componentPath + ".version", "Required field is missing.");
                else if (!versionNode->is_integer()) AddDiagnostic(result.diagnostics, componentPath + ".version", "Expected an integer.", versionNode);
                else
                {
                    const std::optional<std::int64_t> version = versionNode->value<std::int64_t>();
                    if (!version.has_value() || *version <= 0 || *version > std::numeric_limits<std::uint32_t>::max())
                        AddDiagnostic(result.diagnostics, componentPath + ".version", "Component schema version must be a positive uint32 value.", versionNode);
                    else schemaVersion = static_cast<std::uint32_t>(*version);
                }

                ComponentAuthoringObject authored{};
                const toml::node* dataNode = componentTable->get("data");
                if (dataNode == nullptr) AddDiagnostic(result.diagnostics, componentPath + ".data", "Required table is missing.");
                else if (const toml::table* dataTable = dataNode->as_table(); dataTable != nullptr)
                    authored = ReadComponentData(*dataTable, componentPath + ".data", result.diagnostics);
                else AddDiagnostic(result.diagnostics, componentPath + ".data", "Expected a table.", dataNode);

                if (!typeId.has_value() || !schemaVersion.has_value() || dataNode == nullptr || dataNode->as_table() == nullptr) continue;
                std::string error{};
                const ComponentAttachResult attach = scene.AddAuthoredComponent(entry.id, *typeId, *schemaVersion, authored, error);
                if (attach != ComponentAttachResult::Success)
                {
                    std::string message = "Could not attach component '" + *typeId + "': " + std::string{ToString(attach)} + ".";
                    if (!error.empty()) message.append(" ").append(error);
                    AddDiagnostic(result.diagnostics, componentPath, std::move(message), componentNode);
                }
            }
        }

        for (const PendingEntity& entry : pending)
        {
            if (entry.parentSemanticId.empty()) continue;
            const std::optional<EntityId> parent = scene.FindBySemanticId(entry.parentSemanticId);
            if (!parent.has_value())
            {
                AddDiagnostic(result.diagnostics, entry.path + ".parent", "Parent entity '" + entry.parentSemanticId + "' was not found.", entry.table->get("parent"));
                continue;
            }
            const HierarchyResult hierarchy = scene.SetParent(entry.id, *parent, ReparentMode::KeepLocal);
            if (hierarchy != HierarchyResult::Success)
                AddDiagnostic(result.diagnostics, entry.path + ".parent", "Invalid parent relationship: " + std::string{ToString(hierarchy)} + ".", entry.table->get("parent"));
        }
    }

    if (result.diagnostics.empty()) result.scene = std::move(scene);
    return result;
}
} // namespace

SceneLoadResult LoadSceneToml(const std::string_view text, const std::string_view sourceName)
{
    return LoadSceneTomlImpl(text, nullptr, sourceName);
}

SceneLoadResult LoadSceneToml(
    const std::string_view text,
    const ComponentRegistry& registry,
    const std::string_view sourceName)
{
    return LoadSceneTomlImpl(text, &registry, sourceName);
}

SceneSaveResult SaveSceneToml(const Scene& scene)
{
    SceneSaveResult result{};
    if (scene.Metadata().semanticId.empty()) AddDiagnostic(result.diagnostics, "scene.id", "Scene semantic ID must not be empty.");

    std::vector<SaveEntity> entities{};
    entities.reserve(scene.EntityCount());
    scene.ForEachEntity([&](const EntityId id, const Entity& entity)
    {
        if (entity.SemanticId().empty())
        {
            AddDiagnostic(result.diagnostics, "entities[index=" + std::to_string(id.index) + "].id", "Runtime-only entity without a semantic ID cannot be serialized as authored scene data.");
            return;
        }
        if (!IsFiniteTransform(entity.LocalTransform()))
        {
            AddDiagnostic(result.diagnostics, "entities[id=" + std::string{entity.SemanticId()} + "].transform", "Transform values must be finite before serialization.");
            return;
        }
        if (entity.Parent().has_value())
        {
            const Entity* parent = scene.TryGet(*entity.Parent());
            if (parent == nullptr || parent->SemanticId().empty())
            {
                AddDiagnostic(result.diagnostics, "entities[id=" + std::string{entity.SemanticId()} + "].parent", "Authored hierarchy parent must have a stable semantic ID.");
                return;
            }
        }
        std::string componentError{};
        std::vector<AuthoredComponentSnapshot> components = scene.SerializeAuthoredComponents(id, componentError);
        if (!componentError.empty())
        {
            AddDiagnostic(result.diagnostics, "entities[id=" + std::string{entity.SemanticId()} + "].components", componentError);
            return;
        }
        entities.push_back(SaveEntity{.id = id, .entity = &entity, .components = std::move(components)});
    });

    if (!result.diagnostics.empty()) return result;
    std::sort(entities.begin(), entities.end(), [](const SaveEntity& left, const SaveEntity& right)
    {
        return left.entity->SemanticId() < right.entity->SemanticId();
    });

    std::ostringstream output{};
    output.imbue(std::locale::classic());
    output << "format_version = " << CurrentSceneFormatVersion << "\n\n";
    output << "[scene]\n";
    output << "id = " << EscapeTomlString(scene.Metadata().semanticId) << "\n";
    output << "name = " << EscapeTomlString(scene.Metadata().name) << "\n";

    for (const SaveEntity& saveEntity : entities)
    {
        const Entity& entity = *saveEntity.entity;
        const Transform2D& transform = entity.LocalTransform();
        output << "\n[[entities]]\n";
        output << "id = " << EscapeTomlString(entity.SemanticId()) << "\n";
        output << "name = " << EscapeTomlString(entity.Name()) << "\n";
        output << "tags = [";
        const std::vector<std::string>& tags = entity.Tags();
        for (std::size_t index = 0; index < tags.size(); ++index)
        {
            if (index != 0U) output << ", ";
            output << EscapeTomlString(tags[index]);
        }
        output << "]\n";
        if (entity.Parent().has_value())
        {
            const Entity* parent = scene.TryGet(*entity.Parent());
            output << "parent = " << EscapeTomlString(parent->SemanticId()) << "\n";
        }
        output << "\n[entities.transform]\n";
        output << "position = [" << FormatFloat(transform.position.x) << ", " << FormatFloat(transform.position.y) << "]\n";
        output << "rotation_radians = " << FormatFloat(transform.rotationRadians) << "\n";
        output << "scale = [" << FormatFloat(transform.scale.x) << ", " << FormatFloat(transform.scale.y) << "]\n";

        for (const AuthoredComponentSnapshot& component : saveEntity.components)
        {
            output << "\n[[entities.components]]\n";
            output << "type = " << EscapeTomlString(component.typeId) << "\n";
            output << "version = " << component.schemaVersion << "\n\n";
            output << "[entities.components.data]\n";
            for (const ComponentAuthoringField& field : component.data.fields)
            {
                const std::optional<std::string> encoded = SerializeSemanticValue(field.value);
                if (!encoded.has_value())
                {
                    AddDiagnostic(result.diagnostics,
                        "entities[id=" + std::string{entity.SemanticId()} + "].components[type=" + component.typeId + "].data." + field.name,
                        "Component field cannot be represented in canonical TOML.");
                    return result;
                }
                output << EscapeTomlString(field.name) << " = " << *encoded << "\n";
            }
        }
    }

    result.text = output.str();
    return result;
}
} // namespace trace2d::scene
