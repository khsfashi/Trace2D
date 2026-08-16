#include "AuthoringCommands.hpp"

#include <trace2d/agent/SpriteAuthoring.hpp>

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

bool TryParseUnsigned32(const std::string_view text, std::uint32_t& value) noexcept
{
    if (text.empty())
    {
        return false;
    }

    std::uint64_t parsed = 0U;
    const char* const begin = text.data();
    const char* const end = begin + text.size();
    const std::from_chars_result result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end ||
        parsed > static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        return false;
    }

    value = static_cast<std::uint32_t>(parsed);
    return true;
}

bool TryParseSigned64(const std::string_view text, std::int64_t& value) noexcept
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

void PrintSpriteAuthoringUsage()
{
    std::cout
        << "Usage:\n"
        << "  trace2d author sprite --project ROOT --resource PATH [--region ID]"
           " [--sampling nearest|linear] [--source-size W H] [--trim-offset X Y]"
           " [--trim-size W H] [--packed-rect X Y W H] [--pivot X Y DEN] [--json]\n";
}

int PrintUsageError(const bool json, const std::string_view message)
{
    if (json)
    {
        std::cerr << "{\"command\":\"author\",\"kind\":\"sprite\",\"status\":\"error\","
                     "\"code\":\"invalid_request\",\"message\":\""
                  << EscapeJson(message) << "\"}\n";
    }
    else
    {
        std::cerr << message << '\n';
        PrintSpriteAuthoringUsage();
    }
    return ExitUsage;
}

void PrintResultJson(const agent::SpriteAuthoringResult& result)
{
    std::cout << "{\"command\":\"author\",\"kind\":\"sprite\",\"resource\":\""
              << EscapeJson(result.resource) << "\",\"status\":\""
              << (result.Succeeded() ? "ok" : "error")
              << "\",\"committed\":" << (result.committed ? "true" : "false")
              << ",\"validation_passed\":" << (result.validationPassed ? "true" : "false")
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
        const agent::SpriteAuthoringDiagnostic& diagnostic = result.diagnostics.front();
        std::cout << ",\"code\":\"" << agent::ToString(diagnostic.code)
                  << "\",\"path\":\"" << EscapeJson(diagnostic.path)
                  << "\",\"message\":\"" << EscapeJson(diagnostic.message)
                  << "\",\"diagnostic_count\":" << result.diagnostics.size();
    }

    std::cout << "}\n";
}

void PrintResultHuman(const agent::SpriteAuthoringResult& result)
{
    if (!result.Succeeded())
    {
        const agent::SpriteAuthoringDiagnostic& diagnostic = result.diagnostics.front();
        std::cerr << "Sprite authoring failed [" << agent::ToString(diagnostic.code) << "] "
                  << diagnostic.path << ": " << diagnostic.message;
        if (result.diagnostics.size() > 1U)
        {
            std::cerr << " (" << result.diagnostics.size() << " diagnostics)";
        }
        std::cerr << '\n';
        return;
    }

    std::cout << "Sprite authoring\n"
              << "  resource: " << result.resource << '\n'
              << "  validation: passed\n"
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

int RunSpriteAuthorCommand(const int argc, char* argv[])
{
    const bool json = HasJsonFlag(argc, argv);
    if (argc == 4 && std::string_view{argv[3]} == "--help")
    {
        PrintSpriteAuthoringUsage();
        return ExitSuccess;
    }

    std::string projectRoot{};
    std::string resource{};
    std::string regionId{};
    agent::SpriteMutation mutation{};
    agent::SpriteRegionMutation regionMutation{};
    bool hasRegionChange = false;

    bool seenProject = false;
    bool seenResource = false;
    bool seenRegion = false;
    bool seenSampling = false;
    bool seenSourceSize = false;
    bool seenTrimOffset = false;
    bool seenTrimSize = false;
    bool seenPackedRect = false;
    bool seenPivot = false;
    bool seenJson = false;

    std::string errorMessage{};
    const auto claim = [&errorMessage](bool& seen, const std::string_view option) {
        if (seen)
        {
            errorMessage = "Duplicate sprite authoring option: " + std::string{option};
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
        if (argument == "--region")
        {
            if (!claim(seenRegion, argument) || !requireValues(index, 1, argument))
                return PrintUsageError(json, errorMessage);
            regionId = argv[++index];
            if (regionId.empty()) return PrintUsageError(json, "--region requires a non-empty stable id.");
            continue;
        }
        if (argument == "--sampling")
        {
            if (!claim(seenSampling, argument) || !requireValues(index, 1, argument))
                return PrintUsageError(json, errorMessage);
            const std::string_view value{argv[++index]};
            if (value == "nearest")
            {
                mutation.sampling = assets::SpriteSampling::Nearest;
            }
            else if (value == "linear")
            {
                mutation.sampling = assets::SpriteSampling::Linear;
            }
            else
            {
                return PrintUsageError(json, "--sampling must be nearest or linear.");
            }
            continue;
        }
        if (argument == "--source-size")
        {
            if (!claim(seenSourceSize, argument) || !requireValues(index, 2, argument))
                return PrintUsageError(json, errorMessage);
            assets::SpritePixelSize value{};
            if (!TryParseUnsigned32(argv[index + 1], value.width) ||
                !TryParseUnsigned32(argv[index + 2], value.height))
                return PrintUsageError(json, "--source-size values must be unsigned 32-bit integers.");
            index += 2;
            regionMutation.sourceSize = value;
            hasRegionChange = true;
            continue;
        }
        if (argument == "--trim-offset")
        {
            if (!claim(seenTrimOffset, argument) || !requireValues(index, 2, argument))
                return PrintUsageError(json, errorMessage);
            assets::SpritePixelOffset value{};
            if (!TryParseUnsigned32(argv[index + 1], value.x) ||
                !TryParseUnsigned32(argv[index + 2], value.y))
                return PrintUsageError(json, "--trim-offset values must be unsigned 32-bit integers.");
            index += 2;
            regionMutation.trimOffset = value;
            hasRegionChange = true;
            continue;
        }
        if (argument == "--trim-size")
        {
            if (!claim(seenTrimSize, argument) || !requireValues(index, 2, argument))
                return PrintUsageError(json, errorMessage);
            assets::SpritePixelSize value{};
            if (!TryParseUnsigned32(argv[index + 1], value.width) ||
                !TryParseUnsigned32(argv[index + 2], value.height))
                return PrintUsageError(json, "--trim-size values must be unsigned 32-bit integers.");
            index += 2;
            regionMutation.trimSize = value;
            hasRegionChange = true;
            continue;
        }
        if (argument == "--packed-rect")
        {
            if (!claim(seenPackedRect, argument) || !requireValues(index, 4, argument))
                return PrintUsageError(json, errorMessage);
            assets::SpritePixelRect value{};
            if (!TryParseUnsigned32(argv[index + 1], value.x) ||
                !TryParseUnsigned32(argv[index + 2], value.y) ||
                !TryParseUnsigned32(argv[index + 3], value.width) ||
                !TryParseUnsigned32(argv[index + 4], value.height))
                return PrintUsageError(json, "--packed-rect values must be unsigned 32-bit integers.");
            index += 4;
            regionMutation.packedRect = value;
            hasRegionChange = true;
            continue;
        }
        if (argument == "--pivot")
        {
            if (!claim(seenPivot, argument) || !requireValues(index, 3, argument))
                return PrintUsageError(json, errorMessage);
            assets::SpriteRationalPivot value{};
            if (!TryParseSigned64(argv[index + 1], value.xNumerator) ||
                !TryParseSigned64(argv[index + 2], value.yNumerator) ||
                !TryParseSigned64(argv[index + 3], value.denominator))
                return PrintUsageError(json, "--pivot values must be signed 64-bit integers.");
            index += 3;
            regionMutation.pivot = value;
            hasRegionChange = true;
            continue;
        }

        return PrintUsageError(json, "Unknown sprite authoring option: " + std::string{argument});
    }

    if (!seenProject) return PrintUsageError(json, "Sprite authoring requires --project ROOT.");
    if (!seenResource) return PrintUsageError(json, "Sprite authoring requires --resource PATH.");
    if (!mutation.sampling.has_value() && !hasRegionChange)
        return PrintUsageError(json, "Sprite authoring requires at least one explicit typed change.");
    if (hasRegionChange && !seenRegion)
        return PrintUsageError(json, "Region field changes require --region ID.");
    if (!hasRegionChange && seenRegion)
        return PrintUsageError(json, "--region is only valid with an explicit region field change.");

    if (hasRegionChange)
    {
        regionMutation.regionId = regionId;
        mutation.region = regionMutation;
    }

    const agent::SpriteAuthoringResult result = agent::MutateSpriteResource(
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
