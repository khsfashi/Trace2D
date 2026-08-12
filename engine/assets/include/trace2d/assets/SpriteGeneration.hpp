#pragma once

#include <trace2d/assets/SpriteGeneratorInterop.hpp>
#include <trace2d/assets/SpriteQuality.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::assets
{
enum class SpriteGenerationCandidateKind : std::uint8_t
{
    LooseFrames = 0,
    GeneratorManifestAtlas,
};

enum class SpriteGenerationErrorCode : std::uint8_t
{
    InvalidRequest = 0,
    InvalidPlan,
    ProviderFailed,
    ProviderIdentityMismatch,
    CandidateKindMismatch,
    ExpectedFrameCountMismatch,
    InvalidCandidate,
    QualityValidationFailed,
    CanonicalImportFailed,
    GeneratorManifestImportFailed,
};

[[nodiscard]] std::string_view ToString(SpriteGenerationCandidateKind value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteGenerationErrorCode value) noexcept;

struct SpriteGenerationRequest final
{
    std::string_view requestId{};
    std::string_view prompt{};
    std::uint32_t expectedFrameCount{0};
};

struct SpriteGeneratedLooseFrame final
{
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> rgba8{};
};

struct SpriteGeneratedManifestAtlas final
{
    SpriteGeneratorManifestKind manifestKind{SpriteGeneratorManifestKind::SpriteGenComponentRow};
    std::string manifestJson{};
    std::string sheetId{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> rgba8{};
};

struct SpriteGenerationProviderResponse final
{
    std::string requestId{};
    std::string providerId{};
    std::string providerRevision{};
    bool succeeded{false};
    std::string errorMessage{};
    SpriteGenerationCandidateKind candidateKind{SpriteGenerationCandidateKind::LooseFrames};
    std::vector<SpriteGeneratedLooseFrame> looseFrames{};
    std::optional<SpriteGeneratedManifestAtlas> manifestAtlas{};
};

class SpriteGenerationProvider
{
public:
    virtual ~SpriteGenerationProvider() = default;

    [[nodiscard]] virtual SpriteGenerationProviderResponse Generate(
        const SpriteGenerationRequest& request) = 0;
};

struct SpriteGenerationLooseFrameTarget final
{
    std::string_view pageId{};
    std::string_view regionId{};
    std::string_view textureReference{};
    std::optional<SpriteRationalPivot> pivot{};
};

struct SpriteGenerationPostProcessPlan final
{
    SpriteGenerationCandidateKind expectedCandidateKind{SpriteGenerationCandidateKind::LooseFrames};
    std::span<const SpriteGenerationLooseFrameTarget> looseFrameTargets{};
    SpriteLooseFrameImportOptions looseFrameImport{};
    SpriteGeneratorManifestImportOptions manifestImport{};
    SpriteQualityOptions quality{};
};

struct SpriteGenerationDiagnostic final
{
    SpriteGenerationErrorCode code{SpriteGenerationErrorCode::InvalidRequest};
    std::string id{};
    std::string message{};
};

struct SpriteGenerationEvidence final
{
    std::uint32_t schemaVersion{1};
    std::string requestId{};
    std::string providerId{};
    std::string providerRevision{};
    SpriteGenerationCandidateKind candidateKind{SpriteGenerationCandidateKind::LooseFrames};
    bool providerCallPerformed{false};
    std::uint32_t candidateFrameCount{0};
};

struct SpriteGenerationResult final
{
    SpriteGenerationEvidence evidence{};
    std::optional<SpriteQualityResult> quality{};
    std::optional<SpriteImportResult> canonicalImport{};
    std::optional<SpriteGeneratorImportResult> manifestImport{};
    std::vector<SpriteGenerationDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return diagnostics.empty() && canonicalImport.has_value() &&
            canonicalImport->Succeeded();
    }
};

[[nodiscard]] SpriteGenerationResult GenerateAndValidateSprite(
    const SpriteGenerationRequest& request,
    SpriteGenerationProvider& provider,
    const SpriteGenerationPostProcessPlan& plan);

[[nodiscard]] std::string SerializeSpriteGenerationResultJson(
    const SpriteGenerationResult& result);
} // namespace trace2d::assets
