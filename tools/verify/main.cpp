#include <trace2d/agent/WorkResult.hpp>
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
    std::cerr << "Usage: trace2d_verify --spec PATH --result PATH [--json]\n";
}

void PrintDiagnosticsJson(
    const std::string_view source,
    const std::vector<trace2d::agent::WorkSpecDiagnostic>& diagnostics)
{
    std::cerr << "{\"command\":\"verify\",\"status\":\"error\",\"source\":\""
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

std::size_t CountHistoricalFailures(const trace2d::agent::WorkResult& result)
{
    std::size_t count = 0U;
    for (const auto& revision : result.revisions)
    {
        for (const auto& verification : revision.verification)
        {
            if (verification.outcome == trace2d::agent::VerificationOutcome::Failed)
            {
                ++count;
            }
        }
    }
    return count;
}

void PrintEvaluationJson(
    const trace2d::agent::WorkSpec& spec,
    const trace2d::agent::WorkResult& result,
    const trace2d::agent::WorkResultEvaluation& evaluation)
{
    std::cout << "{\"command\":\"verify\",\"format_version\":1"
              << ",\"work_id\":\"" << EscapeJson(spec.id) << '"'
              << ",\"state\":\"" << trace2d::agent::ToString(evaluation.state) << '"'
              << ",\"current_revision\":\"" << EscapeJson(evaluation.currentRevisionId) << '"'
              << ",\"revision_count\":" << result.revisions.size()
              << ",\"historical_failure_count\":" << CountHistoricalFailures(result)
              << ",\"requires_live_truth\":" << (evaluation.RequiresLiveTruth() ? "true" : "false")
              << ",\"outstanding_acceptance\":";
    PrintStringArray(evaluation.outstandingAcceptanceIds);
    std::cout << ",\"review_acceptance\":";
    PrintStringArray(evaluation.reviewAcceptanceIds);
    std::cout << ",\"failures\":[";
    for (std::size_t index = 0; index < evaluation.failures.size(); ++index)
    {
        if (index != 0U) std::cout << ',';
        const auto& failure = evaluation.failures[index];
        std::cout << "{\"code\":\"" << EscapeJson(failure.code)
                  << "\",\"target\":\"" << EscapeJson(failure.target)
                  << "\",\"message\":\"" << EscapeJson(failure.message)
                  << "\",\"reproduction\":\"" << EscapeJson(failure.reproduction)
                  << "\",\"evidence\":";
        PrintStringArray(failure.evidence);
        std::cout << '}';
    }
    std::cout << "],\"revisions\":[";
    for (std::size_t index = 0; index < result.revisions.size(); ++index)
    {
        if (index != 0U) std::cout << ',';
        const auto& revision = result.revisions[index];
        std::cout << "{\"id\":\"" << EscapeJson(revision.id)
                  << "\",\"parent\":\"" << EscapeJson(revision.parentRevisionId)
                  << "\",\"changed_paths\":";
        PrintStringArray(revision.changedPaths);
        std::cout << ",\"limitations\":";
        PrintStringArray(revision.limitations);
        std::cout << ",\"verification_count\":" << revision.verification.size()
                  << ",\"artifact_count\":" << revision.artifacts.size()
                  << ",\"feedback_count\":" << revision.feedback.size() << '}';
    }
    std::cout << "],\"status\":\"ok\"}\n";
}
} // namespace

int main(const int argc, char* argv[])
{
    std::filesystem::path specPath{};
    std::filesystem::path resultPath{};
    bool json = false;

    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument == "--json")
        {
            json = true;
            continue;
        }
        if (argument == "--spec" || argument == "--result")
        {
            if (index + 1 >= argc || std::string_view{argv[index + 1]}.empty())
            {
                PrintUsage();
                return ExitUsage;
            }
            std::filesystem::path& destination = argument == "--spec" ? specPath : resultPath;
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

    if (specPath.empty() || resultPath.empty())
    {
        PrintUsage();
        return ExitUsage;
    }

    std::string specText{};
    std::string resultText{};
    if (!ReadTextFile(specPath, specText))
    {
        std::cerr << "Failed to read work spec: " << specPath.generic_string() << '\n';
        return ExitFailure;
    }
    if (!ReadTextFile(resultPath, resultText))
    {
        std::cerr << "Failed to read work result: " << resultPath.generic_string() << '\n';
        return ExitFailure;
    }

    const auto spec = trace2d::agent::ParseWorkSpecToml(specText, specPath.generic_string());
    if (!spec.Succeeded())
    {
        if (json) PrintDiagnosticsJson(specPath.generic_string(), spec.diagnostics);
        else
        {
            for (const auto& diagnostic : spec.diagnostics)
            {
                std::cerr << diagnostic.path << ": " << diagnostic.message << '\n';
            }
        }
        return ExitFailure;
    }

    const auto result = trace2d::agent::ParseWorkResultToml(resultText, resultPath.generic_string());
    if (!result.Succeeded())
    {
        if (json) PrintDiagnosticsJson(resultPath.generic_string(), result.diagnostics);
        else
        {
            for (const auto& diagnostic : result.diagnostics)
            {
                std::cerr << diagnostic.path << ": " << diagnostic.message << '\n';
            }
        }
        return ExitFailure;
    }

    if (result.result->workId != spec.spec->id)
    {
        std::cerr << "WorkResult work_id does not match WorkSpec id.\n";
        return ExitFailure;
    }

    const auto evaluation = trace2d::agent::EvaluateWorkResult(*spec.spec, *result.result);
    if (json)
    {
        PrintEvaluationJson(*spec.spec, *result.result, evaluation);
    }
    else
    {
        std::cout << "Trace2D verification result\n"
                  << "  work: " << spec.spec->id << '\n'
                  << "  state: " << trace2d::agent::ToString(evaluation.state) << '\n'
                  << "  current revision: " << evaluation.currentRevisionId << '\n'
                  << "  revisions: " << result.result->revisions.size() << '\n'
                  << "  failures: " << evaluation.failures.size() << '\n'
                  << "  review items: " << evaluation.reviewAcceptanceIds.size() << '\n';
    }
    return ExitSuccess;
}
