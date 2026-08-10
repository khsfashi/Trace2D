#pragma once

#include <trace2d/agent/WorkSpec.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::agent
{
enum class VerificationOutcome : std::uint8_t
{
    NotRun = 0,
    Passed,
    Failed,
    ReviewNeeded,
    Approved,
};

[[nodiscard]] std::string_view ToString(VerificationOutcome outcome) noexcept;

enum class WorkResultState : std::uint8_t
{
    Incomplete = 0,
    Failed,
    ReviewNeeded,
    Complete,
};

[[nodiscard]] std::string_view ToString(WorkResultState state) noexcept;

struct WorkFailure final
{
    std::string code{};
    std::string target{};
    std::string message{};
    std::string reproduction{};
    std::vector<std::string> evidence{};
};

struct VerificationRecord final
{
    std::string acceptanceId{};
    VerificationClass verification{VerificationClass::Deterministic};
    VerificationOutcome outcome{VerificationOutcome::NotRun};
    std::string summary{};
    std::vector<std::string> evidence{};
    std::optional<WorkFailure> failure{};
};

struct WorkArtifact final
{
    std::string id{};
    std::string kind{};
    std::string path{};
    std::string description{};
};

struct WorkRevision final
{
    std::string id{};
    std::string parentRevisionId{};
    std::string feedback{};
    std::vector<std::string> changedPaths{};
    std::vector<VerificationRecord> verification{};
    std::vector<WorkArtifact> artifacts{};
    std::vector<std::string> limitations{};
};

struct WorkResult final
{
    std::string workId{};
    std::vector<WorkRevision> revisions{};
};

struct WorkResultParseResult final
{
    std::optional<WorkResult> result{};
    std::vector<WorkSpecDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return result.has_value() && diagnostics.empty();
    }
};

[[nodiscard]] WorkResultParseResult ParseWorkResultToml(
    std::string_view text,
    std::string_view sourceName = {});

struct WorkResultEvaluation final
{
    WorkResultState state{WorkResultState::Incomplete};
    std::string currentRevisionId{};
    std::vector<std::string> outstandingAcceptanceIds{};
    std::vector<std::string> reviewAcceptanceIds{};
    std::vector<WorkFailure> failures{};
    std::vector<ExternalTruthRequirement> externalTruth{};

    [[nodiscard]] bool RequiresLiveTruth() const noexcept
    {
        return !externalTruth.empty();
    }
};

[[nodiscard]] WorkResultEvaluation EvaluateWorkResult(
    const WorkSpec& spec,
    const WorkResult& result);
} // namespace trace2d::agent
