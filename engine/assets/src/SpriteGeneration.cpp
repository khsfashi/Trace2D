#include <trace2d/assets/SpriteGeneration.hpp>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace trace2d::assets
{
namespace
{
using Json = nlohmann::ordered_json;

void AddDiagnostic(
    SpriteGenerationResult& result,
    const SpriteGenerationErrorCode code,
    std::string id,
    std::string message)
{
    result.diagnostics.push_back(SpriteGenerationDiagnostic{
        .code = code,
        .id = std::move(id),
        .message = std::move(message),
    });
}

bool ValidRgba8(
    const std::uint32_t width,
    const std::uint32_t height,
    const std::size_t byteCount) noexcept
{
    if (width == 0U || height == 0U)
    {
        return false;
    }

    const std::uint64_t pixelCount =
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
    const std::uint64_t maximumPixels =
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max() / 4U);
    if (pixelCount > maximumPixels)
    {
        return false;
    }

    return byteCount == static_cast<std::size_t>(pixelCount) * 4U;
}

bool ValidateLoosePlan(
    const SpriteGenerationRequest& request,
    const SpriteGenerationPostProcessPlan& plan,
    SpriteGenerationResult& result)
{
    if (plan.looseFrameTargets.size() != request.expectedFrameCount)
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::InvalidPlan,
            "loose_frame_targets",
            "Loose-frame target count must exactly match expectedFrameCount before provider execution.");
        return false;
    }
    if (plan.looseFrameImport.canonicalAssetId.empty())
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::InvalidPlan,
            "canonical_asset_id",
            "Loose-frame canonical asset ID must not be empty.");
        return false;
    }
    if (plan.looseFrameImport.defaultPivot.denominator <= 0)
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::InvalidPlan,
            "default_pivot",
            "Loose-frame default pivot denominator must be positive.");
        return false;
    }

    for (std::size_t index = 0U; index < plan.looseFrameTargets.size(); ++index)
    {
        const SpriteGenerationLooseFrameTarget& target = plan.looseFrameTargets[index];
        if (target.pageId.empty() || target.regionId.empty() || target.textureReference.empty())
        {
            AddDiagnostic(
                result,
                SpriteGenerationErrorCode::InvalidPlan,
                "loose_frame_targets[" + std::to_string(index) + "]",
                "Loose-frame page ID, region ID and texture reference must all be explicit and non-empty.");
            return false;
        }
        if (target.pivot.has_value() && target.pivot->denominator <= 0)
        {
            AddDiagnostic(
                result,
                SpriteGenerationErrorCode::InvalidPlan,
                std::string{target.regionId},
                "Explicit loose-frame pivot denominator must be positive.");
            return false;
        }

        for (std::size_t previous = 0U; previous < index; ++previous)
        {
            if (plan.looseFrameTargets[previous].regionId == target.regionId)
            {
                AddDiagnostic(
                    result,
                    SpriteGenerationErrorCode::InvalidPlan,
                    std::string{target.regionId},
                    "Loose-frame region IDs must be unique.");
                return false;
            }
            if (plan.looseFrameTargets[previous].pageId == target.pageId)
            {
                AddDiagnostic(
                    result,
                    SpriteGenerationErrorCode::InvalidPlan,
                    std::string{target.pageId},
                    "Loose-frame page IDs must be unique.");
                return false;
            }
        }
    }

    return true;
}

bool ValidateManifestPlan(
    const SpriteGenerationPostProcessPlan& plan,
    SpriteGenerationResult& result)
{
    if (!plan.looseFrameTargets.empty())
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::InvalidPlan,
            "loose_frame_targets",
            "Manifest-atlas orchestration must not also provide loose-frame targets.");
        return false;
    }
    if (plan.manifestImport.canonicalAssetId.empty() ||
        plan.manifestImport.pageId.empty() ||
        plan.manifestImport.textureReference.empty())
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::InvalidPlan,
            "manifest_import",
            "Manifest import requires explicit canonical asset ID, page ID and texture reference.");
        return false;
    }
    return true;
}

bool ValidatePreflight(
    const SpriteGenerationRequest& request,
    const SpriteGenerationPostProcessPlan& plan,
    SpriteGenerationResult& result)
{
    if (request.requestId.empty())
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::InvalidRequest,
            "request_id",
            "Generation request ID must not be empty.");
        return false;
    }
    if (request.prompt.empty())
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::InvalidRequest,
            "prompt",
            "Generation prompt must not be empty.");
        return false;
    }
    if (request.expectedFrameCount == 0U)
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::InvalidRequest,
            "expected_frame_count",
            "Generation expectedFrameCount must be positive.");
        return false;
    }

    switch (plan.expectedCandidateKind)
    {
    case SpriteGenerationCandidateKind::LooseFrames:
        return ValidateLoosePlan(request, plan, result);
    case SpriteGenerationCandidateKind::GeneratorManifestAtlas:
        return ValidateManifestPlan(plan, result);
    }

    AddDiagnostic(
        result,
        SpriteGenerationErrorCode::InvalidPlan,
        "candidate_kind",
        "Unsupported generation candidate kind.");
    return false;
}

bool ValidateProviderEnvelope(
    const SpriteGenerationRequest& request,
    const SpriteGenerationPostProcessPlan& plan,
    const SpriteGenerationProviderResponse& response,
    SpriteGenerationResult& result)
{
    result.evidence.providerId = response.providerId;
    result.evidence.providerRevision = response.providerRevision;
    result.evidence.candidateKind = response.candidateKind;

    if (!response.succeeded)
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::ProviderFailed,
            response.providerId,
            response.errorMessage.empty()
                ? "Generation provider reported failure."
                : response.errorMessage);
        return false;
    }
    if (response.requestId != request.requestId || response.providerId.empty() ||
        response.providerRevision.empty())
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::ProviderIdentityMismatch,
            response.providerId,
            "Successful provider response must echo the request ID and provide non-empty provider ID/revision evidence.");
        return false;
    }
    if (response.candidateKind != plan.expectedCandidateKind)
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::CandidateKindMismatch,
            response.providerId,
            "Provider candidate kind does not match the explicit post-process plan.");
        return false;
    }
    return true;
}

void ProcessLooseFrames(
    const SpriteGenerationRequest& request,
    const SpriteGenerationProviderResponse& response,
    const SpriteGenerationPostProcessPlan& plan,
    SpriteGenerationResult& result)
{
    if (response.manifestAtlas.has_value())
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::InvalidCandidate,
            response.providerId,
            "Loose-frame provider response must not also contain a manifest atlas.");
        return;
    }
    if (response.looseFrames.size() != request.expectedFrameCount)
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::ExpectedFrameCountMismatch,
            response.providerId,
            "Provider loose-frame count must exactly match expectedFrameCount.");
        return;
    }

    result.evidence.candidateFrameCount = request.expectedFrameCount;

    std::vector<SpriteQualityFrameView> qualityViews{};
    qualityViews.reserve(response.looseFrames.size());
    for (std::size_t index = 0U; index < response.looseFrames.size(); ++index)
    {
        const SpriteGeneratedLooseFrame& frame = response.looseFrames[index];
        const SpriteGenerationLooseFrameTarget& target = plan.looseFrameTargets[index];
        if (!ValidRgba8(frame.width, frame.height, frame.rgba8.size()))
        {
            AddDiagnostic(
                result,
                SpriteGenerationErrorCode::InvalidCandidate,
                std::string{target.regionId},
                "Generated loose frame must have positive dimensions and exact RGBA8 byte count.");
            return;
        }

        qualityViews.push_back(SpriteQualityFrameView{
            .id = target.regionId,
            .width = frame.width,
            .height = frame.height,
            .rgba8 = frame.rgba8,
            .pivot = target.pivot,
        });
    }

    SpriteQualityResult quality = AnalyzeAndRepairSpriteQuality(qualityViews, plan.quality);
    if (!quality.Succeeded())
    {
        result.quality = std::move(quality);
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::QualityValidationFailed,
            response.providerId,
            "Generated loose frames failed deterministic SPP2 quality validation/repair.");
        return;
    }

    const bool useRepairedFrames = !quality.repairedFrames.empty();
    if (useRepairedFrames && quality.repairedFrames.size() != response.looseFrames.size())
    {
        result.quality = std::move(quality);
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::QualityValidationFailed,
            response.providerId,
            "SPP2 repaired-frame output did not preserve the expected frame cardinality.");
        return;
    }

    std::vector<SpriteLooseFrameView> importViews{};
    importViews.reserve(response.looseFrames.size());
    for (std::size_t index = 0U; index < response.looseFrames.size(); ++index)
    {
        const SpriteGenerationLooseFrameTarget& target = plan.looseFrameTargets[index];
        if (useRepairedFrames)
        {
            const SpriteQualityRepairedFrame& repaired = quality.repairedFrames[index];
            if (repaired.id != target.regionId ||
                !ValidRgba8(repaired.width, repaired.height, repaired.rgba8.size()))
            {
                result.quality = std::move(quality);
                AddDiagnostic(
                    result,
                    SpriteGenerationErrorCode::QualityValidationFailed,
                    std::string{target.regionId},
                    "SPP2 repaired-frame identity or RGBA8 shape does not match the requested target.");
                return;
            }

            importViews.push_back(SpriteLooseFrameView{
                .pageId = target.pageId,
                .regionId = target.regionId,
                .textureReference = target.textureReference,
                .width = repaired.width,
                .height = repaired.height,
                .rgba8 = repaired.rgba8,
                .pivot = repaired.pivot,
            });
        }
        else
        {
            const SpriteGeneratedLooseFrame& frame = response.looseFrames[index];
            importViews.push_back(SpriteLooseFrameView{
                .pageId = target.pageId,
                .regionId = target.regionId,
                .textureReference = target.textureReference,
                .width = frame.width,
                .height = frame.height,
                .rgba8 = frame.rgba8,
                .pivot = target.pivot,
            });
        }
    }

    SpriteImportResult imported = ImportLooseSpriteFrames(importViews, plan.looseFrameImport);
    result.quality = std::move(quality);
    result.canonicalImport = std::move(imported);
    if (!result.canonicalImport->Succeeded())
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::CanonicalImportFailed,
            response.providerId,
            "Generated loose frames failed deterministic SPP3/S1 canonical import validation.");
    }
}

void ProcessManifestAtlas(
    const SpriteGenerationRequest& request,
    const SpriteGenerationProviderResponse& response,
    const SpriteGenerationPostProcessPlan& plan,
    SpriteGenerationResult& result)
{
    if (!response.looseFrames.empty() || !response.manifestAtlas.has_value())
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::InvalidCandidate,
            response.providerId,
            "Manifest-atlas provider response must contain exactly one manifest atlas and no loose frames.");
        return;
    }

    const SpriteGeneratedManifestAtlas& atlas = *response.manifestAtlas;
    if (atlas.manifestJson.empty() || atlas.sheetId.empty() ||
        !ValidRgba8(atlas.width, atlas.height, atlas.rgba8.size()))
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::InvalidCandidate,
            response.providerId,
            "Generated manifest atlas requires non-empty manifest/sheet identity and exact positive-dimension RGBA8 bytes.");
        return;
    }

    const SpriteImportDecodedImageView sheet{
        .id = atlas.sheetId,
        .width = atlas.width,
        .height = atlas.height,
        .rgba8 = atlas.rgba8,
    };
    SpriteGeneratorImportResult manifestImport = ImportSpriteGeneratorManifestJson(
        atlas.manifestKind,
        atlas.manifestJson,
        sheet,
        plan.manifestImport);
    if (!manifestImport.Succeeded())
    {
        result.manifestImport = std::move(manifestImport);
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::GeneratorManifestImportFailed,
            response.providerId,
            "Generated manifest atlas failed deterministic SPP4/SPP3/S1 validation.");
        return;
    }

    const std::size_t actualFrameCount = manifestImport.canonicalImport.frames.size();
    if (actualFrameCount != request.expectedFrameCount ||
        actualFrameCount > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::ExpectedFrameCountMismatch,
            response.providerId,
            "Validated manifest frame count must exactly match expectedFrameCount before canonical authority is exposed.");
        return;
    }

    result.evidence.candidateFrameCount = static_cast<std::uint32_t>(actualFrameCount);
    result.canonicalImport = manifestImport.canonicalImport;
    result.manifestImport = std::move(manifestImport);
}
} // namespace

std::string_view ToString(const SpriteGenerationCandidateKind value) noexcept
{
    switch (value)
    {
    case SpriteGenerationCandidateKind::LooseFrames:
        return "loose_frames";
    case SpriteGenerationCandidateKind::GeneratorManifestAtlas:
        return "generator_manifest_atlas";
    }
    return "unknown";
}

std::string_view ToString(const SpriteGenerationErrorCode value) noexcept
{
    switch (value)
    {
    case SpriteGenerationErrorCode::InvalidRequest:
        return "invalid_request";
    case SpriteGenerationErrorCode::InvalidPlan:
        return "invalid_plan";
    case SpriteGenerationErrorCode::ProviderFailed:
        return "provider_failed";
    case SpriteGenerationErrorCode::ProviderIdentityMismatch:
        return "provider_identity_mismatch";
    case SpriteGenerationErrorCode::CandidateKindMismatch:
        return "candidate_kind_mismatch";
    case SpriteGenerationErrorCode::ExpectedFrameCountMismatch:
        return "expected_frame_count_mismatch";
    case SpriteGenerationErrorCode::InvalidCandidate:
        return "invalid_candidate";
    case SpriteGenerationErrorCode::QualityValidationFailed:
        return "quality_validation_failed";
    case SpriteGenerationErrorCode::CanonicalImportFailed:
        return "canonical_import_failed";
    case SpriteGenerationErrorCode::GeneratorManifestImportFailed:
        return "generator_manifest_import_failed";
    }
    return "unknown";
}

SpriteGenerationResult GenerateAndValidateSprite(
    const SpriteGenerationRequest& request,
    SpriteGenerationProvider& provider,
    const SpriteGenerationPostProcessPlan& plan)
{
    SpriteGenerationResult result{};
    result.evidence.requestId = std::string{request.requestId};
    result.evidence.candidateKind = plan.expectedCandidateKind;

    if (!ValidatePreflight(request, plan, result))
    {
        return result;
    }

    SpriteGenerationProviderResponse response{};
    result.evidence.providerCallPerformed = true;
    try
    {
        response = provider.Generate(request);
    }
    catch (const std::exception& exception)
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::ProviderFailed,
            "provider",
            std::string{"Generation provider threw an exception: "} + exception.what());
        return result;
    }
    catch (...)
    {
        AddDiagnostic(
            result,
            SpriteGenerationErrorCode::ProviderFailed,
            "provider",
            "Generation provider threw a non-standard exception.");
        return result;
    }

    if (!ValidateProviderEnvelope(request, plan, response, result))
    {
        return result;
    }

    switch (plan.expectedCandidateKind)
    {
    case SpriteGenerationCandidateKind::LooseFrames:
        ProcessLooseFrames(request, response, plan, result);
        break;
    case SpriteGenerationCandidateKind::GeneratorManifestAtlas:
        ProcessManifestAtlas(request, response, plan, result);
        break;
    }

    return result;
}

std::string SerializeSpriteGenerationResultJson(const SpriteGenerationResult& result)
{
    Json root = Json::object();
    root["schema"] = "trace2d.sprite-generation-result.v1";
    root["schema_version"] = result.evidence.schemaVersion;
    root["request_id"] = result.evidence.requestId;
    root["provider_id"] = result.evidence.providerId;
    root["provider_revision"] = result.evidence.providerRevision;
    root["candidate_kind"] = ToString(result.evidence.candidateKind);
    root["provider_call_performed"] = result.evidence.providerCallPerformed;
    root["candidate_frame_count"] = result.evidence.candidateFrameCount;
    root["succeeded"] = result.Succeeded();

    root["quality"] = result.quality.has_value()
        ? Json::parse(SerializeSpriteQualityResultJson(*result.quality))
        : Json{nullptr};
    root["canonical_import"] = result.canonicalImport.has_value()
        ? Json::parse(SerializeSpriteImportResultJson(*result.canonicalImport))
        : Json{nullptr};
    root["manifest_import"] = result.manifestImport.has_value()
        ? Json::parse(SerializeSpriteGeneratorImportResultJson(*result.manifestImport))
        : Json{nullptr};

    root["diagnostics"] = Json::array();
    for (const SpriteGenerationDiagnostic& diagnostic : result.diagnostics)
    {
        root["diagnostics"].push_back(Json{
            {"code", ToString(diagnostic.code)},
            {"id", diagnostic.id},
            {"message", diagnostic.message},
        });
    }

    return root.dump(2);
}
} // namespace trace2d::assets
