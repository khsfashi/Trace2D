#include <trace2d/mcp/McpServer.hpp>
#include <trace2d/mcp/SpriteAnimationMcp.hpp>

#include <trace2d/agent/Inspection.hpp>
#include <trace2d/core/Version.hpp>
#include <trace2d/input/Input.hpp>

#include <nlohmann/json.hpp>

#include <array>
#include <cstddef>
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
constexpr std::uint64_t CacheTtlMilliseconds = 60'000U;

constexpr int JsonRpcParseError = -32700;
constexpr int JsonRpcInvalidRequest = -32600;
constexpr int JsonRpcMethodNotFound = -32601;
constexpr int JsonRpcInvalidParams = -32602;
constexpr int JsonRpcInternalError = -32603;
constexpr int McpUnsupportedProtocolVersion = -32022;

constexpr std::string_view ProtocolVersionMetaKey = "io.modelcontextprotocol/protocolVersion";
constexpr std::string_view ClientCapabilitiesMetaKey = "io.modelcontextprotocol/clientCapabilities";
constexpr std::string_view ServerInfoMetaKey = "io.modelcontextprotocol/serverInfo";

constexpr std::array<std::string_view, 26> LetterControlNames{
    "key_a", "key_b", "key_c", "key_d", "key_e", "key_f", "key_g", "key_h", "key_i",
    "key_j", "key_k", "key_l", "key_m", "key_n", "key_o", "key_p", "key_q", "key_r",
    "key_s", "key_t", "key_u", "key_v", "key_w", "key_x", "key_y", "key_z",
};

Json ServerInfo()
{
    return Json{{"name", "trace2d-mcp"}, {"version", std::string{core::Version()}}};
}

void StampServerInfo(Json& result)
{
    Json meta = Json::object();
    meta[std::string{ServerInfoMetaKey}] = ServerInfo();
    result["_meta"] = std::move(meta);
}

Json MakeJsonRpcError(const Json& id, const int code, const std::string_view message)
{
    Json response = Json::object();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["error"] = Json{{"code", code}, {"message", std::string{message}}};
    return response;
}

Json MakeUnsupportedVersionError(const Json& id, const std::string_view requested)
{
    Json response = MakeJsonRpcError(id, McpUnsupportedProtocolVersion, "Unsupported MCP protocol version.");
    response["error"]["data"] = Json{
        {"supported", Json::array({std::string{ProtocolVersion}})},
        {"requested", std::string{requested}},
    };
    return response;
}

Json MakeJsonRpcResult(const Json& id, Json result)
{
    StampServerInfo(result);
    Json response = Json::object();
    response["jsonrpc"] = "2.0";
    response["id"] = id;
    response["result"] = std::move(result);
    return response;
}

Json MakeErrorPayload(const std::string_view code, const std::string_view message)
{
    return Json{
        {"status", "error"},
        {"error", Json{{"code", std::string{code}}, {"message", std::string{message}}}},
    };
}

Json MakeToolResult(const Json& payload, const bool isError)
{
    Json textContent = Json{{"type", "text"}, {"text", payload.dump()}};
    Json result = Json::object();
    result["resultType"] = "complete";
    result["content"] = Json::array({std::move(textContent)});
    result["structuredContent"] = payload;
    result["isError"] = isError;
    return result;
}

bool ValidateModernMeta(const Json& params, const Json& id, Json& errorResponse)
{
    const Json::const_iterator metaValue = params.find("_meta");
    if (metaValue == params.end() || !metaValue->is_object())
    {
        errorResponse = MakeJsonRpcError(id, JsonRpcInvalidParams, "Modern MCP requests require params._meta.");
        return false;
    }

    const Json& meta = *metaValue;
    const Json::const_iterator protocolVersion = meta.find(std::string{ProtocolVersionMetaKey});
    if (protocolVersion == meta.end() || !protocolVersion->is_string())
    {
        errorResponse = MakeJsonRpcError(id, JsonRpcInvalidParams, "params._meta requires io.modelcontextprotocol/protocolVersion.");
        return false;
    }

    const std::string& requestedVersion = protocolVersion->get_ref<const std::string&>();
    if (requestedVersion != ProtocolVersion)
    {
        errorResponse = MakeUnsupportedVersionError(id, requestedVersion);
        return false;
    }

    const Json::const_iterator capabilities = meta.find(std::string{ClientCapabilitiesMetaKey});
    if (capabilities == meta.end() || !capabilities->is_object())
    {
        errorResponse = MakeJsonRpcError(id, JsonRpcInvalidParams, "params._meta requires object io.modelcontextprotocol/clientCapabilities.");
        return false;
    }

    return true;
}

Json FieldValueToJson(const agent::FieldValue& value)
{
    Json result = Json{{"kind", std::string{agent::ToString(value.kind)}}};
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
    case agent::FieldValueKind::Float2:
        result["value"] = Json::array({value.vectorValue[0], value.vectorValue[1]});
        break;
    case agent::FieldValueKind::Float4:
        result["value"] = Json::array({
            value.vectorValue[0],
            value.vectorValue[1],
            value.vectorValue[2],
            value.vectorValue[3],
        });
        break;
    case agent::FieldValueKind::EntityReference:
    case agent::FieldValueKind::ResourceReference:
    case agent::FieldValueKind::EnumName:
        result["value"] = value.stringValue;
        break;
    }
    return result;
}

Json RuntimeToJson(const agent::RuntimeSnapshot& runtime)
{
    return Json{
        {"frame", runtime.frame},
        {"seed", runtime.seed},
        {"fixed_step_ns", runtime.fixedStepNanoseconds},
        {"simulation_time_ns", runtime.simulationTimeNanoseconds},
    };
}

Json EntityToJson(const agent::EntitySnapshot& entity)
{
    Json components = Json::array();
    for (const agent::ComponentSnapshot& component : entity.components)
    {
        Json fields = Json::array();
        for (const agent::ComponentFieldSnapshot& field : component.fields)
        {
            fields.push_back(Json{{"name", field.name}, {"value", FieldValueToJson(field.value)}});
        }
        components.push_back(Json{{"type", component.type}, {"fields", std::move(fields)}});
    }

    Json result = Json{
        {"handle", Json{{"index", entity.handle.index}, {"generation", entity.handle.generation}}},
        {"id", entity.semanticId},
        {"name", entity.name},
        {"tags", entity.tags},
        {"transform", Json{
            {"position", Json{{"x", entity.transform.position.x}, {"y", entity.transform.position.y}}},
            {"rotation_radians", entity.transform.rotationRadians},
            {"scale", Json{{"x", entity.transform.scale.x}, {"y", entity.transform.scale.y}}},
        }},
        {"components", std::move(components)},
    };

    if (entity.bounds.has_value())
    {
        result["bounds"] = Json{
            {"center", Json{{"x", entity.bounds->center.x}, {"y", entity.bounds->center.y}}},
            {"extents", Json{{"x", entity.bounds->extents.x}, {"y", entity.bounds->extents.y}}},
        };
    }
    else
    {
        result["bounds"] = nullptr;
    }
    return result;
}

Json InspectionToJson(const agent::InspectionSnapshot& snapshot)
{
    Json entities = Json::array();
    for (const agent::EntitySnapshot& entity : snapshot.scene.entities)
    {
        entities.push_back(EntityToJson(entity));
    }
    return Json{
        {"runtime", RuntimeToJson(snapshot.runtime)},
        {"scene", Json{{"id", snapshot.scene.semanticId}, {"name", snapshot.scene.name}, {"entities", std::move(entities)}}},
    };
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
    return Json{
        {"id", element.id},
        {"role", std::string{agent::ToString(element.role)}},
        {"name", element.name},
        {"bounds", Json{{"x", element.bounds.x}, {"y", element.bounds.y}, {"width", element.bounds.width}, {"height", element.bounds.height}}},
        {"visible", element.visible},
        {"enabled", element.enabled},
        {"focused", element.focused},
        {"text", element.text},
        {"activation_count", element.activationCount},
    };
}

Json UiTreeToJson(const agent::UiTreeSnapshot& tree)
{
    Json elements = Json::array();
    for (const agent::UiElementSnapshot& element : tree.elements)
    {
        elements.push_back(UiElementToJson(element));
    }
    return Json{{"width", tree.width}, {"height", tree.height}, {"elements", std::move(elements)}};
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
    if (control == input::InputControl::ArrowLeft) return "arrow_left";
    if (control == input::InputControl::ArrowRight) return "arrow_right";
    if (control == input::InputControl::ArrowUp) return "arrow_up";
    if (control == input::InputControl::ArrowDown) return "arrow_down";
    if (control == input::InputControl::Space) return "space";
    if (control == input::InputControl::Enter) return "enter";
    if (control == input::InputControl::Escape) return "escape";
    if (control == input::InputControl::MouseLeft) return "mouse_left";
    if (control == input::InputControl::MouseMiddle) return "mouse_middle";
    if (control == input::InputControl::MouseRight) return "mouse_right";
    return "unknown";
}

std::optional<input::InputControl> ParseInputControl(const std::string_view name) noexcept
{
    char character = '\0';
    if (name.size() == 1U)
    {
        character = name.front();
    }
    else if (name.size() == 5U && name.substr(0U, 4U) == "key_")
    {
        character = name[4U];
    }

    if (character >= 'A' && character <= 'Z')
    {
        character = static_cast<char>(character - 'A' + 'a');
    }
    if (character >= 'a' && character <= 'z')
    {
        const std::uint16_t first = static_cast<std::uint16_t>(input::InputControl::KeyA);
        return static_cast<input::InputControl>(first + static_cast<std::uint16_t>(character - 'a'));
    }

    if (name == "arrow_left") return input::InputControl::ArrowLeft;
    if (name == "arrow_right") return input::InputControl::ArrowRight;
    if (name == "arrow_up") return input::InputControl::ArrowUp;
    if (name == "arrow_down") return input::InputControl::ArrowDown;
    if (name == "space") return input::InputControl::Space;
    if (name == "enter") return input::InputControl::Enter;
    if (name == "escape") return input::InputControl::Escape;
    if (name == "mouse_left") return input::InputControl::MouseLeft;
    if (name == "mouse_middle") return input::InputControl::MouseMiddle;
    if (name == "mouse_right") return input::InputControl::MouseRight;
    return std::nullopt;
}

std::optional<agent::UiRole> ParseUiRole(const std::string_view role) noexcept
{
    if (role == "panel") return agent::UiRole::Panel;
    if (role == "label") return agent::UiRole::Label;
    if (role == "button") return agent::UiRole::Button;
    if (role == "textbox") return agent::UiRole::TextBox;
    return std::nullopt;
}

bool ReadString(const Json& object, const char* key, std::string& value, std::string& error)
{
    const Json::const_iterator found = object.find(key);
    if (found == object.end() || !found->is_string() || found->get_ref<const std::string&>().empty())
    {
        error = std::string{"Missing required non-empty string argument: "} + key;
        return false;
    }
    value = found->get<std::string>();
    return true;
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
            error = "selector.role must be panel, label, button, or textbox.";
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
        error = "selector requires id, role, or name.";
        return false;
    }
    return true;
}

bool ParseUiExpected(const Json& arguments, agent::UiExpectedState& expected, std::string& error)
{
    const Json::const_iterator expectedValue = arguments.find("expected");
    if (expectedValue == arguments.end() || !expectedValue->is_object())
    {
        error = "Missing required object argument: expected";
        return false;
    }

    const Json& object = *expectedValue;
    auto readOptionalBool = [&](const char* key, std::optional<bool>& destination) -> bool
    {
        const Json::const_iterator found = object.find(key);
        if (found == object.end()) return true;
        if (!found->is_boolean())
        {
            error = std::string{"expected."} + key + " must be boolean.";
            return false;
        }
        destination = found->get<bool>();
        return true;
    };

    if (!readOptionalBool("visible", expected.visible)
        || !readOptionalBool("enabled", expected.enabled)
        || !readOptionalBool("focused", expected.focused))
    {
        return false;
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
        error = "expected requires at least one state field.";
        return false;
    }
    return true;
}

Json GameplayFailureToJson(const testing::GameplayAssertionFailure& failure)
{
    Json relevantInput = Json::array();
    for (const testing::InputStateSnapshot& inputState : failure.snapshot.relevantInput)
    {
        relevantInput.push_back(Json{
            {"control", std::string{InputControlName(inputState.control)}},
            {"held", inputState.state.held},
            {"pressed", inputState.state.pressed},
            {"released", inputState.state.released},
        });
    }

    Json snapshot = Json{
        {"runtime", RuntimeToJson(failure.snapshot.runtime)},
        {"input_frame", failure.snapshot.inputFrame},
        {"relevant_input", std::move(relevantInput)},
    };
    snapshot["entity"] = failure.snapshot.entity.has_value() ? EntityToJson(*failure.snapshot.entity) : Json{nullptr};

    Json result = Json{
        {"code", std::string{testing::ToString(failure.code)}},
        {"selector", failure.selector},
        {"component", failure.componentType},
        {"field", failure.fieldName},
        {"expected", FieldValueToJson(failure.expected)},
        {"frame", failure.frame},
        {"seed", failure.seed},
        {"detail", failure.detail},
        {"snapshot", std::move(snapshot)},
    };
    result["observed"] = failure.observed.has_value() ? FieldValueToJson(*failure.observed) : Json{nullptr};
    return result;
}

Json EmptyObjectSchema()
{
    return Json{{"type", "object"}, {"additionalProperties", false}};
}

Json UiSelectorSchema()
{
    return Json{
        {"type", "object"},
        {"properties", Json{
            {"id", Json{{"type", "string"}, {"minLength", 1}}},
            {"role", Json{{"type", "string"}, {"enum", Json::array({"panel", "label", "button", "textbox"})}}},
            {"name", Json{{"type", "string"}, {"minLength", 1}}},
        }},
        {"minProperties", 1},
        {"additionalProperties", false},
    };
}

Json ToolDefinition(const std::string_view name, const std::string_view description, Json inputSchema)
{
    return Json{
        {"name", std::string{name}},
        {"description", std::string{description}},
        {"inputSchema", std::move(inputSchema)},
    };
}

Json BuildToolList(const std::span<const agent::SpriteAnimatorBinding> spriteAnimators)
{
    Json tools = Json::array();
    tools.push_back(ToolDefinition("trace2d.inspect", "Inspect deterministic runtime and authored scene state through Trace2D::Agent.", EmptyObjectSchema()));
    tools.push_back(ToolDefinition("trace2d.query", "Query entities through the existing semantic selector vocabulary.", Json{
        {"type", "object"}, {"properties", Json{{"selector", Json{{"type", "string"}, {"minLength", 1}}}, {"one", Json{{"type", "boolean"}, {"default", false}}}}}, {"required", Json::array({"selector"})}, {"additionalProperties", false}}));
    tools.push_back(ToolDefinition("trace2d.ui.inspect", "Inspect the engine-owned semantic UI tree without renderer initialization.", EmptyObjectSchema()));
    tools.push_back(ToolDefinition("trace2d.ui.query", "Query semantic UI controls by stable id, role, name, or compound selector.", Json{
        {"type", "object"}, {"properties", Json{{"selector", UiSelectorSchema()}, {"one", Json{{"type", "boolean"}, {"default", false}}}}}, {"required", Json::array({"selector"})}, {"additionalProperties", false}}));

    const Json uiActionSchema = Json{{"type", "object"}, {"properties", Json{{"selector", UiSelectorSchema()}}}, {"required", Json::array({"selector"})}, {"additionalProperties", false}};
    tools.push_back(ToolDefinition("trace2d.ui.focus", "Focus one semantic UI control through the existing Agent facade.", uiActionSchema));
    tools.push_back(ToolDefinition("trace2d.ui.activate", "Activate one semantic UI control through the existing Agent facade.", uiActionSchema));
    tools.push_back(ToolDefinition("trace2d.ui.input_text", "Input text into one focused semantic text box.", Json{
        {"type", "object"}, {"properties", Json{{"selector", UiSelectorSchema()}, {"text", Json{{"type", "string"}}}}}, {"required", Json::array({"selector", "text"})}, {"additionalProperties", false}}));
    tools.push_back(ToolDefinition("trace2d.ui.assert", "Assert semantic UI state and return structured deterministic diagnostics.", Json{
        {"type", "object"}, {"properties", Json{
            {"selector", UiSelectorSchema()},
            {"expected", Json{{"type", "object"}, {"properties", Json{
                {"visible", Json{{"type", "boolean"}}}, {"enabled", Json{{"type", "boolean"}}}, {"focused", Json{{"type", "boolean"}}},
                {"text", Json{{"type", "string"}}}, {"activation_count", Json{{"type", "integer"}, {"minimum", 0}}}}}, {"minProperties", 1}, {"additionalProperties", false}}},
        }}, {"required", Json::array({"selector", "expected"})}, {"additionalProperties", false}}));
    tools.push_back(ToolDefinition("trace2d.input.schedule", "Schedule deterministic virtual input at an explicit future simulation frame.", Json{
        {"type", "object"}, {"properties", Json{
            {"frame", Json{{"type", "integer"}, {"minimum", 1}}}, {"control", Json{{"type", "string"}}},
            {"event", Json{{"type", "string"}, {"enum", Json::array({"press", "release"})}}}}},
        {"required", Json::array({"frame", "control", "event"})}, {"additionalProperties", false}}));
    tools.push_back(ToolDefinition("trace2d.input.inspect", "Inspect deterministic held/pressed/released state for one input control.", Json{
        {"type", "object"}, {"properties", Json{{"control", Json{{"type", "string"}}}}}, {"required", Json::array({"control"})}, {"additionalProperties", false}}));
    tools.push_back(ToolDefinition("trace2d.runtime.step", "Advance the existing GameplayScenario by an explicit bounded frame count.", Json{
        {"type", "object"}, {"properties", Json{{"frames", Json{{"type", "integer"}, {"minimum", 1}, {"maximum", MaxStepFrames}}}}}, {"required", Json::array({"frames"})}, {"additionalProperties", false}}));

    if (!spriteAnimators.empty())
    {
        AppendSpriteAnimationTools(tools);
    }

    tools.push_back(ToolDefinition("trace2d.assert_float", "Run the existing deterministic gameplay float-field assertion.", Json{
        {"type", "object"}, {"properties", Json{
            {"selector", Json{{"type", "string"}, {"minLength", 1}}}, {"component", Json{{"type", "string"}, {"minLength", 1}}},
            {"field", Json{{"type", "string"}, {"minLength", 1}}}, {"expected", Json{{"type", "number"}}}}},
        {"required", Json::array({"selector", "component", "field", "expected"})}, {"additionalProperties", false}}));

    return Json{
        {"resultType", "complete"},
        {"tools", std::move(tools)},
        {"ttlMs", CacheTtlMilliseconds},
        {"cacheScope", "public"},
    };
}

bool IsKnownTool(const std::string_view name) noexcept
{
    return name == "trace2d.inspect" || name == "trace2d.query" || name == "trace2d.ui.inspect"
        || name == "trace2d.ui.query" || name == "trace2d.ui.focus" || name == "trace2d.ui.activate"
        || name == "trace2d.ui.input_text" || name == "trace2d.ui.assert" || name == "trace2d.input.schedule"
        || name == "trace2d.input.inspect" || name == "trace2d.runtime.step" || name == "trace2d.assert_float"
        || IsSpriteAnimationTool(name);
}

Json ExecuteTool(
    const std::string_view name,
    const Json& arguments,
    agent::AgentFacade& agentFacade,
    testing::GameplayScenario& scenario,
    const testing::GameplayFrameUpdate& frameUpdate,
    const std::span<const agent::SpriteAnimatorBinding> spriteAnimators)
{
    if (!arguments.is_object())
    {
        return MakeToolResult(MakeErrorPayload("invalid_arguments", "Tool arguments must be a JSON object."), true);
    }

    if (IsSpriteAnimationTool(name))
    {
        return ExecuteSpriteAnimationTool(name, arguments, agentFacade, spriteAnimators);
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
        Json payload = InspectionToJson(*result.snapshot);
        payload["status"] = "ok";
        return MakeToolResult(payload, false);
    }

    if (name == "trace2d.query")
    {
        std::string selector{};
        std::string error{};
        if (!ReadString(arguments, "selector", selector, error))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        }
        bool one = false;
        const Json::const_iterator oneValue = arguments.find("one");
        if (oneValue != arguments.end())
        {
            if (!oneValue->is_boolean()) return MakeToolResult(MakeErrorPayload("invalid_arguments", "one must be boolean."), true);
            one = oneValue->get<bool>();
        }

        if (one)
        {
            const agent::QueryOneResult result = agentFacade.QueryOne(selector);
            if (!result.Succeeded())
            {
                const agent::QueryError queryError = result.error.value_or(agent::QueryError{.code = agent::QueryErrorCode::InvalidSelector, .message = "Query failed without a structured error."});
                return MakeToolResult(MakeErrorPayload(agent::ToString(queryError.code), queryError.message), true);
            }
            return MakeToolResult(Json{{"status", "ok"}, {"selector", selector}, {"match", EntityToJson(*result.match)}}, false);
        }

        const agent::QueryResult result = agentFacade.Query(selector);
        if (!result.Succeeded())
        {
            const agent::QueryError queryError = result.error.value_or(agent::QueryError{.code = agent::QueryErrorCode::InvalidSelector, .message = "Query failed without a structured error."});
            return MakeToolResult(MakeErrorPayload(agent::ToString(queryError.code), queryError.message), true);
        }
        Json matches = Json::array();
        for (const agent::EntitySnapshot& match : result.matches) matches.push_back(EntityToJson(match));
        return MakeToolResult(Json{{"status", "ok"}, {"selector", selector}, {"matches", std::move(matches)}, {"match_count", result.matches.size()}}, false);
    }

    if (name == "trace2d.ui.inspect")
    {
        const agent::UiTreeResult result = agentFacade.InspectUi();
        if (!result.Succeeded())
        {
            const agent::UiAutomationError uiError = result.error.value_or(agent::UiAutomationError{.code = agent::UiAutomationErrorCode::UiUnavailable, .message = "UI inspection failed without a structured error."});
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
        if (!ParseUiSelector(arguments, selector, error)) return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        bool one = false;
        const Json::const_iterator oneValue = arguments.find("one");
        if (oneValue != arguments.end())
        {
            if (!oneValue->is_boolean()) return MakeToolResult(MakeErrorPayload("invalid_arguments", "one must be boolean."), true);
            one = oneValue->get<bool>();
        }

        if (one)
        {
            const agent::UiQueryOneResult result = agentFacade.QueryOneUi(selector);
            if (!result.Succeeded())
            {
                const agent::UiAutomationError uiError = result.error.value_or(agent::UiAutomationError{.code = agent::UiAutomationErrorCode::InvalidSelector, .message = "UI query failed without a structured error."});
                return MakeToolResult(MakeErrorPayload(agent::ToString(uiError.code), uiError.message), true);
            }
            return MakeToolResult(Json{{"status", "ok"}, {"selector", UiSelectorToJson(selector)}, {"match", UiElementToJson(*result.match)}}, false);
        }

        const agent::UiQueryResult result = agentFacade.QueryUi(selector);
        if (!result.Succeeded())
        {
            const agent::UiAutomationError uiError = result.error.value_or(agent::UiAutomationError{.code = agent::UiAutomationErrorCode::InvalidSelector, .message = "UI query failed without a structured error."});
            return MakeToolResult(MakeErrorPayload(agent::ToString(uiError.code), uiError.message), true);
        }
        Json matches = Json::array();
        for (const agent::UiElementSnapshot& match : result.matches) matches.push_back(UiElementToJson(match));
        return MakeToolResult(Json{{"status", "ok"}, {"selector", UiSelectorToJson(selector)}, {"matches", std::move(matches)}, {"match_count", result.matches.size()}}, false);
    }

    if (name == "trace2d.ui.focus" || name == "trace2d.ui.activate" || name == "trace2d.ui.input_text")
    {
        agent::UiSelector selector{};
        std::string error{};
        if (!ParseUiSelector(arguments, selector, error)) return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);

        agent::UiActionResponse result{};
        if (name == "trace2d.ui.focus") result = agentFacade.FocusUi(selector);
        else if (name == "trace2d.ui.activate") result = agentFacade.ActivateUi(selector);
        else
        {
            const Json::const_iterator text = arguments.find("text");
            if (text == arguments.end() || !text->is_string()) return MakeToolResult(MakeErrorPayload("invalid_arguments", "text must be a string."), true);
            result = agentFacade.InputUiText(selector, text->get_ref<const std::string&>());
        }

        if (!result.Succeeded())
        {
            const agent::UiAutomationError uiError = result.error.value_or(agent::UiAutomationError{.code = agent::UiAutomationErrorCode::ActionRejected, .message = "UI action failed without a structured error."});
            return MakeToolResult(MakeErrorPayload(agent::ToString(uiError.code), uiError.message), true);
        }
        return MakeToolResult(Json{{"status", "ok"}, {"selector", UiSelectorToJson(selector)}, {"element", UiElementToJson(*result.element)}}, false);
    }

    if (name == "trace2d.ui.assert")
    {
        agent::UiSelector selector{};
        agent::UiExpectedState expected{};
        std::string error{};
        if (!ParseUiSelector(arguments, selector, error) || !ParseUiExpected(arguments, expected, error))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        }
        const agent::UiAssertionResult result = agentFacade.AssertUi(selector, expected);
        if (!result.Succeeded())
        {
            const agent::UiAutomationError uiError = result.error.value_or(agent::UiAutomationError{.code = agent::UiAutomationErrorCode::StateMismatch, .message = "UI assertion failed without a structured error."});
            Json payload = MakeErrorPayload(agent::ToString(uiError.code), uiError.message);
            payload["selector"] = UiSelectorToJson(selector);
            if (result.observed.has_value()) payload["observed"] = UiElementToJson(*result.observed);
            return MakeToolResult(payload, true);
        }
        return MakeToolResult(Json{{"status", "ok"}, {"selector", UiSelectorToJson(selector)}, {"observed", UiElementToJson(*result.observed)}}, false);
    }

    if (name == "trace2d.input.schedule")
    {
        std::uint64_t frame = 0U;
        std::string controlText{};
        std::string eventText{};
        std::string error{};
        if (!ReadUnsigned(arguments, "frame", frame, error) || !ReadString(arguments, "control", controlText, error) || !ReadString(arguments, "event", eventText, error))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        }
        const std::optional<input::InputControl> control = ParseInputControl(controlText);
        if (!control.has_value()) return MakeToolResult(MakeErrorPayload("invalid_control", "Unknown input control name."), true);
        if (eventText != "press" && eventText != "release") return MakeToolResult(MakeErrorPayload("invalid_event", "event must be press or release."), true);

        try
        {
            if (eventText == "press") scenario.SchedulePress(frame, *control);
            else scenario.ScheduleRelease(frame, *control);
        }
        catch (const std::exception& exception)
        {
            return MakeToolResult(MakeErrorPayload("schedule_rejected", exception.what()), true);
        }

        return MakeToolResult(Json{
            {"status", "ok"}, {"current_frame", scenario.Runtime().State().frame}, {"scheduled_frame", frame},
            {"control", std::string{InputControlName(*control)}}, {"event", eventText}, {"pending_event_count", scenario.Input().PendingScheduledEventCount()}}, false);
    }

    if (name == "trace2d.input.inspect")
    {
        std::string controlText{};
        std::string error{};
        if (!ReadString(arguments, "control", controlText, error)) return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        const std::optional<input::InputControl> control = ParseInputControl(controlText);
        if (!control.has_value()) return MakeToolResult(MakeErrorPayload("invalid_control", "Unknown input control name."), true);
        const input::InputControlState state = scenario.Input().State(*control);
        return MakeToolResult(Json{
            {"status", "ok"}, {"frame", scenario.Input().CurrentFrame()}, {"control", std::string{InputControlName(*control)}},
            {"held", state.held}, {"pressed", state.pressed}, {"released", state.released}, {"pending_event_count", scenario.Input().PendingScheduledEventCount()}}, false);
    }

    if (name == "trace2d.runtime.step")
    {
        std::uint64_t frames = 0U;
        std::string error{};
        if (!ReadUnsigned(arguments, "frames", frames, error)) return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        if (frames == 0U || frames > MaxStepFrames) return MakeToolResult(MakeErrorPayload("step_out_of_range", "frames must be between 1 and 100000."), true);
        try
        {
            scenario.RunFrames(frames, frameUpdate);
        }
        catch (const std::exception& exception)
        {
            return MakeToolResult(MakeErrorPayload("step_failed", exception.what()), true);
        }
        const runtime::RuntimeState state = scenario.Runtime().State();
        return MakeToolResult(Json{
            {"status", "ok"}, {"frame", state.frame}, {"seed", state.seed}, {"simulation_time_ns", state.simulationTime.count()},
            {"input_frame", scenario.Input().CurrentFrame()}, {"pending_event_count", scenario.Input().PendingScheduledEventCount()}}, false);
    }

    if (name == "trace2d.assert_float")
    {
        std::string selector{};
        std::string component{};
        std::string field{};
        std::string error{};
        if (!ReadString(arguments, "selector", selector, error) || !ReadString(arguments, "component", component, error) || !ReadString(arguments, "field", field, error))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", error), true);
        }
        const Json::const_iterator expectedValue = arguments.find("expected");
        if (expectedValue == arguments.end() || !expectedValue->is_number()) return MakeToolResult(MakeErrorPayload("invalid_arguments", "expected must be a number."), true);
        const float expected = expectedValue->get<float>();
        const std::size_t previousFailureCount = scenario.Report().failures.size();
        if (!scenario.AssertFloatFieldEquals(selector, component, field, expected))
        {
            Json payload = MakeErrorPayload("gameplay_assertion_failed", "Deterministic gameplay assertion failed.");
            if (scenario.Report().failures.size() > previousFailureCount) payload["failure"] = GameplayFailureToJson(scenario.Report().failures.back());
            return MakeToolResult(payload, true);
        }
        return MakeToolResult(Json{
            {"status", "ok"}, {"selector", selector}, {"component", component}, {"field", field}, {"expected", expected},
            {"frame", scenario.Report().frame}, {"seed", scenario.Report().seed}}, false);
    }

    return MakeToolResult(MakeErrorPayload("unknown_tool", "Unknown Trace2D MCP tool."), true);
}

Json BuildDiscoveryResult()
{
    return Json{
        {"resultType", "complete"},
        {"supportedVersions", Json::array({std::string{ProtocolVersion}})},
        {"capabilities", Json{{"tools", Json::object()}}},
        {"instructions", "Use semantic Trace2D tools for deterministic runtime, scene, UI, input, Sprite animation, stepping, and assertions. Coordinates are observational UI bounds, not primary identity."},
        {"ttlMs", CacheTtlMilliseconds},
        {"cacheScope", "public"},
    };
}
} // namespace

McpServer::McpServer(
    agent::AgentFacade& agent,
    testing::GameplayScenario& scenario,
    testing::GameplayFrameUpdate frameUpdate,
    const std::span<const agent::SpriteAnimatorBinding> spriteAnimators)
    : agent_{agent}
    , scenario_{scenario}
    , frameUpdate_{std::move(frameUpdate)}
    , spriteAnimators_{spriteAnimators}
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
        return {};
    }

    try
    {
        Json params = Json::object();
        const Json::const_iterator paramsValue = request.find("params");
        if (paramsValue != request.end())
        {
            if (!paramsValue->is_object())
            {
                return MakeJsonRpcError(id, JsonRpcInvalidParams, "params must be an object.").dump();
            }
            params = *paramsValue;
        }

        if (method == "initialize")
        {
            std::string requestedVersion{"legacy-initialize"};
            const Json::const_iterator versionValue = params.find("protocolVersion");
            if (versionValue != params.end() && versionValue->is_string())
            {
                requestedVersion = versionValue->get<std::string>();
            }
            return MakeUnsupportedVersionError(id, requestedVersion).dump();
        }

        Json metaError{};
        if (!ValidateModernMeta(params, id, metaError))
        {
            return metaError.dump();
        }

        if (method == "server/discover")
        {
            return MakeJsonRpcResult(id, BuildDiscoveryResult()).dump();
        }
        if (method == "ping")
        {
            return MakeJsonRpcResult(id, Json{{"resultType", "complete"}}).dump();
        }
        if (method == "tools/list")
        {
            if (params.contains("cursor"))
            {
                return MakeJsonRpcError(id, JsonRpcInvalidParams, "Trace2D exposes one non-paginated tool page; cursor must be omitted.").dump();
            }
            return MakeJsonRpcResult(id, BuildToolList(spriteAnimators_)).dump();
        }
        if (method == "tools/call")
        {
            const Json::const_iterator nameValue = params.find("name");
            if (nameValue == params.end() || !nameValue->is_string())
            {
                return MakeJsonRpcError(id, JsonRpcInvalidParams, "tools/call requires a tool name.").dump();
            }
            const std::string& toolName = nameValue->get_ref<const std::string&>();
            if (!IsKnownTool(toolName))
            {
                return MakeJsonRpcError(id, JsonRpcInvalidParams, std::string{"Unknown tool: "} + toolName).dump();
            }

            Json arguments = Json::object();
            const Json::const_iterator argumentsValue = params.find("arguments");
            if (argumentsValue != params.end())
            {
                if (!argumentsValue->is_object())
                {
                    return MakeJsonRpcError(id, JsonRpcInvalidParams, "tools/call arguments must be an object.").dump();
                }
                arguments = *argumentsValue;
            }
            return MakeJsonRpcResult(
                       id,
                       ExecuteTool(toolName, arguments, agent_, scenario_, frameUpdate_, spriteAnimators_))
                .dump();
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
