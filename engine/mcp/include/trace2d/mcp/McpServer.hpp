#pragma once

#include <trace2d/agent/SpriteAnimationInspection.hpp>
#include <trace2d/testing/GameplayScenario.hpp>

#include <span>
#include <string>
#include <string_view>

namespace trace2d::agent
{
class AgentFacade;
}

namespace trace2d::mcp
{
inline constexpr std::string_view ProtocolVersion = "2026-07-28";

class McpServer final
{
public:
    McpServer(
        agent::AgentFacade& agent,
        testing::GameplayScenario& scenario,
        testing::GameplayFrameUpdate frameUpdate = {},
        std::span<const agent::SpriteAnimatorBinding> spriteAnimators = {});

    [[nodiscard]] std::string HandleMessage(std::string_view message);

private:
    agent::AgentFacade& agent_;
    testing::GameplayScenario& scenario_;
    testing::GameplayFrameUpdate frameUpdate_{};
    std::span<const agent::SpriteAnimatorBinding> spriteAnimators_{};
};
} // namespace trace2d::mcp
