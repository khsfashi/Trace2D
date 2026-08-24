#include "AgentCommands.hpp"

#include <trace2d/agent/Inspection.hpp>
#include <trace2d/runtime/FixedStepRuntime.hpp>
#include <trace2d/scene/SceneText.hpp>

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace trace2d::tools
{
namespace
{
constexpr int ExitSuccess = 0;
constexpr int ExitUsage = 2;
constexpr int ExitIoFailure = 3;
constexpr int ExitSceneLoadFailure = 4;
constexpr int ExitAgentFailure = 5;

struct SceneFileResult final
{
    std::optional<scene::Scene> scene{};
    std::vector<scene::SceneTextDiagnostic> diagnostics{};
    std::string errorCode{};
    std::string errorMessage{};
    int exitCode{ExitSuccess};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return scene.has_value() && exitCode == ExitSuccess;
    }
};

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

bool ReadTextFile(const std::string& path, std::string& text, std::string& errorMessage)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        errorMessage = "Unable to open scene file: " + path;
        return false;
    }

    std::ostringstream buffer{};
    buffer << input.rdbuf();
    if (input.bad())
    {
        errorMessage = "Failed while reading scene file: " + path;
        return false;
    }

    text = buffer.str();
    return true;
}

SceneFileResult LoadSceneFile(const std::string& path)
{
    SceneFileResult result{};

    std::string sceneText{};
    if (!ReadTextFile(path, sceneText, result.errorMessage))
    {
        result.errorCode = "io_error";
        result.exitCode = ExitIoFailure;
        return result;
    }

    scene::SceneLoadResult loadResult = scene::LoadSceneToml(sceneText, path);
    if (!loadResult.Succeeded())
    {
        result.errorCode = "scene_load_failed";
        result.errorMessage = "Scene text failed validation.";
        result.diagnostics = std::move(loadResult.diagnostics);
        result.exitCode = ExitSceneLoadFailure;
        return result;
    }

    result.scene = std::move(loadResult.scene);
    return result;
}

void WriteJsonFloat(std::ostream& output, const float value)
{
    if (!std::isfinite(value))
    {
        throw std::runtime_error{"Agent snapshot contains a non-finite floating-point value."};
    }

    char buffer[64]{};
    const std::to_chars_result result = std::to_chars(
        buffer,
        buffer + sizeof(buffer),
        value,
        std::chars_format::general,
        std::numeric_limits<float>::max_digits10);

    if (result.ec != std::errc{})
    {
        throw std::runtime_error{"Failed to serialize an agent floating-point value."};
    }

    output.write(buffer, result.ptr - buffer);
}

void WriteFieldValueJson(std::ostream& output, const agent::FieldValue& value)
{
    switch (value.kind)
    {
    case agent::FieldValueKind::Boolean:
        output << (value.booleanValue ? "true" : "false");
        return;
    case agent::FieldValueKind::SignedInteger:
        output << value.signedIntegerValue;
        return;
    case agent::FieldValueKind::UnsignedInteger:
        output << value.unsignedIntegerValue;
        return;
    case agent::FieldValueKind::Float:
        WriteJsonFloat(output, value.floatValue);
        return;
    case agent::FieldValueKind::String:
        output << '"' << EscapeJson(value.stringValue) << '"';
        return;
    case agent::FieldValueKind::Float2:
        output << '[';
        WriteJsonFloat(output, value.vectorValue[0]);
        output << ',';
        WriteJsonFloat(output, value.vectorValue[1]);
        output << ']';
        return;
    case agent::FieldValueKind::Float4:
        output << '[';
        WriteJsonFloat(output, value.vectorValue[0]);
        output << ',';
        WriteJsonFloat(output, value.vectorValue[1]);
        output << ',';
        WriteJsonFloat(output, value.vectorValue[2]);
        output << ',';
        WriteJsonFloat(output, value.vectorValue[3]);
        output << ']';
        return;
    case agent::FieldValueKind::EntityReference:
    case agent::FieldValueKind::ResourceReference:
    case agent::FieldValueKind::EnumName:
        output << '"' << EscapeJson(value.stringValue) << '"';
        return;
    }

    throw std::runtime_error{"Agent snapshot contains an unknown field value kind."};
}

void WriteVector2Json(std::ostream& output, const agent::Vector2Snapshot& value)
{
    output << "{\"x\":";
    WriteJsonFloat(output, value.x);
    output << ",\"y\":";
    WriteJsonFloat(output, value.y);
    output << '}';
}

void WriteTransformJson(std::ostream& output, const agent::Transform2DSnapshot& transform)
{
    output << "{\"position\":";
    WriteVector2Json(output, transform.position);
    output << ",\"rotation_radians\":";
    WriteJsonFloat(output, transform.rotationRadians);
    output << ",\"scale\":";
    WriteVector2Json(output, transform.scale);
    output << '}';
}

void WriteComponentJson(std::ostream& output, const agent::ComponentSnapshot& component)
{
    output << "{\"type\":\"" << EscapeJson(component.type) << "\",\"fields\":[";

    for (std::size_t index = 0; index < component.fields.size(); ++index)
    {
        if (index != 0U)
        {
            output << ',';
        }

        const agent::ComponentFieldSnapshot& field = component.fields[index];
        output << "{\"name\":\"" << EscapeJson(field.name)
               << "\",\"type\":\"" << agent::ToString(field.value.kind)
               << "\",\"value\":";
        WriteFieldValueJson(output, field.value);
        output << '}';
    }

    output << "]}";
}

void WriteEntityJson(std::ostream& output, const agent::EntitySnapshot& entity)
{
    output << "{\"handle\":{\"index\":" << entity.handle.index
           << ",\"generation\":" << entity.handle.generation
           << "},\"id\":\"" << EscapeJson(entity.semanticId)
           << "\",\"name\":\"" << EscapeJson(entity.name)
           << "\",\"tags\":[";

    for (std::size_t index = 0; index < entity.tags.size(); ++index)
    {
        if (index != 0U)
        {
            output << ',';
        }
        output << '"' << EscapeJson(entity.tags[index]) << '"';
    }

    output << "],\"transform\":";
    WriteTransformJson(output, entity.transform);
    output << ",\"bounds\":";

    if (entity.bounds.has_value())
    {
        output << "{\"center\":";
        WriteVector2Json(output, entity.bounds->center);
        output << ",\"extents\":";
        WriteVector2Json(output, entity.bounds->extents);
        output << '}';
    }
    else
    {
        output << "null";
    }

    output << ",\"components\":[";
    for (std::size_t index = 0; index < entity.components.size(); ++index)
    {
        if (index != 0U)
        {
            output << ',';
        }
        WriteComponentJson(output, entity.components[index]);
    }
    output << "]}";
}

void WriteSceneDiagnosticsJson(
    std::ostream& output,
    const std::vector<scene::SceneTextDiagnostic>& diagnostics)
{
    output << '[';
    for (std::size_t index = 0; index < diagnostics.size(); ++index)
    {
        if (index != 0U)
        {
            output << ',';
        }

        const scene::SceneTextDiagnostic& diagnostic = diagnostics[index];
        output << "{\"path\":\"" << EscapeJson(diagnostic.path)
               << "\",\"message\":\"" << EscapeJson(diagnostic.message)
               << "\",\"line\":" << diagnostic.line
               << ",\"column\":" << diagnostic.column << '}';
    }
    output << ']';
}

void PrintErrorJson(
    const std::string_view command,
    const std::string_view code,
    const std::string_view message,
    const std::vector<scene::SceneTextDiagnostic>* diagnostics = nullptr)
{
    std::cerr << "{\"command\":\"" << EscapeJson(command)
              << "\",\"status\":\"error\",\"code\":\"" << EscapeJson(code)
              << "\",\"message\":\"" << EscapeJson(message) << '"';

    if (diagnostics != nullptr)
    {
        std::cerr << ",\"diagnostics\":";
        WriteSceneDiagnosticsJson(std::cerr, *diagnostics);
    }

    std::cerr << "}\n";
}

void PrintSceneLoadErrorHuman(const SceneFileResult& loadResult)
{
    std::cerr << loadResult.errorMessage << '\n';
    for (const scene::SceneTextDiagnostic& diagnostic : loadResult.diagnostics)
    {
        std::cerr << "  " << diagnostic.path << ": " << diagnostic.message;
        if (diagnostic.line != 0U)
        {
            std::cerr << " (" << diagnostic.line << ':' << diagnostic.column << ')';
        }
        std::cerr << '\n';
    }
}

void WriteInspectionJson(std::ostream& output, const agent::InspectionSnapshot& snapshot)
{
    output << "{\"command\":\"inspect\",\"status\":\"ok\",\"runtime\":{\"frame\":"
           << snapshot.runtime.frame << ",\"seed\":" << snapshot.runtime.seed
           << ",\"fixed_step_ns\":" << snapshot.runtime.fixedStepNanoseconds
           << ",\"simulation_time_ns\":" << snapshot.runtime.simulationTimeNanoseconds
           << "},\"scene\":{\"id\":\"" << EscapeJson(snapshot.scene.semanticId)
           << "\",\"name\":\"" << EscapeJson(snapshot.scene.name)
           << "\",\"entity_count\":" << snapshot.scene.entities.size()
           << ",\"entities\":[";

    for (std::size_t index = 0; index < snapshot.scene.entities.size(); ++index)
    {
        if (index != 0U)
        {
            output << ',';
        }
        WriteEntityJson(output, snapshot.scene.entities[index]);
    }

    output << "]}}\n";
}

void WriteInspectionHuman(const agent::InspectionSnapshot& snapshot)
{
    std::cout << "Trace2D inspection\n"
              << "  frame: " << snapshot.runtime.frame << '\n'
              << "  seed: " << snapshot.runtime.seed << '\n'
              << "  scene: " << snapshot.scene.semanticId;

    if (!snapshot.scene.name.empty())
    {
        std::cout << " (" << snapshot.scene.name << ')';
    }

    std::cout << "\n  entities: " << snapshot.scene.entities.size() << '\n';

    for (const agent::EntitySnapshot& entity : snapshot.scene.entities)
    {
        std::cout << "    - " << entity.semanticId
                  << " [" << entity.handle.index << ':' << entity.handle.generation << ']';
        if (!entity.name.empty())
        {
            std::cout << " " << entity.name;
        }
        std::cout << '\n';
    }
}

void WriteSelectorJson(
    std::ostream& output,
    const std::string_view selectorText,
    const agent::SemanticSelector& selector)
{
    output << "{\"text\":\"" << EscapeJson(selectorText)
           << "\",\"kind\":\"" << agent::ToString(selector.kind)
           << "\",\"value\":\"" << EscapeJson(selector.value) << "\"}";
}

void WriteQueryManyJson(
    std::ostream& output,
    const std::string_view selectorText,
    const agent::QueryResult& result)
{
    output << "{\"command\":\"query\",\"status\":\"ok\",\"mode\":\"many\",\"selector\":";
    WriteSelectorJson(output, selectorText, *result.selector);
    output << ",\"match_count\":" << result.matches.size() << ",\"matches\":[";

    for (std::size_t index = 0; index < result.matches.size(); ++index)
    {
        if (index != 0U)
        {
            output << ',';
        }
        WriteEntityJson(output, result.matches[index]);
    }

    output << "]}\n";
}

void WriteQueryOneJson(
    std::ostream& output,
    const std::string_view selectorText,
    const agent::QueryOneResult& result)
{
    output << "{\"command\":\"query\",\"status\":\"ok\",\"mode\":\"one\",\"selector\":";
    WriteSelectorJson(output, selectorText, *result.selector);
    output << ",\"match_count\":1,\"matches\":[";
    WriteEntityJson(output, *result.match);
    output << "]}\n";
}

void WriteQueryHuman(
    const std::string_view selectorText,
    const std::string_view mode,
    const std::vector<agent::EntitySnapshot>& matches)
{
    std::cout << "Trace2D query\n"
              << "  selector: " << selectorText << '\n'
              << "  mode: " << mode << '\n'
              << "  matches: " << matches.size() << '\n';

    for (const agent::EntitySnapshot& entity : matches)
    {
        std::cout << "    - " << entity.semanticId
                  << " [" << entity.handle.index << ':' << entity.handle.generation << ']';
        if (!entity.name.empty())
        {
            std::cout << " " << entity.name;
        }
        std::cout << '\n';
    }
}
} // namespace

int RunInspectCommand(const int argc, char* argv[])
{
    bool json = false;
    std::string scenePath{};
    std::uint64_t frameCount = 0;
    std::uint64_t seed = 0;

    for (int index = 2; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};

        if (argument == "--json")
        {
            json = true;
            continue;
        }

        if (argument == "--scene")
        {
            if (index + 1 >= argc)
            {
                std::cerr << "--scene requires a file path.\n";
                return ExitUsage;
            }
            if (!scenePath.empty())
            {
                std::cerr << "--scene may only be specified once.\n";
                return ExitUsage;
            }

            scenePath = argv[++index];
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

        std::cerr << "Unknown inspect option: " << argument << '\n';
        return ExitUsage;
    }

    if (scenePath.empty())
    {
        std::cerr << "inspect requires --scene PATH.\n";
        return ExitUsage;
    }

    try
    {
        SceneFileResult loadResult = LoadSceneFile(scenePath);
        if (!loadResult.Succeeded())
        {
            if (json)
            {
                const std::vector<scene::SceneTextDiagnostic>* diagnostics =
                    loadResult.diagnostics.empty() ? nullptr : &loadResult.diagnostics;
                PrintErrorJson("inspect", loadResult.errorCode, loadResult.errorMessage, diagnostics);
            }
            else
            {
                PrintSceneLoadErrorHuman(loadResult);
            }
            return loadResult.exitCode;
        }

        runtime::RuntimeConfig runtimeConfig{};
        runtimeConfig.seed = seed;
        runtime::FixedStepRuntime runtime{runtimeConfig};
        runtime.Step(frameCount);

        agent::AgentFacade facade{&runtime, &*loadResult.scene};
        const agent::InspectionResult inspection = facade.Inspect();
        if (!inspection.Succeeded() || !inspection.snapshot.has_value())
        {
            const agent::InspectionError error = inspection.error.value_or(
                agent::InspectionError{
                    .code = agent::InspectionErrorCode::SceneUnavailable,
                    .message = "Inspection failed without a structured result.",
                });

            if (json)
            {
                PrintErrorJson("inspect", agent::ToString(error.code), error.message);
            }
            else
            {
                std::cerr << "Inspection failed [" << agent::ToString(error.code)
                          << "]: " << error.message << '\n';
            }
            return ExitAgentFailure;
        }

        if (json)
        {
            WriteInspectionJson(std::cout, *inspection.snapshot);
        }
        else
        {
            WriteInspectionHuman(*inspection.snapshot);
        }
        return ExitSuccess;
    }
    catch (const std::exception& exception)
    {
        if (json)
        {
            PrintErrorJson("inspect", "inspection_failed", exception.what());
        }
        else
        {
            std::cerr << "Inspection failed: " << exception.what() << '\n';
        }
        return ExitAgentFailure;
    }
}

int RunQueryCommand(const int argc, char* argv[])
{
    bool json = false;
    bool singleResult = false;
    std::string scenePath{};
    std::string selectorText{};
    std::uint64_t frameCount = 0;
    std::uint64_t seed = 0;

    for (int index = 2; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};

        if (argument == "--json")
        {
            json = true;
            continue;
        }

        if (argument == "--one")
        {
            singleResult = true;
            continue;
        }

        if (argument == "--scene" || argument == "--selector")
        {
            if (index + 1 >= argc)
            {
                std::cerr << argument << " requires a value.\n";
                return ExitUsage;
            }

            std::string& destination = argument == "--scene" ? scenePath : selectorText;
            if (!destination.empty())
            {
                std::cerr << argument << " may only be specified once.\n";
                return ExitUsage;
            }

            destination = argv[++index];
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

        std::cerr << "Unknown query option: " << argument << '\n';
        return ExitUsage;
    }

    if (scenePath.empty() || selectorText.empty())
    {
        std::cerr << "query requires --scene PATH and --selector SELECTOR.\n";
        return ExitUsage;
    }

    try
    {
        SceneFileResult loadResult = LoadSceneFile(scenePath);
        if (!loadResult.Succeeded())
        {
            if (json)
            {
                const std::vector<scene::SceneTextDiagnostic>* diagnostics =
                    loadResult.diagnostics.empty() ? nullptr : &loadResult.diagnostics;
                PrintErrorJson("query", loadResult.errorCode, loadResult.errorMessage, diagnostics);
            }
            else
            {
                PrintSceneLoadErrorHuman(loadResult);
            }
            return loadResult.exitCode;
        }

        runtime::RuntimeConfig runtimeConfig{};
        runtimeConfig.seed = seed;
        runtime::FixedStepRuntime runtime{runtimeConfig};
        runtime.Step(frameCount);

        agent::AgentFacade facade{&runtime, &*loadResult.scene};

        if (singleResult)
        {
            agent::QueryOneResult result = facade.QueryOne(selectorText);
            if (!result.Succeeded())
            {
                const agent::QueryError error = result.error.value_or(
                    agent::QueryError{
                        .code = agent::QueryErrorCode::InvalidSelector,
                        .message = "Query failed without a structured result.",
                    });

                if (json)
                {
                    PrintErrorJson("query", agent::ToString(error.code), error.message);
                }
                else
                {
                    std::cerr << "Query failed [" << agent::ToString(error.code)
                              << "]: " << error.message << '\n';
                }
                return ExitAgentFailure;
            }

            if (json)
            {
                WriteQueryOneJson(std::cout, selectorText, result);
            }
            else
            {
                const std::vector<agent::EntitySnapshot> matches{*result.match};
                WriteQueryHuman(selectorText, "one", matches);
            }
            return ExitSuccess;
        }

        agent::QueryResult result = facade.Query(selectorText);
        if (!result.Succeeded())
        {
            const agent::QueryError error = result.error.value_or(
                agent::QueryError{
                    .code = agent::QueryErrorCode::InvalidSelector,
                    .message = "Query failed without a structured result.",
                });

            if (json)
            {
                PrintErrorJson("query", agent::ToString(error.code), error.message);
            }
            else
            {
                std::cerr << "Query failed [" << agent::ToString(error.code)
                          << "]: " << error.message << '\n';
            }
            return ExitAgentFailure;
        }

        if (json)
        {
            WriteQueryManyJson(std::cout, selectorText, result);
        }
        else
        {
            WriteQueryHuman(selectorText, "many", result.matches);
        }
        return ExitSuccess;
    }
    catch (const std::exception& exception)
    {
        if (json)
        {
            PrintErrorJson("query", "query_failed", exception.what());
        }
        else
        {
            std::cerr << "Query failed: " << exception.what() << '\n';
        }
        return ExitAgentFailure;
    }
}
} // namespace trace2d::tools
