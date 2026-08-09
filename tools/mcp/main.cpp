#include <trace2d/agent/Inspection.hpp>
#include <trace2d/mcp/McpServer.hpp>
#include <trace2d/runtime/FixedStepRuntime.hpp>
#include <trace2d/testing/GameplayScenario.hpp>
#include <trace2d/ui/UiText.hpp>

#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{
constexpr int ExitSuccess = 0;
constexpr int ExitUsage = 2;
constexpr int ExitLoadFailure = 3;

void PrintUsage()
{
    std::cerr << "Usage: trace2d_mcp --scene PATH [--ui PATH] [--seed N]\n";
}

bool TryParseUnsigned64(const std::string_view text, std::uint64_t& value) noexcept
{
    if (text.empty())
    {
        return false;
    }

    value = 0U;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool ReadTextFile(const std::string& path, std::string& text)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        return false;
    }

    std::ostringstream buffer{};
    buffer << input.rdbuf();
    if (input.bad())
    {
        return false;
    }

    text = buffer.str();
    return true;
}

void PrintSceneDiagnostics(const trace2d::testing::GameplaySceneLoadResult& result)
{
    for (const trace2d::scene::SceneTextDiagnostic& diagnostic : result.diagnostics)
    {
        std::cerr << diagnostic.path << ": " << diagnostic.message;
        if (diagnostic.line != 0U)
        {
            std::cerr << " (" << diagnostic.line << ':' << diagnostic.column << ')';
        }
        std::cerr << '\n';
    }
}

void PrintUiDiagnostics(const trace2d::ui::UiLoadResult& result)
{
    for (const trace2d::ui::UiTextDiagnostic& diagnostic : result.diagnostics)
    {
        std::cerr << diagnostic.path << ": " << diagnostic.message;
        if (diagnostic.line != 0U)
        {
            std::cerr << " (" << diagnostic.line << ':' << diagnostic.column << ')';
        }
        std::cerr << '\n';
    }
}
} // namespace

int main(const int argc, char* argv[])
{
    std::string scenePath{};
    std::string uiPath{};
    std::uint64_t seed = 42U;

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument == "--scene" || argument == "--ui" || argument == "--seed")
        {
            if (index + 1 >= argc)
            {
                PrintUsage();
                return ExitUsage;
            }

            const std::string_view value{argv[++index]};
            if (argument == "--scene")
            {
                scenePath = value;
            }
            else if (argument == "--ui")
            {
                uiPath = value;
            }
            else if (!TryParseUnsigned64(value, seed))
            {
                std::cerr << "Invalid --seed value: " << value << '\n';
                return ExitUsage;
            }
            continue;
        }

        if (argument == "--help" || argument == "-h")
        {
            PrintUsage();
            return ExitSuccess;
        }

        std::cerr << "Unknown option: " << argument << '\n';
        PrintUsage();
        return ExitUsage;
    }

    if (scenePath.empty())
    {
        PrintUsage();
        return ExitUsage;
    }

    std::string sceneText{};
    if (!ReadTextFile(scenePath, sceneText))
    {
        std::cerr << "Unable to read scene file: " << scenePath << '\n';
        return ExitLoadFailure;
    }

    trace2d::runtime::RuntimeConfig runtimeConfig{};
    runtimeConfig.seed = seed;
    trace2d::testing::GameplayScenario scenario{runtimeConfig};
    const trace2d::testing::GameplaySceneLoadResult sceneLoad = scenario.LoadSceneToml(sceneText, scenePath);
    if (!sceneLoad.Succeeded())
    {
        PrintSceneDiagnostics(sceneLoad);
        return ExitLoadFailure;
    }

    std::optional<trace2d::ui::UiDocument> uiDocument{};
    if (!uiPath.empty())
    {
        std::string uiText{};
        if (!ReadTextFile(uiPath, uiText))
        {
            std::cerr << "Unable to read UI file: " << uiPath << '\n';
            return ExitLoadFailure;
        }

        trace2d::ui::UiLoadResult uiLoad = trace2d::ui::LoadUiToml(uiText, uiPath);
        if (!uiLoad.Succeeded())
        {
            PrintUiDiagnostics(uiLoad);
            return ExitLoadFailure;
        }
        uiDocument = std::move(uiLoad.document);
    }

    trace2d::ui::UiDocument* const ui = uiDocument.has_value() ? &*uiDocument : nullptr;
    trace2d::agent::AgentFacade agent{&scenario.Runtime(), scenario.ActiveScene(), ui};
    trace2d::mcp::McpServer server{agent, scenario};

    std::string line{};
    while (std::getline(std::cin, line))
    {
        if (line.empty())
        {
            continue;
        }

        const std::string response = server.HandleMessage(line);
        if (!response.empty())
        {
            std::cout << response << '\n';
            std::cout.flush();
        }
    }

    return ExitSuccess;
}
