#include <trace2d/agent/Inspection.hpp>
#include <trace2d/agent/WorkResult.hpp>
#include <trace2d/agent/WorkSpec.hpp>
#include <trace2d/assets/SpriteGeneration.hpp>
#include <trace2d/render/Capture.hpp>
#include <trace2d/render/SpriteRenderContract.hpp>
#include <trace2d/runtime/SpriteAnimator2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
using namespace std::chrono_literals;
using trace2d::agent::AgentFacade;
using trace2d::agent::SpriteAnimationAction;
using trace2d::agent::SpriteAnimationActionKind;
using trace2d::agent::SpriteAnimationAssertion;
using trace2d::agent::SpriteAnimationAssertionField;
using trace2d::agent::SpriteAnimationValue;
using trace2d::agent::SpriteAnimatorBinding;
using trace2d::assets::SpriteColorSpace;
using trace2d::assets::SpriteGeneratedLooseFrame;
using trace2d::assets::SpriteGenerationCandidateKind;
using trace2d::assets::SpriteGenerationLooseFrameTarget;
using trace2d::assets::SpriteGenerationPostProcessPlan;
using trace2d::assets::SpriteGenerationProvider;
using trace2d::assets::SpriteGenerationProviderResponse;
using trace2d::assets::SpriteGenerationRequest;
using trace2d::assets::SpriteGenerationResult;
using trace2d::assets::SpriteLooseFrameImportOptions;
using trace2d::assets::SpriteRationalPivot;
using trace2d::assets::SpriteSampling;
using trace2d::render::CaptureRequest;
using trace2d::render::CapturedFrame;
using trace2d::render::ResolvedSpriteRegion;
using trace2d::render::SpriteRenderContractData;
using trace2d::runtime::MakeSpriteAnimator2DState;
using trace2d::runtime::SpriteAnimationClip2D;
using trace2d::runtime::SpriteAnimationDirection;
using trace2d::runtime::SpriteAnimationFrame2D;
using trace2d::runtime::SpriteAnimationLoopMode;
using trace2d::runtime::SpriteAnimationPlaybackState;
using trace2d::runtime::SpriteAnimationRegionSelection2D;
using trace2d::runtime::SpriteAnimator2D;
using trace2d::runtime::SpriteAnimator2DState;

std::vector<std::uint8_t> MakePixels(
    const std::uint32_t width,
    const std::uint32_t height,
    const std::uint8_t value)
{
    return std::vector<std::uint8_t>(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U,
        value);
}

class RecordedGenerationProvider final : public SpriteGenerationProvider
{
public:
    explicit RecordedGenerationProvider(SpriteGenerationProviderResponse response)
        : response_(std::move(response))
    {
    }

    SpriteGenerationProviderResponse Generate(const SpriteGenerationRequest&) override
    {
        ++calls_;
        return response_;
    }

    [[nodiscard]] std::uint32_t Calls() const noexcept
    {
        return calls_;
    }

private:
    SpriteGenerationProviderResponse response_{};
    std::uint32_t calls_{0U};
};

SpriteGenerationProviderResponse MakeRecordedLooseResponse()
{
    SpriteGenerationProviderResponse response{};
    response.requestId = "req-se2e-hero-idle";
    response.providerId = "recorded-provider";
    response.providerRevision = "se2e-fixture-v1";
    response.succeeded = true;
    response.candidateKind = SpriteGenerationCandidateKind::LooseFrames;
    response.looseFrames.push_back(SpriteGeneratedLooseFrame{
        .width = 2U,
        .height = 2U,
        .rgba8 = MakePixels(2U, 2U, 0x20U),
    });
    response.looseFrames.push_back(SpriteGeneratedLooseFrame{
        .width = 2U,
        .height = 2U,
        .rgba8 = MakePixels(2U, 2U, 0x80U),
    });
    return response;
}

std::array<SpriteGenerationLooseFrameTarget, 2> MakeTargets()
{
    return std::array<SpriteGenerationLooseFrameTarget, 2>{
        SpriteGenerationLooseFrameTarget{
            .pageId = "idle-0-page",
            .regionId = "idle/frame-0",
            .textureReference = "textures/generated-idle-0.png",
            .pivot = SpriteRationalPivot{1, 1, 1},
        },
        SpriteGenerationLooseFrameTarget{
            .pageId = "idle-1-page",
            .regionId = "idle/frame-1",
            .textureReference = "textures/generated-idle-1.png",
            .pivot = SpriteRationalPivot{1, 1, 1},
        },
    };
}

SpriteGenerationPostProcessPlan MakePlan(
    const std::span<const SpriteGenerationLooseFrameTarget> targets)
{
    SpriteGenerationPostProcessPlan plan{};
    plan.expectedCandidateKind = SpriteGenerationCandidateKind::LooseFrames;
    plan.looseFrameTargets = targets;
    plan.looseFrameImport = SpriteLooseFrameImportOptions{
        .canonicalAssetId = "sprites/se2e-generated-hero.sprite.toml",
        .sampling = SpriteSampling::Nearest,
        .colorSpace = SpriteColorSpace::Srgb,
        .defaultPivot = SpriteRationalPivot{1, 1, 1},
    };
    return plan;
}

SpriteGenerationRequest MakeRequest()
{
    return SpriteGenerationRequest{
        .requestId = "req-se2e-hero-idle",
        .prompt = "two-frame idle pixel sprite",
        .expectedFrameCount = 2U,
    };
}

class ScopedArtifact final
{
public:
    explicit ScopedArtifact(std::filesystem::path path)
        : path_(std::move(path))
    {
        Remove();
    }

    ScopedArtifact(const ScopedArtifact&) = delete;
    ScopedArtifact& operator=(const ScopedArtifact&) = delete;

    ~ScopedArtifact()
    {
        Remove();
    }

    [[nodiscard]] const std::filesystem::path& Path() const noexcept
    {
        return path_;
    }

private:
    void Remove() const noexcept
    {
        std::error_code error{};
        std::filesystem::remove(path_, error);
    }

    std::filesystem::path path_{};
};

constexpr std::string_view kSe2eWorkSpec = R"toml(
format_version = 1

[work]
id = "sprite-se2e"
intent = "Prove the composed Sprite generation-to-review contract."
state = "implemented"
constraints = []

[[deliverables]]
id = "sprite-flow"
description = "Generated Sprite reaches exact-frame renderer-facing evidence."
state = "implemented"

[[acceptance]]
id = "deterministic-flow"
deliverable = "sprite-flow"
description = "Generation, canonical import, animation, Agent inspection, and render selection agree."
verification = "deterministic"
state = "implemented"

[[acceptance]]
id = "visual-motion-review"
deliverable = "sprite-flow"
description = "Perceptual visual and motion quality is reviewed from capture evidence."
verification = "human"
state = "review_needed"

[[external_truth]]
id = "visual-motion-approval"
kind = "human_approval"
description = "Final perceptual judgment remains live review state."
)toml";

constexpr std::string_view kSe2eWorkResult = R"toml(
format_version = 1

[result]
work_id = "sprite-se2e"

[[revisions]]
id = "se2e-fixture-v1"
changed_paths = ["tests/e2e/SpriteEndToEndTests.cpp"]
limitations = ["Fixture capture proves artifact/frame handoff only; SR8 remains real-GPU presentation authority."]

[[revisions.verification]]
acceptance = "deterministic-flow"
verification = "deterministic"
outcome = "passed"
summary = "Recorded generation, canonical asset identity, exact animation region, Agent assertion, and SR0 render selection agree."
evidence = ["trace2d_sprite_se2e_generation.json", "trace2d_sprite_se2e_frame_1.bmp"]

[[revisions.verification]]
acceptance = "visual-motion-review"
verification = "human"
outcome = "review_needed"
summary = "Capture is available for perceptual review; fixture pixels are not promoted to GPU truth."
evidence = ["trace2d_sprite_se2e_frame_1.bmp"]

[[revisions.artifacts]]
id = "generation-evidence"
kind = "sprite_generation_json"
path = "trace2d_sprite_se2e_generation.json"
description = "SPP5 request/provider/canonical-import evidence"

[[revisions.artifacts]]
id = "frame-1-capture"
kind = "capture_fixture"
path = "trace2d_sprite_se2e_frame_1.bmp"
description = "Exact simulation-frame artifact handoff; SR8 owns real-GPU truth"
)toml";

TEST(SpriteEndToEndTests, RecordedGenerationReachesExactFrameRenderSelectionAndReviewEvidence)
{
    const auto targets = MakeTargets();
    const SpriteGenerationPostProcessPlan plan = MakePlan(targets);
    RecordedGenerationProvider provider{MakeRecordedLooseResponse()};

    const SpriteGenerationResult generation =
        trace2d::assets::GenerateAndValidateSprite(MakeRequest(), provider, plan);

    ASSERT_TRUE(generation.Succeeded());
    EXPECT_EQ(provider.Calls(), 1U);
    ASSERT_TRUE(generation.canonicalImport.has_value());
    auto& asset = generation.canonicalImport->asset;
    ASSERT_EQ(asset.pages.size(), 2U);
    ASSERT_EQ(asset.regions.size(), 2U);
    EXPECT_EQ(asset.id, "sprites/se2e-generated-hero.sprite.toml");
    EXPECT_EQ(asset.regions[1].id, "idle/frame-1");
    EXPECT_EQ(asset.regions[1].pageId, asset.pages[1].id);

    const std::array frames{
        SpriteAnimationFrame2D{0U, 100ns},
        SpriteAnimationFrame2D{1U, 100ns},
    };
    SpriteAnimationClip2D clip{};
    ASSERT_TRUE(SpriteAnimationClip2D::Prepare(
                    &asset,
                    static_cast<std::uint32_t>(asset.regions.size()),
                    frames,
                    clip)
                    .Succeeded());

    SpriteAnimator2DState state{};
    ASSERT_TRUE(MakeSpriteAnimator2DState(
                    clip,
                    0ns,
                    SpriteAnimationPlaybackState::Playing,
                    SpriteAnimationLoopMode::Once,
                    SpriteAnimationDirection::Forward,
                    false,
                    {1U, 1U},
                    state)
                    .Succeeded());

    SpriteAnimator2D animator{};
    ASSERT_TRUE(animator.RestoreState(state).Succeeded());
    AgentFacade agent{};
    const SpriteAnimatorBinding binding{"hero", &animator};

    const auto advance = agent.ActOnSpriteAnimator(
        binding,
        SpriteAnimationAction{
            .kind = SpriteAnimationActionKind::Advance,
            .time = 100ns,
            .emissionCapacity = 1U,
        });
    ASSERT_TRUE(advance.Succeeded());
    ASSERT_TRUE(advance.snapshot.has_value());
    EXPECT_EQ(advance.snapshot->timeNanoseconds, 100);
    EXPECT_EQ(advance.snapshot->frameIndex, 1U);
    EXPECT_EQ(advance.snapshot->regionIndex, 1U);

    const auto assertion = agent.AssertSpriteAnimator(
        binding,
        SpriteAnimationAssertion{
            .field = SpriteAnimationAssertionField::RegionIndex,
            .expected = SpriteAnimationValue::Unsigned(1U),
        });
    ASSERT_TRUE(assertion.Succeeded());
    ASSERT_TRUE(assertion.observed.has_value());
    EXPECT_EQ(*assertion.observed, SpriteAnimationValue::Unsigned(1U));

    SpriteAnimationRegionSelection2D animationSelection{};
    ASSERT_TRUE(animator.TryGetCurrentRegion(animationSelection));
    ASSERT_EQ(animationSelection.asset, &asset);
    ASSERT_EQ(animationSelection.regionIndex, 1U);

    ResolvedSpriteRegion resolved{};
    ASSERT_TRUE(trace2d::render::ResolveSpriteRegionByIndices(
                    animationSelection.asset,
                    1U,
                    animationSelection.regionIndex,
                    resolved)
                    .Succeeded());

    SpriteRenderContractData renderData{};
    ASSERT_TRUE(trace2d::render::ExtractSpriteRenderContract(resolved, renderData).Succeeded());
    ASSERT_EQ(renderData.asset, animationSelection.asset);
    ASSERT_EQ(renderData.regionIndex, animationSelection.regionIndex);
    ASSERT_NE(renderData.region, nullptr);
    EXPECT_EQ(renderData.region->id, "idle/frame-1");
    EXPECT_EQ(renderData.pageIndex, 1U);
    EXPECT_EQ(renderData.pageResource.textureReference, "textures/generated-idle-1.png");

    const ScopedArtifact generationArtifact{
        std::filesystem::current_path() / "trace2d_sprite_se2e_generation.json"};
    {
        std::ofstream stream{generationArtifact.Path(), std::ios::binary | std::ios::trunc};
        ASSERT_TRUE(stream.is_open());
        stream << trace2d::assets::SerializeSpriteGenerationResultJson(generation);
        ASSERT_TRUE(stream.good());
    }
    EXPECT_GT(std::filesystem::file_size(generationArtifact.Path()), 0U);

    const ScopedArtifact captureArtifact{
        std::filesystem::current_path() / "trace2d_sprite_se2e_frame_1.bmp"};
    CaptureRequest captureRequest{};
    captureRequest.simulationFrame = 1U;
    captureRequest.artifactPath = captureArtifact.Path();

    CapturedFrame capturedFrame{};
    capturedFrame.simulationFrame = 1U;
    capturedFrame.width = 2U;
    capturedFrame.height = 2U;
    capturedFrame.rgba8Pixels = MakePixels(2U, 2U, 0x80U);
    trace2d::render::WriteCaptureArtifact(captureRequest, capturedFrame);

    ASSERT_TRUE(std::filesystem::exists(captureArtifact.Path()));
    EXPECT_GT(std::filesystem::file_size(captureArtifact.Path()), 54U);

    const auto parsedSpec = trace2d::agent::ParseWorkSpecToml(kSe2eWorkSpec, "se2e-work.toml");
    ASSERT_TRUE(parsedSpec.Succeeded());
    const auto parsedResult = trace2d::agent::ParseWorkResultToml(kSe2eWorkResult, "se2e-result.toml");
    ASSERT_TRUE(parsedResult.Succeeded());
    ASSERT_EQ(parsedResult.result->revisions.size(), 1U);
    ASSERT_EQ(parsedResult.result->revisions[0].artifacts.size(), 2U);
    EXPECT_EQ(
        parsedResult.result->revisions[0].artifacts[1].path,
        captureArtifact.Path().filename().generic_string());

    const auto evaluation = trace2d::agent::EvaluateWorkResult(*parsedSpec.spec, *parsedResult.result);
    EXPECT_EQ(evaluation.state, trace2d::agent::WorkResultState::ReviewNeeded);
    EXPECT_TRUE(evaluation.outstandingAcceptanceIds.empty());
    ASSERT_EQ(evaluation.reviewAcceptanceIds.size(), 1U);
    EXPECT_EQ(evaluation.reviewAcceptanceIds[0], "visual-motion-review");
    EXPECT_TRUE(evaluation.RequiresLiveTruth());
}

TEST(SpriteEndToEndTests, InvalidGenerationPlanExposesNoCanonicalOrDownstreamAuthority)
{
    const auto targets = MakeTargets();
    const std::span<const SpriteGenerationLooseFrameTarget> incompleteTargets{targets.data(), 1U};
    const SpriteGenerationPostProcessPlan invalidPlan = MakePlan(incompleteTargets);
    RecordedGenerationProvider provider{MakeRecordedLooseResponse()};

    const SpriteGenerationResult generation =
        trace2d::assets::GenerateAndValidateSprite(MakeRequest(), provider, invalidPlan);

    ASSERT_FALSE(generation.Succeeded());
    EXPECT_EQ(provider.Calls(), 0U);
    EXPECT_FALSE(generation.evidence.providerCallPerformed);
    EXPECT_FALSE(generation.quality.has_value());
    EXPECT_FALSE(generation.canonicalImport.has_value());
    EXPECT_FALSE(generation.manifestImport.has_value());

    // No canonical SpriteAsset exists, so animation, Agent inspection, render resolution,
    // capture success, and WorkResult success evidence have no authority to expose.
}
} // namespace
