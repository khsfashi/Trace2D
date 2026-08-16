#include "AuthoringCommands.hpp"

#include <trace2d/agent/ParticleAuthoring.hpp>

#include <charconv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>

namespace trace2d::tools
{
namespace
{
constexpr int ExitSuccess = 0;
constexpr int ExitUsage = 2;
constexpr int ExitAuthoringFailure = 3;

std::string EscapeJson(const std::string_view value)
{
    constexpr char HexDigits[] = "0123456789abcdef";

    std::string escaped{};
    escaped.reserve(value.size());
    for (const unsigned char byte : value)
    {
        switch (byte)
        {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\b': escaped += "\\b"; break;
        case '\f': escaped += "\\f"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
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

bool HasJsonFlag(const int argc, char* argv[]) noexcept
{
    for (int index = 2; index < argc; ++index)
    {
        if (std::string_view{argv[index]} == "--json")
        {
            return true;
        }
    }
    return false;
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

bool TryParseUnsigned32(const std::string_view text, std::uint32_t& value) noexcept
{
    std::uint64_t parsed = 0U;
    if (!TryParseUnsigned64(text, parsed) ||
        parsed > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool TryParseBool(const std::string_view text, bool& value) noexcept
{
    if (text == "true")
    {
        value = true;
        return true;
    }
    if (text == "false")
    {
        value = false;
        return true;
    }
    return false;
}

void PrintParticleAuthoringUsage()
{
    std::cout
        << "Usage:\n"
        << "  trace2d author particle --project ROOT --resource PATH"
           " [--effect-id ID] [--max-particles N] [--duration-frames N]"
           " [--loop true|false] [--play-on-load true|false]"
           " [--emission-start-frame N] [--emission-count N]"
           " [--emission-every-frames N] [--lifetime-frames MIN MAX] [--json]\n";
}

int PrintUsageError(const bool json, const std::string_view message)
{
    if (json)
    {
        std::cerr << "{\"command\":\"author\",\"kind\":\"particle\",\"status\":\"error\","
                     "\"code\":\"invalid_request\",\"message\":\""
                  << EscapeJson(message) << "\"}\n";
    }
    else
    {
        std::cerr << message << '\n';
        PrintParticleAuthoringUsage();
    }
    return ExitUsage;
}

void PrintResultJson(const agent::ParticleAuthoringResult& result)
{
    std::cout << "{\"command\":\"author\",\"kind\":\"particle\",\"resource\":\""
              << EscapeJson(result.resource) << "\",\"status\":\""
              << (result.Succeeded() ? "ok" : "error")
              << "\",\"committed\":" << (result.committed ? "true" : "false")
              << ",\"validation_passed\":" << (result.validationPassed ? "true" : "false")
              << ",\"program_fingerprint\":" << result.programFingerprint
              << ",\"changed_fields\":[";

    for (std::size_t index = 0U; index < result.changedFields.size(); ++index)
    {
        if (index != 0U)
        {
            std::cout << ',';
        }
        std::cout << '"' << EscapeJson(result.changedFields[index]) << '"';
    }
    std::cout << ']';

    if (!result.diagnostics.empty())
    {
        const agent::ParticleAuthoringDiagnostic& diagnostic = result.diagnostics.front();
        std::cout << ",\"code\":\"" << agent::ToString(diagnostic.code) << '"';
        if (!diagnostic.sourceCode.empty())
        {
            std::cout << ",\"source_code\":\"" << EscapeJson(diagnostic.sourceCode) << '"';
        }
        std::cout << ",\"path\":\"" << EscapeJson(diagnostic.path)
                  << "\",\"message\":\"" << EscapeJson(diagnostic.message)
                  << "\",\"diagnostic_count\":" << result.diagnostics.size();
    }

    std::cout << "}\n";
}

void PrintResultHuman(const agent::ParticleAuthoringResult& result)
{
    if (!result.Succeeded())
    {
        const agent::ParticleAuthoringDiagnostic& diagnostic = result.diagnostics.front();
        std::cerr << "Particle authoring failed [" << agent::ToString(diagnostic.code);
        if (!diagnostic.sourceCode.empty())
        {
            std::cerr << '/' << diagnostic.sourceCode;
        }
        std::cerr << "] " << diagnostic.path << ": " << diagnostic.message;
        if (result.diagnostics.size() > 1U)
        {
            std::cerr << " (" << result.diagnostics.size() << " diagnostics)";
        }
        std::cerr << '\n';
        return;
    }

    std::cout << "Particle authoring\n"
              << "  resource: " << result.resource << '\n'
              << "  validation: passed\n"
              << "  program fingerprint: " << result.programFingerprint << '\n'
              << "  committed: " << (result.committed ? "yes" : "no") << '\n'
              << "  changed fields: ";
    if (result.changedFields.empty())
    {
        std::cout << "none";
    }
    else
    {
        for (std::size_t index = 0U; index < result.changedFields.size(); ++index)
        {
            if (index != 0U)
            {
                std::cout << ", ";
            }
            std::cout << result.changedFields[index];
        }
    }
    std::cout << '\n';
}
} // namespace

int RunParticleAuthorCommand(const int argc, char* argv[])
{
    const bool json = HasJsonFlag(argc, argv);
    if (argc == 4 && std::string_view{argv[3]} == "--help")
    {
        PrintParticleAuthoringUsage();
        return ExitSuccess;
    }

    std::string projectRoot{};
    std::string resource{};
    agent::ParticleEffectMutation mutation{};

    bool seenProject = false;
    bool seenResource = false;
    bool seenEffectId = false;
    bool seenMaxParticles = false;
    bool seenDurationFrames = false;
    bool seenLoop = false;
    bool seenPlayOnLoad = false;
    bool seenEmissionStartFrame = false;
    bool seenEmissionCount = false;
    bool seenEmissionEveryFrames = false;
    bool seenLifetimeFrames = false;
    bool seenJson = false;

    std::string errorMessage{};
    const auto claim = [&errorMessage](bool& seen, const std::string_view option) {
        if (seen)
        {
            errorMessage = "Duplicate particle authoring option: " + std::string{option};
            return false;
        }
        seen = true;
        return true;
    };
    const auto requireValues = [&errorMessage, argc](
                                   const int index,
                                   const int count,
                                   const std::string_view option) {
        if (index + count >= argc)
        {
            errorMessage = std::string{option} + " requires " + std::to_string(count) +
                (count == 1 ? " value." : " values.");
            return false;
        }
        return true;
    };

    for (int index = 3; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};

        if (argument == "--json")
        {
            if (!claim(seenJson, argument)) return PrintUsageError(json, errorMessage);
            continue;
        }
        if (argument == "--project")
        {
            if (!claim(seenProject, argument) || !requireValues(index, 1, argument))
                return PrintUsageError(json, errorMessage);
            projectRoot = argv[++index];
            if (projectRoot.empty()) return PrintUsageError(json, "--project requires a non-empty path.");
            continue;
        }
        if (argument == "--resource")
        {
            if (!claim(seenResource, argument) || !requireValues(index, 1, argument))
                return PrintUsageError(json, errorMessage);
            resource = argv[++index];
            if (resource.empty()) return PrintUsageError(json, "--resource requires a non-empty path.");
            continue;
        }
        if (argument == "--effect-id")
        {
            if (!claim(seenEffectId, argument) || !requireValues(index, 1, argument))
                return PrintUsageError(json, errorMessage);
            const std::string_view value{argv[++index]};
            if (value.empty()) return PrintUsageError(json, "--effect-id requires a non-empty semantic id.");
            mutation.semanticId = std::string{value};
            continue;
        }
        if (argument == "--max-particles")
        {
            if (!claim(seenMaxParticles, argument) || !requireValues(index, 1, argument))
                return PrintUsageError(json, errorMessage);
            std::uint32_t value = 0U;
            if (!TryParseUnsigned32(argv[++index], value))
                return PrintUsageError(json, "--max-particles must be an unsigned 32-bit integer.");
            mutation.maxParticles = value;
            continue;
        }
        if (argument == "--duration-frames")
        {
            if (!claim(seenDurationFrames, argument) || !requireValues(index, 1, argument))
                return PrintUsageError(json, errorMessage);
            std::uint32_t value = 0U;
            if (!TryParseUnsigned32(argv[++index], value))
                return PrintUsageError(json, "--duration-frames must be an unsigned 32-bit integer.");
            mutation.durationFrames = value;
            continue;
        }
        if (argument == "--loop")
        {
            if (!claim(seenLoop, argument) || !requireValues(index, 1, argument))
                return PrintUsageError(json, errorMessage);
            bool value = false;
            if (!TryParseBool(argv[++index], value))
                return PrintUsageError(json, "--loop must be true or false.");
            mutation.loop = value;
            continue;
        }
        if (argument == "--play-on-load")
        {
            if (!claim(seenPlayOnLoad, argument) || !requireValues(index, 1, argument))
                return PrintUsageError(json, errorMessage);
            bool value = false;
            if (!TryParseBool(argv[++index], value))
                return PrintUsageError(json, "--play-on-load must be true or false.");
            mutation.playOnLoad = value;
            continue;
        }
        if (argument == "--emission-start-frame")
        {
            if (!claim(seenEmissionStartFrame, argument) || !requireValues(index, 1, argument))
                return PrintUsageError(json, errorMessage);
            std::uint64_t value = 0U;
            if (!TryParseUnsigned64(argv[++index], value))
                return PrintUsageError(json, "--emission-start-frame must be an unsigned 64-bit integer.");
            mutation.emissionStartFrame = static_cast<particles::ParticleFrameIndex>(value);
            continue;
        }
        if (argument == "--emission-count")
        {
            if (!claim(seenEmissionCount, argument) || !requireValues(index, 1, argument))
                return PrintUsageError(json, errorMessage);
            std::uint32_t value = 0U;
            if (!TryParseUnsigned32(argv[++index], value))
                return PrintUsageError(json, "--emission-count must be an unsigned 32-bit integer.");
            mutation.emissionCount = value;
            continue;
        }
        if (argument == "--emission-every-frames")
        {
            if (!claim(seenEmissionEveryFrames, argument) || !requireValues(index, 1, argument))
                return PrintUsageError(json, errorMessage);
            std::uint32_t value = 0U;
            if (!TryParseUnsigned32(argv[++index], value))
                return PrintUsageError(json, "--emission-every-frames must be an unsigned 32-bit integer.");
            mutation.emissionEveryFrames = value;
            continue;
        }
        if (argument == "--lifetime-frames")
        {
            if (!claim(seenLifetimeFrames, argument) || !requireValues(index, 2, argument))
                return PrintUsageError(json, errorMessage);
            particles::ParticleUIntRange value{};
            if (!TryParseUnsigned32(argv[index + 1], value.minValue) ||
                !TryParseUnsigned32(argv[index + 2], value.maxValue))
                return PrintUsageError(json, "--lifetime-frames values must be unsigned 32-bit integers.");
            index += 2;
            mutation.lifetimeFrames = value;
            continue;
        }

        return PrintUsageError(json, "Unknown particle authoring option: " + std::string{argument});
    }

    if (!seenProject) return PrintUsageError(json, "Particle authoring requires --project ROOT.");
    if (!seenResource) return PrintUsageError(json, "Particle authoring requires --resource PATH.");
    if (!mutation.HasChanges())
        return PrintUsageError(json, "Particle authoring requires at least one explicit typed change.");

    const agent::ParticleAuthoringResult result = agent::MutateParticleEffectResource(
        projectRoot,
        resource,
        mutation);

    if (json)
    {
        PrintResultJson(result);
    }
    else
    {
        PrintResultHuman(result);
    }
    return result.Succeeded() ? ExitSuccess : ExitAuthoringFailure;
}
} // namespace trace2d::tools
