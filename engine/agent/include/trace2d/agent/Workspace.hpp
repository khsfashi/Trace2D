#pragma once

#include <trace2d/agent/Inspection.hpp>
#include <trace2d/agent/WorkResult.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::agent
{
enum class WorkspaceProgressState : std::uint8_t
{
    Planned = 0,
    Working,
    Verified,
    ReviewNeeded,
    Failed,
    Approved,
};

[[nodiscard]] std::string_view ToString(WorkspaceProgressState state) noexcept;

struct WorkspaceAcceptanceView final
{
    std::string id{};
    std::string deliverableId{};
    std::string description{};
    VerificationClass verification{VerificationClass::Deterministic};
    VerificationOutcome outcome{VerificationOutcome::NotRun};
    std::string summary{};
    std::vector<std::string> evidence{};
    std::optional<WorkFailure> failure{};
};

struct WorkspaceDeliverableView final
{
    std::string id{};
    std::string description{};
    WorkspaceProgressState state{WorkspaceProgressState::Planned};
    std::vector<std::string> acceptanceIds{};
};

struct WorkspaceReviewItem final
{
    std::string acceptanceId{};
    std::string deliverableId{};
    std::string description{};
    VerificationClass verification{VerificationClass::Human};
    VerificationOutcome outcome{VerificationOutcome::ReviewNeeded};
    std::string target{};
    std::string summary{};
    std::vector<std::string> evidence{};
};

struct WorkspaceRevisionView final
{
    std::string id{};
    std::string parentRevisionId{};
    std::vector<std::string> changedPaths{};
    std::vector<WorkArtifact> artifacts{};
    std::vector<WorkFeedback> feedback{};
    std::vector<std::string> limitations{};
    std::size_t failedVerificationCount{0};
};

struct WorkspaceSnapshot final
{
    std::string workId{};
    std::string intent{};
    WorkResultState resultState{WorkResultState::Incomplete};
    std::string currentRevisionId{};
    std::vector<WorkspaceDeliverableView> deliverables{};
    std::vector<WorkspaceAcceptanceView> acceptance{};
    std::vector<WorkspaceReviewItem> reviewQueue{};
    std::vector<std::string> currentChangedPaths{};
    std::vector<WorkArtifact> currentArtifacts{};
    std::vector<std::string> currentLimitations{};
    std::vector<WorkspaceRevisionView> revisions{};
    std::vector<ExternalTruthRequirement> externalTruth{};
    std::optional<InspectionSnapshot> inspection{};
};

[[nodiscard]] WorkspaceSnapshot BuildWorkspaceSnapshot(
    const WorkSpec& spec,
    const WorkResult& result,
    const InspectionSnapshot* inspection = nullptr);

enum class WorkspaceActionKind : std::uint8_t
{
    Feedback = 0,
    Approve,
};

[[nodiscard]] std::string_view ToString(WorkspaceActionKind kind) noexcept;

struct WorkspaceAction final
{
    WorkspaceActionKind kind{WorkspaceActionKind::Feedback};
    std::string workId{};
    std::string revisionId{};
    std::string acceptanceId{};
    std::string target{};
    std::string message{};
};

struct WorkspaceActionParseResult final
{
    std::optional<WorkspaceAction> action{};
    std::vector<WorkSpecDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return action.has_value() && diagnostics.empty();
    }
};

[[nodiscard]] WorkspaceActionParseResult ParseWorkspaceActionToml(
    std::string_view text,
    std::string_view sourceName = {});

[[nodiscard]] std::string SerializeWorkspaceActionToml(const WorkspaceAction& action);

[[nodiscard]] std::vector<std::string> ValidateWorkspaceAction(
    const WorkspaceSnapshot& snapshot,
    const WorkspaceAction& action);
} // namespace trace2d::agent
