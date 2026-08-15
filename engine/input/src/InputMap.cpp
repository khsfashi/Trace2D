#include <trace2d/input/InputMap.hpp>

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
#include <utility>

namespace trace2d::input
{
namespace
{
constexpr std::uintmax_t MaximumInputMapSourceBytes = 1024U * 1024U;

struct NormalizedReference final
{
    std::string id{};
    std::filesystem::path resolvedPath{};
};

InputMapDiagnostic MakeDiagnostic(
    const InputMapErrorCode code,
    std::string path,
    std::string message,
    const std::string_view reference = {},
    const std::filesystem::path& resolvedPath = {},
    const toml::node* node = nullptr)
{
    InputMapDiagnostic diagnostic{};
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
    std::vector<InputMapDiagnostic>& diagnostics,
    const InputMapErrorCode code,
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
    std::vector<InputMapDiagnostic>& diagnostics)
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
            InputMapErrorCode::SchemaError,
            std::move(fieldPath),
            "Unknown field.",
            &value);
    }
}

std::optional<std::string> ReadRequiredString(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<InputMapDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, std::string{path}, "Expected a string.", node);
        return std::nullopt;
    }
    if (value->empty())
    {
        AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, std::string{path}, "Value must not be empty.", node);
        return std::nullopt;
    }
    return value;
}

std::optional<float> ReadOptionalFloat(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    const float fallback,
    std::vector<InputMapDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        return fallback;
    }

    double value = 0.0;
    if (const std::optional<double> floating = node->value<double>(); floating.has_value())
    {
        value = *floating;
    }
    else if (const std::optional<std::int64_t> integer = node->value<std::int64_t>(); integer.has_value())
    {
        value = static_cast<double>(*integer);
    }
    else
    {
        AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, std::string{path}, "Expected a number.", node);
        return std::nullopt;
    }

    if (!std::isfinite(value) || value < -static_cast<double>(std::numeric_limits<float>::max()) ||
        value > static_cast<double>(std::numeric_limits<float>::max()))
    {
        AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, std::string{path}, "Number is outside the finite float range.", node);
        return std::nullopt;
    }
    return static_cast<float>(value);
}

bool HasSemanticId(
    const InputMapDocument& document,
    const std::string_view semanticId)
{
    return std::any_of(
               document.buttonActions.begin(),
               document.buttonActions.end(),
               [semanticId](const InputMapButtonAction& action) { return action.semanticId == semanticId; }) ||
        std::any_of(
               document.axis1DActions.begin(),
               document.axis1DActions.end(),
               [semanticId](const InputMapAxis1DAction& action) { return action.semanticId == semanticId; });
}

void ValidateButtonActions(
    const toml::node* node,
    InputMapDocument& document,
    std::vector<InputMapDiagnostic>& diagnostics)
{
    if (node == nullptr)
    {
        return;
    }

    const toml::array* actions = node->as_array();
    if (actions == nullptr)
    {
        AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, "buttons", "Expected an array of button action tables.", node);
        return;
    }

    document.buttonActions.reserve(actions->size());
    for (std::size_t index = 0; index < actions->size(); ++index)
    {
        const toml::node* actionNode = actions->get(index);
        const std::string path = "buttons[" + std::to_string(index) + "]";
        if (actionNode == nullptr)
        {
            continue;
        }
        const toml::table* actionTable = actionNode->as_table();
        if (actionTable == nullptr)
        {
            AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, path, "Expected a button action table.", actionNode);
            continue;
        }

        ValidateKnownKeys(*actionTable, path, {"id", "controls"}, diagnostics);
        const std::optional<std::string> semanticId = ReadRequiredString(*actionTable, "id", path + ".id", diagnostics);
        const toml::node* controlsNode = actionTable->get("controls");
        if (controlsNode == nullptr)
        {
            AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, path + ".controls", "Required field is missing.");
            continue;
        }
        const toml::array* controlsArray = controlsNode->as_array();
        if (controlsArray == nullptr || controlsArray->empty())
        {
            AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, path + ".controls", "Expected a non-empty array of control names.", controlsNode);
            continue;
        }

        InputMapButtonAction action{};
        if (semanticId.has_value())
        {
            action.semanticId = *semanticId;
        }
        action.controls.reserve(controlsArray->size());
        for (std::size_t controlIndex = 0; controlIndex < controlsArray->size(); ++controlIndex)
        {
            const toml::node* controlNode = controlsArray->get(controlIndex);
            const std::string controlPath = path + ".controls[" + std::to_string(controlIndex) + "]";
            if (controlNode == nullptr)
            {
                continue;
            }
            const std::optional<std::string> controlName = controlNode->value<std::string>();
            if (!controlName.has_value())
            {
                AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, controlPath, "Expected a control name string.", controlNode);
                continue;
            }
            const std::optional<InputControl> control = ParseInputControl(*controlName);
            if (!control.has_value())
            {
                AddDiagnostic(
                    diagnostics,
                    InputMapErrorCode::SchemaError,
                    controlPath,
                    "Unknown input control '" + *controlName + "'.",
                    controlNode);
                continue;
            }
            if (std::find(action.controls.begin(), action.controls.end(), *control) != action.controls.end())
            {
                AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, controlPath, "Duplicate control binding.", controlNode);
                continue;
            }
            action.controls.push_back(*control);
        }

        if (!semanticId.has_value() || action.controls.empty())
        {
            continue;
        }
        if (HasSemanticId(document, action.semanticId))
        {
            AddDiagnostic(
                diagnostics,
                InputMapErrorCode::SchemaError,
                path + ".id",
                "Duplicate semantic input action ID '" + action.semanticId + "'.",
                actionTable->get("id"));
            continue;
        }
        document.buttonActions.push_back(std::move(action));
    }
}

void ValidateAxisActions(
    const toml::node* node,
    InputMapDocument& document,
    std::vector<InputMapDiagnostic>& diagnostics)
{
    if (node == nullptr)
    {
        return;
    }

    const toml::array* actions = node->as_array();
    if (actions == nullptr)
    {
        AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, "axes1d", "Expected an array of Axis1D action tables.", node);
        return;
    }

    document.axis1DActions.reserve(actions->size());
    for (std::size_t index = 0; index < actions->size(); ++index)
    {
        const toml::node* actionNode = actions->get(index);
        const std::string path = "axes1d[" + std::to_string(index) + "]";
        if (actionNode == nullptr)
        {
            continue;
        }
        const toml::table* actionTable = actionNode->as_table();
        if (actionTable == nullptr)
        {
            AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, path, "Expected an Axis1D action table.", actionNode);
            continue;
        }

        ValidateKnownKeys(*actionTable, path, {"id", "negative", "positive", "analog"}, diagnostics);
        const std::optional<std::string> semanticId = ReadRequiredString(*actionTable, "id", path + ".id", diagnostics);

        InputMapAxis1DAction action{};
        if (semanticId.has_value())
        {
            action.semanticId = *semanticId;
        }

        const toml::node* negativeNode = actionTable->get("negative");
        const toml::node* positiveNode = actionTable->get("positive");
        if ((negativeNode == nullptr) != (positiveNode == nullptr))
        {
            AddDiagnostic(
                diagnostics,
                InputMapErrorCode::SchemaError,
                path,
                "Digital Axis1D binding requires both 'negative' and 'positive'.",
                actionNode);
        }
        else if (negativeNode != nullptr && positiveNode != nullptr)
        {
            const std::optional<std::string> negativeName = negativeNode->value<std::string>();
            const std::optional<std::string> positiveName = positiveNode->value<std::string>();
            if (!negativeName.has_value())
            {
                AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, path + ".negative", "Expected a control name string.", negativeNode);
            }
            if (!positiveName.has_value())
            {
                AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, path + ".positive", "Expected a control name string.", positiveNode);
            }
            if (negativeName.has_value() && positiveName.has_value())
            {
                const std::optional<InputControl> negative = ParseInputControl(*negativeName);
                const std::optional<InputControl> positive = ParseInputControl(*positiveName);
                if (!negative.has_value())
                {
                    AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, path + ".negative", "Unknown input control '" + *negativeName + "'.", negativeNode);
                }
                if (!positive.has_value())
                {
                    AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, path + ".positive", "Unknown input control '" + *positiveName + "'.", positiveNode);
                }
                if (negative.has_value() && positive.has_value())
                {
                    if (*negative == *positive)
                    {
                        AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, path, "Digital Axis1D negative and positive controls must differ.", actionNode);
                    }
                    else
                    {
                        action.negative = *negative;
                        action.positive = *positive;
                        action.hasDigitalBinding = true;
                    }
                }
            }
        }

        const toml::node* analogNode = actionTable->get("analog");
        if (analogNode != nullptr)
        {
            const toml::array* analogArray = analogNode->as_array();
            if (analogArray == nullptr)
            {
                AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, path + ".analog", "Expected an array of analog binding tables.", analogNode);
            }
            else
            {
                action.analogBindings.reserve(analogArray->size());
                for (std::size_t analogIndex = 0; analogIndex < analogArray->size(); ++analogIndex)
                {
                    const toml::node* bindingNode = analogArray->get(analogIndex);
                    const std::string bindingPath = path + ".analog[" + std::to_string(analogIndex) + "]";
                    if (bindingNode == nullptr)
                    {
                        continue;
                    }
                    const toml::table* bindingTable = bindingNode->as_table();
                    if (bindingTable == nullptr)
                    {
                        AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, bindingPath, "Expected an analog binding table.", bindingNode);
                        continue;
                    }

                    ValidateKnownKeys(*bindingTable, bindingPath, {"axis", "deadzone", "scale"}, diagnostics);
                    const std::optional<std::string> axisName = ReadRequiredString(*bindingTable, "axis", bindingPath + ".axis", diagnostics);
                    const std::optional<float> deadzone = ReadOptionalFloat(*bindingTable, "deadzone", bindingPath + ".deadzone", 0.0F, diagnostics);
                    const std::optional<float> scale = ReadOptionalFloat(*bindingTable, "scale", bindingPath + ".scale", 1.0F, diagnostics);
                    if (!axisName.has_value() || !deadzone.has_value() || !scale.has_value())
                    {
                        continue;
                    }

                    const std::optional<InputAxis> axis = ParseInputAxis(*axisName);
                    if (!axis.has_value())
                    {
                        AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, bindingPath + ".axis", "Unknown input axis '" + *axisName + "'.", bindingTable->get("axis"));
                        continue;
                    }
                    if (*deadzone < 0.0F || *deadzone >= 1.0F)
                    {
                        AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, bindingPath + ".deadzone", "Deadzone must be in [0, 1).", bindingTable->get("deadzone"));
                        continue;
                    }
                    if (*scale == 0.0F)
                    {
                        AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, bindingPath + ".scale", "Scale must be non-zero.", bindingTable->get("scale"));
                        continue;
                    }
                    if (std::any_of(
                            action.analogBindings.begin(),
                            action.analogBindings.end(),
                            [axis](const InputMapAnalogBinding& binding) { return binding.axis == *axis; }))
                    {
                        AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, bindingPath + ".axis", "Duplicate analog axis binding.", bindingTable->get("axis"));
                        continue;
                    }
                    action.analogBindings.push_back(InputMapAnalogBinding{
                        .axis = *axis,
                        .deadzone = *deadzone,
                        .scale = *scale,
                    });
                }
            }
        }

        if (!semanticId.has_value())
        {
            continue;
        }
        if (!action.hasDigitalBinding && action.analogBindings.empty())
        {
            AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, path, "Axis1D action requires a digital or analog binding.", actionNode);
            continue;
        }
        if (HasSemanticId(document, action.semanticId))
        {
            AddDiagnostic(diagnostics, InputMapErrorCode::SchemaError, path + ".id", "Duplicate semantic input action ID '" + action.semanticId + "'.", actionTable->get("id"));
            continue;
        }
        document.axis1DActions.push_back(std::move(action));
    }
}

void WriteQuoted(std::ostringstream& stream, const std::string_view value)
{
    stream << '"';
    for (const char character : value)
    {
        switch (character)
        {
        case '\\':
            stream << "\\\\";
            break;
        case '"':
            stream << "\\\"";
            break;
        case '\n':
            stream << "\\n";
            break;
        case '\r':
            stream << "\\r";
            break;
        case '\t':
            stream << "\\t";
            break;
        default:
            stream << character;
            break;
        }
    }
    stream << '"';
}

bool IsAsciiDrivePrefix(const std::string_view reference) noexcept
{
    return reference.size() >= 2U && reference[1] == ':' &&
        std::isalpha(static_cast<unsigned char>(reference[0])) != 0;
}

bool TryNormalizeReference(
    const std::filesystem::path& projectRoot,
    const std::string_view reference,
    NormalizedReference& normalized,
    InputMapDiagnostic* diagnostic)
{
    if (reference.empty())
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = MakeDiagnostic(InputMapErrorCode::InvalidReference, "$reference", "Input-map reference must not be empty.", reference);
        }
        return false;
    }

    std::string portable{reference};
    std::replace(portable.begin(), portable.end(), '\\', '/');
    if (portable.front() == '/' || IsAsciiDrivePrefix(portable))
    {
        if (diagnostic != nullptr)
        {
            *diagnostic = MakeDiagnostic(InputMapErrorCode::InvalidReference, "$reference", "Input-map reference must be project-relative, not absolute.", reference);
        }
        return false;
    }

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
            if (component == ".." || component.find('\0') != std::string_view::npos)
            {
                if (diagnostic != nullptr)
                {
                    *diagnostic = MakeDiagnostic(InputMapErrorCode::InvalidReference, "$reference", "Input-map reference must not traverse outside the project root.", reference);
                }
                return false;
            }
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
            *diagnostic = MakeDiagnostic(InputMapErrorCode::InvalidReference, "$reference", "Input-map reference must identify a file below the project root.", reference);
        }
        return false;
    }

    normalized.resolvedPath = (projectRoot / std::filesystem::path{normalized.id}).lexically_normal();
    return true;
}

std::vector<InputMapButtonAction> CanonicalButtons(const InputMapDocument& document)
{
    std::vector<InputMapButtonAction> actions = document.buttonActions;
    std::sort(actions.begin(), actions.end(), [](const auto& left, const auto& right) { return left.semanticId < right.semanticId; });
    for (InputMapButtonAction& action : actions)
    {
        std::sort(action.controls.begin(), action.controls.end(), [](const InputControl left, const InputControl right) {
            return ToString(left) < ToString(right);
        });
    }
    return actions;
}

std::vector<InputMapAxis1DAction> CanonicalAxes(const InputMapDocument& document)
{
    std::vector<InputMapAxis1DAction> actions = document.axis1DActions;
    std::sort(actions.begin(), actions.end(), [](const auto& left, const auto& right) { return left.semanticId < right.semanticId; });
    for (InputMapAxis1DAction& action : actions)
    {
        std::sort(action.analogBindings.begin(), action.analogBindings.end(), [](const auto& left, const auto& right) {
            return ToString(left.axis) < ToString(right.axis);
        });
    }
    return actions;
}
} // namespace

std::string_view ToString(const InputMapErrorCode code) noexcept
{
    switch (code)
    {
    case InputMapErrorCode::InvalidReference:
        return "invalid_reference";
    case InputMapErrorCode::UnsupportedFormat:
        return "unsupported_format";
    case InputMapErrorCode::MissingFile:
        return "missing_file";
    case InputMapErrorCode::ReadFailure:
        return "read_failure";
    case InputMapErrorCode::ParseError:
        return "parse_error";
    case InputMapErrorCode::SchemaError:
        return "schema_error";
    case InputMapErrorCode::StaleBinding:
        return "stale_binding";
    case InputMapErrorCode::WriteFailure:
        return "write_failure";
    }
    return "unknown_input_map_error";
}

std::string_view ToString(const InputControl control) noexcept
{
    switch (control)
    {
    case InputControl::KeyA: return "KeyA";
    case InputControl::KeyB: return "KeyB";
    case InputControl::KeyC: return "KeyC";
    case InputControl::KeyD: return "KeyD";
    case InputControl::KeyE: return "KeyE";
    case InputControl::KeyF: return "KeyF";
    case InputControl::KeyG: return "KeyG";
    case InputControl::KeyH: return "KeyH";
    case InputControl::KeyI: return "KeyI";
    case InputControl::KeyJ: return "KeyJ";
    case InputControl::KeyK: return "KeyK";
    case InputControl::KeyL: return "KeyL";
    case InputControl::KeyM: return "KeyM";
    case InputControl::KeyN: return "KeyN";
    case InputControl::KeyO: return "KeyO";
    case InputControl::KeyP: return "KeyP";
    case InputControl::KeyQ: return "KeyQ";
    case InputControl::KeyR: return "KeyR";
    case InputControl::KeyS: return "KeyS";
    case InputControl::KeyT: return "KeyT";
    case InputControl::KeyU: return "KeyU";
    case InputControl::KeyV: return "KeyV";
    case InputControl::KeyW: return "KeyW";
    case InputControl::KeyX: return "KeyX";
    case InputControl::KeyY: return "KeyY";
    case InputControl::KeyZ: return "KeyZ";
    case InputControl::ArrowLeft: return "ArrowLeft";
    case InputControl::ArrowRight: return "ArrowRight";
    case InputControl::ArrowUp: return "ArrowUp";
    case InputControl::ArrowDown: return "ArrowDown";
    case InputControl::Space: return "Space";
    case InputControl::Enter: return "Enter";
    case InputControl::Escape: return "Escape";
    case InputControl::MouseLeft: return "MouseLeft";
    case InputControl::MouseMiddle: return "MouseMiddle";
    case InputControl::MouseRight: return "MouseRight";
    case InputControl::GamepadSouth: return "GamepadSouth";
    case InputControl::GamepadEast: return "GamepadEast";
    case InputControl::GamepadWest: return "GamepadWest";
    case InputControl::GamepadNorth: return "GamepadNorth";
    case InputControl::GamepadBack: return "GamepadBack";
    case InputControl::GamepadGuide: return "GamepadGuide";
    case InputControl::GamepadStart: return "GamepadStart";
    case InputControl::GamepadLeftStick: return "GamepadLeftStick";
    case InputControl::GamepadRightStick: return "GamepadRightStick";
    case InputControl::GamepadLeftShoulder: return "GamepadLeftShoulder";
    case InputControl::GamepadRightShoulder: return "GamepadRightShoulder";
    case InputControl::GamepadDpadUp: return "GamepadDpadUp";
    case InputControl::GamepadDpadDown: return "GamepadDpadDown";
    case InputControl::GamepadDpadLeft: return "GamepadDpadLeft";
    case InputControl::GamepadDpadRight: return "GamepadDpadRight";
    case InputControl::Unknown:
    case InputControl::Count:
        break;
    }
    return "Unknown";
}

std::string_view ToString(const InputAxis axis) noexcept
{
    switch (axis)
    {
    case InputAxis::GamepadLeftX: return "GamepadLeftX";
    case InputAxis::GamepadLeftY: return "GamepadLeftY";
    case InputAxis::GamepadRightX: return "GamepadRightX";
    case InputAxis::GamepadRightY: return "GamepadRightY";
    case InputAxis::GamepadLeftTrigger: return "GamepadLeftTrigger";
    case InputAxis::GamepadRightTrigger: return "GamepadRightTrigger";
    case InputAxis::Unknown:
    case InputAxis::Count:
        break;
    }
    return "Unknown";
}

std::optional<InputControl> ParseInputControl(const std::string_view value) noexcept
{
    for (std::size_t index = 1U; index < static_cast<std::size_t>(InputControl::Count); ++index)
    {
        const auto control = static_cast<InputControl>(index);
        if (ToString(control) == value)
        {
            return control;
        }
    }
    return std::nullopt;
}

std::optional<InputAxis> ParseInputAxis(const std::string_view value) noexcept
{
    for (std::size_t index = 1U; index < static_cast<std::size_t>(InputAxis::Count); ++index)
    {
        const auto axis = static_cast<InputAxis>(index);
        if (ToString(axis) == value)
        {
            return axis;
        }
    }
    return std::nullopt;
}

InputMapLoadResult ParseInputMapToml(const std::string_view text, const std::string_view sourceName)
{
    InputMapLoadResult result{};
    toml::table root{};
    try
    {
        root = toml::parse(text, sourceName);
    }
    catch (const toml::parse_error& error)
    {
        InputMapDiagnostic diagnostic{};
        diagnostic.code = InputMapErrorCode::ParseError;
        diagnostic.path = "$";
        diagnostic.message = std::string{error.description()};
        diagnostic.line = static_cast<std::size_t>(error.source().begin.line);
        diagnostic.column = static_cast<std::size_t>(error.source().begin.column);
        result.diagnostics.push_back(std::move(diagnostic));
        return result;
    }

    ValidateKnownKeys(root, "", {"format_version", "buttons", "axes1d"}, result.diagnostics);
    const toml::node* formatNode = root.get("format_version");
    if (formatNode == nullptr)
    {
        AddDiagnostic(result.diagnostics, InputMapErrorCode::UnsupportedFormat, "format_version", "Required field is missing.");
    }
    else if (!formatNode->is_integer())
    {
        AddDiagnostic(result.diagnostics, InputMapErrorCode::UnsupportedFormat, "format_version", "Expected an integer.", formatNode);
    }
    else
    {
        const std::optional<std::int64_t> version = formatNode->value<std::int64_t>();
        if (!version.has_value() || *version != InputMapDocument::FormatVersion)
        {
            AddDiagnostic(result.diagnostics, InputMapErrorCode::UnsupportedFormat, "format_version", "Unsupported input-map format version. Expected 1.", formatNode);
        }
    }

    InputMapDocument document{};
    ValidateButtonActions(root.get("buttons"), document, result.diagnostics);
    ValidateAxisActions(root.get("axes1d"), document, result.diagnostics);
    if (document.buttonActions.empty() && document.axis1DActions.empty())
    {
        AddDiagnostic(result.diagnostics, InputMapErrorCode::SchemaError, "$", "Input map must define at least one semantic action.");
    }

    if (result.diagnostics.empty())
    {
        result.document = std::move(document);
    }
    return result;
}

std::string SaveInputMapToml(const InputMapDocument& document)
{
    std::ostringstream stream{};
    stream.imbue(std::locale::classic());
    stream << "format_version = " << InputMapDocument::FormatVersion << "\n";

    const std::vector<InputMapButtonAction> buttons = CanonicalButtons(document);
    for (const InputMapButtonAction& action : buttons)
    {
        stream << "\n[[buttons]]\nid = ";
        WriteQuoted(stream, action.semanticId);
        stream << "\ncontrols = [";
        for (std::size_t index = 0U; index < action.controls.size(); ++index)
        {
            if (index != 0U)
            {
                stream << ", ";
            }
            WriteQuoted(stream, ToString(action.controls[index]));
        }
        stream << "]\n";
    }

    const std::vector<InputMapAxis1DAction> axes = CanonicalAxes(document);
    stream << std::setprecision(std::numeric_limits<float>::max_digits10);
    for (const InputMapAxis1DAction& action : axes)
    {
        stream << "\n[[axes1d]]\nid = ";
        WriteQuoted(stream, action.semanticId);
        stream << '\n';
        if (action.hasDigitalBinding)
        {
            stream << "negative = ";
            WriteQuoted(stream, ToString(action.negative));
            stream << "\npositive = ";
            WriteQuoted(stream, ToString(action.positive));
            stream << '\n';
        }
        if (!action.analogBindings.empty())
        {
            stream << "analog = [\n";
            for (const InputMapAnalogBinding& binding : action.analogBindings)
            {
                stream << "  { axis = ";
                WriteQuoted(stream, ToString(binding.axis));
                stream << ", deadzone = " << binding.deadzone << ", scale = " << binding.scale << " },\n";
            }
            stream << "]\n";
        }
    }
    return stream.str();
}

InputMapBuildResult BuildActionMap(const InputMapDocument& document)
{
    InputMapBuildResult result{};
    try
    {
        ActionMap actions{};
        for (const InputMapButtonAction& definition : CanonicalButtons(document))
        {
            const ButtonActionId id = actions.AddButtonAction(definition.semanticId);
            for (const InputControl control : definition.controls)
            {
                actions.BindButton(id, control);
            }
        }
        for (const InputMapAxis1DAction& definition : CanonicalAxes(document))
        {
            Axis1DActionId id{};
            if (definition.hasDigitalBinding)
            {
                id = actions.AddAxis1DAction(definition.semanticId, definition.negative, definition.positive);
            }
            else
            {
                id = actions.AddAxis1DAction(definition.semanticId);
            }
            for (const InputMapAnalogBinding& binding : definition.analogBindings)
            {
                actions.BindAxis1DAnalog(id, binding.axis, binding.deadzone, binding.scale);
            }
        }
        actions.Finalize();
        result.actionMap = std::move(actions);
    }
    catch (const std::exception& error)
    {
        result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::SchemaError, "$", error.what()));
    }
    return result;
}

InputMapEditResult RebindControl(
    InputMapDocument& document,
    const std::string_view semanticId,
    const InputControl expectedCurrent,
    const InputControl replacement)
{
    InputMapEditResult result{};
    if (ParseInputControl(ToString(expectedCurrent)) != expectedCurrent || ParseInputControl(ToString(replacement)) != replacement)
    {
        result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::SchemaError, std::string{semanticId}, "Rebind requires known input controls."));
        return result;
    }

    for (InputMapButtonAction& action : document.buttonActions)
    {
        if (action.semanticId != semanticId)
        {
            continue;
        }
        const auto current = std::find(action.controls.begin(), action.controls.end(), expectedCurrent);
        if (current == action.controls.end())
        {
            result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::StaleBinding, "buttons." + action.semanticId, "Expected current control is not bound; authored state changed before commit."));
            return result;
        }
        if (expectedCurrent == replacement)
        {
            return result;
        }
        if (std::find(action.controls.begin(), action.controls.end(), replacement) != action.controls.end())
        {
            result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::SchemaError, "buttons." + action.semanticId, "Replacement would duplicate an existing control binding."));
            return result;
        }
        *current = replacement;
        result.changed = true;
        return result;
    }

    for (InputMapAxis1DAction& action : document.axis1DActions)
    {
        if (action.semanticId != semanticId || !action.hasDigitalBinding)
        {
            continue;
        }
        InputControl* target = nullptr;
        if (action.negative == expectedCurrent)
        {
            target = &action.negative;
        }
        else if (action.positive == expectedCurrent)
        {
            target = &action.positive;
        }
        if (target == nullptr)
        {
            result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::StaleBinding, "axes1d." + action.semanticId, "Expected current control is not bound; authored state changed before commit."));
            return result;
        }
        if (expectedCurrent == replacement)
        {
            return result;
        }
        const InputControl other = target == &action.negative ? action.positive : action.negative;
        if (other == replacement)
        {
            result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::SchemaError, "axes1d." + action.semanticId, "Replacement would make negative and positive controls identical."));
            return result;
        }
        *target = replacement;
        result.changed = true;
        return result;
    }

    result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::StaleBinding, std::string{semanticId}, "Semantic action or expected digital binding was not found."));
    return result;
}

InputMapEditResult RebindAnalogAxis(
    InputMapDocument& document,
    const std::string_view semanticId,
    const InputAxis expectedCurrent,
    const InputAxis replacement)
{
    InputMapEditResult result{};
    if (ParseInputAxis(ToString(expectedCurrent)) != expectedCurrent || ParseInputAxis(ToString(replacement)) != replacement)
    {
        result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::SchemaError, std::string{semanticId}, "Rebind requires known input axes."));
        return result;
    }

    for (InputMapAxis1DAction& action : document.axis1DActions)
    {
        if (action.semanticId != semanticId)
        {
            continue;
        }
        const auto current = std::find_if(
            action.analogBindings.begin(),
            action.analogBindings.end(),
            [expectedCurrent](const InputMapAnalogBinding& binding) { return binding.axis == expectedCurrent; });
        if (current == action.analogBindings.end())
        {
            result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::StaleBinding, "axes1d." + action.semanticId + ".analog", "Expected current axis is not bound; authored state changed before commit."));
            return result;
        }
        if (expectedCurrent == replacement)
        {
            return result;
        }
        if (std::any_of(
                action.analogBindings.begin(),
                action.analogBindings.end(),
                [replacement](const InputMapAnalogBinding& binding) { return binding.axis == replacement; }))
        {
            result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::SchemaError, "axes1d." + action.semanticId + ".analog", "Replacement would duplicate an existing analog binding."));
            return result;
        }
        current->axis = replacement;
        result.changed = true;
        return result;
    }

    result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::StaleBinding, std::string{semanticId}, "Semantic Axis1D action was not found."));
    return result;
}

InputMapStore::InputMapStore(std::filesystem::path projectRoot)
    : projectRoot_{std::move(projectRoot)}
{
}

const std::filesystem::path& InputMapStore::ProjectRoot() const noexcept
{
    return projectRoot_;
}

InputMapLoadResult InputMapStore::Load(const std::string_view projectRelativeReference) const
{
    InputMapLoadResult result{};
    NormalizedReference normalized{};
    InputMapDiagnostic referenceDiagnostic{};
    if (!TryNormalizeReference(projectRoot_, projectRelativeReference, normalized, &referenceDiagnostic))
    {
        result.diagnostics.push_back(std::move(referenceDiagnostic));
        return result;
    }

    std::error_code error{};
    if (!std::filesystem::exists(normalized.resolvedPath, error) || error)
    {
        result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::MissingFile, "$reference", "Input-map file does not exist.", normalized.id, normalized.resolvedPath));
        return result;
    }
    const std::uintmax_t sourceBytes = std::filesystem::file_size(normalized.resolvedPath, error);
    if (error || sourceBytes > MaximumInputMapSourceBytes)
    {
        result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::ReadFailure, "$reference", error ? "Could not determine input-map file size." : "Input-map source exceeds the 1 MiB setup-time limit.", normalized.id, normalized.resolvedPath));
        return result;
    }

    std::ifstream stream{normalized.resolvedPath, std::ios::binary};
    if (!stream)
    {
        result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::ReadFailure, "$reference", "Could not open input-map file.", normalized.id, normalized.resolvedPath));
        return result;
    }
    std::string text((std::istreambuf_iterator<char>{stream}), std::istreambuf_iterator<char>{});
    if (!stream.good() && !stream.eof())
    {
        result.diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::ReadFailure, "$reference", "Could not read input-map file.", normalized.id, normalized.resolvedPath));
        return result;
    }

    result = ParseInputMapToml(text, normalized.id);
    for (InputMapDiagnostic& diagnostic : result.diagnostics)
    {
        diagnostic.reference = normalized.id;
        diagnostic.resolvedPath = normalized.resolvedPath.generic_string();
    }
    return result;
}

std::vector<InputMapDiagnostic> InputMapStore::Save(
    const std::string_view projectRelativeReference,
    const InputMapDocument& document) const
{
    std::vector<InputMapDiagnostic> diagnostics{};
    NormalizedReference normalized{};
    InputMapDiagnostic referenceDiagnostic{};
    if (!TryNormalizeReference(projectRoot_, projectRelativeReference, normalized, &referenceDiagnostic))
    {
        diagnostics.push_back(std::move(referenceDiagnostic));
        return diagnostics;
    }

    const InputMapBuildResult validation = BuildActionMap(document);
    if (!validation.Succeeded())
    {
        diagnostics = validation.diagnostics;
        return diagnostics;
    }

    std::error_code error{};
    const std::filesystem::path parent = normalized.resolvedPath.parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, error);
        if (error)
        {
            diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::WriteFailure, "$reference", "Could not create the input-map parent directory.", normalized.id, normalized.resolvedPath));
            return diagnostics;
        }
    }

    const std::filesystem::path temporary = normalized.resolvedPath.string() + ".tmp";
    {
        std::ofstream stream{temporary, std::ios::binary | std::ios::trunc};
        if (!stream)
        {
            diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::WriteFailure, "$reference", "Could not open temporary input-map file for writing.", normalized.id, temporary));
            return diagnostics;
        }
        const std::string canonical = SaveInputMapToml(document);
        stream.write(canonical.data(), static_cast<std::streamsize>(canonical.size()));
        stream.flush();
        if (!stream)
        {
            diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::WriteFailure, "$reference", "Could not write complete canonical input-map data.", normalized.id, temporary));
            return diagnostics;
        }
    }

    // Full crash-safe persistence is owned by #79. I2 still writes through a validated sibling
    // temporary and replaces only at this explicit setup/rebind boundary.
    std::filesystem::remove(normalized.resolvedPath, error);
    error.clear();
    std::filesystem::rename(temporary, normalized.resolvedPath, error);
    if (error)
    {
        std::error_code cleanupError{};
        std::filesystem::remove(temporary, cleanupError);
        diagnostics.push_back(MakeDiagnostic(InputMapErrorCode::WriteFailure, "$reference", "Could not replace the canonical input-map file.", normalized.id, normalized.resolvedPath));
    }
    return diagnostics;
}
} // namespace trace2d::input
