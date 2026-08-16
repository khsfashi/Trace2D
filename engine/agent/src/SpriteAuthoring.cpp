#include <trace2d/agent/SpriteAuthoring.hpp>

#include "AuthoringFileTransaction.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

namespace trace2d::agent
{
namespace
{
constexpr std::size_t MaximumDiagnostics = 8U;

void AddDiagnostic(
    SpriteAuthoringResult& result,
    const SpriteAuthoringErrorCode code,
    std::string path,
    std::string message)
{
    if (result.diagnostics.size() >= MaximumDiagnostics)
    {
        return;
    }
    result.diagnostics.push_back(SpriteAuthoringDiagnostic{
        .code = code,
        .path = std::move(path),
        .message = std::move(message),
    });
}

void CopyAssetDiagnostics(
    SpriteAuthoringResult& result,
    const SpriteAuthoringErrorCode code,
    const std::vector<assets::SpriteAssetDiagnostic>& diagnostics)
{
    for (const assets::SpriteAssetDiagnostic& diagnostic : diagnostics)
    {
        AddDiagnostic(result, code, diagnostic.path, diagnostic.message);
        if (result.diagnostics.size() >= MaximumDiagnostics)
        {
            break;
        }
    }
}

std::string RegionFieldPath(const std::string_view regionId, const std::string_view field)
{
    std::string path{"regions."};
    path.append(regionId);
    path.push_back('.');
    path.append(field);
    return path;
}
} // namespace

std::string_view ToString(const SpriteAuthoringErrorCode code) noexcept
{
    switch (code)
    {
    case SpriteAuthoringErrorCode::InvalidRequest:
        return "invalid_request";
    case SpriteAuthoringErrorCode::AssetLoadFailed:
        return "asset_load_failed";
    case SpriteAuthoringErrorCode::UnknownRegion:
        return "unknown_region";
    case SpriteAuthoringErrorCode::ValidationFailed:
        return "validation_failed";
    case SpriteAuthoringErrorCode::WriteFailed:
        return "write_failed";
    }
    return "invalid_request";
}

bool SpriteRegionMutation::HasChanges() const noexcept
{
    return sourceSize.has_value() || trimOffset.has_value() || trimSize.has_value() ||
        packedRect.has_value() || pivot.has_value();
}

bool SpriteMutation::HasChanges() const noexcept
{
    return sampling.has_value() || (region.has_value() && region->HasChanges());
}

SpriteAuthoringResult MutateSpriteResource(
    const std::filesystem::path& projectRoot,
    const std::string_view projectRelativeReference,
    const SpriteMutation& mutation)
{
    SpriteAuthoringResult result{};
    result.resource = std::string{projectRelativeReference};

    if (!mutation.HasChanges())
    {
        AddDiagnostic(
            result,
            SpriteAuthoringErrorCode::InvalidRequest,
            "$mutation",
            "Sprite authoring requires at least one explicit typed change.");
        return result;
    }
    if (mutation.region.has_value() &&
        (mutation.region->regionId.empty() || !mutation.region->HasChanges()))
    {
        AddDiagnostic(
            result,
            SpriteAuthoringErrorCode::InvalidRequest,
            "region",
            mutation.region->regionId.empty()
                ? "Region mutation requires a non-empty stable region id."
                : "Region mutation requires at least one region field change.");
        return result;
    }

    assets::SpriteAssetCache cache{projectRoot};
    assets::SpriteAssetLoadResult loaded = cache.Load(projectRelativeReference);
    if (!loaded.Succeeded())
    {
        CopyAssetDiagnostics(result, SpriteAuthoringErrorCode::AssetLoadFailed, loaded.diagnostics);
        if (result.diagnostics.empty())
        {
            AddDiagnostic(
                result,
                SpriteAuthoringErrorCode::AssetLoadFailed,
                "$resource",
                "Canonical Sprite resource failed to load.");
        }
        return result;
    }

    result.resource = loaded.asset->id;
    const assets::SpriteAsset original = *loaded.asset;
    assets::SpriteAsset candidate = original;

    std::optional<std::size_t> regionIndex{};
    if (mutation.region.has_value())
    {
        const auto region = std::find_if(
            candidate.regions.begin(),
            candidate.regions.end(),
            [&mutation](const assets::SpriteRegion& value) {
                return value.id == mutation.region->regionId;
            });
        if (region == candidate.regions.end())
        {
            AddDiagnostic(
                result,
                SpriteAuthoringErrorCode::UnknownRegion,
                "regions." + mutation.region->regionId,
                "Sprite authoring region id was not found in the canonical resource.");
            return result;
        }
        regionIndex = static_cast<std::size_t>(std::distance(candidate.regions.begin(), region));

        if (mutation.region->sourceSize.has_value()) region->sourceSize = *mutation.region->sourceSize;
        if (mutation.region->trimOffset.has_value()) region->trimOffset = *mutation.region->trimOffset;
        if (mutation.region->trimSize.has_value()) region->trimSize = *mutation.region->trimSize;
        if (mutation.region->packedRect.has_value()) region->packedRect = *mutation.region->packedRect;
        if (mutation.region->pivot.has_value()) region->pivot = *mutation.region->pivot;
    }
    if (mutation.sampling.has_value())
    {
        candidate.sampling = *mutation.sampling;
    }

    const std::string serialized = assets::SaveSpriteAssetToml(candidate);
    assets::SpriteAssetLoadResult validated = assets::ParseSpriteAssetToml(
        serialized,
        candidate.id,
        (cache.ProjectRoot() / std::filesystem::path{candidate.id}).generic_string());
    if (!validated.Succeeded())
    {
        CopyAssetDiagnostics(result, SpriteAuthoringErrorCode::ValidationFailed, validated.diagnostics);
        if (result.diagnostics.empty())
        {
            AddDiagnostic(
                result,
                SpriteAuthoringErrorCode::ValidationFailed,
                "$resource",
                "Mutated Sprite candidate failed deterministic validation.");
        }
        return result;
    }
    result.validationPassed = true;

    if (mutation.sampling.has_value() && validated.asset->sampling != original.sampling)
    {
        result.changedFields.emplace_back("sampling");
    }
    if (mutation.region.has_value() && regionIndex.has_value())
    {
        const assets::SpriteRegion& before = original.regions[*regionIndex];
        const assets::SpriteRegion& after = validated.asset->regions[*regionIndex];
        const std::string_view regionId = mutation.region->regionId;

        if (mutation.region->sourceSize.has_value() && after.sourceSize != before.sourceSize)
            result.changedFields.push_back(RegionFieldPath(regionId, "source_size"));
        if (mutation.region->trimOffset.has_value() && after.trimOffset != before.trimOffset)
            result.changedFields.push_back(RegionFieldPath(regionId, "trim_offset"));
        if (mutation.region->trimSize.has_value() && after.trimSize != before.trimSize)
            result.changedFields.push_back(RegionFieldPath(regionId, "trim_size"));
        if (mutation.region->packedRect.has_value() && after.packedRect != before.packedRect)
            result.changedFields.push_back(RegionFieldPath(regionId, "packed_rect"));
        if (mutation.region->pivot.has_value() && after.pivot != before.pivot)
            result.changedFields.push_back(RegionFieldPath(regionId, "pivot"));
    }

    if (result.changedFields.empty())
    {
        return result;
    }

    const std::filesystem::path target = cache.ProjectRoot() / std::filesystem::path{result.resource};
    const std::string canonical = assets::SaveSpriteAssetToml(*validated.asset);
    std::string writeError{};
    if (!detail::CommitAuthoringTextFile(target, canonical, "Sprite", writeError))
    {
        AddDiagnostic(result, SpriteAuthoringErrorCode::WriteFailed, "$resource", std::move(writeError));
        return result;
    }

    result.committed = true;
    return result;
}
} // namespace trace2d::agent
