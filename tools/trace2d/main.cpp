#include <trace2d/core/Version.hpp>
#include <trace2d/platform/Platform.hpp>

#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace
{
void PrintHelp()
{
    std::cout << "Trace2D command line interface\n\n"
              << "Usage:\n"
              << "  trace2d version\n"
              << "  trace2d doctor [--json]\n"
              << "  trace2d run (--headless|--windowed) [--json]\n";
}

int RunDoctor(const bool json)
{
    if (json)
    {
        std::cout << "{\"engine\":\"Trace2D\",\"version\":\"" << trace2d::core::Version()
                  << "\",\"cpp_standard\":20,\"status\":\"ok\"}\n";
        return 0;
    }

    std::cout << "Trace2D doctor\n"
              << "  version: " << trace2d::core::Version() << '\n'
              << "  C++ standard: C++20\n"
              << "  status: ok\n";
    return 0;
}

std::string EscapeJson(const std::string_view value)
{
    std::string escaped{};
    escaped.reserve(value.size());

    for (const char character : value)
    {
        switch (character)
        {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
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
            escaped += character;
            break;
        }
    }

    return escaped;
}

int RunPlatformSmoke(const int argc, char* argv[])
{
    bool json = false;
    bool modeSelected = false;
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
                return 2;
            }

            mode = requestedMode;
            modeSelected = true;
            continue;
        }

        std::cerr << "Unknown run option: " << argument << '\n';
        return 2;
    }

    if (!modeSelected)
    {
        std::cerr << "run requires an explicit startup mode: --headless or --windowed.\n";
        return 2;
    }

    trace2d::platform::PlatformConfig config{};
    config.mode = mode;

    try
    {
        trace2d::platform::Platform platform{config};
        trace2d::platform::PlatformEvent event{};
        while (platform.PollEvent(event))
        {
            if (event.type == trace2d::platform::PlatformEventType::QuitRequested)
            {
                break;
            }
        }

        if (json)
        {
            std::cout << "{\"command\":\"run\",\"mode\":\"" << trace2d::platform::ToString(platform.Mode())
                      << "\",\"window_created\":" << (platform.HasWindow() ? "true" : "false")
                      << ",\"status\":\"ok\"}\n";
            return 0;
        }

        std::cout << "Trace2D platform smoke\n"
                  << "  mode: " << trace2d::platform::ToString(platform.Mode()) << '\n'
                  << "  window created: " << (platform.HasWindow() ? "yes" : "no") << '\n'
                  << "  status: ok\n";
        return 0;
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
            std::cerr << "Trace2D platform startup failed: " << exception.what() << '\n';
        }
        return 3;
    }
}
} // namespace

int main(const int argc, char* argv[])
{
    if (argc < 2)
    {
        PrintHelp();
        return 0;
    }

    const std::string_view command{argv[1]};

    if (command == "version" || command == "--version")
    {
        std::cout << trace2d::core::Version() << '\n';
        return 0;
    }

    if (command == "doctor")
    {
        const bool json = argc >= 3 && std::string_view{argv[2]} == "--json";
        return RunDoctor(json);
    }

    if (command == "run")
    {
        return RunPlatformSmoke(argc, argv);
    }

    std::cerr << "Unknown command: " << command << "\n\n";
    PrintHelp();
    return 2;
}
