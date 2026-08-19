#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/render/SpritePresentation2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <span>
#include <string_view>
#include <utility>

namespace
{
[[nodiscard]] bool GpuSmokeEnabled() noexcept
{
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t valueSize = 0U;
    if (_dupenv_s(&value, &valueSize, "TRACE2D_RUN_GPU_SMOKE") != 0 || value == nullptr)
    {
        return false;
    }
    const bool enabled = std::string_view{value} == "1";
    std::free(value);
    return enabled;
#else
    const char* const value = std::getenv("TRACE2D_RUN_GPU_SMOKE");
    return value != nullptr && std::string_view{value} == "1";
#endif
}

struct Rgba8 final
{
    std::uint8_t red{0U};
    std::uint8_t green{0U};
    std::uint8_t blue{0U};
    std::uint8_t alpha{0U};
};

[[nodiscard]] Rgba8 PixelAt(
    const trace2d::render::CapturedFrame& frame,
    const std::uint32_t x,
    const std::uint32_t y)
{
    const std::size_t offset =
        (static_cast<std::size_t>(y) * frame.width + x) * 4U;
    return Rgba8{
        frame.rgba8Pixels[offset],
        frame.rgba8Pixels[offset + 1U],
        frame.rgba8Pixels[offset + 2U],
        frame.rgba8Pixels[offset + 3U],
    };
}

[[nodiscard]] trace2d::assets::SpriteAsset MakeSingleRegionAsset()
{
    using namespace trace2d::assets;

    SpriteAsset asset{};
    asset.id = "sprites/mat4-acceptance.sprite.toml";
    asset.sampling = SpriteSampling::Nearest;
    asset.pages = {
        SpriteAtlasPage{
            "page",
            "textures/mat4-acceptance.png",
            SpritePixelSize{1U, 1U},
            SpriteColorSpace::Linear,
            SpriteAlphaMode::Straight,
        },
    };
    asset.regions = {
        SpriteRegion{
            "region",
            "page",
            SpritePixelSize{1U, 1U},
            SpritePixelOffset{0U, 0U},
            SpritePixelSize{1U, 1U},
            SpritePixelRect{0U, 0U, 1U, 1U},
            SpriteRationalPivot{0, 0, 1},
            SpritePackedRotation::None,
        },
    };
    return asset;
}

[[nodiscard]] trace2d::render::SpritePresentation2D BuildPresentation(
    const trace2d::assets::SpriteAsset& asset)
{
    using namespace trace2d;

    render::ResolvedSpriteRegion selection{};
    EXPECT_TRUE(render::ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection).Succeeded());

    scene::SpritePose2D pose{};
    pose.transform.position = scene::Vector2{-0.5F, 0.5F};

    render::SpritePresentation2D presentation{};
    EXPECT_TRUE(render::BuildSpritePresentation2D(
        selection,
        pose,
        1.0F,
        render::SpriteAppearance2D{},
        presentation).Succeeded());
    return presentation;
}

[[nodiscard]] trace2d::assets::Shader2DResource MakeFlashShader()
{
    trace2d::assets::Shader2DResource shader{};
    shader.entryPoint = "main";
    shader.canonicalSource = R"(
Texture2D SpriteTexture : register(t0, space2);
SamplerState SpriteSampler : register(s0, space2);

cbuffer MaterialParameters : register(b0, space3)
{
    float4 flashColor;
    float4 flashAmount;
};

struct FragmentInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 sampleBounds : TEXCOORD1;
    float4 tint : TEXCOORD2;
};

float4 main(FragmentInput input) : SV_Target0
{
    const float2 sampleUv = clamp(input.uv, input.sampleBounds.xy, input.sampleBounds.zw);
    const float4 sampledStraight = SpriteTexture.Sample(SpriteSampler, sampleUv);
    const float effectiveAlpha = sampledStraight.a * input.tint.a;
    const float3 basePremultiplied = sampledStraight.rgb * input.tint.rgb * effectiveAlpha;
    const float amount = saturate(flashAmount.x);
    const float3 flashPremultiplied = flashColor.rgb * effectiveAlpha;
    return float4(lerp(basePremultiplied, flashPremultiplied, amount), effectiveAlpha);
}
)";
    return shader;
}

[[nodiscard]] trace2d::assets::Shader2DResource MakeContractMismatchShader()
{
    trace2d::assets::Shader2DResource shader{};
    shader.entryPoint = "main";
    shader.canonicalSource = R"(
struct FragmentInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 sampleBounds : TEXCOORD1;
    float4 tint : TEXCOORD2;
};

float4 main(FragmentInput input) : SV_Target0
{
    return float4(input.uv.x, input.sampleBounds.x, input.tint.r, input.tint.a);
}
)";
    return shader;
}

[[nodiscard]] trace2d::assets::Material2DResource MakeFlashMaterial(
    const trace2d::assets::ResourceHandle<trace2d::assets::Shader2DResource> shader,
    const trace2d::assets::MaterialBlend2D blend =
        trace2d::assets::MaterialBlend2D::Normal)
{
    using namespace trace2d::assets;

    Material2DResource material{};
    material.shader = shader.Untyped();
    material.sampler = MaterialSampler2D::Nearest;
    material.blend = blend;
    material.parameters = {
        MaterialParameterDefault2D{
            "flashColor",
            MaterialParameterValue2D{
                MaterialParameterType2D::Color,
                {1.0F, 0.0F, 0.0F, 1.0F}}},
        MaterialParameterDefault2D{
            "flashAmount",
            MaterialParameterValue2D{
                MaterialParameterType2D::Float,
                {1.0F, 0.0F, 0.0F, 0.0F}}},
    };
    return material;
}

TEST(MaterialGpuSmokeTests, Mat4AcceptanceProvesBatchingRetainedCostsAndCacheSafety)
{
    if (!GpuSmokeEnabled())
    {
        GTEST_SKIP() << "Set TRACE2D_RUN_GPU_SMOKE=1 on a Windows presentation GPU to run MAT4.";
    }

    using namespace trace2d;

    platform::PlatformConfig platformConfig{};
    platformConfig.mode = platform::StartupMode::Windowed;
    platformConfig.windowWidth = 64;
    platformConfig.windowHeight = 64;
    platformConfig.windowTitle = "Trace2D MAT4 GPU acceptance";
    platform::Platform platform{platformConfig};

    render::RendererConfig rendererConfig{};
    rendererConfig.enableDebugValidation = true;
    render::Renderer renderer{rendererConfig, platform};
    assets::ResourceRegistry resources{"project"};

    const auto shader = resources.PublishShader2D(
        "shaders/mat4_flash.shader2d", MakeFlashShader());
    ASSERT_TRUE(shader.Succeeded());
    const auto normalMaterial = resources.PublishMaterial2D(
        "materials/mat4_flash_normal.material2d",
        MakeFlashMaterial(shader.handle));
    ASSERT_TRUE(normalMaterial.Succeeded());
    const auto additiveMaterial = resources.PublishMaterial2D(
        "materials/mat4_flash_additive.material2d",
        MakeFlashMaterial(shader.handle, assets::MaterialBlend2D::Additive));
    ASSERT_TRUE(additiveMaterial.Succeeded());

    const render::MaterialGpuPrepareResult2D normal =
        renderer.PrepareMaterial2D(resources, normalMaterial.handle);
    ASSERT_TRUE(normal.Succeeded()) << normal.diagnostic;
    EXPECT_FALSE(normal.reusedPreparedPipeline);

    const render::MaterialGpuPrepareResult2D normalCached =
        renderer.PrepareMaterial2D(resources, normalMaterial.handle);
    ASSERT_TRUE(normalCached.Succeeded()) << normalCached.diagnostic;
    EXPECT_TRUE(normalCached.reusedPreparedPipeline);
    EXPECT_EQ(normalCached.material.pipelineIdentity, normal.material.pipelineIdentity);

    const render::MaterialGpuPrepareResult2D additive =
        renderer.PrepareMaterial2D(resources, additiveMaterial.handle);
    ASSERT_TRUE(additive.Succeeded()) << additive.diagnostic;
    EXPECT_FALSE(additive.reusedPreparedPipeline);
    EXPECT_NE(additive.material.pipelineIdentity, normal.material.pipelineIdentity);

    const render::RenderMetrics afterPreparation = renderer.Metrics();
    EXPECT_EQ(afterPreparation.materialShaderCompilations, 2U);
    EXPECT_EQ(afterPreparation.materialPipelineCreations, 8U);
    EXPECT_EQ(afterPreparation.materialPipelineCacheHits, 1U);
    EXPECT_EQ(afterPreparation.materialPreparedPipelineBundles, 2U);
    EXPECT_GE(afterPreparation.materialPreparedPipelineBundleCapacity, 2U);

    render::MaterialParameterBinding2D flashAmountBinding{};
    const render::MaterialPrepareStatus2D bindingStatus =
        render::ResolveMaterialParameterBinding2D(
            normal.material.parameterLayout,
            "flashAmount",
            flashAmountBinding);
    ASSERT_TRUE(bindingStatus.Succeeded());

    const std::array<render::ResolvedMaterialParameterOverride2D, 1U> halfOverride{
        render::ResolvedMaterialParameterOverride2D{
            flashAmountBinding,
            render::MaterialFloat2D(0.5F),
        },
    };
    render::MaterialParameterBlock2D halfFlash{};
    const render::MaterialPrepareStatus2D halfStatus =
        render::ApplyMaterialParameterOverrides2D(
            normal.material.defaultParameters,
            halfOverride,
            halfFlash);
    ASSERT_TRUE(halfStatus.Succeeded());
    EXPECT_NE(
        halfFlash.valueIdentity,
        normal.material.defaultParameters.valueIdentity);
    EXPECT_EQ(
        halfFlash.layoutIdentity,
        normal.material.defaultParameters.layoutIdentity);

    constexpr std::array<std::uint8_t, 4U> WhitePixel{255U, 255U, 255U, 255U};
    const render::TextureHandle texture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{1U, 1U, WhitePixel},
        render::SpriteTextureEncoding::Linear);
    const assets::SpriteAsset asset = MakeSingleRegionAsset();
    const render::OrthographicCamera camera{{0.0F, 0.0F}, 2.0F};

    const auto makeCustomDraw = [&](
        const render::PreparedMaterial2D& material,
        const render::MaterialParameterBlock2D& parameters,
        const std::uint64_t stableOrder)
    {
        render::SpritePresentationRenderData draw{};
        draw.presentation = BuildPresentation(asset);
        draw.presentation.appearance.sampler = material.sampler;
        draw.presentation.appearance.blend = material.blend;
        draw.texture = texture;
        draw.order.stableOrder = stableOrder;
        draw.materialPipeline = material.pipelineIdentity;
        draw.materialParameters = &parameters;
        return draw;
    };

    render::SpritePresentationRenderData halfDraw =
        makeCustomDraw(normal.material, halfFlash, 0U);
    const render::RenderMetrics beforeHalf = renderer.Metrics();
    const std::filesystem::path halfOutput =
        std::filesystem::temp_directory_path() / "trace2d_mat4_half_flash.bmp";
    const render::CapturedFrame halfFrame = renderer.CaptureFrame(
        render::CaptureRequest{40U, halfOutput, render::CaptureImageFormat::Bmp},
        camera,
        halfDraw);
    const Rgba8 halfPixel = PixelAt(halfFrame, 32U, 32U);
    EXPECT_GE(halfPixel.red, 248U);
    EXPECT_GE(halfPixel.green, 120U);
    EXPECT_LE(halfPixel.green, 136U);
    EXPECT_GE(halfPixel.blue, 120U);
    EXPECT_LE(halfPixel.blue, 136U);
    EXPECT_GE(halfPixel.alpha, 248U);

    const render::RenderMetrics afterHalf = renderer.Metrics();
    EXPECT_EQ(afterHalf.renderPasses, beforeHalf.renderPasses + 1U);
    EXPECT_EQ(afterHalf.retainedOffscreenColorTargetCount, 1U);
    EXPECT_GT(afterHalf.retainedOffscreenColorTargetBytes, 0U);
    const std::uint64_t retainedColorBytes = afterHalf.retainedOffscreenColorTargetBytes;

    render::SpritePresentationRenderData builtIn{};
    builtIn.presentation = BuildPresentation(asset);
    builtIn.texture = texture;
    builtIn.order.stableOrder = 0U;
    const std::filesystem::path builtInOutput =
        std::filesystem::temp_directory_path() / "trace2d_mat4_builtin.bmp";
    const render::RenderMetrics beforeBuiltIn = renderer.Metrics();
    const render::CapturedFrame builtInFrame = renderer.CaptureFrame(
        render::CaptureRequest{41U, builtInOutput, render::CaptureImageFormat::Bmp},
        camera,
        builtIn);
    const Rgba8 builtInPixel = PixelAt(builtInFrame, 32U, 32U);
    EXPECT_GE(builtInPixel.red, 248U);
    EXPECT_GE(builtInPixel.green, 248U);
    EXPECT_GE(builtInPixel.blue, 248U);
    EXPECT_GE(builtInPixel.alpha, 248U);
    const render::RenderMetrics afterBuiltIn = renderer.Metrics();
    EXPECT_EQ(afterBuiltIn.renderPasses, beforeBuiltIn.renderPasses + 1U);
    EXPECT_EQ(afterBuiltIn.retainedOffscreenColorTargetCount, 1U);
    EXPECT_EQ(afterBuiltIn.retainedOffscreenColorTargetBytes, retainedColorBytes);

    std::array<render::SpritePresentationRenderData, 6U> ordered{};
    ordered[0] = makeCustomDraw(
        normal.material, normal.material.defaultParameters, 0U);
    ordered[1] = makeCustomDraw(
        normal.material, normal.material.defaultParameters, 1U);
    ordered[2] = makeCustomDraw(normal.material, halfFlash, 2U);
    ordered[3] = makeCustomDraw(
        additive.material, additive.material.defaultParameters, 3U);
    ordered[4] = makeCustomDraw(
        normal.material, normal.material.defaultParameters, 4U);
    ordered[5] = builtIn;
    ordered[5].order.stableOrder = 5U;

    const assets::ResourceRegistryStats resourceStatsBeforeBatch = resources.Stats();
    const render::RenderMetrics beforeBatch = renderer.Metrics();
    renderer.RenderFrame(
        camera,
        std::span<const render::SpritePresentationRenderData>{ordered});
    const render::RenderMetrics afterBatch = renderer.Metrics();
    const assets::ResourceRegistryStats resourceStatsAfterBatch = resources.Stats();

    EXPECT_EQ(
        afterBatch.spritePresentationCompatibilityRuns,
        beforeBatch.spritePresentationCompatibilityRuns + 5U);
    EXPECT_EQ(
        afterBatch.spritePresentationDrawCalls,
        beforeBatch.spritePresentationDrawCalls + 5U);
    EXPECT_EQ(
        afterBatch.spritePresentationVisibleSprites,
        beforeBatch.spritePresentationVisibleSprites + 6U);
    EXPECT_EQ(
        afterBatch.spritePresentationUploadedQuads,
        beforeBatch.spritePresentationUploadedQuads + 6U);
    EXPECT_EQ(
        afterBatch.materialPipelineSwitches,
        beforeBatch.materialPipelineSwitches + 3U);
    EXPECT_EQ(
        afterBatch.fragmentUniformUploads,
        beforeBatch.fragmentUniformUploads + 4U);
    EXPECT_EQ(
        afterBatch.fragmentUniformUploadBytes,
        beforeBatch.fragmentUniformUploadBytes + 128U);
    EXPECT_EQ(afterBatch.renderPasses, beforeBatch.renderPasses + 1U);
    EXPECT_EQ(afterBatch.retainedOffscreenColorTargetCount, 1U);
    EXPECT_EQ(afterBatch.retainedOffscreenColorTargetBytes, retainedColorBytes);
    EXPECT_EQ(
        afterBatch.spriteMaskTargetCreations,
        beforeBatch.spriteMaskTargetCreations);
    EXPECT_EQ(
        resourceStatsAfterBatch.canonicalizationCalls,
        resourceStatsBeforeBatch.canonicalizationCalls);
    EXPECT_EQ(
        resourceStatsAfterBatch.filesystemQueries,
        resourceStatsBeforeBatch.filesystemQueries);

    const render::RenderMetrics beforeContractFailure = renderer.Metrics();
    const auto badShader = resources.PublishShader2D(
        "shaders/mat4_contract_mismatch.shader2d",
        MakeContractMismatchShader());
    ASSERT_TRUE(badShader.Succeeded());
    assets::Material2DResource badMaterialPayload{};
    badMaterialPayload.shader = badShader.handle.Untyped();
    const auto badMaterial = resources.PublishMaterial2D(
        "materials/mat4_contract_mismatch.material2d",
        std::move(badMaterialPayload));
    ASSERT_TRUE(badMaterial.Succeeded());

    const render::MaterialGpuPrepareResult2D firstContractFailure =
        renderer.PrepareMaterial2D(resources, badMaterial.handle);
    ASSERT_FALSE(firstContractFailure.Succeeded());
    EXPECT_EQ(
        firstContractFailure.error,
        render::MaterialGpuPrepareError2D::ShaderContractMismatch);
    const render::MaterialGpuPrepareResult2D secondContractFailure =
        renderer.PrepareMaterial2D(resources, badMaterial.handle);
    ASSERT_FALSE(secondContractFailure.Succeeded());
    EXPECT_EQ(
        secondContractFailure.error,
        render::MaterialGpuPrepareError2D::ShaderContractMismatch);
    const render::RenderMetrics afterContractFailure = renderer.Metrics();
    EXPECT_EQ(
        afterContractFailure.materialPreparedPipelineBundles,
        beforeContractFailure.materialPreparedPipelineBundles);
    EXPECT_EQ(
        afterContractFailure.materialPipelineCreations,
        beforeContractFailure.materialPipelineCreations);
    EXPECT_EQ(
        afterContractFailure.materialPipelineCacheHits,
        beforeContractFailure.materialPipelineCacheHits);

    const auto generationShader1 = resources.PublishShader2D(
        "shaders/mat4_generation.shader2d",
        MakeFlashShader());
    ASSERT_TRUE(generationShader1.Succeeded());
    const auto generationMaterial1 = resources.PublishMaterial2D(
        "materials/mat4_generation.material2d",
        MakeFlashMaterial(generationShader1.handle));
    ASSERT_TRUE(generationMaterial1.Succeeded());
    const render::MaterialGpuPrepareResult2D generationPrepared1 =
        renderer.PrepareMaterial2D(resources, generationMaterial1.handle);
    ASSERT_TRUE(generationPrepared1.Succeeded()) << generationPrepared1.diagnostic;
    EXPECT_FALSE(generationPrepared1.reusedPreparedPipeline);

    ASSERT_TRUE(resources.Unload(generationMaterial1.handle.Untyped()).Succeeded());
    ASSERT_TRUE(resources.Unload(generationShader1.handle.Untyped()).Succeeded());

    const auto generationShader2 = resources.PublishShader2D(
        "shaders/mat4_generation.shader2d",
        MakeFlashShader());
    ASSERT_TRUE(generationShader2.Succeeded());
    EXPECT_NE(generationShader2.handle.Untyped(), generationShader1.handle.Untyped());
    const auto generationMaterial2 = resources.PublishMaterial2D(
        "materials/mat4_generation.material2d",
        MakeFlashMaterial(generationShader2.handle));
    ASSERT_TRUE(generationMaterial2.Succeeded());
    const render::RenderMetrics beforeGeneration2 = renderer.Metrics();
    const render::MaterialGpuPrepareResult2D generationPrepared2 =
        renderer.PrepareMaterial2D(resources, generationMaterial2.handle);
    ASSERT_TRUE(generationPrepared2.Succeeded()) << generationPrepared2.diagnostic;
    EXPECT_FALSE(generationPrepared2.reusedPreparedPipeline);
    EXPECT_NE(
        generationPrepared2.material.shaderIdentity,
        generationPrepared1.material.shaderIdentity);
    EXPECT_NE(
        generationPrepared2.material.parameterLayout.identity,
        generationPrepared1.material.parameterLayout.identity);
    EXPECT_NE(
        generationPrepared2.material.pipelineIdentity,
        generationPrepared1.material.pipelineIdentity);

    const render::RenderMetrics afterGeneration2 = renderer.Metrics();
    EXPECT_EQ(
        afterGeneration2.materialPreparedPipelineBundles,
        beforeGeneration2.materialPreparedPipelineBundles + 1U);
    EXPECT_EQ(
        afterGeneration2.materialPipelineCreations,
        beforeGeneration2.materialPipelineCreations + 4U);
    EXPECT_EQ(
        afterGeneration2.materialShaderCompilations,
        beforeGeneration2.materialShaderCompilations + 1U);
    EXPECT_GE(
        afterGeneration2.materialPreparedPipelineBundleCapacity,
        afterGeneration2.materialPreparedPipelineBundles);
}
} // namespace
