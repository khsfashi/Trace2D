#include <trace2d/core/Version.hpp>

#include <iostream>
#include <string_view>

namespace
{
void PrintHelp()
{
    std::cout << "Trace2D command line interface\n\n"
              << "Usage:\n"
              << "  trace2d version\n"
              << "  trace2d doctor [--json]\n";
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

    std::cerr << "Unknown command: " << command << "\n\n";
    PrintHelp();
    return 2;
}
