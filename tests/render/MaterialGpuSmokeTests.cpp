#include "GpuQaFixtureOutcome.hpp"

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
    asset.id = "sprites/mat3-smoke.sprite.toml";
    asset.sampling = SpriteSampling::Nearest;
    asset.pages = {
        SpriteAtlasPage{
            "page",
            "textures/mat3-smoke.png",
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

[[nodiscard]] trace2d::assets::Material2DResource MakeFlashMaterial(
    const trace2d::assets::ResourceHandle<trace2d::assets::Shader2DResource> shader)
{
    using namespace trace2d::assets;

    Material2DResource material{};
    material.shader = shader.Untyped();
    material.sampler = MaterialSampler2D::Nearest;
    material.blend = MaterialBlend2D::Normal;
    material.parameters = {
        MaterialParameterDefault2D{
            "flashColor",
            MaterialParameterValue2D{MaterialParameterType2D::Color, {1.0F, 0.0F, 0.0F, 1.0F}}},
        MaterialParameterDefault2D{
            "flashAmount",
            MaterialParameterValue2D{MaterialParameterType2D::Float, {1.0F, 0.0F, 0.0F, 0.0F}}},
    };
    return material;
}

TEST(MaterialGpu2DTests, PrepareDiagnosticsHaveStableNames)
{
    using trace2d::render::MaterialGpuPrepareError2D;
    using trace2d::render::ToString;

    EXPECT_EQ(ToString(MaterialGpuPrepareError2D::None), "none");
    EXPECT_EQ(ToString(MaterialGpuPrepareError2D::InvalidMaterialHandle), "invalid_material_handle");
    EXPECT_EQ(ToString(MaterialGpuPrepareError2D::ShaderCompilationFailed), "shader_compilation_failed");
    EXPECT_EQ(ToString(MaterialGpuPrepareError2D::ShaderContractMismatch), "shader_contract_mismatch");
    EXPECT_EQ(ToString(MaterialGpuPrepareError2D::PipelineCreationFailed), "pipeline_creation_failed");
}

TEST(MaterialGpuSmokeTests, CachedFlashMaterialExecutesAndFailedShaderDoesNotPoisonCache)
{
    constexpr std::string_view TestName =
        "MaterialGpuSmokeTests.CachedFlashMaterialExecutesAndFailedShaderDoesNotPoisonCache";
    using trace2d::test::GpuQaFailureCategory;
    trace2d::test::GpuQaFixtureOutcome outcome{TestName};

    if (!GpuSmokeEnabled())
    {
        GTEST_SKIP() << "Set TRACE2D_RUN_GPU_SMOKE=1 on a Windows machine with a presentation GPU to run MAT3.";
    }

    using namespace trace2d;

    outcome.SetFailurePoint(
        "device_initialization",
        GpuQaFailureCategory::GpuDeviceInitializationFailure);
    platform::PlatformConfig platformConfig{};
    platformConfig.mode = platform::StartupMode::Windowed;
    platformConfig.windowWidth = 64;
    platformConfig.windowHeight = 64;
    platformConfig.windowTitle = "Trace2D MAT3 GPU smoke";
    platform::Platform platform{platformConfig};

    render::RendererConfig rendererConfig{};
    rendererConfig.enableDebugValidation = true;
    render::Renderer renderer{rendererConfig, platform};

    outcome.SetFailurePoint(
        "pipeline_or_resource_creation",
        GpuQaFailureCategory::PipelineOrResourceCreationFailure);
    assets::ResourceRegistry resources{"project"};
    const auto shader = resources.PublishShader2D("shaders/mat3_flash.shader2d", MakeFlashShader());
    ASSERT_TRUE(shader.Succeeded());
    const auto material = resources.PublishMaterial2D(
        "materials/mat3_flash.material2d",
        MakeFlashMaterial(shader.handle));
    ASSERT_TRUE(material.Succeeded());

    const render::MaterialGpuPrepareResult2D prepared =
        renderer.PrepareMaterial2D(resources, material.handle);
    outcome.SetMaterialPrepareFailure(prepared.error);
    ASSERT_TRUE(prepared.Succeeded()) << prepared.diagnostic;
    EXPECT_FALSE(prepared.reusedPreparedPipeline);
    EXPECT_NE(prepared.material.pipelineIdentity, render::BuiltInSpriteMaterialPipelineIdentity);
    EXPECT_EQ(prepared.material.defaultParameters.ActivePackedBytes(), 32U);

    outcome.SetFailurePoint("comparison", GpuQaFailureCategory::ComparisonMismatch);
    const render::RenderMetrics afterFirstPrepare = renderer.Metrics();
    EXPECT_EQ(afterFirstPrepare.materialShaderCompilations, 1U);
    EXPECT_EQ(afterFirstPrepare.materialPipelineCreations, 4U);
    EXPECT_EQ(afterFirstPrepare.materialPipelineCacheHits, 0U);
    EXPECT_EQ(afterFirstPrepare.spritePipelineCreations, 21U);

    outcome.SetFailurePoint(
        "pipeline_or_resource_creation",
        GpuQaFailureCategory::PipelineOrResourceCreationFailure);
    const render::MaterialGpuPrepareResult2D cached =
        renderer.PrepareMaterial2D(resources, material.handle);
    outcome.SetMaterialPrepareFailure(cached.error);
    ASSERT_TRUE(cached.Succeeded()) << cached.diagnostic;
    EXPECT_TRUE(cached.reusedPreparedPipeline);
    EXPECT_EQ(cached.material.pipelineIdentity, prepared.material.pipelineIdentity);

    outcome.SetFailurePoint("comparison", GpuQaFailureCategory::ComparisonMismatch);
    const render::RenderMetrics afterCachedPrepare = renderer.Metrics();
    EXPECT_EQ(afterCachedPrepare.materialShaderCompilations, 1U);
    EXPECT_EQ(afterCachedPrepare.materialPipelineCreations, 4U);
    EXPECT_EQ(afterCachedPrepare.materialPipelineCacheHits, 1U);
    EXPECT_EQ(afterCachedPrepare.spritePipelineCreations, 21U);

    outcome.SetFailurePoint(
        "pipeline_or_resource_creation",
        GpuQaFailureCategory::PipelineOrResourceCreationFailure);
    constexpr std::array<std::uint8_t, 4U> WhitePixel{255U, 255U, 255U, 255U};
    const render::TextureHandle texture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{1U, 1U, WhitePixel},
        render::SpriteTextureEncoding::Linear);
    const assets::SpriteAsset asset = MakeSingleRegionAsset();

    render::SpritePresentationRenderData custom{};
    custom.presentation = BuildPresentation(asset);
    custom.presentation.appearance.sampler = prepared.material.sampler;
    custom.presentation.appearance.blend = prepared.material.blend;
    custom.texture = texture;
    custom.materialPipeline = prepared.material.pipelineIdentity;
    custom.materialParameters = &prepared.material.defaultParameters;

    outcome.SetFailurePoint(
        "readback_capture",
        GpuQaFailureCategory::ReadbackOrCaptureFailure);
    const render::OrthographicCamera camera{{0.0F, 0.0F}, 2.0F};
    const std::filesystem::path output =
        std::filesystem::temp_directory_path() / "trace2d_mat3_flash.bmp";
    const render::CapturedFrame customFrame = renderer.CaptureFrame(
        render::CaptureRequest{30U, output, render::CaptureImageFormat::Bmp},
        camera,
        custom);
    ASSERT_EQ(customFrame.width, 64U);
    ASSERT_EQ(customFrame.height, 64U);

    outcome.SetFailurePoint("comparison", GpuQaFailureCategory::ComparisonMismatch);
    const Rgba8 customPixel = PixelAt(customFrame, 32U, 32U);
    EXPECT_GE(customPixel.red, 248U);
    EXPECT_LE(customPixel.green, 6U);
    EXPECT_LE(customPixel.blue, 6U);
    EXPECT_GE(customPixel.alpha, 248U);

    const render::RenderMetrics afterCustomDraw = renderer.Metrics();
    EXPECT_EQ(afterCustomDraw.materialShaderCompilations, 1U);
    EXPECT_EQ(afterCustomDraw.materialPipelineCreations, 4U);
    EXPECT_GE(afterCustomDraw.fragmentUniformUploads, 1U);
    EXPECT_GE(afterCustomDraw.fragmentUniformUploadBytes, 32U);

    render::SpritePresentationRenderData builtIn{};
    builtIn.presentation = BuildPresentation(asset);
    builtIn.texture = texture;

    outcome.SetFailurePoint(
        "readback_capture",
        GpuQaFailureCategory::ReadbackOrCaptureFailure);
    const std::filesystem::path builtInOutput =
        std::filesystem::temp_directory_path() / "trace2d_mat3_builtin.bmp";
    const render::CapturedFrame builtInFrame = renderer.CaptureFrame(
        render::CaptureRequest{31U, builtInOutput, render::CaptureImageFormat::Bmp},
        camera,
        builtIn);

    outcome.SetFailurePoint("comparison", GpuQaFailureCategory::ComparisonMismatch);
    const Rgba8 builtInPixel = PixelAt(builtInFrame, 32U, 32U);
    EXPECT_GE(builtInPixel.red, 248U);
    EXPECT_GE(builtInPixel.green, 248U);
    EXPECT_GE(builtInPixel.blue, 248U);
    EXPECT_GE(builtInPixel.alpha, 248U);

    outcome.SetFailurePoint(
        "shader_compile_or_reflection",
        GpuQaFailureCategory::ShaderCompileOrReflectionFailure);
    assets::Shader2DResource badShader{};
    badShader.entryPoint = "main";
    badShader.canonicalSource = "float4 main( : SV_Target0 { definitely_not_hlsl }";
    const auto badShaderPublication = resources.PublishShader2D(
        "shaders/mat3_bad.shader2d",
        std::move(badShader));
    ASSERT_TRUE(badShaderPublication.Succeeded());
    assets::Material2DResource badMaterial{};
    badMaterial.shader = badShaderPublication.handle.Untyped();
    const auto badMaterialPublication = resources.PublishMaterial2D(
        "materials/mat3_bad.material2d",
        std::move(badMaterial));
    ASSERT_TRUE(badMaterialPublication.Succeeded());

    const std::uint64_t cacheHitsBeforeFailure = renderer.Metrics().materialPipelineCacheHits;
    const render::MaterialGpuPrepareResult2D firstFailure =
        renderer.PrepareMaterial2D(resources, badMaterialPublication.handle);
    EXPECT_FALSE(firstFailure.Succeeded());
    EXPECT_EQ(firstFailure.error, render::MaterialGpuPrepareError2D::ShaderCompilationFailed);
    const render::MaterialGpuPrepareResult2D secondFailure =
        renderer.PrepareMaterial2D(resources, badMaterialPublication.handle);
    EXPECT_FALSE(secondFailure.Succeeded());
    EXPECT_EQ(secondFailure.error, render::MaterialGpuPrepareError2D::ShaderCompilationFailed);

    outcome.SetFailurePoint("comparison", GpuQaFailureCategory::ComparisonMismatch);
    EXPECT_EQ(renderer.Metrics().materialPipelineCacheHits, cacheHitsBeforeFailure);
    EXPECT_EQ(renderer.Metrics().materialPipelineCreations, 4U);
}
} // namespace
