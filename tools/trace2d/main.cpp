#include <trace2d/agent/Inspection.hpp>
#include <trace2d/core/Version.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/runtime/FixedStepRuntime.hpp>
#include <trace2d/scene/SceneText.hpp>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace
{
constexpr int ExitSuccess = 0;
constexpr int ExitUsage = 2;
constexpr int ExitIoFailure = 3;
constexpr int ExitSceneLoadFailure = 4;
constexpr int ExitInspectionFailure = 5;

void PrintHelp()
{
    std::cout << "Trace2D command line interface\n\n"
              << "Usage:\n"
              << "  trace2d version\n"
              << "  trace2d doctor [--json]\n"
              << "  trace2d run (--headless|--windowed) [--frames N] [--seed N] [--json]\n"
              << "  trace2d inspect --scene PATH [--frames N] [--seed N] [--json]\n";
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

void WriteJsonFloat(std::ostream& output, const float value)
{
    if (!std::isfinite(value))
    {
        throw std::runtime_error{"Inspection contains a non-finite floating-point value."};
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
        throw std::runtime_error{"Failed to serialize an inspection floating-point value."};
    }

    output.write(buffer, result.ptr - buffer);
}

void WriteFieldValueJson(std::ostream& output, const trace2d::agent::FieldValue& value)
{
    switch (value.kind)
    {
    case trace2d::agent::FieldValueKind::Boolean:
        output << (value.booleanValue ? "true" : "false");
        return;
    case trace2d::agent::FieldValueKind::SignedInteger:
        output << value.signedIntegerValue;
        return;
    case trace2d::agent::FieldValueKind::UnsignedInteger:
        output << value.unsignedIntegerValue;
        return;
    case trace2d::agent::FieldValueKind::Float:
        WriteJsonFloat(output, value.floatValue);
        return;
    case trace2d::agent::FieldValueKind::String:
        output << '"' << EscapeJson(value.stringValue) << '"';
        return;
    }

    throw std::runtime_error{"Inspection contains an unknown field value kind."};
}

void WriteVector2Json(std::ostream& output, const trace2d::agent::Vector2Snapshot& value)
{
    output << "{\"x\":";
    WriteJsonFloat(output, value.x);
    output << ",\"y\":";
    WriteJsonFloat(output, value.y);
    output << '}';
}

void WriteTransformJson(std::ostream& output, const trace2d::agent::Transform2DSnapshot& transform)
{
    output << "{\"position\":";
    WriteVector2Json(output, transform.position);
    output << ",\"rotation_radians\":";
    WriteJsonFloat(output, transform.rotationRadians);
    output << ",\"scale\":";
    WriteVector2Json(output, transform.scale);
    output << '}';
}

void WriteComponentJson(std::ostream& output, const trace2d::agent::ComponentSnapshot& component)
{
    output << "{\"type\":\"" << EscapeJson(component.type) << "\",\"fields\":[";

    for (std::size_t index = 0; index < component.fields.size(); ++index)
    {
        if (index != 0)
        {
            output << ',';
        }

        const trace2d::agent::ComponentFieldSnapshot& field = component.fields[index];
        output << "{\"name\":\"" << EscapeJson(field.name)
               << "\",\"type\":\"" << trace2d::agent::ToString(field.value.kind)
               << "\",\"value\":";
        WriteFieldValueJson(output, field.value);
        output << '}';
    }

    output << "]}";
}

void WriteEntityJson(std::ostream& output, const trace2d::agent::EntitySnapshot& entity)
{
    output << "{\"handle\":{\"index\":" << entity.handle.index
           << ",\"generation\":" << entity.handle.generation
           << "},\"id\":\"" << EscapeJson(entity.semanticId)
           << "\",\"name\":\"" << EscapeJson(entity.name)
           << "\",\"tags\":[";

    for (std::size_t index = 0; index < entity.tags.size(); ++index)
    {
        if (index != 0)
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
        if (index != 0)
        {
            output << ',';
        }
        WriteComponentJson(output, entity.components[index]);
    }
    output << "]}";
}

void WriteInspectionJson(std::ostream& output, const trace2d::agent::InspectionSnapshot& snapshot)
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
        if (index != 0)
        {
            output << ',';
        }
        WriteEntityJson(output, snapshot.scene.entities[index]);
    }

    output << "]}}\n";
}

void WriteInspectionHuman(const trace2d::agent::InspectionSnapshot& snapshot)
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

    for (const trace2d::agent::EntitySnapshot& entity : snapshot.scene.entities)
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

void WriteSceneDiagnosticsJson(
    std::ostream& output,
    const std::vector<trace2d::scene::SceneTextDiagnostic>& diagnostics)
{
    output << '[';
    for (std::size_t index = 0; index < diagnostics.size(); ++index)
    {
        if (index != 0)
        {
            output << ',';
        }

        const trace2d::scene::SceneTextDiagnostic& diagnostic = diagnostics[index];
        output << "{\"path\":\"" << EscapeJson(diagnostic.path)
               << "\",\"message\":\"" << EscapeJson(diagnostic.message)
               << "\",\"line\":" << diagnostic.line
               << ",\"column\":" << diagnostic.column << '}';
    }
    output << ']';
}

void PrintInspectErrorJson(
    const std::string_view code,
    const std::string_view message,
    const std::vector<trace2d::scene::SceneTextDiagnostic>* diagnostics = nullptr)
{
    std::cerr << "{\"command\":\"inspect\",\"status\":\"error\",\"code\":\""
              << EscapeJson(code) << "\",\"message\":\"" << EscapeJson(message) << '"';

    if (diagnostics != nullptr)
    {
        std::cerr << ",\"diagnostics\":";
        WriteSceneDiagnosticsJson(std::cerr, *diagnostics);
    }

    std::cerr << "}\n";
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
        return ExitIoFailure;
    }
}

int RunInspect(const int argc, char* argv[])
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

            scenePath = argv[index + 1];
            ++index;
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
        std::string sceneText{};
        std::string ioError{};
        if (!ReadTextFile(scenePath, sceneText, ioError))
        {
            if (json)
            {
                PrintInspectErrorJson("io_error", ioError);
            }
            else
            {
                std::cerr << ioError << '\n';
            }
            return ExitIoFailure;
        }

        trace2d::scene::SceneLoadResult loadResult =
            trace2d::scene::LoadSceneToml(sceneText, scenePath);
        if (!loadResult.Succeeded())
        {
            constexpr std::string_view message = "Scene text failed validation.";
            if (json)
            {
                PrintInspectErrorJson(
                    "scene_load_failed",
                    message,
                    &loadResult.diagnostics);
            }
            else
            {
                std::cerr << message << '\n';
                for (const trace2d::scene::SceneTextDiagnostic& diagnostic : loadResult.diagnostics)
                {
                    std::cerr << "  " << diagnostic.path << ": " << diagnostic.message;
                    if (diagnostic.line != 0)
                    {
                        std::cerr << " (" << diagnostic.line << ':' << diagnostic.column << ')';
                    }
                    std::cerr << '\n';
                }
            }
            return ExitSceneLoadFailure;
        }

        trace2d::runtime::RuntimeConfig runtimeConfig{};
        runtimeConfig.seed = seed;
        trace2d::runtime::FixedStepRuntime runtime{runtimeConfig};
        runtime.Step(frameCount);

        trace2d::agent::AgentFacade facade{&runtime, &*loadResult.scene};
        const trace2d::agent::InspectionResult inspection = facade.Inspect();
        if (!inspection.Succeeded() || !inspection.snapshot.has_value())
        {
            const trace2d::agent::InspectionError error = inspection.error.value_or(
                trace2d::agent::InspectionError{
                    .code = trace2d::agent::InspectionErrorCode::SceneUnavailable,
                    .message = "Inspection failed without a structured result.",
                });

            if (json)
            {
                PrintInspectErrorJson(trace2d::agent::ToString(error.code), error.message);
            }
            else
            {
                std::cerr << "Inspection failed [" << trace2d::agent::ToString(error.code)
                          << "]: " << error.message << '\n';
            }
            return ExitInspectionFailure;
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
            PrintInspectErrorJson("inspection_failed", exception.what());
        }
        else
        {
            std::cerr << "Inspection failed: " << exception.what() << '\n';
        }
        return ExitInspectionFailure;
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
        return RunInspect(argc, argv);
    }

    std::cerr << "Unknown command: " << command << "\n\n";
    PrintHelp();
    return ExitUsage;
}
