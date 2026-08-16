#include <trace2d/agent/SpriteAuthoring.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace trace2d::agent
{
namespace
{
constexpr std::size_t MaximumDiagnostics = 8U;
constexpr std::uint32_t MaximumTemporaryPathAttempts = 8U;
std::atomic<std::uint64_t> TemporarySerial{0U};

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

std::uint64_t ProcessId() noexcept
{
#ifdef _WIN32
    return static_cast<std::uint64_t>(::GetCurrentProcessId());
#else
    return static_cast<std::uint64_t>(::getpid());
#endif
}

std::optional<std::filesystem::path> MakeTemporaryPath(
    const std::filesystem::path& target,
    std::string& errorMessage)
{
    for (std::uint32_t attempt = 0U; attempt < MaximumTemporaryPathAttempts; ++attempt)
    {
        std::filesystem::path candidate = target;
        candidate += ".trace2d-authoring." + std::to_string(ProcessId()) + "." +
            std::to_string(TemporarySerial.fetch_add(1U, std::memory_order_relaxed)) + ".tmp";

        std::error_code error{};
        const bool exists = std::filesystem::exists(candidate, error);
        if (error)
        {
            errorMessage = "Unable to inspect temporary Sprite authoring path: " + error.message();
            return std::nullopt;
        }
        if (!exists)
        {
            return candidate;
        }
    }

    errorMessage = "Unable to reserve a unique sibling temporary path for Sprite authoring.";
    return std::nullopt;
}

bool WriteTemporaryFile(
    const std::filesystem::path& path,
    const std::string_view contents,
    std::string& errorMessage)
{
    std::ofstream output{path, std::ios::binary | std::ios::trunc};
    if (!output)
    {
        errorMessage = "Unable to open temporary Sprite resource for writing.";
        return false;
    }

    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.flush();
    if (!output)
    {
        errorMessage = "Unable to write temporary Sprite resource completely.";
        output.close();
        return false;
    }

    output.close();
    if (!output)
    {
        errorMessage = "Unable to close temporary Sprite resource after writing.";
        return false;
    }
    return true;
}

bool ReplaceFileAtomically(
    const std::filesystem::path& source,
    const std::filesystem::path& target,
    std::string& errorMessage)
{
#ifdef _WIN32
    if (::MoveFileExW(
            source.c_str(),
            target.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0)
    {
        errorMessage = "Unable to atomically replace Sprite resource: " +
            std::system_category().message(static_cast<int>(::GetLastError()));
        return false;
    }
    return true;
#else
    std::error_code error{};
    std::filesystem::rename(source, target, error);
    if (error)
    {
        errorMessage = "Unable to atomically replace Sprite resource: " + error.message();
        return false;
    }
    return true;
#endif
}

void RemoveTemporaryFile(const std::filesystem::path& path) noexcept
{
    std::error_code ignored{};
    static_cast<void>(std::filesystem::remove(path, ignored));
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
    std::string writeError{};
    const std::optional<std::filesystem::path> temporary = MakeTemporaryPath(target, writeError);
    if (!temporary.has_value())
    {
        AddDiagnostic(result, SpriteAuthoringErrorCode::WriteFailed, "$resource", std::move(writeError));
        return result;
    }

    const std::string canonical = assets::SaveSpriteAssetToml(*validated.asset);
    if (!WriteTemporaryFile(*temporary, canonical, writeError))
    {
        RemoveTemporaryFile(*temporary);
        AddDiagnostic(result, SpriteAuthoringErrorCode::WriteFailed, "$resource", std::move(writeError));
        return result;
    }
    if (!ReplaceFileAtomically(*temporary, target, writeError))
    {
        RemoveTemporaryFile(*temporary);
        AddDiagnostic(result, SpriteAuthoringErrorCode::WriteFailed, "$resource", std::move(writeError));
        return result;
    }

    result.committed = true;
    return result;
}
} // namespace trace2d::agent