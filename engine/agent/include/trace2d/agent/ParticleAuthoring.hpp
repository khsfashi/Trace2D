#pragma once

#include <trace2d/particles/ParticleEffect.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::agent
{
enum class ParticleAuthoringErrorCode
{
    InvalidRequest,
    AssetLoadFailed,
    ValidationFailed,
    WriteFailed,
};

[[nodiscard]] std::string_view ToString(ParticleAuthoringErrorCode code) noexcept;

struct ParticleEffectMutation final
{
    std::optional<std::string> semanticId{};
    std::optional<std::uint32_t> maxParticles{};
    std::optional<std::uint32_t> durationFrames{};
    std::optional<bool> loop{};
    std::optional<bool> playOnLoad{};
    std::optional<particles::ParticleFrameIndex> emissionStartFrame{};
    std::optional<std::uint32_t> emissionCount{};
    std::optional<std::uint32_t> emissionEveryFrames{};
    std::optional<particles::ParticleUIntRange> lifetimeFrames{};

    [[nodiscard]] bool HasChanges() const noexcept;
};

struct ParticleAuthoringDiagnostic final
{
    ParticleAuthoringErrorCode code = ParticleAuthoringErrorCode::InvalidRequest;
    std::string sourceCode{};
    std::string path{};
    std::string message{};
};

struct ParticleAuthoringResult final
{
    std::string resource{};
    bool committed = false;
    bool validationPassed = false;
    std::uint64_t programFingerprint = 0U;
    std::vector<std::string> changedFields{};
    std::vector<ParticleAuthoringDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return diagnostics.empty();
    }
};

// Applies one bounded typed Particle effect mutation. The existing production parser,
// compiler, and structural budgets remain authoritative. Validation failures never modify
// the resource.
[[nodiscard]] ParticleAuthoringResult MutateParticleEffectResource(
    const std::filesystem::path& projectRoot,
    std::string_view projectRelativeReference,
    const ParticleEffectMutation& mutation,
    const particles::ParticleReferenceLimits& limits = {});
} // namespace trace2d::agent
