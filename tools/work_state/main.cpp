#include <trace2d/agent/WorkSpec.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
constexpr int ExitSuccess = 0;
constexpr int ExitUsage = 2;
constexpr int ExitFailure = 3;

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

bool ReadTextFile(const std::filesystem::path& path, std::string& text)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        return false;
    }
    std::ostringstream buffer{};
    buffer << input.rdbuf();
    if (!input.good() && !input.eof())
    {
        return false;
    }
    text = buffer.str();
    return true;
}

void PrintUsage()
{
    std::cerr << "Usage: trace2d_work_state --spec PATH --capabilities PATH [--json]\n";
}

void PrintDiagnosticsJson(
    const std::string_view source,
    const std::vector<trace2d::agent::WorkSpecDiagnostic>& diagnostics)
{
    std::cerr << "{\"command\":\"work-state\",\"status\":\"error\",\"source\":\""
              << EscapeJson(source) << "\",\"diagnostics\":[";
    for (std::size_t index = 0; index < diagnostics.size(); ++index)
    {
        if (index != 0U) std::cerr << ',';
        const auto& diagnostic = diagnostics[index];
        std::cerr << "{\"path\":\"" << EscapeJson(diagnostic.path)
                  << "\",\"message\":\"" << EscapeJson(diagnostic.message)
                  << "\",\"line\":" << diagnostic.line
                  << ",\"column\":" << diagnostic.column << '}';
    }
    std::cerr << "]}\n";
}

void PrintStringArray(const std::vector<std::string>& values)
{
    std::cout << '[';
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index != 0U) std::cout << ',';
        std::cout << '"' << EscapeJson(values[index]) << '"';
    }
    std::cout << ']';
}

void PrintEvaluationJson(
    const trace2d::agent::WorkSpec& spec,
    const trace2d::agent::WorkEvaluation& evaluation)
{
    std::cout << "{\"command\":\"work-state\",\"format_version\":1"
              << ",\"work_id\":\"" << EscapeJson(spec.id) << '"'
              << ",\"work_state\":\"" << trace2d::agent::ToString(spec.state) << '"'
              << ",\"local_readiness\":\"" << trace2d::agent::ToString(evaluation.localReadiness) << '"'
              << ",\"requires_live_truth\":" << (evaluation.RequiresLiveTruth() ? "true" : "false")
              << ",\"capability_requirements\":[";

    for (std::size_t index = 0; index < evaluation.capabilityRequirements.size(); ++index)
    {
        if (index != 0U) std::cout << ',';
        const auto& capability = evaluation.capabilityRequirements[index];
        std::cout << "{\"deliverable\":\"" << EscapeJson(capability.deliverableId)
                  << "\",\"capability\":\"" << EscapeJson(capability.capabilityId)
                  << "\",\"minimum\":\"" << trace2d::agent::ToString(capability.minimum)
                  << "\",\"declared\":" << (capability.declared ? "true" : "false")
                  << ",\"available\":" << (capability.available ? "true" : "false")
                  << ",\"tested\":" << (capability.tested ? "true" : "false")
                  << ",\"production_supported\":" << (capability.productionSupported ? "true" : "false")
                  << ",\"deterministic_verification\":" << (capability.deterministicVerification ? "true" : "false")
                  << ",\"presentation_evidence\":" << (capability.presentationEvidence ? "true" : "false")
                  << ",\"hardware_evidence\":" << (capability.hardwareEvidence ? "true" : "false")
                  << ",\"human_judgment\":" << (capability.humanJudgment ? "true" : "false")
                  << ",\"eligible\":" << (capability.eligible ? "true" : "false")
                  << ",\"reason\":\"" << EscapeJson(capability.reason) << "\"}";
    }

    std::cout << "],\"outstanding_acceptance\":";
    PrintStringArray(evaluation.outstandingAcceptanceIds);
    std::cout << ",\"review_acceptance\":";
    PrintStringArray(evaluation.reviewAcceptanceIds);
    std::cout << ",\"external_truth\":[";
    for (std::size_t index = 0; index < evaluation.externalTruth.size(); ++index)
    {
        if (index != 0U) std::cout << ',';
        const auto& external = evaluation.externalTruth[index];
        std::cout << "{\"id\":\"" << EscapeJson(external.id)
                  << "\",\"kind\":\"" << trace2d::agent::ToString(external.kind)
                  << "\",\"description\":\"" << EscapeJson(external.description) << "\"}";
    }
    std::cout << "],\"status\":\"ok\"}\n";
}
} // namespace

int main(const int argc, char* argv[])
{
    std::filesystem::path specPath{};
    std::filesystem::path capabilityPath{};
    bool json = false;

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument == "--json")
        {
            json = true;
            continue;
        }
        if (argument == "--spec" || argument == "--capabilities")
        {
            if (index + 1 >= argc || std::string_view{argv[index + 1]}.empty())
            {
                PrintUsage();
                return ExitUsage;
            }
            std::filesystem::path& destination = argument == "--spec" ? specPath : capabilityPath;
            if (!destination.empty())
            {
                PrintUsage();
                return ExitUsage;
            }
            destination = argv[++index];
            continue;
        }
        PrintUsage();
        return ExitUsage;
    }

    if (specPath.empty() || capabilityPath.empty())
    {
        PrintUsage();
        return ExitUsage;
    }

    std::string specText{};
    std::string capabilityText{};
    if (!ReadTextFile(specPath, specText))
    {
        std::cerr << "Failed to read work spec: " << specPath.generic_string() << '\n';
        return ExitFailure;
    }
    if (!ReadTextFile(capabilityPath, capabilityText))
    {
        std::cerr << "Failed to read capability catalog: " << capabilityPath.generic_string() << '\n';
        return ExitFailure;
    }

    const auto specResult = trace2d::agent::ParseWorkSpecToml(specText, specPath.generic_string());
    if (!specResult.Succeeded())
    {
        if (json) PrintDiagnosticsJson(specPath.generic_string(), specResult.diagnostics);
        else
        {
            for (const auto& diagnostic : specResult.diagnostics)
            {
                std::cerr << diagnostic.path << ": " << diagnostic.message << '\n';
            }
        }
        return ExitFailure;
    }

    const auto capabilityResult = trace2d::agent::ParseCapabilityCatalogToml(
        capabilityText,
        capabilityPath.generic_string());
    if (!capabilityResult.Succeeded())
    {
        if (json) PrintDiagnosticsJson(capabilityPath.generic_string(), capabilityResult.diagnostics);
        else
        {
            for (const auto& diagnostic : capabilityResult.diagnostics)
            {
                std::cerr << diagnostic.path << ": " << diagnostic.message << '\n';
            }
        }
        return ExitFailure;
    }

    const auto evaluation = trace2d::agent::EvaluateWork(*specResult.spec, *capabilityResult.catalog);
    if (json)
    {
        PrintEvaluationJson(*specResult.spec, evaluation);
    }
    else
    {
        std::cout << "Trace2D work state\n"
                  << "  work: " << specResult.spec->id << '\n'
                  << "  authored state: " << trace2d::agent::ToString(specResult.spec->state) << '\n'
                  << "  local readiness: " << trace2d::agent::ToString(evaluation.localReadiness) << '\n'
                  << "  live truth required: " << (evaluation.RequiresLiveTruth() ? "yes" : "no") << '\n'
                  << "  outstanding acceptance: " << evaluation.outstandingAcceptanceIds.size() << '\n';
    }
    return ExitSuccess;
}
