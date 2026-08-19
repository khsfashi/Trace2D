#include <trace2d/agent/AssetCandidateSet.hpp>

#include <toml++/toml.hpp>

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace trace2d::agent
{
namespace
{
constexpr std::int64_t AssetCandidateSetFormatVersion = 1;

void AddDiagnostic(
    std::vector<AssetCandidateDiagnostic>& diagnostics,
    std::string path,
    std::string message,
    const toml::node* node = nullptr)
{
    AssetCandidateDiagnostic diagnostic{};
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
    std::vector<AssetCandidateDiagnostic>& diagnostics)
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
    std::vector<AssetCandidateDiagnostic>& diagnostics)
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

std::optional<std::uint32_t> ReadRequiredPositiveUint32(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<AssetCandidateDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required field is missing.");
        return std::nullopt;
    }

    const std::optional<std::int64_t> value = node->value<std::int64_t>();
    if (!value.has_value() || *value <= 0 ||
        static_cast<std::uint64_t>(*value) > std::numeric_limits<std::uint32_t>::max())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected a positive 32-bit integer.", node);
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*value);
}

void ReadRequiredUniqueStringArray(
    const toml::table& table,
    const std::string_view key,
    const std::string_view path,
    std::vector<std::string>& output,
    std::vector<AssetCandidateDiagnostic>& diagnostics)
{
    const toml::node* node = table.get(key);
    if (node == nullptr)
    {
        AddDiagnostic(diagnostics, std::string{path}, "Required array is missing.");
        return;
    }

    const toml::array* array = node->as_array();
    if (array == nullptr || array->empty())
    {
        AddDiagnostic(diagnostics, std::string{path}, "Expected at least one artifact id.", node);
        return;
    }

    output.reserve(array->size());
    std::unordered_set<std::string> seen{};
    seen.reserve(array->size());
    for (std::size_t index = 0; index < array->size(); ++index)
    {
        const toml::node* item = array->get(index);
        const std::optional<std::string> value =
            item == nullptr ? std::nullopt : item->value<std::string>();
        const std::string itemPath = std::string{path} + "[" + std::to_string(index) + "]";
        if (!value.has_value() || value->empty())
        {
            AddDiagnostic(diagnostics, itemPath, "Expected a non-empty artifact id.", item);
            continue;
        }
        if (!seen.insert(*value).second)
        {
            AddDiagnostic(diagnostics, itemPath, "Duplicate artifact id '" + *value + "'.", item);
            continue;
        }
        output.push_back(*value);
    }
}
} // namespace

AssetCandidateSetParseResult ParseAssetCandidateSetToml(
    const std::string_view text,
    const std::string_view sourceName)
{
    AssetCandidateSetParseResult output{};
    toml::table root{};
    try
    {
        root = toml::parse(text, sourceName);
    }
    catch (const toml::parse_error& error)
    {
        AssetCandidateDiagnostic diagnostic{};
        diagnostic.path = "$";
        diagnostic.message = std::string{error.description()};
        diagnostic.line = static_cast<std::size_t>(error.source().begin.line);
        diagnostic.column = static_cast<std::size_t>(error.source().begin.column);
        output.diagnostics.push_back(std::move(diagnostic));
        return output;
    }

    ValidateKnownKeys(root, "", {"format_version", "candidate_set", "candidates"}, output.diagnostics);

    const toml::node* versionNode = root.get("format_version");
    const std::optional<std::int64_t> version =
        versionNode == nullptr ? std::nullopt : versionNode->value<std::int64_t>();
    if (!version.has_value() || *version != AssetCandidateSetFormatVersion)
    {
        AddDiagnostic(
            output.diagnostics,
            "format_version",
            "Expected integer format_version = 1.",
            versionNode);
    }

    AssetCandidateSet candidateSet{};
    const toml::node* setNode = root.get("candidate_set");
    const toml::table* setTable = setNode == nullptr ? nullptr : setNode->as_table();
    if (setTable == nullptr)
    {
        AddDiagnostic(output.diagnostics, "candidate_set", "Expected a candidate_set table.", setNode);
    }
    else
    {
        ValidateKnownKeys(
            *setTable,
            "candidate_set",
            {"production_set", "item", "work_id", "revision"},
            output.diagnostics);

        const std::optional<std::string> productionSet = ReadRequiredString(
            *setTable,
            "production_set",
            "candidate_set.production_set",
            output.diagnostics);
        const std::optional<std::string> item =
            ReadRequiredString(*setTable, "item", "candidate_set.item", output.diagnostics);
        const std::optional<std::string> work =
            ReadRequiredString(*setTable, "work_id", "candidate_set.work_id", output.diagnostics);
        const std::optional<std::string> revision =
            ReadRequiredString(*setTable, "revision", "candidate_set.revision", output.diagnostics);

        if (productionSet.has_value()) candidateSet.productionSetId = *productionSet;
        if (item.has_value()) candidateSet.itemId = *item;
        if (work.has_value()) candidateSet.workId = *work;
        if (revision.has_value()) candidateSet.revisionId = *revision;
    }

    const toml::node* candidatesNode = root.get("candidates");
    const toml::array* candidates = candidatesNode == nullptr ? nullptr : candidatesNode->as_array();
    if (candidates == nullptr || candidates->empty())
    {
        AddDiagnostic(
            output.diagnostics,
            "candidates",
            "Expected at least one candidate table.",
            candidatesNode);
    }
    else
    {
        candidateSet.candidates.reserve(candidates->size());
        std::unordered_set<std::string> candidateIds{};
        std::unordered_set<std::uint32_t> ordinals{};
        candidateIds.reserve(candidates->size());
        ordinals.reserve(candidates->size());

        for (std::size_t index = 0; index < candidates->size(); ++index)
        {
            const toml::node* candidateNode = candidates->get(index);
            const toml::table* candidateTable =
                candidateNode == nullptr ? nullptr : candidateNode->as_table();
            const std::string path = "candidates[" + std::to_string(index) + "]";
            if (candidateTable == nullptr)
            {
                AddDiagnostic(output.diagnostics, path, "Expected a candidate table.", candidateNode);
                continue;
            }

            ValidateKnownKeys(
                *candidateTable,
                path,
                {"id", "ordinal", "artifacts"},
                output.diagnostics);

            AssetCandidate candidate{};
            const std::optional<std::string> id =
                ReadRequiredString(*candidateTable, "id", path + ".id", output.diagnostics);
            const std::optional<std::uint32_t> ordinal = ReadRequiredPositiveUint32(
                *candidateTable,
                "ordinal",
                path + ".ordinal",
                output.diagnostics);
            ReadRequiredUniqueStringArray(
                *candidateTable,
                "artifacts",
                path + ".artifacts",
                candidate.artifactIds,
                output.diagnostics);

            if (id.has_value())
            {
                candidate.id = *id;
                if (!candidateIds.insert(*id).second)
                {
                    AddDiagnostic(
                        output.diagnostics,
                        path + ".id",
                        "Duplicate candidate id '" + *id + "'.",
                        candidateTable->get("id"));
                }
            }
            if (ordinal.has_value())
            {
                candidate.ordinal = *ordinal;
                if (!ordinals.insert(*ordinal).second)
                {
                    AddDiagnostic(
                        output.diagnostics,
                        path + ".ordinal",
                        "Duplicate candidate ordinal " + std::to_string(*ordinal) + ".",
                        candidateTable->get("ordinal"));
                }
            }
            candidateSet.candidates.push_back(std::move(candidate));
        }
    }

    if (output.diagnostics.empty())
    {
        output.candidateSet = std::move(candidateSet);
    }
    return output;
}

std::vector<AssetCandidateDiagnostic> ValidateAssetCandidateSetAgainstProductionSpec(
    const AssetCandidateSet& candidateSet,
    const AssetProductionSpec& productionSpec)
{
    std::vector<AssetCandidateDiagnostic> diagnostics{};

    if (candidateSet.productionSetId != productionSpec.id)
    {
        AddDiagnostic(
            diagnostics,
            "candidate_set.production_set",
            "Candidate set production_set must match AssetProductionSpec id '" + productionSpec.id + "'.");
    }
    if (candidateSet.workId != productionSpec.workSpecId)
    {
        AddDiagnostic(
            diagnostics,
            "candidate_set.work_id",
            "Candidate set work_id must match AssetProductionSpec work_spec '" + productionSpec.workSpecId + "'.");
    }

    const auto item = std::find_if(
        productionSpec.items.begin(),
        productionSpec.items.end(),
        [&candidateSet](const AssetProductionItem& value)
        {
            return value.id == candidateSet.itemId;
        });
    if (item == productionSpec.items.end())
    {
        AddDiagnostic(
            diagnostics,
            "candidate_set.item",
            "Candidate set item does not exist in the referenced AssetProductionSpec.");
    }

    if (productionSpec.candidatesPerItem == 0)
    {
        AddDiagnostic(
            diagnostics,
            "candidate_set",
            "Referenced AssetProductionSpec has no positive candidate budget.");
        return diagnostics;
    }

    if (candidateSet.candidates.size() > productionSpec.candidatesPerItem)
    {
        AddDiagnostic(
            diagnostics,
            "candidates",
            "Candidate count exceeds AssetProductionSpec candidates_per_item budget.");
    }

    for (std::size_t index = 0; index < candidateSet.candidates.size(); ++index)
    {
        if (candidateSet.candidates[index].ordinal > productionSpec.candidatesPerItem)
        {
            AddDiagnostic(
                diagnostics,
                "candidates[" + std::to_string(index) + "].ordinal",
                "Candidate ordinal exceeds AssetProductionSpec candidates_per_item budget.");
        }
    }

    return diagnostics;
}

AssetCandidateComparisonResult BuildAssetCandidateComparison(
    const AssetCandidateSet& candidateSet,
    const AssetProductionSpec& productionSpec,
    const WorkspaceSnapshot& workspace)
{
    AssetCandidateComparisonResult output{};
    output.diagnostics = ValidateAssetCandidateSetAgainstProductionSpec(candidateSet, productionSpec);

    if (workspace.workId != candidateSet.workId)
    {
        AddDiagnostic(
            output.diagnostics,
            "candidate_set.work_id",
            "Candidate set work_id does not match the Workspace work id.");
    }
    if (workspace.currentRevisionId != candidateSet.revisionId)
    {
        AddDiagnostic(
            output.diagnostics,
            "candidate_set.revision",
            "Candidate set revision is stale or does not match the current Workspace revision.");
    }

    std::unordered_map<std::string, const WorkArtifact*> artifacts{};
    artifacts.reserve(workspace.currentArtifacts.size());
    for (const WorkArtifact& artifact : workspace.currentArtifacts)
    {
        if (artifact.id.empty())
        {
            AddDiagnostic(
                output.diagnostics,
                "workspace.current_artifacts",
                "Workspace current artifact has an empty id.");
            continue;
        }
        if (!artifacts.emplace(artifact.id, &artifact).second)
        {
            AddDiagnostic(
                output.diagnostics,
                "workspace.current_artifacts",
                "Workspace current revision contains duplicate artifact id '" + artifact.id + "'.");
        }
    }

    AssetCandidateComparison comparison{};
    comparison.productionSetId = candidateSet.productionSetId;
    comparison.itemId = candidateSet.itemId;
    comparison.workId = candidateSet.workId;
    comparison.revisionId = candidateSet.revisionId;
    comparison.candidates.reserve(candidateSet.candidates.size());

    for (std::size_t candidateIndex = 0; candidateIndex < candidateSet.candidates.size(); ++candidateIndex)
    {
        const AssetCandidate& candidate = candidateSet.candidates[candidateIndex];
        AssetCandidateComparisonEntry entry{};
        entry.id = candidate.id;
        entry.ordinal = candidate.ordinal;
        entry.artifacts.reserve(candidate.artifactIds.size());

        for (std::size_t artifactIndex = 0; artifactIndex < candidate.artifactIds.size(); ++artifactIndex)
        {
            const std::string& artifactId = candidate.artifactIds[artifactIndex];
            const auto found = artifacts.find(artifactId);
            if (found == artifacts.end())
            {
                AddDiagnostic(
                    output.diagnostics,
                    "candidates[" + std::to_string(candidateIndex) + "].artifacts[" +
                        std::to_string(artifactIndex) + "]",
                    "Artifact id '" + artifactId + "' is not owned by the current Workspace revision.");
                continue;
            }
            entry.artifacts.push_back(*found->second);
        }
        comparison.candidates.push_back(std::move(entry));
    }

    if (output.diagnostics.empty())
    {
        output.comparison = std::move(comparison);
    }
    return output;
}
} // namespace trace2d::agent
