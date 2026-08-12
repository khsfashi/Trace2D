#include <trace2d/mcp/SpriteAnimationMcp.hpp>

#include <trace2d/agent/Inspection.hpp>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace trace2d::mcp
{
namespace
{
using Json = nlohmann::json;

Json MakeErrorPayload(const std::string_view code, const std::string_view message)
{
    return Json{
        {"status", "error"},
        {"error", Json{{"code", std::string{code}}, {"message", std::string{message}}}},
    };
}

Json MakeToolResult(const Json& payload, const bool isError)
{
    Json result = Json::object();
    result["resultType"] = "complete";
    result["content"] = Json::array({Json{{"type", "text"}, {"text", payload.dump()}}});
    result["structuredContent"] = payload;
    result["isError"] = isError;
    return result;
}

Json ToolDefinition(
    const std::string_view name,
    const std::string_view description,
    Json inputSchema,
    const bool readOnly)
{
    return Json{
        {"name", std::string{name}},
        {"description", std::string{description}},
        {"inputSchema", std::move(inputSchema)},
        {"annotations", Json{
            {"readOnlyHint", readOnly},
            {"destructiveHint", false},
            {"idempotentHint", readOnly},
            {"openWorldHint", false},
        }},
    };
}

Json EntityIdProperty()
{
    return Json{{"type", "string"}, {"minLength", 1}};
}

Json SnapshotToJson(const agent::SpriteAnimatorSnapshot& snapshot)
{
    return Json{
        {"entity_id", snapshot.entitySemanticId},
        {"clip_duration_ns", snapshot.clipDurationNanoseconds},
        {"clip_frame_count", snapshot.clipFrameCount},
        {"clip_event_count", snapshot.clipEventCount},
        {"time_ns", snapshot.timeNanoseconds},
        {"frame_index", snapshot.frameIndex},
        {"region_index", snapshot.regionIndex},
        {"playback", std::string{agent::ToString(snapshot.playback)}},
        {"loop_mode", std::string{agent::ToString(snapshot.loopMode)}},
        {"direction", std::string{agent::ToString(snapshot.direction)}},
        {"completed", snapshot.completed},
        {"speed", Json{
            {"numerator", snapshot.speedNumerator},
            {"denominator", snapshot.speedDenominator},
            {"remainder", snapshot.speedRemainder},
        }},
    };
}

Json EmissionToJson(const agent::SpriteAnimationEmissionSnapshot& emission)
{
    return Json{
        {"kind", std::string{agent::ToString(emission.kind)}},
        {"event_id", emission.eventId},
        {"authored_ordinal", emission.authoredOrdinal},
        {"time_ns", emission.timeNanoseconds},
        {"direction", std::string{agent::ToString(emission.direction)}},
    };
}

Json ValueToJson(const agent::SpriteAnimationValue& value)
{
    Json result{{"kind", std::string{agent::ToString(value.kind)}}};
    switch (value.kind)
    {
    case agent::SpriteAnimationValueKind::Boolean:
        result["value"] = value.booleanValue;
        break;
    case agent::SpriteAnimationValueKind::SignedInteger:
        result["value"] = value.signedIntegerValue;
        break;
    case agent::SpriteAnimationValueKind::UnsignedInteger:
        result["value"] = value.unsignedIntegerValue;
        break;
    case agent::SpriteAnimationValueKind::String:
        result["value"] = value.stringValue;
        break;
    }
    return result;
}

Json ContextToJson(const agent::SpriteAnimationAssertionContext& context)
{
    return Json{
        {"entity_id", context.entitySemanticId},
        {"time_ns", context.timeNanoseconds},
        {"frame_index", context.frameIndex},
        {"region_index", context.regionIndex},
        {"playback", std::string{agent::ToString(context.playback)}},
        {"loop_mode", std::string{agent::ToString(context.loopMode)}},
        {"direction", std::string{agent::ToString(context.direction)}},
        {"completed", context.completed},
    };
}

Json ErrorToJson(const agent::SpriteAnimationInspectionError& error)
{
    return Json{
        {"code", std::string{agent::ToString(error.code)}},
        {"message", error.message},
        {"runtime_error", std::string{agent::ToString(error.runtimeError)}},
    };
}

bool ReadNonEmptyString(
    const Json& object,
    const char* key,
    std::string& value,
    std::string& error)
{
    const auto found = object.find(key);
    if (found == object.end() || !found->is_string() || found->get_ref<const std::string&>().empty())
    {
        error = std::string{"Missing required non-empty string argument: "} + key;
        return false;
    }
    value = found->get<std::string>();
    return true;
}

bool ReadSigned(
    const Json& object,
    const char* key,
    std::int64_t& value,
    std::string& error)
{
    const auto found = object.find(key);
    if (found == object.end() || !found->is_number_integer())
    {
        error = std::string{"Missing required signed integer argument: "} + key;
        return false;
    }
    if (found->is_number_unsigned())
    {
        const std::uint64_t unsignedValue = found->get<std::uint64_t>();
        if (unsignedValue > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        {
            error = std::string{"Signed integer argument is out of range: "} + key;
            return false;
        }
        value = static_cast<std::int64_t>(unsignedValue);
        return true;
    }
    value = found->get<std::int64_t>();
    return true;
}

bool ReadUnsigned32(
    const Json& object,
    const char* key,
    std::uint32_t& value,
    std::string& error)
{
    const auto found = object.find(key);
    if (found == object.end() || !found->is_number_integer())
    {
        error = std::string{"Missing required non-negative integer argument: "} + key;
        return false;
    }

    std::uint64_t candidate = 0U;
    if (found->is_number_unsigned())
    {
        candidate = found->get<std::uint64_t>();
    }
    else
    {
        const std::int64_t signedValue = found->get<std::int64_t>();
        if (signedValue < 0)
        {
            error = std::string{"Argument must be non-negative: "} + key;
            return false;
        }
        candidate = static_cast<std::uint64_t>(signedValue);
    }

    if (candidate > std::numeric_limits<std::uint32_t>::max())
    {
        error = std::string{"Integer argument exceeds uint32 range: "} + key;
        return false;
    }
    value = static_cast<std::uint32_t>(candidate);
    return true;
}

bool ReadSize(
    const Json& object,
    const char* key,
    std::size_t& value,
    std::string& error)
{
    std::uint32_t parsed = 0U;
    if (!ReadUnsigned32(object, key, parsed, error)) return false;
    value = parsed;
    return true;
}

[[nodiscard]] const agent::SpriteAnimatorBinding* FindBinding(
    const std::span<const agent::SpriteAnimatorBinding> bindings,
    const std::string_view entityId) noexcept
{
    for (const agent::SpriteAnimatorBinding& binding : bindings)
    {
        if (binding.entitySemanticId == entityId) return &binding;
    }
    return nullptr;
}

[[nodiscard]] std::optional<agent::SpriteAnimationActionKind> ParseActionKind(
    const std::string_view value) noexcept
{
    if (value == "play") return agent::SpriteAnimationActionKind::Play;
    if (value == "pause") return agent::SpriteAnimationActionKind::Pause;
    if (value == "stop") return agent::SpriteAnimationActionKind::Stop;
    if (value == "reset") return agent::SpriteAnimationActionKind::Reset;
    if (value == "restart") return agent::SpriteAnimationActionKind::Restart;
    if (value == "seek") return agent::SpriteAnimationActionKind::Seek;
    if (value == "set_speed") return agent::SpriteAnimationActionKind::SetSpeed;
    if (value == "set_direction") return agent::SpriteAnimationActionKind::SetDirection;
    if (value == "advance") return agent::SpriteAnimationActionKind::Advance;
    return std::nullopt;
}

[[nodiscard]] std::optional<runtime::SpriteAnimationDirection> ParseDirection(
    const std::string_view value) noexcept
{
    if (value == "forward") return runtime::SpriteAnimationDirection::Forward;
    if (value == "reverse") return runtime::SpriteAnimationDirection::Reverse;
    return std::nullopt;
}

bool ParseAction(
    const Json& arguments,
    agent::SpriteAnimationAction& action,
    std::string& error)
{
    std::string actionText{};
    if (!ReadNonEmptyString(arguments, "action", actionText, error)) return false;
    const auto kind = ParseActionKind(actionText);
    if (!kind.has_value())
    {
        error = "action must be play, pause, stop, reset, restart, seek, set_speed, set_direction, or advance.";
        return false;
    }
    action.kind = *kind;

    if (*kind == agent::SpriteAnimationActionKind::Seek)
    {
        std::int64_t time = 0;
        if (!ReadSigned(arguments, "time_ns", time, error)) return false;
        action.time = runtime::SpriteAnimationTime2D{time};
    }
    else if (*kind == agent::SpriteAnimationActionKind::Advance)
    {
        std::int64_t delta = 0;
        if (!ReadSigned(arguments, "delta_ns", delta, error)) return false;
        action.time = runtime::SpriteAnimationTime2D{delta};
        if (!ReadSize(arguments, "emission_capacity", action.emissionCapacity, error)) return false;
        if (action.emissionCapacity > agent::MaxSpriteAnimationAgentEmissions)
        {
            error = "emission_capacity must be between 0 and 4096.";
            return false;
        }
    }
    else if (*kind == agent::SpriteAnimationActionKind::SetSpeed)
    {
        if (!ReadUnsigned32(arguments, "speed_numerator", action.speed.numerator, error)
            || !ReadUnsigned32(arguments, "speed_denominator", action.speed.denominator, error))
        {
            return false;
        }
    }
    else if (*kind == agent::SpriteAnimationActionKind::SetDirection)
    {
        std::string directionText{};
        if (!ReadNonEmptyString(arguments, "direction", directionText, error)) return false;
        const auto direction = ParseDirection(directionText);
        if (!direction.has_value())
        {
            error = "direction must be forward or reverse.";
            return false;
        }
        action.direction = *direction;
    }
    return true;
}

[[nodiscard]] std::optional<agent::SpriteAnimationAssertionField> ParseAssertionField(
    const std::string_view field) noexcept
{
    using Field = agent::SpriteAnimationAssertionField;
    if (field == "clip_duration_ns") return Field::ClipDurationNanoseconds;
    if (field == "clip_frame_count") return Field::ClipFrameCount;
    if (field == "clip_event_count") return Field::ClipEventCount;
    if (field == "time_ns") return Field::TimeNanoseconds;
    if (field == "frame_index") return Field::FrameIndex;
    if (field == "region_index") return Field::RegionIndex;
    if (field == "playback") return Field::Playback;
    if (field == "loop_mode") return Field::LoopMode;
    if (field == "direction") return Field::Direction;
    if (field == "completed") return Field::Completed;
    if (field == "speed_numerator") return Field::SpeedNumerator;
    if (field == "speed_denominator") return Field::SpeedDenominator;
    if (field == "speed_remainder") return Field::SpeedRemainder;
    return std::nullopt;
}

bool ParseExpected(
    const agent::SpriteAnimationAssertionField field,
    const Json& value,
    agent::SpriteAnimationValue& expected,
    std::string& error)
{
    using Field = agent::SpriteAnimationAssertionField;
    if (field == Field::Completed)
    {
        if (!value.is_boolean())
        {
            error = "expected must be boolean for completed.";
            return false;
        }
        expected = agent::SpriteAnimationValue::Boolean(value.get<bool>());
        return true;
    }

    if (field == Field::Playback || field == Field::LoopMode || field == Field::Direction)
    {
        if (!value.is_string())
        {
            error = "expected must be a string for enum assertion fields.";
            return false;
        }
        const std::string& text = value.get_ref<const std::string&>();
        if ((field == Field::Playback && text != "stopped" && text != "playing" && text != "paused")
            || (field == Field::LoopMode && text != "once" && text != "loop" && text != "ping_pong")
            || (field == Field::Direction && text != "forward" && text != "reverse"))
        {
            error = "expected contains an invalid enum value for the selected assertion field.";
            return false;
        }
        expected = agent::SpriteAnimationValue::String(text);
        return true;
    }

    if (field == Field::ClipDurationNanoseconds || field == Field::TimeNanoseconds)
    {
        if (!value.is_number_integer())
        {
            error = "expected must be an integer for nanosecond assertion fields.";
            return false;
        }
        if (value.is_number_unsigned())
        {
            const std::uint64_t parsed = value.get<std::uint64_t>();
            if (parsed > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            {
                error = "expected nanosecond value exceeds int64 range.";
                return false;
            }
            expected = agent::SpriteAnimationValue::Signed(static_cast<std::int64_t>(parsed));
        }
        else
        {
            expected = agent::SpriteAnimationValue::Signed(value.get<std::int64_t>());
        }
        return true;
    }

    if (!value.is_number_integer())
    {
        error = "expected must be a non-negative integer for the selected assertion field.";
        return false;
    }
    std::uint64_t parsed = 0U;
    if (value.is_number_unsigned())
    {
        parsed = value.get<std::uint64_t>();
    }
    else
    {
        const std::int64_t signedValue = value.get<std::int64_t>();
        if (signedValue < 0)
        {
            error = "expected must be non-negative for the selected assertion field.";
            return false;
        }
        parsed = static_cast<std::uint64_t>(signedValue);
    }
    expected = agent::SpriteAnimationValue::Unsigned(parsed);
    return true;
}

Json ErrorResult(
    const agent::SpriteAnimationInspectionError& error,
    const std::optional<agent::SpriteAnimatorSnapshot>& snapshot = std::nullopt)
{
    Json payload{{"status", "error"}, {"error", ErrorToJson(error)}};
    payload["snapshot"] = snapshot.has_value() ? SnapshotToJson(*snapshot) : Json{nullptr};
    return MakeToolResult(payload, true);
}
} // namespace

bool IsSpriteAnimationTool(const std::string_view name) noexcept
{
    return name == "trace2d.sprite_animation.inspect"
        || name == "trace2d.sprite_animation.action"
        || name == "trace2d.sprite_animation.assert";
}

void AppendSpriteAnimationTools(nlohmann::json& tools)
{
    const Json entityOnlySchema{
        {"type", "object"},
        {"properties", Json{{"entity_id", EntityIdProperty()}}},
        {"required", Json::array({"entity_id"})},
        {"additionalProperties", false},
    };
    tools.push_back(ToolDefinition(
        "trace2d.sprite_animation.inspect",
        "Inspect authoritative headless SpriteAnimator2D state for one bound entity.",
        entityOnlySchema,
        true));

    tools.push_back(ToolDefinition(
        "trace2d.sprite_animation.action",
        "Apply one explicit SpriteAnimator2D action; advance uses exact integer nanoseconds and bounded emission output.",
        Json{
            {"type", "object"},
            {"properties", Json{
                {"entity_id", EntityIdProperty()},
                {"action", Json{{"type", "string"}, {"enum", Json::array({
                    "play", "pause", "stop", "reset", "restart", "seek", "set_speed", "set_direction", "advance"})}}},
                {"time_ns", Json{{"type", "integer"}}},
                {"delta_ns", Json{{"type", "integer"}}},
                {"speed_numerator", Json{{"type", "integer"}, {"minimum", 0}, {"maximum", std::numeric_limits<std::uint32_t>::max()}}},
                {"speed_denominator", Json{{"type", "integer"}, {"minimum", 0}, {"maximum", std::numeric_limits<std::uint32_t>::max()}}},
                {"direction", Json{{"type", "string"}, {"enum", Json::array({"forward", "reverse"})}}},
                {"emission_capacity", Json{{"type", "integer"}, {"minimum", 0}, {"maximum", agent::MaxSpriteAnimationAgentEmissions}}},
            }},
            {"required", Json::array({"entity_id", "action"})},
            {"additionalProperties", false},
        },
        false));

    tools.push_back(ToolDefinition(
        "trace2d.sprite_animation.assert",
        "Assert one authoritative SpriteAnimator2D scalar at the current exact state without implicit waiting or stepping.",
        Json{
            {"type", "object"},
            {"properties", Json{
                {"entity_id", EntityIdProperty()},
                {"field", Json{{"type", "string"}, {"enum", Json::array({
                    "clip_duration_ns", "clip_frame_count", "clip_event_count", "time_ns", "frame_index", "region_index",
                    "playback", "loop_mode", "direction", "completed", "speed_numerator", "speed_denominator", "speed_remainder"})}}},
                {"expected", Json::object()},
            }},
            {"required", Json::array({"entity_id", "field", "expected"})},
            {"additionalProperties", false},
        },
        true));
}

nlohmann::json ExecuteSpriteAnimationTool(
    const std::string_view name,
    const nlohmann::json& arguments,
    agent::AgentFacade& agentFacade,
    const std::span<const agent::SpriteAnimatorBinding> bindings)
{
    if (!arguments.is_object())
    {
        return MakeToolResult(MakeErrorPayload("invalid_arguments", "Tool arguments must be a JSON object."), true);
    }

    std::string entityId{};
    std::string parseError{};
    if (!ReadNonEmptyString(arguments, "entity_id", entityId, parseError))
    {
        return MakeToolResult(MakeErrorPayload("invalid_arguments", parseError), true);
    }
    const agent::SpriteAnimatorBinding* binding = FindBinding(bindings, entityId);
    if (binding == nullptr)
    {
        return MakeToolResult(
            MakeErrorPayload("animator_not_bound", "No SpriteAnimator2D binding matches entity_id."),
            true);
    }

    if (name == "trace2d.sprite_animation.inspect")
    {
        const agent::SpriteAnimatorInspectionResult result = agentFacade.InspectSpriteAnimator(*binding);
        if (!result.Succeeded())
        {
            return ErrorResult(result.error.value_or(agent::SpriteAnimationInspectionError{
                .code = agent::SpriteAnimationInspectionErrorCode::AnimatorUnavailable,
                .message = "Sprite animation inspection failed without a structured error.",
            }));
        }
        return MakeToolResult(Json{{"status", "ok"}, {"snapshot", SnapshotToJson(*result.snapshot)}}, false);
    }

    if (name == "trace2d.sprite_animation.action")
    {
        agent::SpriteAnimationAction action{};
        if (!ParseAction(arguments, action, parseError))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", parseError), true);
        }

        const agent::SpriteAnimationActionResult result = agentFacade.ActOnSpriteAnimator(*binding, action);
        if (!result.Succeeded())
        {
            const agent::SpriteAnimationInspectionError error = result.error.value_or(agent::SpriteAnimationInspectionError{
                .code = agent::SpriteAnimationInspectionErrorCode::RuntimeRejected,
                .message = "Sprite animation action failed without a structured error.",
            });
            Json payload{{"status", "error"}, {"error", ErrorToJson(error)}};
            payload["snapshot"] = result.snapshot.has_value() ? SnapshotToJson(*result.snapshot) : Json{nullptr};
            payload["emissions"] = Json::array();
            return MakeToolResult(payload, true);
        }

        Json emissions = Json::array();
        for (const agent::SpriteAnimationEmissionSnapshot& emission : result.emissions)
        {
            emissions.push_back(EmissionToJson(emission));
        }
        return MakeToolResult(Json{
            {"status", "ok"},
            {"action", std::string{agent::ToString(action.kind)}},
            {"snapshot", SnapshotToJson(*result.snapshot)},
            {"emissions", std::move(emissions)},
        }, false);
    }

    if (name == "trace2d.sprite_animation.assert")
    {
        std::string fieldText{};
        if (!ReadNonEmptyString(arguments, "field", fieldText, parseError))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", parseError), true);
        }
        const auto field = ParseAssertionField(fieldText);
        if (!field.has_value())
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", "Unknown Sprite animation assertion field."), true);
        }

        const auto expectedValue = arguments.find("expected");
        if (expectedValue == arguments.end())
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", "Missing required argument: expected"), true);
        }

        agent::SpriteAnimationValue expected{};
        if (!ParseExpected(*field, *expectedValue, expected, parseError))
        {
            return MakeToolResult(MakeErrorPayload("invalid_arguments", parseError), true);
        }

        const agent::SpriteAnimationAssertion assertion{.field = *field, .expected = expected};
        const agent::SpriteAnimationAssertionResult result = agentFacade.AssertSpriteAnimator(*binding, assertion);

        Json payload{
            {"status", result.Succeeded() ? "ok" : "error"},
            {"field", fieldText},
            {"expected", ValueToJson(assertion.expected)},
            {"context", ContextToJson(result.context)},
        };
        payload["observed"] = result.observed.has_value() ? ValueToJson(*result.observed) : Json{nullptr};
        if (result.error.has_value()) payload["error"] = ErrorToJson(*result.error);
        return MakeToolResult(payload, !result.Succeeded());
    }

    return MakeToolResult(MakeErrorPayload("unknown_tool", "Unknown Sprite animation MCP tool."), true);
}
} // namespace trace2d::mcp
