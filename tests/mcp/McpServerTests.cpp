#include <trace2d/agent/Inspection.hpp>
#include <trace2d/input/Input.hpp>
#include <trace2d/mcp/McpServer.hpp>
#include <trace2d/scene/Scene.hpp>
#include <trace2d/ui/UiText.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace
{
using Json = nlohmann::json;

constexpr std::string_view TestUi = R"(format_version = 1

[canvas]
width = 160
height = 96

[[elements]]
id = "start"
kind = "button"
bounds = [8, 8, 96, 24]
name = "Start Game"
text = "Start Game"

[[elements]]
id = "player_name"
kind = "text_input"
bounds = [8, 40, 120, 24]
name = "Player Name"
text = "Player"
)";

trace2d::scene::Scene MakeScene()
{
    trace2d::scene::Scene scene{{.semanticId = "mcp_test", .name = "MCP Test"}};
    trace2d::scene::EntityDescriptor player{};
    player.semanticId = "player";
    player.name = "Player";
    scene.CreateEntity(std::move(player));
    return scene;
}

Json ModernMeta()
{
    return Json{
        {"io.modelcontextprotocol/protocolVersion", std::string{trace2d::mcp::ProtocolVersion}},
        {"io.modelcontextprotocol/clientInfo", Json{{"name", "trace2d-tests"}, {"version", "1"}}},
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

    Json request = Json::object();
    request["jsonrpc"] = "2.0";
    request["id"] = id;
    request["method"] = std::string{method};
    request["params"] = std::move(requestParams);

    const std::string responseText = server.HandleMessage(request.dump());
    EXPECT_FALSE(responseText.empty());

    Json response = Json::parse(responseText, nullptr, false);
    EXPECT_FALSE(response.is_discarded());
    EXPECT_EQ(response.value("jsonrpc", std::string{}), "2.0");
    EXPECT_EQ(response.value("id", 0U), id);
    return response;
}

Json LegacyInitialize(trace2d::mcp::McpServer& server, const std::uint64_t id)
{
    const Json request{
        {"jsonrpc", "2.0"},
        {"id", id},
        {"method", "initialize"},
        {"params", Json{
            {"protocolVersion", std::string{trace2d::mcp::LegacyProtocolVersion}},
            {"clientInfo", Json{{"name", "legacy-test"}, {"version", "1"}}},
            {"capabilities", Json::object()},
        }},
    };
    return Json::parse(server.HandleMessage(request.dump()));
}

Json ToolCall(
    trace2d::mcp::McpServer& server,
    const std::uint64_t id,
    const std::string_view name,
    const Json& arguments = Json::object())
{
    Json params = Json::object();
    params["name"] = std::string{name};
    params["arguments"] = arguments;
    return RpcRequest(server, id, "tools/call", params);
}

const Json& StructuredContent(const Json& response)
{
    return response.at("result").at("structuredContent");
}

void MovePlayerWhileRightHeld(trace2d::testing::GameplayFrameContext& context)
{
    if (!context.input.Held(trace2d::input::InputControl::KeyD))
    {
        return;
    }

    const auto playerId = context.scene.FindBySemanticId("player");
    if (!playerId.has_value())
    {
        return;
    }

    trace2d::scene::Entity* const player = context.scene.TryGet(*playerId);
    if (player != nullptr)
    {
        player->Transform().position.x += 1.0F;
    }
}

TEST(McpServerTests, DiscoveryAndToolListAreDeterministicAndCacheable)
{
    trace2d::testing::GameplayScenario scenario{};
    scenario.LoadScene(MakeScene());
    trace2d::agent::AgentFacade agent{&scenario.Runtime(), scenario.ActiveScene()};
    trace2d::mcp::McpServer server{agent, scenario};

    const Json discovery = RpcRequest(server, 1U, "server/discover");
    ASSERT_TRUE(discovery.contains("result"));
    EXPECT_EQ(discovery["result"]["resultType"], "complete");
    EXPECT_EQ(discovery["result"]["supportedVersions"][0], trace2d::mcp::ProtocolVersion);
    EXPECT_EQ(discovery["result"]["supportedVersions"][1], trace2d::mcp::LegacyProtocolVersion);
    EXPECT_EQ(discovery["result"]["ttlMs"], 60'000U);
    EXPECT_EQ(discovery["result"]["cacheScope"], "public");
    EXPECT_EQ(discovery["result"]["_meta"]["io.modelcontextprotocol/serverInfo"]["name"], "trace2d-mcp");

    const Json tools = RpcRequest(server, 2U, "tools/list");
    ASSERT_TRUE(tools.contains("result"));
    EXPECT_EQ(tools["result"]["resultType"], "complete");
    EXPECT_EQ(tools["result"]["ttlMs"], 60'000U);
    EXPECT_EQ(tools["result"]["cacheScope"], "public");

    const Json& toolList = tools["result"]["tools"];
    ASSERT_TRUE(toolList.is_array());
    ASSERT_EQ(toolList.size(), 12U);
    EXPECT_EQ(toolList.front()["name"], "trace2d.inspect");
    EXPECT_EQ(toolList.back()["name"], "trace2d.assert_float");
}

TEST(McpServerTests, LegacyInitializeRemainsAvailableForStdioFallback)
{
    trace2d::testing::GameplayScenario scenario{};
    scenario.LoadScene(MakeScene());
    trace2d::agent::AgentFacade agent{&scenario.Runtime(), scenario.ActiveScene()};
    trace2d::mcp::McpServer server{agent, scenario};

    const Json response = LegacyInitialize(server, 3U);
    ASSERT_TRUE(response.contains("result"));
    EXPECT_EQ(response["result"]["protocolVersion"], trace2d::mcp::LegacyProtocolVersion);
    EXPECT_EQ(response["result"]["serverInfo"]["name"], "trace2d-mcp");
}

TEST(McpServerTests, ScheduledInputStepQueryAndGameplayAssertionReuseExistingContracts)
{
    trace2d::runtime::RuntimeConfig config{};
    config.seed = 42U;
    trace2d::testing::GameplayScenario scenario{config};
    scenario.LoadScene(MakeScene());
    trace2d::agent::AgentFacade agent{&scenario.Runtime(), scenario.ActiveScene()};
    trace2d::mcp::McpServer server{agent, scenario, MovePlayerWhileRightHeld};

    const Json pressResponse = ToolCall(
        server,
        10U,
        "trace2d.input.schedule",
        Json{{"frame", 1U}, {"control", "key_d"}, {"event", "press"}});
    EXPECT_FALSE(pressResponse["result"]["isError"].get<bool>());

    const Json releaseResponse = ToolCall(
        server,
        11U,
        "trace2d.input.schedule",
        Json{{"frame", 3U}, {"control", "key_d"}, {"event", "release"}});
    EXPECT_FALSE(releaseResponse["result"]["isError"].get<bool>());

    const Json stepResponse = ToolCall(server, 12U, "trace2d.runtime.step", Json{{"frames", 3U}});
    ASSERT_FALSE(stepResponse["result"]["isError"].get<bool>());
    EXPECT_EQ(StructuredContent(stepResponse)["frame"], 3U);
    EXPECT_EQ(StructuredContent(stepResponse)["seed"], 42U);
    EXPECT_EQ(StructuredContent(stepResponse)["input_frame"], 3U);

    const Json inputState = ToolCall(server, 13U, "trace2d.input.inspect", Json{{"control", "key_d"}});
    ASSERT_FALSE(inputState["result"]["isError"].get<bool>());
    EXPECT_FALSE(StructuredContent(inputState)["held"].get<bool>());
    EXPECT_TRUE(StructuredContent(inputState)["released"].get<bool>());

    const Json query = ToolCall(
        server,
        14U,
        "trace2d.query",
        Json{{"selector", "#player"}, {"one", true}});
    ASSERT_FALSE(query["result"]["isError"].get<bool>());
    EXPECT_FLOAT_EQ(StructuredContent(query)["match"]["transform"]["position"]["x"].get<float>(), 2.0F);

    const Json assertion = ToolCall(
        server,
        15U,
        "trace2d.assert_float",
        Json{{"selector", "#player"}, {"component", "Transform2D"}, {"field", "position.x"}, {"expected", 2.0F}});
    ASSERT_FALSE(assertion["result"]["isError"].get<bool>());
    EXPECT_EQ(StructuredContent(assertion)["frame"], 3U);
    EXPECT_EQ(StructuredContent(assertion)["seed"], 42U);

    const Json failedAssertion = ToolCall(
        server,
        16U,
        "trace2d.assert_float",
        Json{{"selector", "#player"}, {"component", "Transform2D"}, {"field", "position.x"}, {"expected", 99.0F}});
    ASSERT_TRUE(failedAssertion["result"]["isError"].get<bool>());
    const Json& failure = StructuredContent(failedAssertion)["failure"];
    EXPECT_EQ(failure["code"], "value_mismatch");
    EXPECT_EQ(failure["frame"], 3U);
    EXPECT_EQ(failure["seed"], 42U);
    EXPECT_EQ(failure["snapshot"]["runtime"]["frame"], 3U);
}

TEST(McpServerTests, SemanticUiActionsNeedNoRendererOrCoordinates)
{
    trace2d::testing::GameplayScenario scenario{};
    scenario.LoadScene(MakeScene());

    trace2d::ui::UiLoadResult uiLoad = trace2d::ui::LoadUiToml(TestUi, "mcp_test_ui.trace2d.toml");
    ASSERT_TRUE(uiLoad.Succeeded());
    ASSERT_TRUE(uiLoad.document.has_value());
    trace2d::ui::UiDocument ui = std::move(*uiLoad.document);

    trace2d::agent::AgentFacade agent{&scenario.Runtime(), scenario.ActiveScene(), &ui};
    trace2d::mcp::McpServer server{agent, scenario};

    const Json textBoxSelector = Json{{"role", "textbox"}, {"name", "Player Name"}};
    const Json focus = ToolCall(server, 20U, "trace2d.ui.focus", Json{{"selector", textBoxSelector}});
    ASSERT_FALSE(focus["result"]["isError"].get<bool>());
    EXPECT_TRUE(StructuredContent(focus)["element"]["focused"].get<bool>());

    const Json inputText = ToolCall(
        server,
        21U,
        "trace2d.ui.input_text",
        Json{{"selector", textBoxSelector}, {"text", "Ada"}});
    ASSERT_FALSE(inputText["result"]["isError"].get<bool>());
    EXPECT_EQ(StructuredContent(inputText)["element"]["text"], "Ada");

    const Json textAssertion = ToolCall(
        server,
        22U,
        "trace2d.ui.assert",
        Json{{"selector", textBoxSelector}, {"expected", Json{{"focused", true}, {"text", "Ada"}}}});
    ASSERT_FALSE(textAssertion["result"]["isError"].get<bool>());

    const Json startSelector = Json{{"role", "button"}, {"name", "Start Game"}};
    const Json activate = ToolCall(server, 23U, "trace2d.ui.activate", Json{{"selector", startSelector}});
    ASSERT_FALSE(activate["result"]["isError"].get<bool>());
    EXPECT_EQ(StructuredContent(activate)["element"]["activation_count"], 1U);

    const Json failedUiAssertion = ToolCall(
        server,
        24U,
        "trace2d.ui.assert",
        Json{{"selector", startSelector}, {"expected", Json{{"activation_count", 2U}}}});
    ASSERT_TRUE(failedUiAssertion["result"]["isError"].get<bool>());
    EXPECT_EQ(StructuredContent(failedUiAssertion)["error"]["code"], "state_mismatch");
    EXPECT_EQ(StructuredContent(failedUiAssertion)["observed"]["activation_count"], 1U);
}
} // namespace
