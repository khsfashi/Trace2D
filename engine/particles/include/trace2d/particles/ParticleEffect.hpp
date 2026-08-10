#pragma once

#include <trace2d/particles/ParticleReference.hpp>
#include <trace2d/scene/Scene.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace trace2d::particles
{
enum class ParticleEffectBackend : std::uint8_t
{
    Cpu = 0,
    Gpu = 1,
};

enum class ParticleBlendMode : std::uint8_t
{
    Alpha = 0,
    Additive = 1,
};

struct ParticleEffectLifecycle final
{
    std::uint32_t durationFrames{1};
    bool loop{false};
    bool playOnLoad{true};

    [[nodiscard]] bool operator==(const ParticleEffectLifecycle&) const noexcept = default;
};

struct ParticleEffectAsset final
{
    std::string id{};
    std::string semanticId{};
    ParticleEffectBackend backend{ParticleEffectBackend::Cpu};
    ParticleEffectLifecycle lifecycle{};
    ParticleReferenceDefinition definition{};
    std::vector<ParticleBurst> bursts{};
    std::vector<std::string> spriteReferences{};
    ParticleBlendMode blendMode{ParticleBlendMode::Alpha};
};

enum class ParticleEffectErrorCode : std::uint8_t
{
    InvalidReference = 0,
    UnsupportedFormat,
    MissingFile,
    ReadFailure,
    ParseError,
    SchemaError,
    CapacityExceedsLimit,
};

[[nodiscard]] std::string_view ToString(ParticleEffectErrorCode code) noexcept;

struct ParticleEffectDiagnostic final
{
    ParticleEffectErrorCode code{ParticleEffectErrorCode::SchemaError};
    std::string reference{};
    std::string resolvedPath{};
    std::string path{};
    std::string message{};
    std::size_t line{0};
    std::size_t column{0};
};

struct ParticleEffectLoadResult final
{
    std::shared_ptr<const ParticleEffectAsset> asset{};
    std::vector<ParticleEffectDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return asset != nullptr && diagnostics.empty();
    }
};

struct ParticleEffectCacheMetrics final
{
    std::uint64_t requests{0};
    std::uint64_t cacheHits{0};
    std::uint64_t cacheMisses{0};
    std::uint64_t successfulImports{0};
    std::uint64_t failedImports{0};
    std::size_t cachedAssets{0};
};

[[nodiscard]] ParticleEffectLoadResult ParseParticleEffectToml(
    std::string_view text,
    std::string_view canonicalAssetId,
    const ParticleReferenceLimits& limits = {},
    std::string_view sourceName = {});

[[nodiscard]] std::string SaveParticleEffectToml(const ParticleEffectAsset& asset);

class ParticleEffectCache final
{
public:
    explicit ParticleEffectCache(
        std::filesystem::path projectRoot,
        ParticleReferenceLimits limits = {});

    ParticleEffectCache(const ParticleEffectCache&) = delete;
    ParticleEffectCache& operator=(const ParticleEffectCache&) = delete;
    ParticleEffectCache(ParticleEffectCache&&) noexcept = default;
    ParticleEffectCache& operator=(ParticleEffectCache&&) noexcept = default;
    ~ParticleEffectCache() = default;

    [[nodiscard]] ParticleEffectLoadResult Load(std::string_view projectRelativeReference);
    [[nodiscard]] bool Invalidate(std::string_view projectRelativeReference);
    void Clear() noexcept;

    [[nodiscard]] const std::filesystem::path& ProjectRoot() const noexcept;
    [[nodiscard]] const ParticleReferenceLimits& Limits() const noexcept;
    [[nodiscard]] ParticleEffectCacheMetrics Metrics() const noexcept;

private:
    std::filesystem::path projectRoot_{};
    ParticleReferenceLimits limits_{};
    std::unordered_map<std::string, std::shared_ptr<const ParticleEffectAsset>> cache_{};
    std::uint64_t requests_{0};
    std::uint64_t cacheHits_{0};
    std::uint64_t cacheMisses_{0};
    std::uint64_t successfulImports_{0};
    std::uint64_t failedImports_{0};
};

struct ParticleEmitter2DSceneReference final
{
    std::string entityId{};
    std::string effectReference{};
    ParticleEmitterStableId stableId{0};

    [[nodiscard]] bool operator==(const ParticleEmitter2DSceneReference&) const noexcept = default;
};

struct ParticleSceneLoadResult final
{
    std::optional<scene::Scene> scene{};
    std::vector<ParticleEmitter2DSceneReference> emitters{};
    std::vector<ParticleEffectDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return scene.has_value() && diagnostics.empty();
    }
};

[[nodiscard]] ParticleSceneLoadResult LoadParticleSceneToml(
    std::string_view text,
    std::string_view sourceName = {});

enum class ParticleEmitter2DError : std::uint8_t
{
    None = 0,
    MissingEffect,
    BackendUnavailable,
    ReferencePrepareFailed,
};

struct ParticleEmitter2DPrepareResult final
{
    ParticleEmitter2DError error{ParticleEmitter2DError::None};
    ParticleReferenceError referenceError{ParticleReferenceError::None};

    [[nodiscard]] bool Ok() const noexcept
    {
        return error == ParticleEmitter2DError::None;
    }
};

class ParticleEmitter2D final
{
public:
    ParticleEmitter2D() noexcept = default;
    ~ParticleEmitter2D() = default;

    ParticleEmitter2D(const ParticleEmitter2D&) = delete;
    ParticleEmitter2D& operator=(const ParticleEmitter2D&) = delete;
    ParticleEmitter2D(ParticleEmitter2D&&) noexcept = default;
    ParticleEmitter2D& operator=(ParticleEmitter2D&&) noexcept = default;

    [[nodiscard]] ParticleEmitter2DPrepareResult Prepare(
        std::shared_ptr<const ParticleEffectAsset> effect,
        std::uint64_t globalSeed,
        ParticleEmitterStableId stableId,
        const ParticleReferenceLimits& limits = {}) noexcept;

    void Reset() noexcept;
    void Play() noexcept;
    void Restart() noexcept;
    void Stop() noexcept;
    [[nodiscard]] bool Step() noexcept;

    [[nodiscard]] bool IsPrepared() const noexcept;
    [[nodiscard]] bool IsPlaying() const noexcept;
    [[nodiscard]] std::uint32_t CycleFrame() const noexcept;
    [[nodiscard]] std::uint64_t CompletedLoops() const noexcept;
    [[nodiscard]] const std::shared_ptr<const ParticleEffectAsset>& Effect() const noexcept;
    [[nodiscard]] ParticleReferenceEmitter& Reference() noexcept;
    [[nodiscard]] const ParticleReferenceEmitter& Reference() const noexcept;

private:
    std::shared_ptr<const ParticleEffectAsset> effect_{};
    ParticleReferenceEmitter reference_{};
    std::uint32_t cycleFrame_{0};
    std::uint64_t completedLoops_{0};
    bool playing_{false};
    bool resetBeforeNextStep_{false};
};
} // namespace trace2d::particles
