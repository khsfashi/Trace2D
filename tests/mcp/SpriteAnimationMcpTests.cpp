#include <trace2d/agent/Inspection.hpp>
#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/mcp/McpServer.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace
{
using namespace std::chrono_literals;
using Json = nlohmann::json;
using trace2d::agent::AgentFacade;
using trace2d::agent::SpriteAnimatorBinding;
using trace2d::assets::SpriteAsset;
using trace2d::runtime::MakeSpriteAnimator2DState;
using trace2d::runtime::SpriteAnimationClip2D;
using trace2d::runtime::SpriteAnimationDirection;
using trace2d::runtime::SpriteAnimationEvent2D;
using trace2d::runtime::SpriteAnimationFrame2D;
using trace2d::runtime::SpriteAnimationLoopMode;
using trace2d::runtime::SpriteAnimationPlaybackState;
using trace2d::runtime::SpriteAnimator2D;
using trace2d::runtime::SpriteAnimator2DState;

Json ModernMeta()
{
    return Json{
        {"io.modelcontextprotocol/protocolVersion", std::string{trace2d::mcp::ProtocolVersion}},
        {"io.modelcontextprotocol/clientInfo", Json{{"name", "sprite-animation-tests"}, {"version", "1"}}},
        {"io.modelcontextprotocol/clientCapabilities", Json::object()},
    };
}

Json RpcRequest(
    trace2d::mcp::McpServer& server,
    const std::uint64_t id,
    const std::string_view method,
    const Json& params = Json::object())
{
    Json requestParams = params;
    requestParams["_meta"] = ModernMeta();

    const Json request{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", std::string{method}},
        {"params", std::move(requestParams)},
    };

    const std::string responseText = server.HandleMessage(request.dump());
    EXPECT_FALSE(responseText.empty());
    Json response = Json::parse(responseText, nullptr, false);
    EXPECT_FALSE(response.is_discarded());
    return response;
}

Json ToolCall(
    trace2d::mcp::McpServer& server,
    const std::uint64_t id,
    const std::string_view name,
    const Json& arguments = Json::object())
{
    return RpcRequest(
        server,
        id,
        "tools/call",
        Json{{"name", std::string{name}}, {"arguments", arguments}});
}

const Json& StructuredContent(const Json& response)
{
    return response.at("result").at("structuredContent");
}

[[nodiscard]] SpriteAsset MakeSpriteAsset(const std::size_t regionCount)
{
    SpriteAsset asset{};
    asset.regions.resize(regionCount);
    return asset;
}

[[nodiscard]] SpriteAnimationClip2D PrepareClip(const SpriteAsset& asset)
{
    const std::array frames{
        SpriteAnimationFrame2D{0U, 100ns},
        SpriteAnimationFrame2D{1U, 100ns},
        SpriteAnimationFrame2D{2U, 100ns},
    };
    const std::array events{
        SpriteAnimationEvent2D{11U, 50ns, 0U},
        SpriteAnimationEvent2D{12U, 100ns, 1U},
        SpriteAnimationEvent2D{13U, 100ns, 2U},
        SpriteAnimationEvent2D{14U, 250ns, 3U},
    };

    SpriteAnimationClip2D clip{};
    EXPECT_TRUE(SpriteAnimationClip2D::Prepare(
                    &asset,
                    static_cast<std::uint32_t>(asset.regions.size()),
                    frames,
                    events,
                    clip)
                    .Succeeded());
    return clip;
}

[[nodiscard]] SpriteAnimator2D MakeAnimator(
    SpriteAnimationClip2D& clip,
    const std::chrono::nanoseconds time = 40ns,
    const SpriteAnimationLoopMode loopMode = SpriteAnimationLoopMode::Once)
{
    SpriteAnimator2DState state{};
    EXPECT_TRUE(MakeSpriteAnimator2DState(
                    clip,
                    time,
                    SpriteAnimationPlaybackState::Playing,
                    loopMode,
                    SpriteAnimationDirection::Forward,
                    false,
                    {1U, 1U},
                    state)
                    .Succeeded());

    SpriteAnimator2D animator{};
    EXPECT_TRUE(animator.RestoreState(state).Succeeded());
    return animator;
}

TEST(SpriteAnimationMcpTests, AdvertisesSpriteToolsOnlyWhenAnimatorBindingsExist)
{
    trace2d::testing::GameplayScenario scenario{};
    AgentFacade agent{};

    trace2d::mcp::McpServer unboundServer{agent, scenario};
    const Json unboundTools = RpcRequest(unboundServer, 1U, "tools/list");
    ASSERT_TRUE(unboundTools.contains("result"));
    ASSERT_EQ(unboundTools["result"]["tools"].size(), 12U);

    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareClip(asset);
    SpriteAnimator2D animator = MakeAnimator(clip);
    const std::array bindings{SpriteAnimatorBinding{"hero", &animator}};
    trace2d::mcp::McpServer boundServer{agent, scenario, {}, bindings};

    const Json boundTools = RpcRequest(boundServer, 2U, "tools/list");
    const Json& tools = boundTools["result"]["tools"];
    ASSERT_EQ(tools.size(), 15U);
    EXPECT_EQ(tools[11]["name"], "trace2d.sprite_animation.inspect");
    EXPECT_TRUE(tools[11]["annotations"]["readOnlyHint"].get<bool>());
    EXPECT_EQ(tools[12]["name"], "trace2d.sprite_animation.action");
    EXPECT_FALSE(tools[12]["annotations"]["readOnlyHint"].get<bool>());
    EXPECT_EQ(tools[13]["name"], "trace2d.sprite_animation.assert");
    EXPECT_TRUE(tools[13]["annotations"]["readOnlyHint"].get<bool>());
    EXPECT_EQ(tools[14]["name"], "trace2d.assert_float");
}

TEST(SpriteAnimationMcpTests, InspectAdvanceAndAssertUseOneAuthoritativeAnimator)
{
    trace2d::testing::GameplayScenario scenario{};
    AgentFacade agent{};
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareClip(asset);
    SpriteAnimator2D animator = MakeAnimator(clip);
    const std::array bindings{SpriteAnimatorBinding{"hero", &animator}};
    trace2d::mcp::McpServer server{agent, scenario, {}, bindings};

    const Json inspect = ToolCall(
        server,
        10U,
        "trace2d.sprite_animation.inspect",
        Json{{"entity_id", "hero"}});
    ASSERT_FALSE(inspect["result"]["isError"].get<bool>());
    EXPECT_EQ(StructuredContent(inspect)["snapshot"]["time_ns"], 40);
    EXPECT_EQ(StructuredContent(inspect)["snapshot"]["frame_index"], 0U);
    EXPECT_EQ(StructuredContent(inspect)["snapshot"]["region_index"], 0U);

    const Json advance = ToolCall(
        server,
        11U,
        "trace2d.sprite_animation.action",
        Json{
            {"entity_id", "hero"},
            {"action", "advance"},
            {"delta_ns", 70},
            {"emission_capacity", 4U},
        });
    ASSERT_FALSE(advance["result"]["isError"].get<bool>());
    const Json& advancePayload = StructuredContent(advance);
    EXPECT_EQ(advancePayload["snapshot"]["time_ns"], 110);
    EXPECT_EQ(advancePayload["snapshot"]["frame_index"], 1U);
    ASSERT_EQ(advancePayload["emissions"].size(), 3U);
    EXPECT_EQ(advancePayload["emissions"][0]["event_id"], 11U);
    EXPECT_EQ(advancePayload["emissions"][1]["event_id"], 12U);
    EXPECT_EQ(advancePayload["emissions"][2]["event_id"], 13U);
    EXPECT_EQ(animator.State().time, 110ns);

    const Json assertion = ToolCall(
        server,
        12U,
        "trace2d.sprite_animation.assert",
        Json{{"entity_id", "hero"}, {"field", "frame_index"}, {"expected", 1U}});
    ASSERT_FALSE(assertion["result"]["isError"].get<bool>());
    EXPECT_EQ(StructuredContent(assertion)["observed"]["value"], 1U);

    const Json failedAssertion = ToolCall(
        server,
        13U,
        "trace2d.sprite_animation.assert",
        Json{{"entity_id", "hero"}, {"field", "playback"}, {"expected", "paused"}});
    ASSERT_TRUE(failedAssertion["result"]["isError"].get<bool>());
    const Json& failedPayload = StructuredContent(failedAssertion);
    EXPECT_EQ(failedPayload["error"]["code"], "state_mismatch");
    EXPECT_EQ(failedPayload["expected"]["value"], "paused");
    EXPECT_EQ(failedPayload["observed"]["value"], "playing");
    EXPECT_EQ(failedPayload["context"]["time_ns"], 110);
    EXPECT_EQ(failedPayload["context"]["frame_index"], 1U);
}

TEST(SpriteAnimationMcpTests, CapacityFailureIsStructuredAndTransactional)
{
    trace2d::testing::GameplayScenario scenario{};
    AgentFacade agent{};
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareClip(asset);
    SpriteAnimator2D animator = MakeAnimator(clip);
    const SpriteAnimator2DState before = animator.State();
    const std::array bindings{SpriteAnimatorBinding{"hero", &animator}};
    trace2d::mcp::McpServer server{agent, scenario, {}, bindings};

    const Json response = ToolCall(
        server,
        20U,
        "trace2d.sprite_animation.action",
        Json{
            {"entity_id", "hero"},
            {"action", "advance"},
            {"delta_ns", 70},
            {"emission_capacity", 1U},
        });

    ASSERT_TRUE(response["result"]["isError"].get<bool>());
    const Json& payload = StructuredContent(response);
    EXPECT_EQ(payload["error"]["code"], "output_capacity_exceeded");
    EXPECT_EQ(payload["error"]["runtime_error"], "output_capacity_exceeded");
    EXPECT_EQ(payload["snapshot"]["time_ns"], 40);
    EXPECT_TRUE(payload["emissions"].empty());
    EXPECT_EQ(animator.State(), before);
}

TEST(SpriteAnimationMcpTests, UnknownBindingAndInvalidActionReturnStableErrors)
{
    trace2d::testing::GameplayScenario scenario{};
    AgentFacade agent{};
    const SpriteAsset asset = MakeSpriteAsset(3U);
    SpriteAnimationClip2D clip = PrepareClip(asset);
    SpriteAnimator2D animator = MakeAnimator(clip);
    const std::array bindings{SpriteAnimatorBinding{"hero", &animator}};
    trace2d::mcp::McpServer server{agent, scenario, {}, bindings};

    const Json missing = ToolCall(
        server,
        30U,
        "trace2d.sprite_animation.inspect",
        Json{{"entity_id", "missing"}});
    ASSERT_TRUE(missing["result"]["isError"].get<bool>());
    EXPECT_EQ(StructuredContent(missing)["error"]["code"], "animator_not_bound");

    const Json invalid = ToolCall(
        server,
        31U,
        "trace2d.sprite_animation.action",
        Json{{"entity_id", "hero"}, {"action", "advance"}, {"delta_ns", -1}, {"emission_capacity", 4U}});
    ASSERT_TRUE(invalid["result"]["isError"].get<bool>());
    EXPECT_EQ(StructuredContent(invalid)["error"]["code"], "runtime_rejected");
    EXPECT_EQ(StructuredContent(invalid)["error"]["runtime_error"], "negative_delta");
    EXPECT_EQ(animator.State().time, 40ns);
}
} // namespace
