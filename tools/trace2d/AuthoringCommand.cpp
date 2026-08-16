#include "AgentCommands.hpp"
#include "AuthoringCommands.hpp"

#include <iostream>
#include <string_view>

namespace trace2d::tools
{
namespace
{
constexpr int ExitSuccess = 0;
constexpr int ExitUsage = 2;

void PrintAuthoringUsage()
{
    std::cout
        << "Usage:\n"
        << "  trace2d author sprite ...\n"
        << "  trace2d author particle ...\n";
}
} // namespace

int RunAuthorCommand(const int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cerr << "author requires a resource kind.\n";
        PrintAuthoringUsage();
        return ExitUsage;
    }

    const std::string_view kind{argv[2]};
    if (kind == "--help")
    {
        PrintAuthoringUsage();
        return ExitSuccess;
    }
    if (kind == "sprite")
    {
        return RunSpriteAuthorCommand(argc, argv);
    }
    if (kind == "particle")
    {
        return RunParticleAuthorCommand(argc, argv);
    }

    std::cerr << "Unsupported authoring resource kind: " << kind << '\n';
    PrintAuthoringUsage();
    return ExitUsage;
}
} // namespace trace2d::tools
