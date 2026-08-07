#include "AgentCommands.hpp"

#include <trace2d/core/Version.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/runtime/FixedStepRuntime.hpp>

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

namespace
{
constexpr int ExitSuccess = 0;
constexpr int ExitUsage = 2;
constexpr int ExitRuntimeFailure = 3;

void PrintHelp()
{
    std::cout << "Trace2D command line interface\n\n"
              << "Usage:\n"
              << "  trace2d version\n"
              << "  trace2d doctor [--json]\n"
              << "  trace2d run (--headless|--windowed) [--frames N] [--seed N] [--json]\n"
              << "  trace2d inspect --scene PATH [--frames N] [--seed N] [--json]\n"
              << "  trace2d query --scene PATH --selector SELECTOR [--one] [--frames N] [--seed N] [--json]\n";
}

int RunDoctor(const bool json)
{
    if (json)
    {
        std::cout << "{\"engine\":\"Trace2D\",\"version\":\"" << trace2d::core::Version()
                  << "\",\"cpp_standard\":20,\"status\":\"ok\"}\n";
        return ExitSuccess;
    }

    std::cout << "Trace2D doctor\n"
              << "  version: " << trace2d::core::Version() << '\n'
              << "  C++ standard: C++20\n"
              << "  status: ok\n";
    return ExitSuccess;
}

std::string EscapeJson(const std::string_view value)
{
    constexpr char HexDigits[] = "0123456789abcdef";

    std::string escaped{};
    escaped.reserve(value.size());

    for (const unsigned char byte : value)
    {
        switch (byte)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (byte < 0x20U)
            {
                escaped += "\\u00";
                escaped += HexDigits[(byte >> 4U) & 0x0FU];
                escaped += HexDigits[byte & 0x0FU];
            }
            else
            {
                escaped += static_cast<char>(byte);
            }
            break;
        }
    }

    return escaped;
}

bool TryParseUnsigned64(const std::string_view text, std::uint64_t& value) noexcept
{
    if (text.empty())
    {
        return false;
    }

    value = 0;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

int RunRuntime(const int argc, char* argv[])
{
    bool json = false;
    bool modeSelected = false;
    std::uint64_t frameCount = 0;
    std::uint64_t seed = 0;
    trace2d::platform::StartupMode mode = trace2d::platform::StartupMode::Headless;

    for (int index = 2; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};

        if (argument == "--json")
        {
            json = true;
            continue;
        }

        if (argument == "--headless" || argument == "--windowed")
        {
            const trace2d::platform::StartupMode requestedMode =
                argument == "--headless" ? trace2d::platform::StartupMode::Headless
                                         : trace2d::platform::StartupMode::Windowed;

            if (modeSelected && mode != requestedMode)
            {
                std::cerr << "Select exactly one startup mode: --headless or --windowed.\n";
                return ExitUsage;
            }

            mode = requestedMode;
            modeSelected = true;
            continue;
        }

        if (argument == "--frames" || argument == "--seed")
        {
            if (index + 1 >= argc)
            {
                std::cerr << argument << " requires an unsigned integer value.\n";
                return ExitUsage;
            }

            std::uint64_t parsedValue = 0;
            const std::string_view valueText{argv[index + 1]};
            if (!TryParseUnsigned64(valueText, parsedValue))
            {
                std::cerr << "Invalid value for " << argument << ": " << valueText << '\n';
                return ExitUsage;
            }

            if (argument == "--frames")
            {
                frameCount = parsedValue;
            }
            else
            {
                seed = parsedValue;
            }

            ++index;
            continue;
        }

        std::cerr << "Unknown run option: " << argument << '\n';
        return ExitUsage;
    }

    if (!modeSelected)
    {
        std::cerr << "run requires an explicit startup mode: --headless or --windowed.\n";
        return ExitUsage;
    }

    trace2d::platform::PlatformConfig platformConfig{};
    platformConfig.mode = mode;

    trace2d::runtime::RuntimeConfig runtimeConfig{};
    runtimeConfig.seed = seed;

    try
    {
        trace2d::platform::Platform platform{platformConfig};
        trace2d::runtime::FixedStepRuntime runtime{runtimeConfig};

        trace2d::platform::PlatformEvent event{};
        while (platform.PollEvent(event))
        {
            if (event.type == trace2d::platform::PlatformEventType::QuitRequested)
            {
                break;
            }
        }

        runtime.Step(frameCount);
        const trace2d::runtime::RuntimeState state = runtime.State();

        if (json)
        {
            std::cout << "{\"command\":\"run\",\"mode\":\"" << trace2d::platform::ToString(platform.Mode())
                      << "\",\"window_created\":" << (platform.HasWindow() ? "true" : "false")
                      << ",\"frame\":" << state.frame << ",\"seed\":" << state.seed
                      << ",\"fixed_step_ns\":" << runtime.Config().fixedTimestep.count()
                      << ",\"simulation_time_ns\":" << state.simulationTime.count()
                      << ",\"status\":\"ok\"}\n";
            return ExitSuccess;
        }

        std::cout << "Trace2D runtime\n"
                  << "  mode: " << trace2d::platform::ToString(platform.Mode()) << '\n'
                  << "  window created: " << (platform.HasWindow() ? "yes" : "no") << '\n'
                  << "  frame: " << state.frame << '\n'
                  << "  seed: " << state.seed << '\n'
                  << "  fixed step: " << runtime.Config().fixedTimestep.count() << " ns\n"
                  << "  simulation time: " << state.simulationTime.count() << " ns\n"
                  << "  status: ok\n";
        return ExitSuccess;
    }
    catch (const std::exception& exception)
    {
        if (json)
        {
            std::cerr << "{\"command\":\"run\",\"mode\":\"" << trace2d::platform::ToString(mode)
                      << "\",\"status\":\"error\",\"message\":\"" << EscapeJson(exception.what()) << "\"}\n";
        }
        else
        {
            std::cerr << "Trace2D runtime startup failed: " << exception.what() << '\n';
        }
        return ExitRuntimeFailure;
    }
}
} // namespace

int main(const int argc, char* argv[])
{
    if (argc < 2)
    {
        PrintHelp();
        return ExitSuccess;
    }

    const std::string_view command{argv[1]};

    if (command == "version" || command == "--version")
    {
        std::cout << trace2d::core::Version() << '\n';
        return ExitSuccess;
    }

    if (command == "doctor")
    {
        const bool json = argc >= 3 && std::string_view{argv[2]} == "--json";
        return RunDoctor(json);
    }

    if (command == "run")
    {
        return RunRuntime(argc, argv);
    }

    if (command == "inspect")
    {
        return trace2d::tools::RunInspectCommand(argc, argv);
    }

    if (command == "query")
    {
        return trace2d::tools::RunQueryCommand(argc, argv);
    }

    std::cerr << "Unknown command: " << command << "\n\n";
    PrintHelp();
    return ExitUsage;
}
