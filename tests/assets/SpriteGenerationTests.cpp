#include <trace2d/assets/SpriteGeneration.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace trace2d::assets
{
namespace
{
std::vector<std::uint8_t> MakePixels(
    const std::uint32_t width,
    const std::uint32_t height,
    const std::uint8_t value)
{
    return std::vector<std::uint8_t>(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U,
        value);
}

class FakeGenerationProvider final : public SpriteGenerationProvider
{
public:
    explicit FakeGenerationProvider(SpriteGenerationProviderResponse response)
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
    std::uint32_t calls_{0};
};

class ThrowingGenerationProvider final : public SpriteGenerationProvider
{
public:
    SpriteGenerationProviderResponse Generate(const SpriteGenerationRequest&) override
    {
        ++calls_;
        throw std::runtime_error{"offline provider failure"};
    }

    [[nodiscard]] std::uint32_t Calls() const noexcept
    {
        return calls_;
    }

private:
    std::uint32_t calls_{0};
};

SpriteGenerationProviderResponse MakeLooseResponse()
{
    SpriteGenerationProviderResponse response{};
    response.requestId = "req-hero-idle";
    response.providerId = "recorded-provider";
    response.providerRevision = "fixture-v1";
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

std::array<SpriteGenerationLooseFrameTarget, 2> MakeLooseTargets()
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

SpriteGenerationPostProcessPlan MakeLoosePlan(
    const std::span<const SpriteGenerationLooseFrameTarget> targets)
{
    SpriteGenerationPostProcessPlan plan{};
    plan.expectedCandidateKind = SpriteGenerationCandidateKind::LooseFrames;
    plan.looseFrameTargets = targets;
    plan.looseFrameImport = SpriteLooseFrameImportOptions{
        .canonicalAssetId = "sprites/generated-hero.sprite.toml",
        .sampling = SpriteSampling::Nearest,
        .colorSpace = SpriteColorSpace::Srgb,
        .defaultPivot = SpriteRationalPivot{1, 1, 1},
    };
    return plan;
}

std::string PerfectPixelOneFrameManifest()
{
    return R"json({
  "app": "perfectpixel",
  "generator": "perfectpixel/component-lane",
  "schema": "perfectpixel.sprite/2",
  "version": 2,
  "character": "hero",
  "sheet": {
    "image": "generated-atlas.png",
    "width": 2,
    "height": 2,
    "cellWidth": 2,
    "cellHeight": 2
  },
  "animations": {
    "idle": {
      "row": 0,
      "frames": 1,
      "fps": 10,
      "loop": true,
      "durationMs": 100,
      "pivot": {"x":1,"y":1},
      "rects": [
        {"x":0,"y":0,"w":2,"h":2}
      ],
      "trims": [
        {"x":0,"y":0,"w":2,"h":2}
      ]
    }
  }
})json";
}

SpriteGenerationProviderResponse MakeManifestResponse()
{
    SpriteGenerationProviderResponse response{};
    response.requestId = "req-manifest";
    response.providerId = "recorded-perfectpixel-adapter";
    response.providerRevision = "fixture-v2";
    response.succeeded = true;
    response.candidateKind = SpriteGenerationCandidateKind::GeneratorManifestAtlas;
    response.manifestAtlas = SpriteGeneratedManifestAtlas{
        .manifestKind = SpriteGeneratorManifestKind::PerfectPixelV2,
        .manifestJson = PerfectPixelOneFrameManifest(),
        .sheetId = "generated-atlas.png",
        .width = 2U,
        .height = 2U,
        .rgba8 = MakePixels(2U, 2U, 0x60U),
    };
    return response;
}

SpriteGenerationPostProcessPlan MakeManifestPlan()
{
    SpriteGenerationPostProcessPlan plan{};
    plan.expectedCandidateKind = SpriteGenerationCandidateKind::GeneratorManifestAtlas;
    plan.manifestImport = SpriteGeneratorManifestImportOptions{
        .canonicalAssetId = "sprites/generated-manifest.sprite.toml",
        .pageId = "main",
        .textureReference = "textures/generated-atlas.png",
        .sampling = SpriteSampling::Nearest,
        .colorSpace = SpriteColorSpace::Srgb,
        .spriteGenDefaultPivot = std::nullopt,
    };
    return plan;
}
} // namespace

TEST(SpriteGenerationTests, LooseFramesRunProviderOnceThenQualityAndCanonicalImport)
{
    const auto targets = MakeLooseTargets();
    const SpriteGenerationPostProcessPlan plan = MakeLoosePlan(targets);
    const SpriteGenerationRequest request{
        .requestId = "req-hero-idle",
        .prompt = "two-frame idle pixel sprite",
        .expectedFrameCount = 2U,
    };
    FakeGenerationProvider provider{MakeLooseResponse()};

    const SpriteGenerationResult result = GenerateAndValidateSprite(request, provider, plan);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(provider.Calls(), 1U);
    EXPECT_TRUE(result.evidence.providerCallPerformed);
    EXPECT_EQ(result.evidence.providerId, "recorded-provider");
    EXPECT_EQ(result.evidence.providerRevision, "fixture-v1");
    EXPECT_EQ(result.evidence.candidateFrameCount, 2U);
    ASSERT_TRUE(result.quality.has_value());
    ASSERT_TRUE(result.quality->Succeeded());
    ASSERT_TRUE(result.canonicalImport.has_value());
    ASSERT_EQ(result.canonicalImport->asset.pages.size(), 2U);
    ASSERT_EQ(result.canonicalImport->asset.regions.size(), 2U);
    EXPECT_EQ(result.canonicalImport->asset.regions[0].id, "idle/frame-0");
    EXPECT_EQ(result.canonicalImport->asset.regions[1].id, "idle/frame-1");
    EXPECT_EQ(result.canonicalImport->asset.regions[0].pivot, (SpriteRationalPivot{1, 1, 1}));
    EXPECT_FALSE(result.manifestImport.has_value());
}

TEST(SpriteGenerationTests, SameRecordedResponseProducesByteIdenticalStructuralEvidence)
{
    const auto targets = MakeLooseTargets();
    const SpriteGenerationPostProcessPlan plan = MakeLoosePlan(targets);
    const SpriteGenerationRequest request{
        .requestId = "req-hero-idle",
        .prompt = "two-frame idle pixel sprite",
        .expectedFrameCount = 2U,
    };
    const SpriteGenerationProviderResponse recorded = MakeLooseResponse();
    FakeGenerationProvider firstProvider{recorded};
    FakeGenerationProvider secondProvider{recorded};

    const SpriteGenerationResult first = GenerateAndValidateSprite(request, firstProvider, plan);
    const SpriteGenerationResult second = GenerateAndValidateSprite(request, secondProvider, plan);

    ASSERT_TRUE(first.Succeeded());
    ASSERT_TRUE(second.Succeeded());
    EXPECT_EQ(
        SerializeSpriteGenerationResultJson(first),
        SerializeSpriteGenerationResultJson(second));
}

TEST(SpriteGenerationTests, InvalidPlanFailsBeforeAnyProviderExecution)
{
    const auto targets = MakeLooseTargets();
    const std::span<const SpriteGenerationLooseFrameTarget> oneTarget{targets.data(), 1U};
    const SpriteGenerationPostProcessPlan plan = MakeLoosePlan(oneTarget);
    const SpriteGenerationRequest request{
        .requestId = "req-hero-idle",
        .prompt = "two-frame idle pixel sprite",
        .expectedFrameCount = 2U,
    };
    FakeGenerationProvider provider{MakeLooseResponse()};

    const SpriteGenerationResult result = GenerateAndValidateSprite(request, provider, plan);

    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(provider.Calls(), 0U);
    EXPECT_FALSE(result.evidence.providerCallPerformed);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].code, SpriteGenerationErrorCode::InvalidPlan);
    EXPECT_FALSE(result.canonicalImport.has_value());
}

TEST(SpriteGenerationTests, ProviderExceptionsFailTransactionally)
{
    const auto targets = MakeLooseTargets();
    const SpriteGenerationPostProcessPlan plan = MakeLoosePlan(targets);
    const SpriteGenerationRequest request{
        .requestId = "req-hero-idle",
        .prompt = "two-frame idle pixel sprite",
        .expectedFrameCount = 2U,
    };
    ThrowingGenerationProvider provider{};

    const SpriteGenerationResult result = GenerateAndValidateSprite(request, provider, plan);

    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(provider.Calls(), 1U);
    EXPECT_TRUE(result.evidence.providerCallPerformed);
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].code, SpriteGenerationErrorCode::ProviderFailed);
    EXPECT_FALSE(result.quality.has_value());
    EXPECT_FALSE(result.canonicalImport.has_value());
}

TEST(SpriteGenerationTests, ManifestAtlasReusesSpp4AndS1WithoutLiveProviderDependency)
{
    const SpriteGenerationPostProcessPlan plan = MakeManifestPlan();
    const SpriteGenerationRequest request{
        .requestId = "req-manifest",
        .prompt = "one-frame generated hero idle",
        .expectedFrameCount = 1U,
    };
    FakeGenerationProvider provider{MakeManifestResponse()};

    const SpriteGenerationResult result = GenerateAndValidateSprite(request, provider, plan);

    ASSERT_TRUE(result.Succeeded());
    EXPECT_EQ(provider.Calls(), 1U);
    EXPECT_EQ(result.evidence.candidateFrameCount, 1U);
    EXPECT_FALSE(result.quality.has_value());
    ASSERT_TRUE(result.manifestImport.has_value());
    ASSERT_TRUE(result.manifestImport->Succeeded());
    ASSERT_TRUE(result.canonicalImport.has_value());
    ASSERT_EQ(result.canonicalImport->frames.size(), 1U);
    ASSERT_EQ(result.canonicalImport->asset.regions.size(), 1U);
    EXPECT_EQ(result.canonicalImport->asset.regions[0].id, "idle/frame-0");
}

TEST(SpriteGenerationTests, ManifestExpectedCountMismatchDoesNotExposeCanonicalAuthority)
{
    const SpriteGenerationPostProcessPlan plan = MakeManifestPlan();
    const SpriteGenerationRequest request{
        .requestId = "req-manifest",
        .prompt = "two frames requested but provider returned one",
        .expectedFrameCount = 2U,
    };
    FakeGenerationProvider provider{MakeManifestResponse()};

    const SpriteGenerationResult result = GenerateAndValidateSprite(request, provider, plan);

    ASSERT_FALSE(result.Succeeded());
    EXPECT_EQ(provider.Calls(), 1U);
    EXPECT_FALSE(result.canonicalImport.has_value());
    EXPECT_FALSE(result.manifestImport.has_value());
    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(
        result.diagnostics[0].code,
        SpriteGenerationErrorCode::ExpectedFrameCountMismatch);
}
} // namespace trace2d::assets
