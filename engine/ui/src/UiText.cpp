#include <trace2d/ui/UiText.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace trace2d::ui
{
namespace
{
constexpr std::int64_t UiFormatVersion = 1;

void AddDiagnostic(
    std::vector<UiTextDiagnostic>& diagnostics,
    std::string path,
    std::string message,
    const toml::node* node = nullptr)
{
    UiTextDiagnostic diagnostic{};
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
    std::vector<UiTextDiagnostic>& diagnostics)
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
    std::vector<UiTextDiagnostic>& diagnostics)
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

    if (value->empty())
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
    std::vector<UiTextDiagnostic>& diagnostics)
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

std::optional<std::uint32_t> ReadUInt32(
    const toml::node& node,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics,
    const bool requirePositive)
{
    if (!node.is_integer())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected an integer.", &node);
        return std::nullopt;
    }

    const std::optional<std::int64_t> value = node.value<std::int64_t>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Integer value is not representable.", &node);
        return std::nullopt;
    }

    const std::int64_t minimum = requirePositive ? 1 : 0;
    const std::int64_t maximum = static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max());
    if (*value < minimum || *value > maximum)
    {
        AddDiagnostic(
            diagnostics,
            std::string{path},
            requirePositive ? "Expected a positive 32-bit unsigned integer."
                            : "Expected a non-negative 32-bit unsigned integer.",
            &node);
        return std::nullopt;
    }

    return static_cast<std::uint32_t>(*value);
}

std::optional<std::uint32_t> ReadRequiredUInt32(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    return ReadUInt32(*node, path, diagnostics, true);
}

std::optional<bool> ReadOptionalBool(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        return true;
    }

    const std::optional<bool> value = node->value<bool>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a boolean.", node);
    }
    return value;
}

std::optional<UiElementKind> ReadKind(
    const toml::table& table,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const toml::node* node = table.get("kind");
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    const std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a string.", node);
        return std::nullopt;
    }

    if (*value == "panel")
    {
        return UiElementKind::Panel;
    }
    if (*value == "label")
    {
        return UiElementKind::Label;
    }
    if (*value == "button")
    {
        return UiElementKind::Button;
    }
    if (*value == "text_input")
    {
        return UiElementKind::TextInput;
    }

    AddDiagnostic(
        diagnostics,
        std::string{path},
        "Unsupported UI kind. Expected panel, label, button, or text_input.",
        node);
    return std::nullopt;
}

std::optional<UiRect> ReadBounds(
    const toml::table& table,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const toml::node* node = table.get("bounds");
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    const toml::array* array = node->as_array();
    if (array == nullptr || array->size() != 4U)
    {
        AddDiagnostic(
            diagnostics,
            std::string{path},
            "Expected [x, y, width, height] with exactly four integers.",
            node);
        return std::nullopt;
    }

    const toml::node* xNode = array->get(0U);
    const toml::node* yNode = array->get(1U);
    const toml::node* widthNode = array->get(2U);
    const toml::node* heightNode = array->get(3U);
    if (xNode == nullptr || yNode == nullptr || widthNode == nullptr || heightNode == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Invalid bounds array.", node);
        return std::nullopt;
    }

    const std::optional<std::uint32_t> x =
        ReadUInt32(*xNode, std::string{path} + "[0]", diagnostics, false);
    const std::optional<std::uint32_t> y =
        ReadUInt32(*yNode, std::string{path} + "[1]", diagnostics, false);
    const std::optional<std::uint32_t> width =
        ReadUInt32(*widthNode, std::string{path} + "[2]", diagnostics, true);
    const std::optional<std::uint32_t> height =
        ReadUInt32(*heightNode, std::string{path} + "[3]", diagnostics, true);

    if (!x.has_value() || !y.has_value() || !width.has_value() || !height.has_value())
    {
        return std::nullopt;
    }

    return UiRect{*x, *y, *width, *height};
}
} // namespace

UiLoadResult LoadUiToml(const std::string_view text, const std::string_view sourceName)
{
    UiLoadResult result{};
    toml::table root{};

    try
    {
        root = toml::parse(text, sourceName);
    }
    catch (const toml::parse_error& error)
    {
        UiTextDiagnostic diagnostic{};
        diagnostic.path = "$";
        diagnostic.message = std::string{error.description()};
        diagnostic.line = static_cast<std::size_t>(error.source().begin.line);
        diagnostic.column = static_cast<std::size_t>(error.source().begin.column);
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }

    ValidateKnownKeys(root, "", {"format_version", "canvas", "elements"}, result.diagnostics);

    const toml::node* formatNode = root.get("format_version");
    if (formatNode == nullptr)
    {
        AddDiagnostic(result.diagnostics, "format_version", "Required field is missing.");
    }
    else if (!formatNode->is_integer())
    {
        AddDiagnostic(result.diagnostics, "format_version", "Expected an integer.", formatNode);
    }
    else
    {
        const std::optional<std::int64_t> version = formatNode->value<std::int64_t>();
        if (!version.has_value())
        {
            AddDiagnostic(result.diagnostics, "format_version", "Integer value is not representable.", formatNode);
        }
        else if (*version != UiFormatVersion)
        {
            AddDiagnostic(
                result.diagnostics,
                "format_version",
                "Unsupported UI format version. Expected 1.",
                formatNode);
        }
    }

    std::optional<std::uint32_t> canvasWidth{};
    std::optional<std::uint32_t> canvasHeight{};
    const toml::node* canvasNode = root.get("canvas");
    if (canvasNode == nullptr)
    {
        AddDiagnostic(result.diagnostics, "canvas", "Required table is missing.");
    }
    else if (const toml::table* canvasTable = canvasNode->as_table(); canvasTable != nullptr)
    {
        ValidateKnownKeys(*canvasTable, "canvas", {"width", "height"}, result.diagnostics);
        canvasWidth = ReadRequiredUInt32(*canvasTable, "width", "canvas.width", result.diagnostics);
        canvasHeight = ReadRequiredUInt32(*canvasTable, "height", "canvas.height", result.diagnostics);
    }
    else
    {
        AddDiagnostic(result.diagnostics, "canvas", "Expected a table.", canvasNode);
    }

    UiDocument document{canvasWidth.value_or(0U), canvasHeight.value_or(0U)};

    const toml::node* elementsNode = root.get("elements");
    if (elementsNode != nullptr)
    {
        const toml::array* elements = elementsNode->as_array();
        if (elements == nullptr)
        {
            AddDiagnostic(result.diagnostics, "elements", "Expected an array of UI element tables.", elementsNode);
        }
        else
        {
            document.ReserveElements(elements->size());
            for (std::size_t index = 0U; index < elements->size(); ++index)
            {
                const toml::node* elementNode = elements->get(index);
                const std::string elementPath = "elements[" + std::to_string(index) + "]";
                if (elementNode == nullptr)
                {
                    continue;
                }

                const toml::table* elementTable = elementNode->as_table();
                if (elementTable == nullptr)
                {
                    AddDiagnostic(result.diagnostics, elementPath, "Expected a UI element table.", elementNode);
                    continue;
                }

                ValidateKnownKeys(
                    *elementTable,
                    elementPath,
                    {"id", "kind", "bounds", "text", "enabled"},
                    result.diagnostics);

                const std::optional<std::string> id = ReadRequiredString(
                    *elementTable,
                    "id",
                    elementPath + ".id",
                    result.diagnostics);
                const std::optional<UiElementKind> kind = ReadKind(
                    *elementTable,
                    elementPath + ".kind",
                    result.diagnostics);
                const std::optional<UiRect> bounds = ReadBounds(
                    *elementTable,
                    elementPath + ".bounds",
                    result.diagnostics);
                const std::optional<std::string> elementText = ReadOptionalString(
                    *elementTable,
                    "text",
                    elementPath + ".text",
                    result.diagnostics);
                const std::optional<bool> enabled = ReadOptionalBool(
                    *elementTable,
                    "enabled",
                    elementPath + ".enabled",
                    result.diagnostics);

                if (!id.has_value() || !kind.has_value() || !bounds.has_value() ||
                    !elementText.has_value() || !enabled.has_value() ||
                    !canvasWidth.has_value() || !canvasHeight.has_value())
                {
                    continue;
                }

                UiElement element{};
                element.id = *id;
                element.kind = *kind;
                element.bounds = *bounds;
                element.text = *elementText;
                element.enabled = *enabled;

                const UiActionResult addResult = document.AddElement(std::move(element));
                if (addResult == UiActionResult::DuplicateId)
                {
                    AddDiagnostic(
                        result.diagnostics,
                        elementPath + ".id",
                        "Duplicate UI element ID '" + *id + "'.",
                        elementTable->get("id"));
                }
                else if (addResult == UiActionResult::InvalidBounds)
                {
                    AddDiagnostic(
                        result.diagnostics,
                        elementPath + ".bounds",
                        "Bounds must have positive size and remain inside the UI canvas.",
                        elementTable->get("bounds"));
                }
                else if (addResult != UiActionResult::Success)
                {
                    AddDiagnostic(
                        result.diagnostics,
                        elementPath,
                        "UI element could not be added: " + std::string{ToString(addResult)} + ".",
                        elementNode);
                }
            }
        }
    }

    if (result.diagnostics.empty() && document.HasValidSize())
    {
        result.document = std::move(document);
    }

    return result;
}
} // namespace trace2d::ui
