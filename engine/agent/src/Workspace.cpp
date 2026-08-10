#include <trace2d/agent/Workspace.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace trace2d::agent
{
namespace
{
constexpr std::int64_t WorkspaceActionFormatVersion = 1;

void AddDiagnostic(
    std::vector<WorkSpecDiagnostic>& diagnostics,
    std::string path,
    std::string message,
    const toml::node* node = nullptr)
{
    WorkSpecDiagnostic diagnostic{};
    diagnostic.path = std::move(path);
    diagnostic.message = std::move(message);
    if (node != nullptr)
    {
        const toml::source_position position = node->source().begin;
        diagnostic.line = static_cast<std::size_t>(position.line);
        diagnostic.column = static_cast<std::size_t>(position.column);
    }
    diagnostics.push_back(std::move(diagnostic));
}

bool IsKnownKey(
    const std::string_view key,
    const std::initializer_list<std::string_view> knownKeys)
{
    return std::find(knownKeys.begin(), knownKeys.end(), key) != knownKeys.end();
}

void ValidateKnownKeys(
    const toml::table& table,
    const std::string_view path,
    const std::initializer_list<std::string_view> knownKeys,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    for (const auto& [key, value] : table)
    {
        const std::string_view keyView = key.str();
        if (IsKnownKey(keyView, knownKeys))
        {
            continue;
        }

        std::string fieldPath{path};
        if (!fieldPath.empty())
        {
            fieldPath.push_back('.');
        }
        fieldPath.append(keyView);
        AddDiagnostic(diagnostics, std::move(fieldPath), "Unknown field.", &value);
    }
}

std::optional<std::string> ReadRequiredString(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    const std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value() || value->empty())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a non-empty string.", node);
        return std::nullopt;
    }
    return value;
}

std::optional<std::string> ReadOptionalString(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        return std::nullopt;
    }

    const std::optional<std::string> value = node->value<std::string>();
    if (!value.has_value() || value->empty())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a non-empty string.", node);
        return std::nullopt;
    }
    return value;
}

WorkspaceProgressState ProgressFromAuthoredState(const WorkItemState state) noexcept
{
    switch (state)
    {
    case WorkItemState::Requested:
    case WorkItemState::Planned:
        return WorkspaceProgressState::Planned;
    case WorkItemState::Implemented:
        return WorkspaceProgressState::Working;
    case WorkItemState::Verified:
        return WorkspaceProgressState::Verified;
    case WorkItemState::ReviewNeeded:
        return WorkspaceProgressState::ReviewNeeded;
    case WorkItemState::Approved:
        return WorkspaceProgressState::Approved;
    case WorkItemState::Failed:
        return WorkspaceProgressState::Failed;
    }
    return WorkspaceProgressState::Planned;
}

const VerificationRecord* FindVerification(
    const WorkRevision* currentRevision,
    const std::string_view acceptanceId) noexcept
{
    if (currentRevision == nullptr)
    {
        return nullptr;
    }

    const auto found = std::find_if(
        currentRevision->verification.begin(),
        currentRevision->verification.end(),
        [acceptanceId](const VerificationRecord& record)
        {
            return record.acceptanceId == acceptanceId;
        });
    return found == currentRevision->verification.end() ? nullptr : &*found;
}

bool IsSubjective(const VerificationClass verification) noexcept
{
    return verification == VerificationClass::Multimodal || verification == VerificationClass::Human;
}

bool IsAcceptanceComplete(
    const VerificationClass verification,
    const VerificationOutcome outcome) noexcept
{
    if (IsSubjective(verification))
    {
        return outcome == VerificationOutcome::Approved;
    }
    return outcome == VerificationOutcome::Passed || outcome == VerificationOutcome::Approved;
}

bool IsAcceptanceReviewable(
    const VerificationClass verification,
    const VerificationOutcome outcome) noexcept
{
    return IsSubjective(verification) &&
        (outcome == VerificationOutcome::Passed || outcome == VerificationOutcome::ReviewNeeded);
}

WorkspaceProgressState DeriveDeliverableState(
    const WorkDeliverable& deliverable,
    const WorkSpec& spec,
    const WorkRevision* currentRevision)
{
    bool hasAcceptance = false;
    bool hasSubjective = false;
    bool hasIncomplete = false;
    bool hasReview = false;

    for (const auto& criterion : spec.acceptance)
    {
        if (criterion.deliverableId != deliverable.id)
        {
            continue;
        }
        hasAcceptance = true;
        hasSubjective = hasSubjective || IsSubjective(criterion.verification);

        const VerificationRecord* record = FindVerification(currentRevision, criterion.id);
        const VerificationOutcome outcome =
            record == nullptr ? VerificationOutcome::NotRun : record->outcome;
        if (outcome == VerificationOutcome::Failed)
        {
            return WorkspaceProgressState::Failed;
        }
        if (IsAcceptanceReviewable(criterion.verification, outcome))
        {
            hasReview = true;
            continue;
        }
        if (!IsAcceptanceComplete(criterion.verification, outcome))
        {
            hasIncomplete = true;
        }
    }

    if (!hasAcceptance)
    {
        return ProgressFromAuthoredState(deliverable.state);
    }
    if (hasIncomplete)
    {
        const WorkspaceProgressState authored = ProgressFromAuthoredState(deliverable.state);
        return authored == WorkspaceProgressState::Planned ? authored : WorkspaceProgressState::Working;
    }
    if (hasReview)
    {
        return WorkspaceProgressState::ReviewNeeded;
    }
    return hasSubjective ? WorkspaceProgressState::Approved : WorkspaceProgressState::Verified;
}

std::string EscapeTomlString(const std::string_view value)
{
    std::string escaped{};
    escaped.reserve(value.size());
    for (const char character : value)
    {
        switch (character)
        {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped += character; break;
        }
    }
    return escaped;
}
} // namespace

std::string_view ToString(const WorkspaceProgressState state) noexcept
{
    switch (state)
    {
    case WorkspaceProgressState::Planned: return "planned";
    case WorkspaceProgressState::Working: return "working";
    case WorkspaceProgressState::Verified: return "verified";
    case WorkspaceProgressState::ReviewNeeded: return "review_needed";
    case WorkspaceProgressState::Failed: return "failed";
    case WorkspaceProgressState::Approved: return "approved";
    }
    return "planned";
}

WorkspaceSnapshot BuildWorkspaceSnapshot(const WorkSpec& spec, const WorkResult& result)
{
    WorkspaceSnapshot snapshot{};
    snapshot.workId = spec.id;
    snapshot.intent = spec.intent;

    const WorkResultEvaluation evaluation = EvaluateWorkResult(spec, result);
    snapshot.resultState = evaluation.state;
    snapshot.currentRevisionId = evaluation.currentRevisionId;
    snapshot.externalTruth = evaluation.externalTruth;

    const WorkRevision* currentRevision = result.revisions.empty() ? nullptr : &result.revisions.back();
    if (currentRevision != nullptr)
    {
        snapshot.currentChangedPaths = currentRevision->changedPaths;
        snapshot.currentArtifacts = currentRevision->artifacts;
        snapshot.currentLimitations = currentRevision->limitations;
    }

    snapshot.acceptance.reserve(spec.acceptance.size());
    for (const auto& criterion : spec.acceptance)
    {
        WorkspaceAcceptanceView view{};
        view.id = criterion.id;
        view.deliverableId = criterion.deliverableId;
        view.description = criterion.description;
        view.verification = criterion.verification;

        const VerificationRecord* record = FindVerification(currentRevision, criterion.id);
        if (record != nullptr)
        {
            view.outcome = record->outcome;
            view.summary = record->summary;
            view.evidence = record->evidence;
            view.failure = record->failure;
        }
        snapshot.acceptance.push_back(std::move(view));
    }

    snapshot.deliverables.reserve(spec.deliverables.size());
    for (const auto& deliverable : spec.deliverables)
    {
        WorkspaceDeliverableView view{};
        view.id = deliverable.id;
        view.description = deliverable.description;
        view.state = DeriveDeliverableState(deliverable, spec, currentRevision);
        for (const auto& criterion : spec.acceptance)
        {
            if (criterion.deliverableId == deliverable.id)
            {
                view.acceptanceIds.push_back(criterion.id);
            }
        }
        snapshot.deliverables.push_back(std::move(view));
    }

    if (evaluation.state == WorkResultState::ReviewNeeded)
    {
        snapshot.reviewQueue.reserve(evaluation.reviewAcceptanceIds.size());
        for (const auto& acceptanceId : evaluation.reviewAcceptanceIds)
        {
            const auto criterion = std::find_if(
                spec.acceptance.begin(),
                spec.acceptance.end(),
                [&acceptanceId](const AcceptanceCriterion& value)
                {
                    return value.id == acceptanceId;
                });
            if (criterion == spec.acceptance.end())
            {
                continue;
            }

            WorkspaceReviewItem item{};
            item.acceptanceId = criterion->id;
            item.deliverableId = criterion->deliverableId;
            item.description = criterion->description;
            item.verification = criterion->verification;
            item.target = "acceptance/" + criterion->id;

            const VerificationRecord* record = FindVerification(currentRevision, criterion->id);
            if (record != nullptr)
            {
                item.outcome = record->outcome;
                item.summary = record->summary;
                item.evidence = record->evidence;
            }
            snapshot.reviewQueue.push_back(std::move(item));
        }
    }

    snapshot.revisions.reserve(result.revisions.size());
    for (const auto& revision : result.revisions)
    {
        WorkspaceRevisionView view{};
        view.id = revision.id;
        view.parentRevisionId = revision.parentRevisionId;
        view.changedPaths = revision.changedPaths;
        view.artifacts = revision.artifacts;
        view.feedback = revision.feedback;
        view.limitations = revision.limitations;
        view.failedVerificationCount = static_cast<std::size_t>(std::count_if(
            revision.verification.begin(),
            revision.verification.end(),
            [](const VerificationRecord& record)
            {
                return record.outcome == VerificationOutcome::Failed;
            }));
        snapshot.revisions.push_back(std::move(view));
    }

    return snapshot;
}

std::string_view ToString(const WorkspaceActionKind kind) noexcept
{
    switch (kind)
    {
    case WorkspaceActionKind::Feedback: return "feedback";
    case WorkspaceActionKind::Approve: return "approve";
    }
    return "feedback";
}

WorkspaceActionParseResult ParseWorkspaceActionToml(
    const std::string_view text,
    const std::string_view sourceName)
{
    WorkspaceActionParseResult output{};
    toml::table root{};
    try
    {
        root = toml::parse(text, sourceName);
    }
    catch (const toml::parse_error& error)
    {
        WorkSpecDiagnostic diagnostic{};
        diagnostic.path = "$";
        diagnostic.message = std::string{error.description()};
        diagnostic.line = static_cast<std::size_t>(error.source().begin.line);
        diagnostic.column = static_cast<std::size_t>(error.source().begin.column);
        output.diagnostics.push_back(std::move(diagnostic));
        return output;
    }

    ValidateKnownKeys(root, "", {"format_version", "action"}, output.diagnostics);

    const toml::node* versionNode = root.get("format_version");
    const std::optional<std::int64_t> version =
        versionNode == nullptr ? std::nullopt : versionNode->value<std::int64_t>();
    if (!version.has_value() || *version != WorkspaceActionFormatVersion)
    {
        AddDiagnostic(
            output.diagnostics,
            "format_version",
            "Only Workspace action format_version = 1 is supported.",
            versionNode);
    }

    const toml::node* actionNode = root.get("action");
    const toml::table* actionTable = actionNode == nullptr ? nullptr : actionNode->as_table();
    if (actionTable == nullptr)
    {
        AddDiagnostic(output.diagnostics, "action", "Expected an action table.", actionNode);
        return output;
    }

    ValidateKnownKeys(
        *actionTable,
        "action",
        {"kind", "work_id", "revision", "acceptance", "target", "message"},
        output.diagnostics);

    WorkspaceAction action{};
    const std::optional<std::string> kind =
        ReadRequiredString(*actionTable, "kind", "action.kind", output.diagnostics);
    if (kind.has_value())
    {
        if (*kind == "feedback") action.kind = WorkspaceActionKind::Feedback;
        else if (*kind == "approve") action.kind = WorkspaceActionKind::Approve;
        else AddDiagnostic(
            output.diagnostics,
            "action.kind",
            "Supported action kinds are feedback and approve.",
            actionTable->get("kind"));
    }

    const std::optional<std::string> workId =
        ReadRequiredString(*actionTable, "work_id", "action.work_id", output.diagnostics);
    const std::optional<std::string> revision =
        ReadRequiredString(*actionTable, "revision", "action.revision", output.diagnostics);
    const std::optional<std::string> acceptance =
        ReadOptionalString(*actionTable, "acceptance", "action.acceptance", output.diagnostics);
    const std::optional<std::string> target =
        ReadOptionalString(*actionTable, "target", "action.target", output.diagnostics);
    const std::optional<std::string> message =
        ReadOptionalString(*actionTable, "message", "action.message", output.diagnostics);

    if (workId.has_value()) action.workId = *workId;
    if (revision.has_value()) action.revisionId = *revision;
    if (acceptance.has_value()) action.acceptanceId = *acceptance;
    if (target.has_value()) action.target = *target;
    if (message.has_value()) action.message = *message;

    if (output.diagnostics.empty())
    {
        output.action = std::move(action);
    }
    return output;
}

std::string SerializeWorkspaceActionToml(const WorkspaceAction& action)
{
    std::ostringstream output{};
    output << "format_version = 1\n\n[action]\n"
           << "kind = \"" << EscapeTomlString(ToString(action.kind)) << "\"\n"
           << "work_id = \"" << EscapeTomlString(action.workId) << "\"\n"
           << "revision = \"" << EscapeTomlString(action.revisionId) << "\"\n";
    if (!action.acceptanceId.empty())
    {
        output << "acceptance = \"" << EscapeTomlString(action.acceptanceId) << "\"\n";
    }
    if (!action.target.empty())
    {
        output << "target = \"" << EscapeTomlString(action.target) << "\"\n";
    }
    if (!action.message.empty())
    {
        output << "message = \"" << EscapeTomlString(action.message) << "\"\n";
    }
    return output.str();
}

std::vector<std::string> ValidateWorkspaceAction(
    const WorkspaceSnapshot& snapshot,
    const WorkspaceAction& action)
{
    std::vector<std::string> errors{};
    if (action.workId != snapshot.workId)
    {
        errors.emplace_back("Workspace action work_id does not match the snapshot work_id.");
    }
    if (snapshot.currentRevisionId.empty() || action.revisionId != snapshot.currentRevisionId)
    {
        errors.emplace_back("Workspace action must target the current revision.");
    }

    const auto acceptance = std::find_if(
        snapshot.acceptance.begin(),
        snapshot.acceptance.end(),
        [&action](const WorkspaceAcceptanceView& value)
        {
            return value.id == action.acceptanceId;
        });
    if (!action.acceptanceId.empty() && acceptance == snapshot.acceptance.end())
    {
        errors.emplace_back("Workspace action acceptance does not exist in the current WorkSpec.");
    }

    if (action.kind == WorkspaceActionKind::Feedback)
    {
        if (action.message.empty())
        {
            errors.emplace_back("Feedback actions require a non-empty message.");
        }
        return errors;
    }

    if (action.acceptanceId.empty())
    {
        errors.emplace_back("Approval actions require an acceptance ID.");
        return errors;
    }

    const auto reviewItem = std::find_if(
        snapshot.reviewQueue.begin(),
        snapshot.reviewQueue.end(),
        [&action](const WorkspaceReviewItem& value)
        {
            return value.acceptanceId == action.acceptanceId;
        });
    if (reviewItem == snapshot.reviewQueue.end())
    {
        errors.emplace_back("Approval actions may only approve an item in the current review queue.");
        return errors;
    }

    if (!action.target.empty() && action.target != reviewItem->target)
    {
        errors.emplace_back("Approval action target must match the review item's stable semantic target.");
    }
    return errors;
}
} // namespace trace2d::agent
