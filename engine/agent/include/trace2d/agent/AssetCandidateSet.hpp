#pragma once

#include <trace2d/agent/AssetProductionSpec.hpp>
#include <trace2d/agent/Workspace.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::agent
{
struct AssetCandidateDiagnostic final
{
    std::string path{};
    std::string message{};
    std::size_t line{0};
    std::size_t column{0};
};

struct AssetCandidate final
{
    std::string id{};
    std::uint32_t ordinal{0};
    std::vector<std::string> artifactIds{};
};

struct AssetCandidateSet final
{
    std::string productionSetId{};
    std::string itemId{};
    std::string workId{};
    std::string revisionId{};
    std::vector<AssetCandidate> candidates{};
};

struct AssetCandidateSetParseResult final
{
    std::optional<AssetCandidateSet> candidateSet{};
    std::vector<AssetCandidateDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return candidateSet.has_value() && diagnostics.empty();
    }
};

// CandidateSet owns only bounded grouping/reference metadata. Approval,
// feedback, verification and revision truth remain in WorkResult/Workspace.
[[nodiscard]] AssetCandidateSetParseResult ParseAssetCandidateSetToml(
    std::string_view text,
    std::string_view sourceName = {});

// Setup-time validation binds candidate grouping to the already-committed
// production request. It does not inspect providers or perform generation.
[[nodiscard]] std::vector<AssetCandidateDiagnostic> ValidateAssetCandidateSetAgainstProductionSpec(
    const AssetCandidateSet& candidateSet,
    const AssetProductionSpec& productionSpec);

struct AssetCandidateComparisonEntry final
{
    std::string id{};
    std::uint32_t ordinal{0};
    std::vector<WorkArtifact> artifacts{};
};

struct AssetCandidateComparison final
{
    std::string productionSetId{};
    std::string itemId{};
    std::string workId{};
    std::string revisionId{};
    std::vector<AssetCandidateComparisonEntry> candidates{};
};

struct AssetCandidateComparisonResult final
{
    std::optional<AssetCandidateComparison> comparison{};
    std::vector<AssetCandidateDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return comparison.has_value() && diagnostics.empty();
    }
};

// Compose a comparison-only view from the current Workspace result. Artifact
// metadata is reused from the exact current WorkResult revision; decoded/GPU
// payloads and selection/approval state are not duplicated here.
[[nodiscard]] AssetCandidateComparisonResult BuildAssetCandidateComparison(
    const AssetCandidateSet& candidateSet,
    const AssetProductionSpec& productionSpec,
    const WorkspaceSnapshot& workspace);
} // namespace trace2d::agent
