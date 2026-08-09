#include <trace2d/mcp/McpServer.hpp>

#include <trace2d/agent/Inspection.hpp>
#include <trace2d/core/Version.hpp>
#include <trace2d/input/Input.hpp>

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace trace2d::mcp
{
namespace
{
using Json = nlohmann::json;

constexpr std::uint64_t MaxStepFrames = 100'000U;
constexpr std::uint64_t ToolListTtlMilliseconds = 60'000U;

constexpr int JsonRpcParseError = -32700;
constexpr int JsonRpcInvalidRequest = -32600;
constexpr int JsonRpcMethodNotFound = -32601;
constexpr int JsonRpcInvalidParams = -32602;
constexpr int JsonRpcInternalError = -32603;

constexpr std::array<std::string_view, 26> LetterControlNames{
    "key_a", "key_b", "key_c", "key_d", "key_e", "key_f", "key_g", "key_h", "key_i",
    "key_j", "key_k", "key_l", "key_m", "key_n", "key_o", "key_p", "key_q", "key_r",
    "key_s", "key_t", "key_u", "key_v", "key_w", "key_x", "key_y", "key_z",
};

Json MakeJsonRpcError(const Json& id, const int code, const std::string_view message)
{
    Json error = Json::object();
    error["code"] = code;
    error["message"] = std::string{message};

    Json response = Json::object();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["error"] = std::move(error);
    return response;
}

Json MakeJsonRpcResult(const Json& id, Json result)
{
    Json response = Json::object();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = std::move(result);
    return response;
}

Json MakeErrorPayload(const std::string_view code, const std::string_view message)
{
    Json error = Json::object();
    error["code"] = std::string{code};
    error["message"] = std::string{message};

    Json payload = Json::object();
    payload["status"] = "error";
    payload["error"] = std::move(error);
    return payload;
}

Json MakeToolResult(const Json& payload, const bool isError)
{
    Json textContent = Json::object();
    textContent["type"] = "text";
    textContent["text"] = payload.dump();

    Json content = Json::array();
    content.push_back(std::move(textContent));

    Json result = Json::object();
    result["resultType"] = "complete";
    result["content"] = std::move(content);
    result["structuredContent"] = payload;
    result["isError"] = isError;
    return result;
}

Json FieldValueToJson(const agent::FieldValue& value)
{
    Json result = Json::object();
    result["kind"] = std::string{agent::ToString(value.kind)};

    switch (value.kind)
    {
    case agent::FieldValueKind::Boolean:
        result["value"] = value.booleanValue;
        break;
    case agent::FieldValueKind::SignedInteger:
        result["value"] = value.signedIntegerValue;
        break;
    case agent::FieldValueKind::UnsignedInteger:
        result["value"] = value.unsignedIntegerValue;
        break;
    case agent::FieldValueKind::Float:
        result["value"] = value.floatValue;
        break;
    case agent::FieldValueKind::String:
        result["value"] = value.stringValue;
        break;
    }

    return result;
}

Json RuntimeSnapshotToJson(const agent::RuntimeSnapshot& runtime)
{
    Json result = Json::object();
    result["frame"] = runtime.frame;
    result["seed"] = runtime.seed;
    result["fixed_step_ns"] = runtime.fixedStepNanoseconds;
    result["simulation_time_ns"] = runtime.simulationTimeNanoseconds;
    return result;
}

Json EntitySnapshotToJson(const agent::EntitySnapshot& entity)
{
    Json handle = Json::object();
    handle["index"] = entity.handle.index;
    handle["generation"] = entity.handle.generation;

    Json transform = Json::object();
    transform["position"] = Json{{"x", entity.transform.position.x}, {"y", entity.transform.position.y}};
    transform["rotation_radians"] = entity.transform.rotationRadians;
    transform["scale"] = Json{{"x", entity.transform.scale.x}, {"y", entity.transform.scale.y}};

    Json components = Json::array();
    for (const agent::ComponentSnapshot& component : entity.components)
    {
        Json fields = Json::array();
        for (const agent::ComponentFieldSnapshot& field : component.fields)
        {
            Json fieldJson = Json::object();
            fieldJson["name"] = field.name;
            fieldJson["value"] = FieldValueToJson(field.value);
            fields.push_back(std::move(fieldJson));
        }

        Json componentJson = Json::object();
        componentJson["type"] = component.type;
        componentJson["fields"] = std::move(fields);
        components.push_back(std::move(componentJson));
    }

    Json result = Json::object();
    result["handle"] = std::move(handle);
    result["id"] = entity.semanticId;
    result["name"] = entity.name;
    result["tags"] = entity.tags;
    result["transform"] = std::move(transform);
    if (entity.bounds.has_value())
    {
        Json bounds = Json::object();
        bounds["center"] = Json{{"x", entity.bounds->center.x}, {"y", entity.bounds->center.y}};
        bounds["extents"] = Json{{"x", entity.bounds->extents.x}, {"y", entity.bounds->extents.y}};
        result["bounds"] = std::move(bounds);
    }
    else
    {
        result["bounds"] = nullptr;
    }
    result["components"] = std::move(components);
    return result;
}

Json InspectionSnapshotToJson(const agent::InspectionSnapshot& snapshot)
{
    Json entities = Json::array();
    for (const agent::EntitySnapshot& entity : snapshot.scene.entities)
    {
        entities.push_back(EntitySnapshotToJson(entity));
    }

    Json scene = Json::object();
    scene["id"] = snapshot.scene.semanticId;
    scene["name"] = snapshot.scene.name;
    scene["entities"] = std::move(entities);

    Json result = Json::object();
    result["runtime"] = RuntimeSnapshotToJson(snapshot.runtime);
    result["scene"] = std::move(scene);
    return result;
}

Json UiSelectorToJson(const agent::UiSelector& selector)
{
    Json result = Json::object();
    if (selector.id.has_value())
    {
        result["id"] = *selector.id;
    }
    if (selector.role.has_value())
    {
        result["role"] = std::string{agent::ToString(*selector.role)};
    }
    if (selector.name.has_value())
    {
        result["name"] = *selector.name;
    }
    return result;
}

Json UiElementToJson(const agent::UiElementSnapshot& element)
{
    Json bounds = Json::object();
    bounds["x"] = element.bounds.x;
    bounds["y"] = element.bounds.y;
    bounds["width"] = element.bounds.width;
    bounds["height"] = element.bounds.height;

    Json result = Json::object();
    result["id"] = element.id;
    result["role"] = std::string{agent::ToString(element.role)};
    result["name"] = element.name;
    result["bounds"] = std::move(bounds);
    result["visible"] = element.visible;
    result["enabled"] = element.enabled;
    result["focused"] = element.focused;
    result["text"] = element.text;
    result["activation_count"] = element.activationCount;
    return result;
}

Json UiTreeToJson(const agent::UiTreeSnapshot& tree)
{
    Json elements = Json::array();
    for (const agent::UiElementSnapshot& element : tree.elements)
    {
        elements.push_back(UiElementToJson(element));
    }

    Json result = Json::object();
    result["width"] = tree.width;
    result["height"] = tree.height;
    result["elements"] = std::move(elements);
    return result;
}

std::string_view InputControlName(const input::InputControl control) noexcept
{
    const std::uint16_t numeric = static_cast<std::uint16_t>(control);
    const std::uint16_t firstLetter = static_cast<std::uint16_t>(input::InputControl::KeyA);
    const std::uint16_t lastLetter = static_cast<std::uint16_t>(input::InputControl::KeyZ);
    if (numeric >= firstLetter && numeric <= lastLetter)
    {
        return LetterControlNames[static_cast<std::size_t>(numeric - firstLetter)];
    }

    switch (control)
    {
    case input::InputControl::ArrowLeft:
        return "arrow_left";
    case input::InputControl::ArrowRight:
        return "arrow_right";
    case input::InputControl::ArrowUp:
        return "arrow_up";
    case input::InputControl::ArrowDown:
        return "arrow_down";
    case input::InputControl::Space:
        return "space";
    case input::InputControl::Enter:
        return "enter";
    case input::InputControl::Escape:
        return "escape";
    case input::InputControl::MouseLeft:
        return "mouse_left";
    case input::InputControl::MouseMiddle:
        return "mouse_middle";
    case input::InputControl::MouseRight:
        return "mouse_right";
    case input::InputControl::Unknown:
    case input::InputControl::Count:
        return "unknown";
    default:
        return "unknown";
    }
}

std::optional<input::InputControl> ParseInputControl(const std::string_view name) noexcept
{
    if (name.size() == 1U)
    {
        char character = name.front();
        if (character >= 'A' && character <= 'Z')
        {
            character = static_cast<char>(character - 'A' + 'a');
        }
        if (character >= 'a' && character <= 'z')
        {
            const std::uint16_t offset = static_cast<std::uint16_t>(character - 'a');
            const std::uint16_t first = static_cast<std::uint16_t>(input::InputControl::KeyA);
            return static_cast<input::InputControl>(first + offset);
        }
    }

    if (name.size() == 5U && name.substr(0U, 4U) == "key_")
    {
        const char character = name[4U];
        if (character >= 'a' && character <= 'z')
        {
            const std::uint16_t offset = static_cast<std::uint16_t>(character - 'a');
            const std::uint16_t first = static_cast<std::uint16_t>(input::InputControl::KeyA);
            return static_cast<input::InputControl>(first + offset);
        }
    }

    if (name == "arrow_left")
    {
        return input::InputControl::ArrowLeft;
    }
    if (name == "arrow_right")
    {
        return input::InputControl::ArrowRight;
    }
    if (name == "arrow_up")
    {
        return input::InputControl::ArrowUp;
    }
    if (name == "arrow_down")
    {
        return input::InputControl::ArrowDown;
    }
    if (name == "space")
    {
        return input::InputControl::Space;
    }
    if (name == "enter")
    {
        return input::InputControl::Enter;
    }
    if (name == "escape")
    {
        return input::InputControl::Escape;
    }
    if (name == "mouse_left")
    {
        return input::InputControl::MouseLeft;
    }
    if (name == "mouse_middle")
    {
        return input::InputControl::MouseMiddle;
    }
    if (name == "mouse_right")
    {
        return input::InputControl::MouseRight;
    }

    return std::nullopt;
}

std::optional<agent::UiRole> ParseUiRole(const std::string_view role) noexcept
{
    if (role == "panel")
    {
        return agent::UiRole::Panel;
    }
    if (role == "label")
    {
        return agent::UiRole::Label;
    }
    if (role == "button")
    {
        return agent::UiRole::Button;
    }
    if (role == "textbox")
    {
        return agent::UiRole::TextBox;
    }
    return std::nullopt;
}

bool ReadUnsigned(const Json& object, const char* key, std::uint64_t& value, std::string& error)
{
    const Json::const_iterator found = object.find(key);
    if (found == object.end())
    {
        error = std::string{"Missing required unsigned integer argument: "} + key;
        return false;
    }

    if (found->is_number_unsigned())
    {
        value = found->get<std::uint64_t>();
        return true;
    }

    if (found->is_number_integer())
    {
        const std::int64_t signedValue = found->get<std::int64_t>();
        if (signedValue >= 0)
        {
            value = static_cast<std::uint64_t>(signedValue);
            return true;
        }
    }

    error = std::string{"Argument must be a non-negative integer: "} + key;
    return false;
}

bool ReadRequiredString(const Json& object, const char* key, std::string& value, std::string& error)
{
    const Json::const_iterator found = object.find(key);
    if (found == object.end() || !found->is_string())
    {
        error = std::string{"Missing required string argument: "} + key;
        return false;
    }

    value = found->get<std::string>();
    if (value.empty())
    {
        error = std::string{"String argument must not be empty: "} + key;
        return false;
    }
    return true;
}

bool ReadOptionalBool(const Json& object, const char* key, const bool defaultValue, bool& value, std::string& error)
{
    const Json::const_iterator found = object.find(key);
    if (found == object.end())
    {
        value = defaultValue;
        return true;
    }
    if (!found->is_boolean())
    {
        error = std::string{"Argument must be boolean: "} + key;
        return false;
    }
    value = found->get<bool>();
    return true;
}

bool ParseUiSelector(const Json& arguments, agent::UiSelector& selector, std::string& error)
{
    const Json::const_iterator selectorValue = arguments.find("selector");
    if (selectorValue == arguments.end() || !selectorValue->is_object())
    {
        error = "Missing required object argument: selector";
        return false;
    }

    const Json& object = *selectorValue;
    const Json::const_iterator id = object.find("id");
    if (id != object.end())
    {
        if (!id->is_string() || id->get_ref<const std::string&>().empty())
        {
            error = "selector.id must be a non-empty string.";
            return false;
        }
        selector.id = id->get<std::string>();
    }

    const Json::const_iterator role = object.find("role");
    if (role != object.end())
    {
        if (!role->is_string())
        {
            error = "selector.role must be a string.";
            return false;
        }
        const std::optional<agent::UiRole> parsedRole = ParseUiRole(role->get_ref<const std::string&>());
        if (!parsedRole.has_value())
        {
            error = "selector.role must be one of panel, label, button, textbox.";
            return false;
        }
        selector.role = *parsedRole;
    }

    const Json::const_iterator name = object.find("name");
    if (name != object.end())
    {
        if (!name->is_string() || name->get_ref<const std::string&>().empty())
        {
            error = "selector.name must be a non-empty string.";
            return false;
        }
        selector.name = name->get<std::string>();
    }

    if (!selector.id.has_value() && !selector.role.has_value() && !selector.name.has_value())
    {
        error = "selector must include at least one of id, role, or name.";
        return false;
    }
    return true;
}

bool ParseUiExpectedState(const Json& arguments, agent::UiExpectedState& expected, std::string& error)
{
    const Json::const_iterator expectedValue = arguments.find("expected");
    if (expectedValue == arguments.end() || !expectedValue->is_object())
    {
        error = "Missing required object argument: expected";
        return false;
    }

    const Json& object = *expectedValue;
    const Json::const_iterator visible = object.find("visible");
    if (visible != object.end())
    {
        if (!visible->is_boolean())
        {
            error = "expected.visible must be boolean.";
            return false;
        }
        expected.visible = visible->get<bool>();
    }

    const Json::const_iterator enabled = object.find("enabled");
    if (enabled != object.end())
    {
        if (!enabled->is_boolean())
        {
            error = "expected.enabled must be boolean.";
            return false;
        }
        expected.enabled = enabled->get<bool>();
    }

    const Json::const_iterator focused = object.find("focused");
    if (focused != object.end())
    {
        if (!focused->is_boolean())
        {
            error = "expected.focused must be boolean.";
            return false;
        }
        expected.focused = focused->get<bool>();
    }

    const Json::const_iterator text = object.find("text");
    if (text != object.end())
    {
        if (!text->is_string())
        {
            error = "expected.text must be a string.";
            return false;
        }
        expected.text = text->get<std::string>();
    }

    const Json::const_iterator activationCount = object.find("activation_count");
    if (activationCount != object.end())
    {
        std::uint64_t parsed = 0U;
        if (!ReadUnsigned(object, "activation_count", parsed, error))
        {
            return false;
        }
        expected.activationCount = parsed;
    }

    if (!expected.visible.has_value() && !expected.enabled.has_value() && !expected.focused.has_value()
        && !expected.text.has_value() && !expected.activationCount.has_value())
    {
        error = "expected must include at least one state field.";
        return false;
    }
    return true;
}

Json GameplayFailureToJson(const testing::GameplayAssertionFailure& failure)
{
    Json relevantInput = Json::array();
    for (const testing::InputStateSnapshot& inputState : failure.snapshot.relevantInput)
    {
        Json state = Json::object();
        state["control"] = std::string{InputControlName(inputState.control)};
        state["held"] = inputState.state.held;
        state["pressed"] = inputState.state.pressed;
        state["released"] = inputState.state.released;
        relevantInput.push_back(std::move(state));
    }

    Json snapshot = Json::object();
    snapshot["runtime"] = RuntimeSnapshotToJson(failure.snapshot.runtime);
    snapshot["input_frame"] = failure.snapshot.inputFrame;
    snapshot["relevant_input"] = std::move(relevantInput);
    if (failure.snapshot.entity.has_value())
    {
        snapshot["entity"] = EntitySnapshotToJson(*failure.snapshot.entity);
    }
    else
    {
        snapshot["entity"] = nullptr;
    }

    Json result = Json::object();
    result["code"] = std::string{testing::ToString(failure.code)};
    result["selector"] = failure.selector;
    result["component"] = failure.componentType;
    result["field"] = failure.fieldName;
    result["expected"] = FieldValueToJson(failure.expected);
    if (failure.observed.has_value())
    {
        result["observed"] = FieldValueToJson(*failure.observed);
    }
    else
    {
        result["observed"] = nullptr;
    }
    result["frame"] = failure.frame;
    result["seed"] = failure.seed;
    result["detail"] = failure.detail;
    result["snapshot"] = std::move(snapshot);
    return result;
}

Json EmptyObjectSchema()
{
    return Json{{"type", "object"}, {"additionalProperties", false}};
}

Json UiSelectorSchema()
{
    Json properties = Json::object();
    properties["id"] = Json{{"type", "string"}, {"minLength", 1}};
    properties["role"] = Json{{"type", "string"}, {"enum", Json::array({"panel", "label", "button", "textbox"})}};
    properties["name"] = Json{{"type", "string"}, {"minLength", 1}};

    Json schema = Json::object();
    schema["type"] = "object";
    schema["properties"] = std::move(properties);
    schema["minProperties"] = 1;
    schema["additionalProperties"] = false;
    return schema;
}

Json ToolDefinition(const std::string_view name, const std::string_view description, Json inputSchema)
{
    Json tool = Json::object();
    tool["name"] = std::string{name};
    tool["description"] = std::string{description};
    tool["inputSchema"] = std::move(inputSchema);
    return tool;
}

Json BuildToolList()
{
    Json tools = Json::array();

    tools.push_back(ToolDefinition(
        "trace2d.inspect",
        "Inspect deterministic runtime and authored scene state through Trace2D::Agent.",
        EmptyObjectSchema()));

    Json queryProperties = Json::object();
    queryProperties["selector"] = Json{{"type", "string"}, {"minLength", 1}};
    queryProperties["one"] = Json{{"type", "boolean"}, {"default", false}};
    Json querySchema = Json{{"type", "object"}, {"properties", queryProperties}, {"required", Json::array({"selector"})}, {"additionalProperties", false}};
    tools.push_back(ToolDefinition(
        "trace2d.query",
        "Query entities by the existing semantic selector vocabulary such as #player or tag:enemy.",
        std::move(querySchema)));

    tools.push_back(ToolDefinition(
        "trace2d.ui.inspect",
        "Inspect the complete engine-owned semantic UI tree without renderer initialization.",
        EmptyObjectSchema()));

    Json uiQueryProperties = Json::object();
    uiQueryProperties["selector"] = UiSelectorSchema();
    uiQueryProperties["one"] = Json{{"type", "boolean"}, {"default", false}};
    Json uiQuerySchema = Json{{"type", "object"}, {"properties", uiQueryProperties}, {"required", Json::array({"selector"})}, {"additionalProperties", false}};
    tools.push_back(ToolDefinition(
        "trace2d.ui.query",
        "Query semantic UI controls by stable id, role, name, or a compound selector.",
        std::move(uiQuerySchema)));

    Json uiActionProperties = Json::object();
    uiActionProperties["selector"] = UiSelectorSchema();
    Json uiActionSchema = Json{{"type", "object"}, {"properties", uiActionProperties}, {"required", Json::array({"selector"})}, {"additionalProperties", false}};
    tools.push_back(ToolDefinition(
        "trace2d.ui.focus",
        "Focus one semantic UI control through the existing Agent facade.",
        uiActionSchema));
    tools.push_back(ToolDefinition(
        "trace2d.ui.activate",
        "Activate one semantic UI control through the existing Agent facade.",
        uiActionSchema));

    Json inputTextProperties = Json::object();
    inputTextProperties["selector"] = UiSelectorSchema();
    inputTextProperties["text"] = Json{{"type", "string"}};
    Json inputTextSchema = Json{{"type", "object"}, {"properties", inputTextProperties}, {"required", Json::array({"selector", "text"})}, {"additionalProperties", false}};
    tools.push_back(ToolDefinition(
        "trace2d.ui.input_text",
        "Input text into one focused semantic text box through the existing Agent facade.",
        std::move(inputTextSchema)));

    Json expectedProperties = Json::object();
    expectedProperties["visible"] = Json{{"type", "boolean"}};
    expectedProperties["enabled"] = Json{{"type", "boolean"}};
    expectedProperties["focused"] = Json{{"type", "boolean"}};
    expectedProperties["text"] = Json{{"type", "string"}};
    expectedProperties["activation_count"] = Json{{"type", "integer"}, {"minimum", 0}};
    Json expectedSchema = Json{{"type", "object"}, {"properties", expectedProperties}, {"minProperties", 1}, {"additionalProperties", false}};
    Json assertUiProperties = Json::object();
    assertUiProperties["selector"] = UiSelectorSchema();
    assertUiProperties["expected"] = std::move(expectedSchema);
    Json assertUiSchema = Json{{"type", "object"}, {"properties", assertUiProperties}, {"required", Json::array({"selector", "expected"})}, {"additionalProperties", false}};
    tools.push_back(ToolDefinition(
        "trace2d.ui.assert",
        "Assert semantic UI state and return the existing structured deterministic failure diagnostics.",
        std::move(assertUiSchema)));

    Json scheduleProperties = Json::object();
    scheduleProperties["frame"] = Json{{"type", "integer"}, {"minimum", 1}};
    scheduleProperties["control"] = Json{{"type", "string"}, {"description", "key_a..key_z, arrows, space, enter, escape, or mouse buttons"}};
    scheduleProperties["event"] = Json{{"type", "string"}, {"enum", Json::array({"press", "release"})}};
    Json scheduleSchema = Json{{"type", "object"}, {"properties", scheduleProperties}, {"required", Json::array({"frame", "control", "event"})}, {"additionalProperties", false}};
    tools.push_back(ToolDefinition(
        "trace2d.input.schedule",
        "Schedule deterministic virtual input at an explicit future simulation frame.",
        std::move(scheduleSchema)));

    Json inputInspectProperties = Json::object();
    inputInspectProperties["control"] = Json{{"type", "string"}};
    Json inputInspectSchema = Json{{"type", "object"}, {"properties", inputInspectProperties}, {"required", Json::array({"control"})}, {"additionalProperties", false}};
    tools.push_back(ToolDefinition(
        "trace2d.input.inspect",
        "Inspect deterministic held/pressed/released state for one engine input control.",
        std::move(inputInspectSchema)));

    Json stepProperties = Json::object();
    stepProperties["frames"] = Json{{"type", "integer"}, {"minimum", 1}, {"maximum", MaxStepFrames}};
    Json stepSchema = Json{{"type", "object"}, {"properties", stepProperties}, {"required", Json::array({"frames"})}, {"additionalProperties", false}};
    tools.push_back(ToolDefinition(
        "trace2d.runtime.step",
        "Advance the existing GameplayScenario by an explicit bounded frame count.",
        std::move(stepSchema)));

    Json assertFloatProperties = Json::object();
    assertFloatProperties["selector"] = Json{{"type", "string"}, {"minLength", 1}};
    assertFloatProperties["component"] = Json{{"type", "string"}, {"minLength", 1}};
    assertFloatProperties["field"] = Json{{"type", "string"}, {"minLength", 1}};
    assertFloatProperties["expected"] = Json{{"type", "number"}};
    Json assertFloatSchema = Json{{"type", "object"}, {"properties", assertFloatProperties}, {"required", Json::array({"selector", "component", "field", "expected"})}, {"additionalProperties", false}};
    tools.push_back(ToolDefinition(
        "trace2d.assert_float",
        "Run the existing deterministic gameplay float-field assertion and return its structured failure snapshot.",
        std::move(assertFloatSchema)));

    Json result = Json::object();
    result["resultType"] = "complete";
    result["tools"] = std::move(tools);
    result["ttlMs"] = ToolListTtlMilliseconds;
    result["cacheScope"] = "public";
    return result;
}

bool IsKnownTool(const std::string_view name) noexcept
{
    return name == "trace2d.inspect"
        || name == "trace2d.query"
        || name == "trace2d.ui.inspect"
        || name == "trace2d.ui.query"
        || name == "trace2d.ui.focus"
        || name == "trace2d.ui.activate"
        || name == "trace2d.ui.input_text"
        || name == "trace2d.ui.assert"
        || name == "trace2d.input.schedule"
        || name == "trace2d.input.inspect"
        || name == "trace2d.runtime.step"
        || name == "trace2d.assert_float";
}

Json ExecuteTool(
    const std::string_view name,
    const Json& arguments,
    agent::AgentFacade& agentFacade,
    testing::GameplayScenario& scenario,
    const testing::GameplayFrameUpdate& frameUpdate)
{
    if (!arguments.is_object())
    {
        return MakeToolResult(MakeErrorPayload("invalid_arguments", "Tool arguments must be a JSON object."), true);
    }

    if (name == "trace2d.inspect")
    {
        const agent::InspectionResult result = agentFacade.Inspect();
        if (!result.Succeeded())
        {
            const agent::InspectionError error = result.error.value_or(agent::InspectionError{
                .code = agent::InspectionErrorCode::RuntimeUnavailable,
                .message = "Inspection failed without a structured error.",
            });
            return MakeToolResult(MakeErrorPayload(agent::ToString(error.code), error.message), true);
        }

        Json payload = InspectionSnapshotToJson(*result.snapshot);
        payload["status"] = "ok";
        return MakeToolResult(payload, false);
    }

    if (name == "trace2d.query")
    {
        std::string selector{};
        std::string error{};
        bool one = false;
        if (!ReadRequiredString(arguments, "selector", selector, error)
            || !ReadOptionalBool(arguments, "one", false, one, error))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        }

        Json payload = Json::object();
        payload["status"] = "ok";
        payload["selector"] = selector;
        if (one)
        {
            const agent::QueryOneResult result = agentFacade.QueryOne(selector);
            if (!result.Succeeded())
            {
                const agent::QueryError queryError = result.error.value_or(agent::QueryError{
                    .code = agent::QueryErrorCode::InvalidSelector,
                    .message = "Semantic query failed without a structured error.",
                });
                return MakeToolResult(MakeErrorPayload(agent::ToString(queryError.code), queryError.message), true);
            }
            payload["match"] = EntitySnapshotToJson(*result.match);
        }
        else
        {
            const agent::QueryResult result = agentFacade.Query(selector);
            if (!result.Succeeded())
            {
                const agent::QueryError queryError = result.error.value_or(agent::QueryError{
                    .code = agent::QueryErrorCode::InvalidSelector,
                    .message = "Semantic query failed without a structured error.",
                });
                return MakeToolResult(MakeErrorPayload(agent::ToString(queryError.code), queryError.message), true);
            }

            Json matches = Json::array();
            for (const agent::EntitySnapshot& match : result.matches)
            {
                matches.push_back(EntitySnapshotToJson(match));
            }
            payload["matches"] = std::move(matches);
            payload["match_count"] = result.matches.size();
        }
        return MakeToolResult(payload, false);
    }

    if (name == "trace2d.ui.inspect")
    {
        const agent::UiTreeResult result = agentFacade.InspectUi();
        if (!result.Succeeded())
        {
            const agent::UiAutomationError uiError = result.error.value_or(agent::UiAutomationError{
                .code = agent::UiAutomationErrorCode::UiUnavailable,
                .message = "UI inspection failed without a structured error.",
            });
            return MakeToolResult(MakeErrorPayload(agent::ToString(uiError.code), uiError.message), true);
        }

        Json payload = UiTreeToJson(*result.tree);
        payload["status"] = "ok";
        return MakeToolResult(payload, false);
    }

    if (name == "trace2d.ui.query")
    {
        agent::UiSelector selector{};
        std::string error{};
        bool one = false;
        if (!ParseUiSelector(arguments, selector, error)
            || !ReadOptionalBool(arguments, "one", false, one, error))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        }

        Json payload = Json::object();
        payload["status"] = "ok";
        payload["selector"] = UiSelectorToJson(selector);
        if (one)
        {
            const agent::UiQueryOneResult result = agentFacade.QueryOneUi(selector);
            if (!result.Succeeded())
            {
                const agent::UiAutomationError uiError = result.error.value_or(agent::UiAutomationError{
                    .code = agent::UiAutomationErrorCode::InvalidSelector,
                    .message = "UI query failed without a structured error.",
                });
                return MakeToolResult(MakeErrorPayload(agent::ToString(uiError.code), uiError.message), true);
            }
            payload["match"] = UiElementToJson(*result.match);
        }
        else
        {
            const agent::UiQueryResult result = agentFacade.QueryUi(selector);
            if (!result.Succeeded())
            {
                const agent::UiAutomationError uiError = result.error.value_or(agent::UiAutomationError{
                    .code = agent::UiAutomationErrorCode::InvalidSelector,
                    .message = "UI query failed without a structured error.",
                });
                return MakeToolResult(MakeErrorPayload(agent::ToString(uiError.code), uiError.message), true);
            }
            Json matches = Json::array();
            for (const agent::UiElementSnapshot& match : result.matches)
            {
                matches.push_back(UiElementToJson(match));
            }
            payload["matches"] = std::move(matches);
            payload["match_count"] = result.matches.size();
        }
        return MakeToolResult(payload, false);
    }

    if (name == "trace2d.ui.focus" || name == "trace2d.ui.activate" || name == "trace2d.ui.input_text")
    {
        agent::UiSelector selector{};
        std::string error{};
        if (!ParseUiSelector(arguments, selector, error))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        }

        agent::UiActionResponse result{};
        if (name == "trace2d.ui.focus")
        {
            result = agentFacade.FocusUi(selector);
        }
        else if (name == "trace2d.ui.activate")
        {
            result = agentFacade.ActivateUi(selector);
        }
        else
        {
            std::string text{};
            const Json::const_iterator textValue = arguments.find("text");
            if (textValue == arguments.end() || !textValue->is_string())
            {
                return MakeToolResult(MakeErrorPayload("invalid_arguments", "Missing required string argument: text"), true);
            }
            text = textValue->get<std::string>();
            result = agentFacade.InputUiText(selector, text);
        }

        if (!result.Succeeded())
        {
            const agent::UiAutomationError uiError = result.error.value_or(agent::UiAutomationError{
                .code = agent::UiAutomationErrorCode::ActionRejected,
                .message = "UI action failed without a structured error.",
            });
            return MakeToolResult(MakeErrorPayload(agent::ToString(uiError.code), uiError.message), true);
        }

        Json payload = Json::object();
        payload["status"] = "ok";
        payload["selector"] = UiSelectorToJson(selector);
        payload["element"] = UiElementToJson(*result.element);
        return MakeToolResult(payload, false);
    }

    if (name == "trace2d.ui.assert")
    {
        agent::UiSelector selector{};
        agent::UiExpectedState expected{};
        std::string error{};
        if (!ParseUiSelector(arguments, selector, error) || !ParseUiExpectedState(arguments, expected, error))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        }

        const agent::UiAssertionResult result = agentFacade.AssertUi(selector, expected);
        if (!result.Succeeded())
        {
            const agent::UiAutomationError uiError = result.error.value_or(agent::UiAutomationError{
                .code = agent::UiAutomationErrorCode::StateMismatch,
                .message = "UI assertion failed without a structured error.",
            });
            Json payload = MakeErrorPayload(agent::ToString(uiError.code), uiError.message);
            payload["selector"] = UiSelectorToJson(selector);
            if (result.observed.has_value())
            {
                payload["observed"] = UiElementToJson(*result.observed);
            }
            return MakeToolResult(payload, true);
        }

        Json payload = Json::object();
        payload["status"] = "ok";
        payload["selector"] = UiSelectorToJson(selector);
        payload["observed"] = UiElementToJson(*result.observed);
        return MakeToolResult(payload, false);
    }

    if (name == "trace2d.input.schedule")
    {
        std::uint64_t frame = 0U;
        std::string controlText{};
        std::string eventText{};
        std::string error{};
        if (!ReadUnsigned(arguments, "frame", frame, error)
            || !ReadRequiredString(arguments, "control", controlText, error)
            || !ReadRequiredString(arguments, "event", eventText, error))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        }

        const std::optional<input::InputControl> control = ParseInputControl(controlText);
        if (!control.has_value())
        {
            return MakeToolResult(MakeErrorPayload("invalid_control", "Unknown input control name."), true);
        }
        if (eventText != "press" && eventText != "release")
        {
            return MakeToolResult(MakeErrorPayload("invalid_event", "event must be press or release."), true);
        }

        try
        {
            if (eventText == "press")
            {
                scenario.SchedulePress(frame, *control);
            }
            else
            {
                scenario.ScheduleRelease(frame, *control);
            }
        }
        catch (const std::exception& exception)
        {
            return MakeToolResult(MakeErrorPayload("schedule_rejected", exception.what()), true);
        }

        Json payload = Json::object();
        payload["status"] = "ok";
        payload["current_frame"] = scenario.Runtime().State().frame;
        payload["scheduled_frame"] = frame;
        payload["control"] = std::string{InputControlName(*control)};
        payload["event"] = eventText;
        payload["pending_event_count"] = scenario.Input().PendingScheduledEventCount();
        return MakeToolResult(payload, false);
    }

    if (name == "trace2d.input.inspect")
    {
        std::string controlText{};
        std::string error{};
        if (!ReadRequiredString(arguments, "control", controlText, error))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        }
        const std::optional<input::InputControl> control = ParseInputControl(controlText);
        if (!control.has_value())
        {
            return MakeToolResult(MakeErrorPayload("invalid_control", "Unknown input control name."), true);
        }

        const input::InputControlState state = scenario.Input().State(*control);
        Json payload = Json::object();
        payload["status"] = "ok";
        payload["frame"] = scenario.Input().CurrentFrame();
        payload["control"] = std::string{InputControlName(*control)};
        payload["held"] = state.held;
        payload["pressed"] = state.pressed;
        payload["released"] = state.released;
        payload["pending_event_count"] = scenario.Input().PendingScheduledEventCount();
        return MakeToolResult(payload, false);
    }

    if (name == "trace2d.runtime.step")
    {
        std::uint64_t frames = 0U;
        std::string error{};
        if (!ReadUnsigned(arguments, "frames", frames, error))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        }
        if (frames == 0U || frames > MaxStepFrames)
        {
            return MakeToolResult(MakeErrorPayload("step_out_of_range", "frames must be between 1 and 100000."), true);
        }

        try
        {
            scenario.RunFrames(frames, frameUpdate);
        }
        catch (const std::exception& exception)
        {
            return MakeToolResult(MakeErrorPayload("step_failed", exception.what()), true);
        }

        const runtime::RuntimeState runtimeState = scenario.Runtime().State();
        Json payload = Json::object();
        payload["status"] = "ok";
        payload["frame"] = runtimeState.frame;
        payload["seed"] = runtimeState.seed;
        payload["simulation_time_ns"] = runtimeState.simulationTime.count();
        payload["input_frame"] = scenario.Input().CurrentFrame();
        payload["pending_event_count"] = scenario.Input().PendingScheduledEventCount();
        return MakeToolResult(payload, false);
    }

    if (name == "trace2d.assert_float")
    {
        std::string selector{};
        std::string component{};
        std::string field{};
        std::string error{};
        if (!ReadRequiredString(arguments, "selector", selector, error)
            || !ReadRequiredString(arguments, "component", component, error)
            || !ReadRequiredString(arguments, "field", field, error))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        }

        const Json::const_iterator expectedValue = arguments.find("expected");
        if (expectedValue == arguments.end() || !expectedValue->is_number())
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", "expected must be a number."), true);
        }
        const float expected = expectedValue->get<float>();

        const std::size_t failureCountBefore = scenario.Report().failures.size();
        const bool passed = scenario.AssertFloatFieldEquals(selector, component, field, expected);
        if (!passed)
        {
            Json payload = MakeErrorPayload("gameplay_assertion_failed", "Deterministic gameplay assertion failed.");
            if (scenario.Report().failures.size() > failureCountBefore)
            {
                payload["failure"] = GameplayFailureToJson(scenario.Report().failures.back());
            }
            return MakeToolResult(payload, true);
        }

        Json payload = Json::object();
        payload["status"] = "ok";
        payload["selector"] = selector;
        payload["component"] = component;
        payload["field"] = field;
        payload["expected"] = expected;
        payload["frame"] = scenario.Report().frame;
        payload["seed"] = scenario.Report().seed;
        return MakeToolResult(payload, false);
    }

    return MakeToolResult(MakeErrorPayload("unknown_tool", "Unknown Trace2D MCP tool."), true);
}

Json BuildDiscoveryResult()
{
    Json result = Json::object();
    result["resultType"] = "complete";
    result["supportedVersions"] = Json::array({std::string{ProtocolVersion}, std::string{LegacyProtocolVersion}});
    result["capabilities"] = Json{{"tools", Json::object()}};
    result["serverInfo"] = Json{{"name", "trace2d-mcp"}, {"version", std::string{core::Version()}}};
    result["instructions"] = "Use semantic Trace2D tools for deterministic runtime, scene, UI, input, stepping, and assertions. Coordinates are observational UI bounds, not primary identity.";
    return result;
}

Json BuildLegacyInitializeResult(const Json& params)
{
    std::string negotiatedVersion{LegacyProtocolVersion};
    const Json::const_iterator requested = params.find("protocolVersion");
    if (requested != params.end() && requested->is_string())
    {
        const std::string& version = requested->get_ref<const std::string&>();
        if (version == ProtocolVersion || version == LegacyProtocolVersion)
        {
            negotiatedVersion = version;
        }
    }

    Json result = Json::object();
    result["protocolVersion"] = negotiatedVersion;
    result["capabilities"] = Json{{"tools", Json{{"listChanged", false}}}};
    result["serverInfo"] = Json{{"name", "trace2d-mcp"}, {"version", std::string{core::Version()}}};
    result["instructions"] = "Trace2D protocol adapter over the existing Agent and GameplayScenario contracts.";
    return result;
}
} // namespace

McpServer::McpServer(
    agent::AgentFacade& agent,
    testing::GameplayScenario& scenario,
    testing::GameplayFrameUpdate frameUpdate)
    : agent_{agent}
    , scenario_{scenario}
    , frameUpdate_{std::move(frameUpdate)}
{
}

std::string McpServer::HandleMessage(const std::string_view message)
{
    Json request = Json::parse(message.begin(), message.end(), nullptr, false);
    if (request.is_discarded())
    {
        return MakeJsonRpcError(nullptr, JsonRpcParseError, "Parse error").dump();
    }

    if (!request.is_object())
    {
        return MakeJsonRpcError(nullptr, JsonRpcInvalidRequest, "JSON-RPC request must be an object.").dump();
    }

    const bool hasId = request.contains("id");
    const Json id = hasId ? request["id"] : Json{nullptr};
    if (request.value("jsonrpc", std::string{}) != "2.0")
    {
        return MakeJsonRpcError(hasId ? id : Json{nullptr}, JsonRpcInvalidRequest, "jsonrpc must equal 2.0.").dump();
    }

    const Json::const_iterator methodValue = request.find("method");
    if (methodValue == request.end() || !methodValue->is_string())
    {
        return MakeJsonRpcError(hasId ? id : Json{nullptr}, JsonRpcInvalidRequest, "method must be a string.").dump();
    }

    const std::string& method = methodValue->get_ref<const std::string&>();
    if (!hasId)
    {
        if (method == "notifications/initialized" || method == "notifications/cancelled")
        {
            return {};
        }
        return {};
    }

    try
    {
        if (method == "server/discover")
        {
            return MakeJsonRpcResult(id, BuildDiscoveryResult()).dump();
        }

        if (method == "initialize")
        {
            Json params = Json::object();
            const Json::const_iterator paramsValue = request.find("params");
            if (paramsValue != request.end())
            {
                if (!paramsValue->is_object())
                {
                    return MakeJsonRpcError(id, JsonRpcInvalidParams, "initialize params must be an object.").dump();
                }
                params = *paramsValue;
            }
            return MakeJsonRpcResult(id, BuildLegacyInitializeResult(params)).dump();
        }

        if (method == "ping")
        {
            return MakeJsonRpcResult(id, Json{{"resultType", "complete"}}).dump();
        }

        if (method == "tools/list")
        {
            const Json::const_iterator paramsValue = request.find("params");
            if (paramsValue != request.end() && !paramsValue->is_object())
            {
                return MakeJsonRpcError(id, JsonRpcInvalidParams, "tools/list params must be an object.").dump();
            }
            if (paramsValue != request.end())
            {
                const Json::const_iterator cursor = paramsValue->find("cursor");
                if (cursor != paramsValue->end())
                {
                    return MakeJsonRpcError(id, JsonRpcInvalidParams, "Trace2D exposes one stable non-paginated tool page; cursor must be omitted.").dump();
                }
            }
            return MakeJsonRpcResult(id, BuildToolList()).dump();
        }

        if (method == "tools/call")
        {
            const Json::const_iterator paramsValue = request.find("params");
            if (paramsValue == request.end() || !paramsValue->is_object())
            {
                return MakeJsonRpcError(id, JsonRpcInvalidParams, "tools/call requires object params.").dump();
            }

            const Json::const_iterator nameValue = paramsValue->find("name");
            if (nameValue == paramsValue->end() || !nameValue->is_string())
            {
                return MakeJsonRpcError(id, JsonRpcInvalidParams, "tools/call requires a tool name.").dump();
            }
            const std::string& toolName = nameValue->get_ref<const std::string&>();
            if (!IsKnownTool(toolName))
            {
                return MakeJsonRpcError(id, JsonRpcInvalidParams, std::string{"Unknown tool: "} + toolName).dump();
            }

            Json arguments = Json::object();
            const Json::const_iterator argumentsValue = paramsValue->find("arguments");
            if (argumentsValue != paramsValue->end())
            {
                if (!argumentsValue->is_object())
                {
                    return MakeJsonRpcError(id, JsonRpcInvalidParams, "tools/call arguments must be an object.").dump();
                }
                arguments = *argumentsValue;
            }

            return MakeJsonRpcResult(
                id,
                ExecuteTool(toolName, arguments, agent_, scenario_, frameUpdate_)).dump();
        }

        return MakeJsonRpcError(id, JsonRpcMethodNotFound, "Method not found").dump();
    }
    catch (const std::exception& exception)
    {
        Json error = MakeJsonRpcError(id, JsonRpcInternalError, "Internal MCP server error.");
        error["error"]["data"] = Json{{"detail", exception.what()}};
        return error.dump();
    }
}
} // namespace trace2d::mcp
