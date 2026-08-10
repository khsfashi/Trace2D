#include <trace2d/agent/WorkResult.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace trace2d::agent
{
namespace
{
constexpr std::int64_t WorkResultFormatVersion = 1;

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

bool ParseToml(
    const std::string_view text,
    const std::string_view sourceName,
    toml::table& root,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    try
    {
        root = toml::parse(text, sourceName);
        return true;
    }
    catch (const toml::parse_error& error)
    {
        WorkSpecDiagnostic diagnostic{};
        diagnostic.path = "$";
        diagnostic.message = std::string{error.description()};
        diagnostic.line = static_cast<std::size_t>(error.source().begin.line);
        diagnostic.column = static_cast<std::size_t>(error.source().begin.column);
        diagnostics.push_back(std::move(diagnostic));
        return false;
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
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a string.", node);
        return std::nullopt;
    }
    return value;
}

void ReadStringArray(
    const toml::table& table,
    const std::string_view key,
    const std::string& path,
    std::vector<std::string>& destination,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        return;
    }

    const toml::array* array = node->as_array();
    if (array == nullptr)
    {
        AddDiagnostic(diagnostics, path, "Expected an array of strings.", node);
        return;
    }

    destination.reserve(array->size());
    for (std::size_t index = 0; index < array->size(); ++index)
    {
        const toml::node* item = array->get(index);
        const std::optional<std::string> value =
            item == nullptr ? std::nullopt : item->value<std::string>();
        if (!value.has_value() || value->empty())
        {
            AddDiagnostic(
                diagnostics,
                path + "[" + std::to_string(index) + "]",
                "Expected a non-empty string.",
                item);
            continue;
        }
        destination.push_back(*value);
    }
}

std::optional<VerificationClass> ParseVerificationClass(
    const toml::node* node,
    const std::string& path,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    const std::optional<std::string> value =
        node == nullptr ? std::nullopt : node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, path, "Expected a verification-class string.", node);
        return std::nullopt;
    }

    if (*value == "deterministic") return VerificationClass::Deterministic;
    if (*value == "presentation") return VerificationClass::Presentation;
    if (*value == "multimodal") return VerificationClass::Multimodal;
    if (*value == "human") return VerificationClass::Human;

    AddDiagnostic(
        diagnostics,
        path,
        "Supported verification classes are deterministic, presentation, multimodal, and human.",
        node);
    return std::nullopt;
}

std::optional<VerificationOutcome> ParseOutcome(
    const toml::node* node,
    const std::string& path,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    const std::optional<std::string> value =
        node == nullptr ? std::nullopt : node->value<std::string>();
    if (!value.has_value())
    {
        AddDiagnostic(diagnostics, path, "Expected a verification-outcome string.", node);
        return std::nullopt;
    }

    if (*value == "not_run") return VerificationOutcome::NotRun;
    if (*value == "passed") return VerificationOutcome::Passed;
    if (*value == "failed") return VerificationOutcome::Failed;
    if (*value == "review_needed") return VerificationOutcome::ReviewNeeded;
    if (*value == "approved") return VerificationOutcome::Approved;

    AddDiagnostic(
        diagnostics,
        path,
        "Supported outcomes are not_run, passed, failed, review_needed, and approved.",
        node);
    return std::nullopt;
}

bool HasFailureFields(const toml::table& table) noexcept
{
    return table.contains("failure_code") ||
        table.contains("failure_target") ||
        table.contains("failure_message") ||
        table.contains("reproduction");
}

void ParseVerificationRecords(
    const toml::table& revisionTable,
    const std::size_t revisionIndex,
    WorkRevision& revision,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    const toml::node* verificationNode = revisionTable.get("verification");
    if (verificationNode == nullptr)
    {
        return;
    }

    const toml::array* verificationArray = verificationNode->as_array();
    if (verificationArray == nullptr)
    {
        AddDiagnostic(
            diagnostics,
            "revisions[" + std::to_string(revisionIndex) + "].verification",
            "Expected an array of verification tables.",
            verificationNode);
        return;
    }

    std::unordered_set<std::string> acceptanceIds{};
    revision.verification.reserve(verificationArray->size());
    for (std::size_t index = 0; index < verificationArray->size(); ++index)
    {
        const toml::node* itemNode = verificationArray->get(index);
        const toml::table* item = itemNode == nullptr ? nullptr : itemNode->as_table();
        const std::string path = "revisions[" + std::to_string(revisionIndex) + "].verification[" +
            std::to_string(index) + "]";
        if (item == nullptr)
        {
            AddDiagnostic(diagnostics, path, "Expected a verification table.", itemNode);
            continue;
        }

        ValidateKnownKeys(
            *item,
            path,
            {
                "acceptance",
                "verification",
                "outcome",
                "summary",
                "evidence",
                "failure_code",
                "failure_target",
                "failure_message",
                "reproduction",
            },
            diagnostics);

        VerificationRecord record{};
        const std::optional<std::string> acceptance =
            ReadRequiredString(*item, "acceptance", path + ".acceptance", diagnostics);
        const std::optional<VerificationClass> verification =
            ParseVerificationClass(item->get("verification"), path + ".verification", diagnostics);
        const std::optional<VerificationOutcome> outcome =
            ParseOutcome(item->get("outcome"), path + ".outcome", diagnostics);
        const std::optional<std::string> summary =
            ReadRequiredString(*item, "summary", path + ".summary", diagnostics);
        ReadStringArray(*item, "evidence", path + ".evidence", record.evidence, diagnostics);

        if (acceptance.has_value()) record.acceptanceId = *acceptance;
        if (verification.has_value()) record.verification = *verification;
        if (outcome.has_value()) record.outcome = *outcome;
        if (summary.has_value()) record.summary = *summary;

        if (acceptance.has_value() && !acceptanceIds.insert(*acceptance).second)
        {
            AddDiagnostic(diagnostics, path + ".acceptance", "Duplicate acceptance result in one revision.");
        }

        if (outcome.has_value() &&
            (*outcome == VerificationOutcome::Passed || *outcome == VerificationOutcome::Approved) &&
            record.evidence.empty())
        {
            AddDiagnostic(
                diagnostics,
                path + ".evidence",
                "Passed or approved verification requires at least one evidence reference.");
        }

        if (outcome.has_value() && *outcome == VerificationOutcome::Failed)
        {
            WorkFailure failure{};
            const std::optional<std::string> code =
                ReadRequiredString(*item, "failure_code", path + ".failure_code", diagnostics);
            const std::optional<std::string> target =
                ReadRequiredString(*item, "failure_target", path + ".failure_target", diagnostics);
            const std::optional<std::string> message =
                ReadRequiredString(*item, "failure_message", path + ".failure_message", diagnostics);
            const std::optional<std::string> reproduction =
                ReadRequiredString(*item, "reproduction", path + ".reproduction", diagnostics);
            if (code.has_value()) failure.code = *code;
            if (target.has_value()) failure.target = *target;
            if (message.has_value()) failure.message = *message;
            if (reproduction.has_value()) failure.reproduction = *reproduction;
            failure.evidence = record.evidence;
            record.failure = std::move(failure);
        }
        else if (outcome.has_value() && HasFailureFields(*item))
        {
            AddDiagnostic(
                diagnostics,
                path,
                "Failure fields are valid only when outcome = 'failed'.",
                itemNode);
        }

        revision.verification.push_back(std::move(record));
    }
}

void ParseArtifacts(
    const toml::table& revisionTable,
    const std::size_t revisionIndex,
    WorkRevision& revision,
    std::vector<WorkSpecDiagnostic>& diagnostics)
{
    const toml::node* artifactsNode = revisionTable.get("artifacts");
    if (artifactsNode == nullptr)
    {
        return;
    }

    const toml::array* artifactsArray = artifactsNode->as_array();
    if (artifactsArray == nullptr)
    {
        AddDiagnostic(
            diagnostics,
            "revisions[" + std::to_string(revisionIndex) + "].artifacts",
            "Expected an array of artifact tables.",
            artifactsNode);
        return;
    }

    std::unordered_set<std::string> artifactIds{};
    revision.artifacts.reserve(artifactsArray->size());
    for (std::size_t index = 0; index < artifactsArray->size(); ++index)
    {
        const toml::node* itemNode = artifactsArray->get(index);
        const toml::table* item = itemNode == nullptr ? nullptr : itemNode->as_table();
        const std::string path = "revisions[" + std::to_string(revisionIndex) + "].artifacts[" +
            std::to_string(index) + "]";
        if (item == nullptr)
        {
            AddDiagnostic(diagnostics, path, "Expected an artifact table.", itemNode);
            continue;
        }

        ValidateKnownKeys(*item, path, {"id", "kind", "path", "description"}, diagnostics);

        WorkArtifact artifact{};
        const std::optional<std::string> id =
            ReadRequiredString(*item, "id", path + ".id", diagnostics);
        const std::optional<std::string> kind =
            ReadRequiredString(*item, "kind", path + ".kind", diagnostics);
        const std::optional<std::string> artifactPath =
            ReadRequiredString(*item, "path", path + ".path", diagnostics);
        const std::optional<std::string> description =
            ReadOptionalString(*item, "description", path + ".description", diagnostics);

        if (id.has_value()) artifact.id = *id;
        if (kind.has_value()) artifact.kind = *kind;
        if (artifactPath.has_value()) artifact.path = *artifactPath;
        if (description.has_value()) artifact.description = *description;

        if (id.has_value() && !artifactIds.insert(*id).second)
        {
            AddDiagnostic(diagnostics, path + ".id", "Duplicate artifact id in one revision.");
        }
        revision.artifacts.push_back(std::move(artifact));
    }
}

bool OutcomeCompletesCriterion(
    const AcceptanceCriterion& criterion,
    const VerificationOutcome outcome) noexcept
{
    if (outcome == VerificationOutcome::Approved)
    {
        return true;
    }
    if (criterion.verification == VerificationClass::Deterministic ||
        criterion.verification == VerificationClass::Presentation)
    {
        return outcome == VerificationOutcome::Passed;
    }
    return false;
}

bool OutcomeNeedsReview(
    const AcceptanceCriterion& criterion,
    const VerificationOutcome outcome) noexcept
{
    if (outcome == VerificationOutcome::ReviewNeeded)
    {
        return true;
    }
    return (criterion.verification == VerificationClass::Multimodal ||
            criterion.verification == VerificationClass::Human) &&
        outcome == VerificationOutcome::Passed;
}
} // namespace

std::string_view ToString(const VerificationOutcome outcome) noexcept
{
    switch (outcome)
    {
    case VerificationOutcome::NotRun: return "not_run";
    case VerificationOutcome::Passed: return "passed";
    case VerificationOutcome::Failed: return "failed";
    case VerificationOutcome::ReviewNeeded: return "review_needed";
    case VerificationOutcome::Approved: return "approved";
    }
    return "unknown";
}

std::string_view ToString(const WorkResultState state) noexcept
{
    switch (state)
    {
    case WorkResultState::Incomplete: return "incomplete";
    case WorkResultState::Failed: return "failed";
    case WorkResultState::ReviewNeeded: return "review_needed";
    case WorkResultState::Complete: return "complete";
    }
    return "unknown";
}

WorkResultParseResult ParseWorkResultToml(
    const std::string_view text,
    const std::string_view sourceName)
{
    WorkResultParseResult parsed{};
    toml::table root{};
    if (!ParseToml(text, sourceName, root, parsed.diagnostics))
    {
        return parsed;
    }

    ValidateKnownKeys(root, "", {"format_version", "result", "revisions"}, parsed.diagnostics);

    const toml::node* formatNode = root.get("format_version");
    const std::optional<std::int64_t> formatVersion =
        formatNode == nullptr ? std::nullopt : formatNode->value<std::int64_t>();
    if (!formatVersion.has_value() || *formatVersion != WorkResultFormatVersion)
    {
        AddDiagnostic(parsed.diagnostics, "format_version", "Expected integer format_version = 1.", formatNode);
    }

    WorkResult result{};
    const toml::node* resultNode = root.get("result");
    const toml::table* resultTable = resultNode == nullptr ? nullptr : resultNode->as_table();
    if (resultTable == nullptr)
    {
        AddDiagnostic(parsed.diagnostics, "result", "Required result table is missing.", resultNode);
    }
    else
    {
        ValidateKnownKeys(*resultTable, "result", {"work_id"}, parsed.diagnostics);
        const std::optional<std::string> workId =
            ReadRequiredString(*resultTable, "work_id", "result.work_id", parsed.diagnostics);
        if (workId.has_value()) result.workId = *workId;
    }

    const toml::node* revisionsNode = root.get("revisions");
    const toml::array* revisionsArray = revisionsNode == nullptr ? nullptr : revisionsNode->as_array();
    if (revisionsArray == nullptr || revisionsArray->empty())
    {
        AddDiagnostic(parsed.diagnostics, "revisions", "Expected at least one revision table.", revisionsNode);
    }
    else
    {
        std::unordered_set<std::string> revisionIds{};
        std::optional<std::string> previousRevisionId{};
        result.revisions.reserve(revisionsArray->size());

        for (std::size_t index = 0; index < revisionsArray->size(); ++index)
        {
            const toml::node* revisionNode = revisionsArray->get(index);
            const toml::table* revisionTable =
                revisionNode == nullptr ? nullptr : revisionNode->as_table();
            const std::string path = "revisions[" + std::to_string(index) + "]";
            if (revisionTable == nullptr)
            {
                AddDiagnostic(parsed.diagnostics, path, "Expected a revision table.", revisionNode);
                previousRevisionId.reset();
                continue;
            }

            ValidateKnownKeys(
                *revisionTable,
                path,
                {"id", "parent", "changed_paths", "limitations", "verification", "artifacts"},
                parsed.diagnostics);

            WorkRevision revision{};
            const std::optional<std::string> id =
                ReadRequiredString(*revisionTable, "id", path + ".id", parsed.diagnostics);
            const std::optional<std::string> parent =
                ReadOptionalString(*revisionTable, "parent", path + ".parent", parsed.diagnostics);
            if (id.has_value()) revision.id = *id;
            if (parent.has_value()) revision.parentRevisionId = *parent;

            ReadStringArray(
                *revisionTable,
                "changed_paths",
                path + ".changed_paths",
                revision.changedPaths,
                parsed.diagnostics);
            ReadStringArray(
                *revisionTable,
                "limitations",
                path + ".limitations",
                revision.limitations,
                parsed.diagnostics);
            ParseVerificationRecords(*revisionTable, index, revision, parsed.diagnostics);
            ParseArtifacts(*revisionTable, index, revision, parsed.diagnostics);

            if (id.has_value() && !revisionIds.insert(*id).second)
            {
                AddDiagnostic(parsed.diagnostics, path + ".id", "Duplicate revision id.");
            }

            if (index == 0U)
            {
                if (parent.has_value() && !parent->empty())
                {
                    AddDiagnostic(
                        parsed.diagnostics,
                        path + ".parent",
                        "The first revision must not have a parent.",
                        revisionTable->get("parent"));
                }
            }
            else if (!previousRevisionId.has_value())
            {
                AddDiagnostic(
                    parsed.diagnostics,
                    path + ".parent",
                    "Cannot validate revision lineage because the preceding revision is invalid.",
                    revisionTable->get("parent"));
            }
            else if (!parent.has_value() || *parent != *previousRevisionId)
            {
                AddDiagnostic(
                    parsed.diagnostics,
                    path + ".parent",
                    "Revision parent must reference the immediately preceding revision '" +
                        *previousRevisionId + "'.",
                    revisionTable->get("parent"));
            }

            result.revisions.push_back(std::move(revision));
            previousRevisionId = id;
        }
    }

    if (parsed.diagnostics.empty())
    {
        parsed.result = std::move(result);
    }
    return parsed;
}

WorkResultEvaluation EvaluateWorkResult(const WorkSpec& spec, const WorkResult& result)
{
    WorkResultEvaluation evaluation{};
    evaluation.externalTruth = spec.externalTruth;
    if (result.workId != spec.id || result.revisions.empty())
    {
        evaluation.state = WorkResultState::Incomplete;
        for (const AcceptanceCriterion& criterion : spec.acceptance)
        {
            evaluation.outstandingAcceptanceIds.push_back(criterion.id);
        }
        return evaluation;
    }

    const WorkRevision& current = result.revisions.back();
    evaluation.currentRevisionId = current.id;

    std::unordered_map<std::string, const VerificationRecord*> records{};
    records.reserve(current.verification.size());
    for (const VerificationRecord& record : current.verification)
    {
        records.emplace(record.acceptanceId, &record);
    }

    bool hasFailure = false;
    bool needsReview = false;
    for (const AcceptanceCriterion& criterion : spec.acceptance)
    {
        const auto found = records.find(criterion.id);
        if (found == records.end())
        {
            evaluation.outstandingAcceptanceIds.push_back(criterion.id);
            continue;
        }

        const VerificationRecord& record = *found->second;
        if (record.verification != criterion.verification)
        {
            evaluation.outstandingAcceptanceIds.push_back(criterion.id);
            continue;
        }
        if (record.outcome == VerificationOutcome::Failed)
        {
            hasFailure = true;
            if (record.failure.has_value())
            {
                evaluation.failures.push_back(*record.failure);
            }
            continue;
        }
        if (OutcomeNeedsReview(criterion, record.outcome))
        {
            needsReview = true;
            evaluation.reviewAcceptanceIds.push_back(criterion.id);
            continue;
        }
        if (!OutcomeCompletesCriterion(criterion, record.outcome))
        {
            evaluation.outstandingAcceptanceIds.push_back(criterion.id);
        }
    }

    if (hasFailure)
    {
        evaluation.state = WorkResultState::Failed;
    }
    else if (!evaluation.outstandingAcceptanceIds.empty())
    {
        evaluation.state = WorkResultState::Incomplete;
    }
    else if (needsReview)
    {
        evaluation.state = WorkResultState::ReviewNeeded;
    }
    else
    {
        evaluation.state = WorkResultState::Complete;
    }
    return evaluation;
}
} // namespace trace2d::agent
