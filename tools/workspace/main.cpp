#include <trace2d/agent/Workspace.hpp>

#include <algorithm>
#include <cctype>
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

struct Options final
{
    std::filesystem::path specPath{};
    std::filesystem::path resultPath{};
    std::filesystem::path htmlPath{};
    std::filesystem::path actionOutPath{};
    bool json{false};
    std::string feedback{};
    std::string feedbackAcceptance{};
    std::string target{};
    std::string approveAcceptance{};
    std::string message{};
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

std::string EscapeHtml(const std::string_view value)
{
    std::string escaped{};
    escaped.reserve(value.size());
    for (const char character : value)
    {
        switch (character)
        {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '"': escaped += "&quot;"; break;
        case '\'': escaped += "&#39;"; break;
        default: escaped += character; break;
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

bool WriteTextFile(const std::filesystem::path& path, const std::string_view text)
{
    const std::filesystem::path parent = path.parent_path();
    std::error_code error{};
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, error);
        if (error)
        {
            return false;
        }
    }

    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
    {
        return false;
    }
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    return output.good();
}

void PrintUsage()
{
    std::cerr
        << "Usage:\n"
        << "  trace2d_workspace --spec PATH --result PATH [--json] [--html PATH]\n"
        << "  trace2d_workspace --spec PATH --result PATH --feedback TEXT [--acceptance ID] [--target ID] [--action-out PATH]\n"
        << "  trace2d_workspace --spec PATH --result PATH --approve ACCEPTANCE [--message TEXT] [--action-out PATH]\n";
}

bool ReadValueArgument(
    const int argc,
    char* argv[],
    int& index,
    std::string_view argument,
    std::string& destination)
{
    if (index + 1 >= argc || std::string_view{argv[index + 1]}.empty() || !destination.empty())
    {
        std::cerr << "Invalid or duplicate value for " << argument << ".\n";
        return false;
    }
    destination = argv[++index];
    return true;
}

bool ParseOptions(const int argc, char* argv[], Options& options)
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument{argv[index]};
        if (argument == "--json")
        {
            if (options.json) return false;
            options.json = true;
            continue;
        }
        if (argument == "--spec" || argument == "--result" || argument == "--html" || argument == "--action-out")
        {
            if (index + 1 >= argc || std::string_view{argv[index + 1]}.empty()) return false;
            std::filesystem::path* destination = nullptr;
            if (argument == "--spec") destination = &options.specPath;
            else if (argument == "--result") destination = &options.resultPath;
            else if (argument == "--html") destination = &options.htmlPath;
            else destination = &options.actionOutPath;
            if (!destination->empty()) return false;
            *destination = argv[++index];
            continue;
        }
        if (argument == "--feedback")
        {
            if (!ReadValueArgument(argc, argv, index, argument, options.feedback)) return false;
            continue;
        }
        if (argument == "--acceptance")
        {
            if (!ReadValueArgument(argc, argv, index, argument, options.feedbackAcceptance)) return false;
            continue;
        }
        if (argument == "--target")
        {
            if (!ReadValueArgument(argc, argv, index, argument, options.target)) return false;
            continue;
        }
        if (argument == "--approve")
        {
            if (!ReadValueArgument(argc, argv, index, argument, options.approveAcceptance)) return false;
            continue;
        }
        if (argument == "--message")
        {
            if (!ReadValueArgument(argc, argv, index, argument, options.message)) return false;
            continue;
        }
        return false;
    }

    if (options.specPath.empty() || options.resultPath.empty())
    {
        return false;
    }

    const bool feedbackMode = !options.feedback.empty();
    const bool approveMode = !options.approveAcceptance.empty();
    if (feedbackMode && approveMode)
    {
        return false;
    }
    if (!feedbackMode && !approveMode && (!options.actionOutPath.empty() || !options.feedbackAcceptance.empty() ||
        !options.target.empty() || !options.message.empty()))
    {
        return false;
    }
    if (approveMode && (!options.feedbackAcceptance.empty() || !options.target.empty()))
    {
        return false;
    }
    if ((feedbackMode || approveMode) && (options.json || !options.htmlPath.empty()))
    {
        return false;
    }
    return true;
}

void PrintStringArray(std::ostream& output, const std::vector<std::string>& values)
{
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index != 0U) output << ',';
        output << '"' << EscapeJson(values[index]) << '"';
    }
    output << ']';
}

void PrintArtifactJson(std::ostream& output, const trace2d::agent::WorkArtifact& artifact)
{
    output << "{\"id\":\"" << EscapeJson(artifact.id)
           << "\",\"kind\":\"" << EscapeJson(artifact.kind)
           << "\",\"path\":\"" << EscapeJson(artifact.path)
           << "\",\"description\":\"" << EscapeJson(artifact.description) << "\"}";
}

void PrintSnapshotJson(const trace2d::agent::WorkspaceSnapshot& snapshot)
{
    std::cout << "{\"command\":\"workspace\",\"format_version\":1"
              << ",\"work_id\":\"" << EscapeJson(snapshot.workId) << '"'
              << ",\"intent\":\"" << EscapeJson(snapshot.intent) << '"'
              << ",\"state\":\"" << trace2d::agent::ToString(snapshot.resultState) << '"'
              << ",\"current_revision\":\"" << EscapeJson(snapshot.currentRevisionId) << '"'
              << ",\"deliverables\":[";
    for (std::size_t index = 0; index < snapshot.deliverables.size(); ++index)
    {
        if (index != 0U) std::cout << ',';
        const auto& deliverable = snapshot.deliverables[index];
        std::cout << "{\"id\":\"" << EscapeJson(deliverable.id)
                  << "\",\"description\":\"" << EscapeJson(deliverable.description)
                  << "\",\"state\":\"" << trace2d::agent::ToString(deliverable.state)
                  << "\",\"acceptance\":";
        PrintStringArray(std::cout, deliverable.acceptanceIds);
        std::cout << '}';
    }
    std::cout << "],\"acceptance\":[";
    for (std::size_t index = 0; index < snapshot.acceptance.size(); ++index)
    {
        if (index != 0U) std::cout << ',';
        const auto& item = snapshot.acceptance[index];
        std::cout << "{\"id\":\"" << EscapeJson(item.id)
                  << "\",\"deliverable\":\"" << EscapeJson(item.deliverableId)
                  << "\",\"description\":\"" << EscapeJson(item.description)
                  << "\",\"verification\":\"" << trace2d::agent::ToString(item.verification)
                  << "\",\"outcome\":\"" << trace2d::agent::ToString(item.outcome)
                  << "\",\"summary\":\"" << EscapeJson(item.summary)
                  << "\",\"evidence\":";
        PrintStringArray(std::cout, item.evidence);
        if (item.failure.has_value())
        {
            std::cout << ",\"failure\":{\"code\":\"" << EscapeJson(item.failure->code)
                      << "\",\"target\":\"" << EscapeJson(item.failure->target)
                      << "\",\"message\":\"" << EscapeJson(item.failure->message)
                      << "\",\"reproduction\":\"" << EscapeJson(item.failure->reproduction) << "\"}";
        }
        std::cout << '}';
    }
    std::cout << "],\"review_queue\":[";
    for (std::size_t index = 0; index < snapshot.reviewQueue.size(); ++index)
    {
        if (index != 0U) std::cout << ',';
        const auto& item = snapshot.reviewQueue[index];
        std::cout << "{\"acceptance\":\"" << EscapeJson(item.acceptanceId)
                  << "\",\"deliverable\":\"" << EscapeJson(item.deliverableId)
                  << "\",\"description\":\"" << EscapeJson(item.description)
                  << "\",\"verification\":\"" << trace2d::agent::ToString(item.verification)
                  << "\",\"outcome\":\"" << trace2d::agent::ToString(item.outcome)
                  << "\",\"target\":\"" << EscapeJson(item.target)
                  << "\",\"summary\":\"" << EscapeJson(item.summary)
                  << "\",\"evidence\":";
        PrintStringArray(std::cout, item.evidence);
        std::cout << '}';
    }
    std::cout << "],\"current_changes\":";
    PrintStringArray(std::cout, snapshot.currentChangedPaths);
    std::cout << ",\"current_artifacts\":[";
    for (std::size_t index = 0; index < snapshot.currentArtifacts.size(); ++index)
    {
        if (index != 0U) std::cout << ',';
        PrintArtifactJson(std::cout, snapshot.currentArtifacts[index]);
    }
    std::cout << "],\"current_limitations\":";
    PrintStringArray(std::cout, snapshot.currentLimitations);
    std::cout << ",\"external_truth\":[";
    for (std::size_t index = 0; index < snapshot.externalTruth.size(); ++index)
    {
        if (index != 0U) std::cout << ',';
        const auto& truth = snapshot.externalTruth[index];
        std::cout << "{\"id\":\"" << EscapeJson(truth.id)
                  << "\",\"kind\":\"" << trace2d::agent::ToString(truth.kind)
                  << "\",\"description\":\"" << EscapeJson(truth.description) << "\"}";
    }
    std::cout << "],\"revisions\":[";
    for (std::size_t index = 0; index < snapshot.revisions.size(); ++index)
    {
        if (index != 0U) std::cout << ',';
        const auto& revision = snapshot.revisions[index];
        std::cout << "{\"id\":\"" << EscapeJson(revision.id)
                  << "\",\"parent\":\"" << EscapeJson(revision.parentRevisionId)
                  << "\",\"failed_verification_count\":" << revision.failedVerificationCount
                  << ",\"changed_paths\":";
        PrintStringArray(std::cout, revision.changedPaths);
        std::cout << ",\"limitations\":";
        PrintStringArray(std::cout, revision.limitations);
        std::cout << ",\"feedback\":[";
        for (std::size_t feedbackIndex = 0; feedbackIndex < revision.feedback.size(); ++feedbackIndex)
        {
            if (feedbackIndex != 0U) std::cout << ',';
            const auto& feedback = revision.feedback[feedbackIndex];
            std::cout << "{\"id\":\"" << EscapeJson(feedback.id)
                      << "\",\"target\":\"" << EscapeJson(feedback.target)
                      << "\",\"message\":\"" << EscapeJson(feedback.message) << "\"}";
        }
        std::cout << "]}";
    }
    std::cout << "],\"status\":\"ok\"}\n";
}

std::string LowerExtension(const std::string_view path)
{
    std::string extension = std::filesystem::path{path}.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char value)
    {
        return static_cast<char>(std::tolower(value));
    });
    return extension;
}

void WriteArtifactHtml(std::ostream& output, const trace2d::agent::WorkArtifact& artifact)
{
    const std::string path = EscapeHtml(artifact.path);
    const std::string extension = LowerExtension(artifact.path);
    output << "<article class=artifact><strong>" << EscapeHtml(artifact.id) << "</strong> <span class=muted>"
           << EscapeHtml(artifact.kind) << "</span><p>" << EscapeHtml(artifact.description) << "</p>";
    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".webp" || extension == ".gif")
    {
        output << "<img loading=lazy src=\"" << path << "\" alt=\"" << EscapeHtml(artifact.description) << "\">";
    }
    else if (extension == ".mp4" || extension == ".webm")
    {
        output << "<video controls preload=metadata src=\"" << path << "\"></video>";
    }
    else if (extension == ".wav" || extension == ".mp3" || extension == ".ogg" || extension == ".flac")
    {
        output << "<audio controls preload=metadata src=\"" << path << "\"></audio>";
    }
    else
    {
        output << "<a href=\"" << path << "\">Open artifact</a>";
    }
    output << "</article>";
}

std::string BuildWorkspaceHtml(
    const trace2d::agent::WorkspaceSnapshot& snapshot,
    const Options& options)
{
    std::ostringstream output{};
    output << "<!doctype html><html lang=en><head><meta charset=utf-8><meta name=viewport content=\"width=device-width,initial-scale=1\">"
           << "<title>Trace2D Workspace - " << EscapeHtml(snapshot.workId) << "</title>"
           << "<style>body{font-family:system-ui,sans-serif;margin:0;background:#101215;color:#eef1f4}main{max-width:1100px;margin:auto;padding:32px}"
           << "section{background:#191d22;border:1px solid #2c333b;border-radius:12px;padding:20px;margin:16px 0}h1,h2{margin-top:0}.muted{color:#9ba7b4}"
           << "table{width:100%;border-collapse:collapse}th,td{text-align:left;padding:10px;border-bottom:1px solid #303740;vertical-align:top}code{white-space:pre-wrap}"
           << ".pill{display:inline-block;border:1px solid #4a5664;border-radius:999px;padding:3px 9px}.artifact img,.artifact video{max-width:100%;max-height:420px;display:block;margin-top:10px}"
           << ".artifact audio{width:100%}.artifact{padding:12px 0;border-bottom:1px solid #303740}a{color:#8bc5ff}</style></head><body><main>"
           << "<h1>Trace2D Workspace</h1><p class=muted>Derived review surface. Engine/project truth remains authoritative.</p>"
           << "<section><h2>Intent</h2><p>" << EscapeHtml(snapshot.intent) << "</p><p><span class=pill>"
           << trace2d::agent::ToString(snapshot.resultState) << "</span> Current revision: <code>" << EscapeHtml(snapshot.currentRevisionId) << "</code></p></section>";

    output << "<section><h2>Progress</h2><table><thead><tr><th>Deliverable</th><th>State</th><th>Description</th></tr></thead><tbody>";
    for (const auto& deliverable : snapshot.deliverables)
    {
        output << "<tr><td><code>" << EscapeHtml(deliverable.id) << "</code></td><td>"
               << trace2d::agent::ToString(deliverable.state) << "</td><td>" << EscapeHtml(deliverable.description) << "</td></tr>";
    }
    output << "</tbody></table></section>";

    output << "<section><h2>Review queue</h2>";
    if (snapshot.reviewQueue.empty())
    {
        output << "<p class=muted>No subjective/human review item is currently eligible.</p>";
    }
    for (const auto& item : snapshot.reviewQueue)
    {
        output << "<article><h3>" << EscapeHtml(item.description) << "</h3><p><code>" << EscapeHtml(item.acceptanceId)
               << "</code> · " << trace2d::agent::ToString(item.verification) << " · " << trace2d::agent::ToString(item.outcome)
               << "</p><p>" << EscapeHtml(item.summary) << "</p><p class=muted>Stable target: <code>" << EscapeHtml(item.target) << "</code></p>";
        output << "<p>Approve packet: <code>trace2d_workspace --spec &quot;" << EscapeHtml(options.specPath.generic_string())
               << "&quot; --result &quot;" << EscapeHtml(options.resultPath.generic_string()) << "&quot; --approve &quot;"
               << EscapeHtml(item.acceptanceId) << "&quot; --action-out workspace-action.toml</code></p></article>";
    }
    output << "</section>";

    output << "<section><h2>Recent changes</h2><ul>";
    for (const auto& path : snapshot.currentChangedPaths) output << "<li><code>" << EscapeHtml(path) << "</code></li>";
    output << "</ul>";
    if (!snapshot.currentLimitations.empty())
    {
        output << "<h3>Known limitations</h3><ul>";
        for (const auto& value : snapshot.currentLimitations) output << "<li>" << EscapeHtml(value) << "</li>";
        output << "</ul>";
    }
    output << "</section>";

    output << "<section><h2>Artifacts</h2>";
    if (snapshot.currentArtifacts.empty()) output << "<p class=muted>No current-revision artifacts.</p>";
    for (const auto& artifact : snapshot.currentArtifacts) WriteArtifactHtml(output, artifact);
    output << "</section>";

    output << "<section><h2>Verification</h2><table><thead><tr><th>Acceptance</th><th>Class</th><th>Outcome</th><th>Summary</th></tr></thead><tbody>";
    for (const auto& item : snapshot.acceptance)
    {
        output << "<tr><td><code>" << EscapeHtml(item.id) << "</code></td><td>" << trace2d::agent::ToString(item.verification)
               << "</td><td>" << trace2d::agent::ToString(item.outcome) << "</td><td>" << EscapeHtml(item.summary);
        if (item.failure.has_value())
        {
            output << "<br><strong>" << EscapeHtml(item.failure->code) << ":</strong> " << EscapeHtml(item.failure->message)
                   << "<br><code>" << EscapeHtml(item.failure->reproduction) << "</code>";
        }
        output << "</td></tr>";
    }
    output << "</tbody></table></section>";

    output << "<section><h2>Revision history</h2>";
    for (const auto& revision : snapshot.revisions)
    {
        output << "<article><h3><code>" << EscapeHtml(revision.id) << "</code></h3><p class=muted>Parent: "
               << EscapeHtml(revision.parentRevisionId.empty() ? "none" : revision.parentRevisionId)
               << " · Failed checks: " << revision.failedVerificationCount << "</p>";
        for (const auto& feedback : revision.feedback)
        {
            output << "<p><strong>Feedback:</strong> " << EscapeHtml(feedback.message);
            if (!feedback.target.empty()) output << " <span class=muted>(" << EscapeHtml(feedback.target) << ")</span>";
            output << "</p>";
        }
        output << "</article>";
    }
    output << "</section>";

    output << "<section><h2>Ask AI to modify</h2><p>Feedback is emitted as a versioned action packet; it does not mutate engine state.</p><code>trace2d_workspace --spec &quot;"
           << EscapeHtml(options.specPath.generic_string()) << "&quot; --result &quot;" << EscapeHtml(options.resultPath.generic_string())
           << "&quot; --feedback &quot;Describe the requested revision&quot; --target &quot;stable/semantic/target&quot; --action-out workspace-action.toml</code></section>";

    if (!snapshot.externalTruth.empty())
    {
        output << "<section><h2>Live external truth still required</h2><ul>";
        for (const auto& truth : snapshot.externalTruth)
        {
            output << "<li><code>" << EscapeHtml(truth.id) << "</code> · " << trace2d::agent::ToString(truth.kind)
                   << " — " << EscapeHtml(truth.description) << "</li>";
        }
        output << "</ul></section>";
    }
    output << "</main></body></html>";
    return output.str();
}

void PrintSnapshotText(const trace2d::agent::WorkspaceSnapshot& snapshot)
{
    std::cout << "Trace2D Workspace\n"
              << "  work: " << snapshot.workId << '\n'
              << "  state: " << trace2d::agent::ToString(snapshot.resultState) << '\n'
              << "  revision: " << snapshot.currentRevisionId << '\n'
              << "  review items: " << snapshot.reviewQueue.size() << '\n'
              << "  current artifacts: " << snapshot.currentArtifacts.size() << '\n'
              << "  live truth requirements: " << snapshot.externalTruth.size() << "\n\nProgress\n";
    for (const auto& deliverable : snapshot.deliverables)
    {
        std::cout << "  [" << trace2d::agent::ToString(deliverable.state) << "] " << deliverable.id
                  << " - " << deliverable.description << '\n';
    }
    if (!snapshot.reviewQueue.empty())
    {
        std::cout << "\nReview queue\n";
        for (const auto& item : snapshot.reviewQueue)
        {
            std::cout << "  " << item.acceptanceId << " - " << item.description << "\n"
                      << "    target: " << item.target << '\n';
        }
    }
}
} // namespace

int main(const int argc, char* argv[])
{
    Options options{};
    if (!ParseOptions(argc, argv, options))
    {
        PrintUsage();
        return ExitUsage;
    }

    std::string specText{};
    std::string resultText{};
    if (!ReadTextFile(options.specPath, specText))
    {
        std::cerr << "Failed to read work spec: " << options.specPath.generic_string() << '\n';
        return ExitFailure;
    }
    if (!ReadTextFile(options.resultPath, resultText))
    {
        std::cerr << "Failed to read work result: " << options.resultPath.generic_string() << '\n';
        return ExitFailure;
    }

    const auto spec = trace2d::agent::ParseWorkSpecToml(specText, options.specPath.generic_string());
    const auto result = trace2d::agent::ParseWorkResultToml(resultText, options.resultPath.generic_string());
    if (!spec.Succeeded() || !result.Succeeded())
    {
        const auto& diagnostics = !spec.Succeeded() ? spec.diagnostics : result.diagnostics;
        for (const auto& diagnostic : diagnostics)
        {
            std::cerr << diagnostic.path << ": " << diagnostic.message << '\n';
        }
        return ExitFailure;
    }
    if (result.result->workId != spec.spec->id)
    {
        std::cerr << "WorkResult work_id does not match WorkSpec id.\n";
        return ExitFailure;
    }

    const trace2d::agent::WorkspaceSnapshot snapshot =
        trace2d::agent::BuildWorkspaceSnapshot(*spec.spec, *result.result);

    if (!options.feedback.empty() || !options.approveAcceptance.empty())
    {
        trace2d::agent::WorkspaceAction action{};
        action.workId = snapshot.workId;
        action.revisionId = snapshot.currentRevisionId;
        if (!options.feedback.empty())
        {
            action.kind = trace2d::agent::WorkspaceActionKind::Feedback;
            action.acceptanceId = options.feedbackAcceptance;
            action.target = options.target;
            action.message = options.feedback;
        }
        else
        {
            action.kind = trace2d::agent::WorkspaceActionKind::Approve;
            action.acceptanceId = options.approveAcceptance;
            action.target = "acceptance/" + options.approveAcceptance;
            action.message = options.message;
        }

        const std::vector<std::string> errors = trace2d::agent::ValidateWorkspaceAction(snapshot, action);
        if (!errors.empty())
        {
            for (const auto& error : errors) std::cerr << error << '\n';
            return ExitFailure;
        }

        const std::string packet = trace2d::agent::SerializeWorkspaceActionToml(action);
        if (options.actionOutPath.empty())
        {
            std::cout << packet;
        }
        else if (!WriteTextFile(options.actionOutPath, packet))
        {
            std::cerr << "Failed to write Workspace action: " << options.actionOutPath.generic_string() << '\n';
            return ExitFailure;
        }
        else
        {
            std::cout << "Wrote Workspace action: " << options.actionOutPath.generic_string() << '\n';
        }
        return ExitSuccess;
    }

    if (!options.htmlPath.empty())
    {
        const std::string html = BuildWorkspaceHtml(snapshot, options);
        if (!WriteTextFile(options.htmlPath, html))
        {
            std::cerr << "Failed to write Workspace HTML: " << options.htmlPath.generic_string() << '\n';
            return ExitFailure;
        }
    }

    if (options.json) PrintSnapshotJson(snapshot);
    else PrintSnapshotText(snapshot);
    if (!options.htmlPath.empty())
    {
        std::cerr << "Workspace HTML: " << options.htmlPath.generic_string() << '\n';
    }
    return ExitSuccess;
}
