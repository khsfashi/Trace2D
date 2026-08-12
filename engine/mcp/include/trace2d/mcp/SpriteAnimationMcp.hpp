#pragma once

#include <trace2d/agent/SpriteAnimationInspection.hpp>

#include <nlohmann/json_fwd.hpp>

#include <span>
#include <string_view>

namespace trace2d::agent
{
class AgentFacade;
}

namespace trace2d::mcp
{
[[nodiscard]] bool IsSpriteAnimationTool(std::string_view name) noexcept;
void AppendSpriteAnimationTools(nlohmann::json& tools);
[[nodiscard]] nlohmann::json ExecuteSpriteAnimationTool(
    std::string_view name,
    const nlohmann::json& arguments,
    agent::AgentFacade& agent,
    std::span<const agent::SpriteAnimatorBinding> bindings);
} // namespace trace2d::mcp
