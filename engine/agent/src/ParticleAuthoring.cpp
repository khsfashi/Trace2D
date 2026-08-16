#include <trace2d/agent/ParticleAuthoring.hpp>

#include "AuthoringFileTransaction.hpp"

#include <string>
#include <utility>

namespace trace2d::agent
{
namespace
{
constexpr std::size_t MaximumDiagnostics = 8U;

void AddDiagnostic(
    ParticleAuthoringResult& result,
    const ParticleAuthoringErrorCode code,
    std::string sourceCode,
    std::string path,
    std::string message)
{
    if (result.diagnostics.size() >= MaximumDiagnostics)
    {
        return;
    }
    result.diagnostics.push_back(ParticleAuthoringDiagnostic{
        .code = code,
        .sourceCode = std::move(sourceCode),
        .path = std::move(path),
        .message = std::move(message),
    });
}

void CopyEffectDiagnostics(
    ParticleAuthoringResult& result,
    const ParticleAuthoringErrorCode code,
    const std::vector<particles::ParticleEffectDiagnostic>& diagnostics)
{
    for (const particles::ParticleEffectDiagnostic& diagnostic : diagnostics)
    {
        AddDiagnostic(
            result,
            code,
            std::string{particles::ToString(diagnostic.code)},
            diagnostic.path,
            diagnostic.message);
        if (result.diagnostics.size() >= MaximumDiagnostics)
        {
            break;
        }
    }
}
} // namespace

std::string_view ToString(const ParticleAuthoringErrorCode code) noexcept
{
    switch (code)
    {
    case ParticleAuthoringErrorCode::InvalidRequest:
        return "invalid_request";
    case ParticleAuthoringErrorCode::AssetLoadFailed:
        return "asset_load_failed";
    case ParticleAuthoringErrorCode::ValidationFailed:
        return "validation_failed";
    case ParticleAuthoringErrorCode::WriteFailed:
        return "write_failed";
    }
    return "invalid_request";
}

bool ParticleEffectMutation::HasChanges() const noexcept
{
    return semanticId.has_value() || maxParticles.has_value() || durationFrames.has_value() ||
        loop.has_value() || playOnLoad.has_value() || emissionStartFrame.has_value() ||
        emissionCount.has_value() || emissionEveryFrames.has_value() || lifetimeFrames.has_value();
}

ParticleAuthoringResult MutateParticleEffectResource(
    const std::filesystem::path& projectRoot,
    const std::string_view projectRelativeReference,
    const ParticleEffectMutation& mutation,
    const particles::ParticleReferenceLimits& limits)
{
    ParticleAuthoringResult result{};
    result.resource = std::string{projectRelativeReference};

    if (!mutation.HasChanges())
    {
        AddDiagnostic(
            result,
            ParticleAuthoringErrorCode::InvalidRequest,
            {},
            "$mutation",
            "Particle authoring requires at least one explicit typed change.");
        return result;
    }
    if (mutation.semanticId.has_value() && mutation.semanticId->empty())
    {
        AddDiagnostic(
            result,
            ParticleAuthoringErrorCode::InvalidRequest,
            {},
            "effect.id",
            "Particle effect id must not be empty.");
        return result;
    }

    // Load the current canonical resource under the production authority's normal limits.
    // The caller-provided limits describe the desired post-mutation budget and are applied
    // to the candidate below, so an over-budget-but-production-valid resource can be repaired.
    particles::ParticleEffectCache cache{projectRoot};
    particles::ParticleEffectLoadResult loaded = cache.Load(projectRelativeReference);
    if (!loaded.Succeeded())
    {
        CopyEffectDiagnostics(result, ParticleAuthoringErrorCode::AssetLoadFailed, loaded.diagnostics);
        if (result.diagnostics.empty())
        {
            AddDiagnostic(
                result,
                ParticleAuthoringErrorCode::AssetLoadFailed,
                {},
                "$resource",
                "Canonical Particle effect resource failed to load.");
        }
        return result;
    }

    result.resource = loaded.asset->id;
    const particles::ParticleEffectAsset original = *loaded.asset;
    particles::ParticleEffectAsset candidate = original;

    if (mutation.semanticId.has_value()) candidate.semanticId = *mutation.semanticId;
    if (mutation.maxParticles.has_value()) candidate.definition.maxParticles = *mutation.maxParticles;
    if (mutation.durationFrames.has_value()) candidate.lifecycle.durationFrames = *mutation.durationFrames;
    if (mutation.loop.has_value()) candidate.lifecycle.loop = *mutation.loop;
    if (mutation.playOnLoad.has_value()) candidate.lifecycle.playOnLoad = *mutation.playOnLoad;
    if (mutation.emissionStartFrame.has_value()) candidate.definition.periodicStartFrame = *mutation.emissionStartFrame;
    if (mutation.emissionCount.has_value()) candidate.definition.periodicCount = *mutation.emissionCount;
    if (mutation.emissionEveryFrames.has_value()) candidate.definition.periodicEveryFrames = *mutation.emissionEveryFrames;
    if (mutation.lifetimeFrames.has_value()) candidate.definition.lifetimeFrames = *mutation.lifetimeFrames;

    const std::string serialized = particles::SaveParticleEffectToml(candidate);
    particles::ParticleEffectLoadResult validated = particles::ParseParticleEffectToml(
        serialized,
        candidate.id,
        limits,
        (cache.ProjectRoot() / std::filesystem::path{candidate.id}).generic_string());
    if (!validated.Succeeded())
    {
        CopyEffectDiagnostics(result, ParticleAuthoringErrorCode::ValidationFailed, validated.diagnostics);
        if (result.diagnostics.empty())
        {
            AddDiagnostic(
                result,
                ParticleAuthoringErrorCode::ValidationFailed,
                {},
                "$resource",
                "Mutated Particle effect candidate failed deterministic validation.");
        }
        return result;
    }
    result.validationPassed = true;

    const particles::ParticleEffectAsset& after = *validated.asset;
    if (mutation.semanticId.has_value() && after.semanticId != original.semanticId)
        result.changedFields.emplace_back("effect.id");
    if (mutation.maxParticles.has_value() && after.definition.maxParticles != original.definition.maxParticles)
        result.changedFields.emplace_back("effect.max_particles");
    if (mutation.durationFrames.has_value() && after.lifecycle.durationFrames != original.lifecycle.durationFrames)
        result.changedFields.emplace_back("effect.duration_frames");
    if (mutation.loop.has_value() && after.lifecycle.loop != original.lifecycle.loop)
        result.changedFields.emplace_back("effect.loop");
    if (mutation.playOnLoad.has_value() && after.lifecycle.playOnLoad != original.lifecycle.playOnLoad)
        result.changedFields.emplace_back("effect.play_on_load");
    if (mutation.emissionStartFrame.has_value() &&
        after.definition.periodicStartFrame != original.definition.periodicStartFrame)
        result.changedFields.emplace_back("emission.start_frame");
    if (mutation.emissionCount.has_value() &&
        after.definition.periodicCount != original.definition.periodicCount)
        result.changedFields.emplace_back("emission.count");
    if (mutation.emissionEveryFrames.has_value() &&
        after.definition.periodicEveryFrames != original.definition.periodicEveryFrames)
        result.changedFields.emplace_back("emission.every_frames");
    if (mutation.lifetimeFrames.has_value() &&
        after.definition.lifetimeFrames != original.definition.lifetimeFrames)
        result.changedFields.emplace_back("lifetime.frames");

    if (result.changedFields.empty())
    {
        return result;
    }

    const std::filesystem::path target = cache.ProjectRoot() / std::filesystem::path{result.resource};
    const std::string canonical = particles::SaveParticleEffectToml(after);
    std::string writeError{};
    if (!detail::CommitAuthoringTextFile(target, canonical, "Particle", writeError))
    {
        AddDiagnostic(
            result,
            ParticleAuthoringErrorCode::WriteFailed,
            {},
            "$resource",
            std::move(writeError));
        return result;
    }

    result.committed = true;
    return result;
}
} // namespace trace2d::agent
