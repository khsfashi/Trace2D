#include <trace2d/ui/UiText.hpp>

#include <trace2d/ui/UiLayout.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace trace2d::ui
{
namespace
{
constexpr std::int64_t UiFormatVersion = 1;

struct ParsedUiElement final
{
    std::size_t sourceIndex{0U};
    UiElement element{};
    UiLayoutNodeSpec layout{};
};

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

[[nodiscard]] bool IsKnownKey(
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

[[nodiscard]] std::optional<std::string> ReadRequiredString(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const toml::node* const node = table.get(key);
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

[[nodiscard]] std::optional<std::string> ReadOptionalString(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const toml::node* const node = table.get(key);
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

[[nodiscard]] std::optional<std::string> ReadOptionalParent(
    const toml::table& table,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const toml::node* const node = table.get("parent");
    if (node == nullptr)
    {
        return std::string{};
    }

    std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a string.", node);
        return std::nullopt;
    }
    if (value->empty())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Parent ID must not be empty when present.", node);
        return std::nullopt;
    }
    return value;
}

[[nodiscard]] std::optional<std::uint32_t> ReadUInt32(
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

[[nodiscard]] std::optional<std::int32_t> ReadInt32(
    const toml::node& node,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    if (!node.is_integer())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected an integer.", &node);
        return std::nullopt;
    }

    const std::optional<std::int64_t> value = node.value<std::int64_t>();
    if (!value.has_value() ||
        *value < static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()) ||
        *value > static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()))
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a signed 32-bit integer.", &node);
        return std::nullopt;
    }
    return static_cast<std::int32_t>(*value);
}

[[nodiscard]] std::optional<std::uint32_t> ReadRequiredUInt32(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const toml::node* const node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }
    return ReadUInt32(*node, path, diagnostics, true);
}

[[nodiscard]] std::optional<bool> ReadOptionalBool(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const toml::node* const node = table.get(key);
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

[[nodiscard]] std::optional<UiElementKind> ReadKind(
    const toml::table& table,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const toml::node* const node = table.get("kind");
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

[[nodiscard]] std::optional<UiLayoutPlacementMode> ReadPlacementMode(
    const toml::table& table,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const toml::node* const node = table.get("placement");
    if (node == nullptr)
    {
        return UiLayoutPlacementMode::Absolute;
    }

    const std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a string.", node);
        return std::nullopt;
    }
    if (*value == "absolute")
    {
        return UiLayoutPlacementMode::Absolute;
    }
    if (*value == "anchored_fixed")
    {
        return UiLayoutPlacementMode::AnchoredFixed;
    }

    AddDiagnostic(
        diagnostics,
        std::string{path},
        "Unsupported placement. Expected absolute or anchored_fixed.",
        node);
    return std::nullopt;
}

[[nodiscard]] std::optional<UiRect> ReadBounds(
    const toml::table& table,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const toml::node* const node = table.get("bounds");
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    const toml::array* const array = node->as_array();
    if (array == nullptr || array->size() != 4U)
    {
        AddDiagnostic(
            diagnostics,
            std::string{path},
            "Expected [x, y, width, height] with exactly four integers.",
            node);
        return std::nullopt;
    }

    const toml::node* const xNode = array->get(0U);
    const toml::node* const yNode = array->get(1U);
    const toml::node* const widthNode = array->get(2U);
    const toml::node* const heightNode = array->get(3U);
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

[[nodiscard]] std::optional<std::array<std::uint32_t, 2U>> ReadUInt32Pair(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics,
    const bool requirePositive)
{
    const toml::node* const node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    const toml::array* const array = node->as_array();
    if (array == nullptr || array->size() != 2U)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected exactly two integers.", node);
        return std::nullopt;
    }

    const toml::node* const firstNode = array->get(0U);
    const toml::node* const secondNode = array->get(1U);
    if (firstNode == nullptr || secondNode == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Invalid two-integer array.", node);
        return std::nullopt;
    }

    const std::optional<std::uint32_t> first =
        ReadUInt32(*firstNode, std::string{path} + "[0]", diagnostics, requirePositive);
    const std::optional<std::uint32_t> second =
        ReadUInt32(*secondNode, std::string{path} + "[1]", diagnostics, requirePositive);
    if (!first.has_value() || !second.has_value())
    {
        return std::nullopt;
    }
    return std::array<std::uint32_t, 2U>{*first, *second};
}

[[nodiscard]] std::optional<std::array<std::int32_t, 2U>> ReadInt32Pair(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const toml::node* const node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    const toml::array* const array = node->as_array();
    if (array == nullptr || array->size() != 2U)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected exactly two signed integers.", node);
        return std::nullopt;
    }

    const toml::node* const firstNode = array->get(0U);
    const toml::node* const secondNode = array->get(1U);
    if (firstNode == nullptr || secondNode == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Invalid two-integer array.", node);
        return std::nullopt;
    }

    const std::optional<std::int32_t> first =
        ReadInt32(*firstNode, std::string{path} + "[0]", diagnostics);
    const std::optional<std::int32_t> second =
        ReadInt32(*secondNode, std::string{path} + "[1]", diagnostics);
    if (!first.has_value() || !second.has_value())
    {
        return std::nullopt;
    }
    return std::array<std::int32_t, 2U>{*first, *second};
}

[[nodiscard]] std::optional<UiNormalizedPoint> ReadNormalizedPoint(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const std::optional<std::array<std::uint32_t, 2U>> value =
        ReadUInt32Pair(table, key, path, diagnostics, false);
    if (!value.has_value())
    {
        return std::nullopt;
    }
    if ((*value)[0] > UiNormalizedUnit || (*value)[1] > UiNormalizedUnit)
    {
        AddDiagnostic(
            diagnostics,
            std::string{path},
            "Normalized coordinates must be in the inclusive range 0..1024.",
            table.get(key));
        return std::nullopt;
    }

    return UiNormalizedPoint{
        static_cast<std::uint16_t>((*value)[0]),
        static_cast<std::uint16_t>((*value)[1]),
    };
}

[[nodiscard]] std::optional<UiAnchoredPlacement> ReadAnchoredPlacement(
    const toml::table& table,
    const std::string_view elementPath,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    const std::optional<UiNormalizedPoint> anchor =
        ReadNormalizedPoint(table, "anchor", std::string{elementPath} + ".anchor", diagnostics);
    const std::optional<UiNormalizedPoint> pivot =
        ReadNormalizedPoint(table, "pivot", std::string{elementPath} + ".pivot", diagnostics);
    const std::optional<std::array<std::int32_t, 2U>> offset =
        ReadInt32Pair(table, "offset", std::string{elementPath} + ".offset", diagnostics);
    const std::optional<std::array<std::uint32_t, 2U>> size =
        ReadUInt32Pair(table, "size", std::string{elementPath} + ".size", diagnostics, true);

    if (!anchor.has_value() || !pivot.has_value() || !offset.has_value() || !size.has_value())
    {
        return std::nullopt;
    }

    return UiAnchoredPlacement{
        .anchor = *anchor,
        .pivot = *pivot,
        .offsetX = (*offset)[0],
        .offsetY = (*offset)[1],
        .width = (*size)[0],
        .height = (*size)[1],
    };
}

[[nodiscard]] bool HasAnchoredFields(const toml::table& table) noexcept
{
    return table.get("anchor") != nullptr || table.get("pivot") != nullptr ||
           table.get("offset") != nullptr || table.get("size") != nullptr;
}

[[nodiscard]] bool FitsCanvas(
    const UiRect& bounds,
    const std::uint32_t width,
    const std::uint32_t height) noexcept
{
    if (bounds.width == 0U || bounds.height == 0U || bounds.x >= width || bounds.y >= height)
    {
        return false;
    }
    return bounds.width <= width - bounds.x && bounds.height <= height - bounds.y;
}

void ValidateDuplicateIds(
    const std::vector<ParsedUiElement>& parsed,
    std::vector<UiTextDiagnostic>& diagnostics)
{
    std::vector<std::size_t> order(parsed.size());
    std::iota(order.begin(), order.end(), std::size_t{0U});
    std::sort(
        order.begin(),
        order.end(),
        [&parsed](const std::size_t lhs, const std::size_t rhs)
        {
            if (parsed[lhs].element.id != parsed[rhs].element.id)
            {
                return parsed[lhs].element.id < parsed[rhs].element.id;
            }
            return parsed[lhs].sourceIndex < parsed[rhs].sourceIndex;
        });

    for (std::size_t index = 1U; index < order.size(); ++index)
    {
        const ParsedUiElement& previous = parsed[order[index - 1U]];
        const ParsedUiElement& current = parsed[order[index]];
        if (previous.element.id == current.element.id)
        {
            AddDiagnostic(
                diagnostics,
                "elements[" + std::to_string(current.sourceIndex) + "].id",
                "Duplicate UI element ID '" + current.element.id + "'.");
        }
    }
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

    const toml::node* const formatNode = root.get("format_version");
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
    const toml::node* const canvasNode = root.get("canvas");
    if (canvasNode == nullptr)
    {
        AddDiagnostic(result.diagnostics, "canvas", "Required table is missing.");
    }
    else if (const toml::table* const canvasTable = canvasNode->as_table(); canvasTable != nullptr)
    {
        ValidateKnownKeys(*canvasTable, "canvas", {"width", "height"}, result.diagnostics);
        canvasWidth = ReadRequiredUInt32(*canvasTable, "width", "canvas.width", result.diagnostics);
        canvasHeight = ReadRequiredUInt32(*canvasTable, "height", "canvas.height", result.diagnostics);

        if (canvasWidth.has_value() && *canvasWidth > MaxUiCanvasDimension)
        {
            AddDiagnostic(
                result.diagnostics,
                "canvas.width",
                "UI canvas width exceeds the supported maximum of 4096.",
                canvasTable->get("width"));
        }
        if (canvasHeight.has_value() && *canvasHeight > MaxUiCanvasDimension)
        {
            AddDiagnostic(
                result.diagnostics,
                "canvas.height",
                "UI canvas height exceeds the supported maximum of 4096.",
                canvasTable->get("height"));
        }
    }
    else
    {
        AddDiagnostic(result.diagnostics, "canvas", "Expected a table.", canvasNode);
    }

    std::vector<ParsedUiElement> parsed{};
    const toml::node* const elementsNode = root.get("elements");
    if (elementsNode != nullptr)
    {
        const toml::array* const elements = elementsNode->as_array();
        if (elements == nullptr)
        {
            AddDiagnostic(result.diagnostics, "elements", "Expected an array of UI element tables.", elementsNode);
        }
        else
        {
            parsed.reserve(elements->size());
            for (std::size_t index = 0U; index < elements->size(); ++index)
            {
                const toml::node* const elementNode = elements->get(index);
                const std::string elementPath = "elements[" + std::to_string(index) + "]";
                if (elementNode == nullptr)
                {
                    continue;
                }

                const toml::table* const elementTable = elementNode->as_table();
                if (elementTable == nullptr)
                {
                    AddDiagnostic(result.diagnostics, elementPath, "Expected a UI element table.", elementNode);
                    continue;
                }

                ValidateKnownKeys(
                    *elementTable,
                    elementPath,
                    {"id", "kind", "parent", "placement", "bounds", "anchor", "pivot", "offset", "size",
                     "name", "text", "visible", "enabled"},
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
                const std::optional<std::string> parent = ReadOptionalParent(
                    *elementTable,
                    elementPath + ".parent",
                    result.diagnostics);
                const std::optional<UiLayoutPlacementMode> placement = ReadPlacementMode(
                    *elementTable,
                    elementPath + ".placement",
                    result.diagnostics);
                const std::optional<std::string> elementName = ReadOptionalString(
                    *elementTable,
                    "name",
                    elementPath + ".name",
                    result.diagnostics);
                const std::optional<std::string> elementText = ReadOptionalString(
                    *elementTable,
                    "text",
                    elementPath + ".text",
                    result.diagnostics);
                const std::optional<bool> visible = ReadOptionalBool(
                    *elementTable,
                    "visible",
                    elementPath + ".visible",
                    result.diagnostics);
                const std::optional<bool> enabled = ReadOptionalBool(
                    *elementTable,
                    "enabled",
                    elementPath + ".enabled",
                    result.diagnostics);

                std::optional<UiRect> absoluteBounds{};
                std::optional<UiAnchoredPlacement> anchored{};
                if (placement.has_value() && *placement == UiLayoutPlacementMode::Absolute)
                {
                    absoluteBounds = ReadBounds(
                        *elementTable,
                        elementPath + ".bounds",
                        result.diagnostics);
                    if (HasAnchoredFields(*elementTable))
                    {
                        AddDiagnostic(
                            result.diagnostics,
                            elementPath + ".placement",
                            "anchor, pivot, offset, and size require placement = \"anchored_fixed\".",
                            elementTable->get("placement"));
                    }
                }
                else if (placement.has_value() && *placement == UiLayoutPlacementMode::AnchoredFixed)
                {
                    if (elementTable->get("bounds") != nullptr)
                    {
                        AddDiagnostic(
                            result.diagnostics,
                            elementPath + ".bounds",
                            "bounds is incompatible with anchored_fixed placement; use anchor/pivot/offset/size.",
                            elementTable->get("bounds"));
                    }
                    anchored = ReadAnchoredPlacement(*elementTable, elementPath, result.diagnostics);
                }

                const bool hasGeometry = placement.has_value() &&
                    ((*placement == UiLayoutPlacementMode::Absolute && absoluteBounds.has_value()) ||
                     (*placement == UiLayoutPlacementMode::AnchoredFixed && anchored.has_value()));
                if (!id.has_value() || !kind.has_value() || !parent.has_value() || !placement.has_value() ||
                    !elementName.has_value() || !elementText.has_value() || !visible.has_value() ||
                    !enabled.has_value() || !hasGeometry)
                {
                    continue;
                }

                if (*placement == UiLayoutPlacementMode::Absolute && parent->empty() &&
                    canvasWidth.has_value() && canvasHeight.has_value() &&
                    !FitsCanvas(*absoluteBounds, *canvasWidth, *canvasHeight))
                {
                    AddDiagnostic(
                        result.diagnostics,
                        elementPath + ".bounds",
                        "Bounds must have positive size and remain inside the UI canvas.",
                        elementTable->get("bounds"));
                    continue;
                }

                ParsedUiElement authored{};
                authored.sourceIndex = index;
                authored.element.id = *id;
                authored.element.kind = *kind;
                authored.element.parentId = *parent;
                authored.element.name = elementName->empty() ? *elementText : *elementName;
                authored.element.text = *elementText;
                authored.element.visible = *visible;
                authored.element.enabled = *enabled;

                authored.layout.id = *id;
                authored.layout.parentId = *parent;
                authored.layout.placementMode = *placement;
                if (*placement == UiLayoutPlacementMode::Absolute)
                {
                    authored.layout.localBounds = *absoluteBounds;
                }
                else
                {
                    authored.layout.anchored = *anchored;
                }

                parsed.push_back(std::move(authored));
            }
        }
    }

    ValidateDuplicateIds(parsed, result.diagnostics);
    if (!result.diagnostics.empty() || !canvasWidth.has_value() || !canvasHeight.has_value())
    {
        return result;
    }

    UiLayoutTree layout{*canvasWidth, *canvasHeight};
    layout.ReserveNodes(parsed.size());
    for (ParsedUiElement& authored : parsed)
    {
        const UiLayoutResult addResult = layout.AddNode(std::move(authored.layout));
        if (addResult != UiLayoutResult::Success)
        {
            AddDiagnostic(
                result.diagnostics,
                "elements[" + std::to_string(authored.sourceIndex) + "]",
                "UI layout node could not be added: " + std::string{ToString(addResult)} + ".");
        }
    }
    if (!result.diagnostics.empty())
    {
        return result;
    }

    const UiLayoutResult finalizeResult = layout.Finalize();
    if (finalizeResult != UiLayoutResult::Success)
    {
        AddDiagnostic(
            result.diagnostics,
            "layout",
            "UI layout finalization failed: " + std::string{ToString(finalizeResult)} + ".");
        return result;
    }

    const std::span<const UiResolvedLayoutNode> resolvedNodes = layout.Nodes();
    if (resolvedNodes.size() != parsed.size())
    {
        AddDiagnostic(result.diagnostics, "layout", "Resolved UI layout node count changed unexpectedly.");
        return result;
    }

    UiDocument document{*canvasWidth, *canvasHeight};
    document.ReserveElements(parsed.size());
    for (std::size_t index = 0U; index < parsed.size(); ++index)
    {
        ParsedUiElement& authored = parsed[index];
        const UiResolvedLayoutNode& resolved = resolvedNodes[index];
        authored.element.parentId = resolved.parentId;
        authored.element.parentIndex = resolved.parentIndex == InvalidUiLayoutIndex
            ? InvalidUiElementIndex
            : resolved.parentIndex;
        authored.element.depth = resolved.depth;
        authored.element.localBounds = resolved.resolvedLocalBounds;
        authored.element.bounds = resolved.bounds;

        const UiActionResult addResult = document.AddElement(std::move(authored.element));
        if (addResult != UiActionResult::Success)
        {
            AddDiagnostic(
                result.diagnostics,
                "elements[" + std::to_string(authored.sourceIndex) + "]",
                "Resolved UI element could not be published: " + std::string{ToString(addResult)} + ".");
        }
    }

    if (result.diagnostics.empty() && document.HasValidSize())
    {
        result.document = std::move(document);
    }
    return result;
}
} // namespace trace2d::ui
