#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/render/SpritePresentation2D.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
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

enum class TargetEncoding : std::uint8_t
{
    Linear,
    Srgb,
};

struct Rgba8 final
{
    std::uint8_t red{0U};
    std::uint8_t green{0U};
    std::uint8_t blue{0U};
    std::uint8_t alpha{0U};
};

[[nodiscard]] float SrgbEncode(const float linear) noexcept
{
    const float clamped = std::clamp(linear, 0.0F, 1.0F);
    return clamped <= 0.0031308F
        ? 12.92F * clamped
        : 1.055F * std::pow(clamped, 1.0F / 2.4F) - 0.055F;
}

[[nodiscard]] float SrgbDecode(const float encoded) noexcept
{
    const float clamped = std::clamp(encoded, 0.0F, 1.0F);
    return clamped <= 0.04045F
        ? clamped / 12.92F
        : std::pow((clamped + 0.055F) / 1.055F, 2.4F);
}

[[nodiscard]] std::uint8_t ToByte(const float value) noexcept
{
    return static_cast<std::uint8_t>(
        std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

[[nodiscard]] Rgba8 EncodeTarget(
    const trace2d::render::SpriteLinearRgba& linear,
    const TargetEncoding encoding) noexcept
{
    const auto encode = [encoding](const float value)
    {
        return ToByte(encoding == TargetEncoding::Srgb ? SrgbEncode(value) : value);
    };
    return Rgba8{
        encode(linear.red),
        encode(linear.green),
        encode(linear.blue),
        ToByte(linear.alpha),
    };
}

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

[[nodiscard]] int RgbError(const Rgba8 left, const Rgba8 right) noexcept
{
    return std::abs(static_cast<int>(left.red) - static_cast<int>(right.red)) +
        std::abs(static_cast<int>(left.green) - static_cast<int>(right.green)) +
        std::abs(static_cast<int>(left.blue) - static_cast<int>(right.blue));
}

void ExpectRgbaNear(const Rgba8 actual, const Rgba8 expected, const int tolerance)
{
    EXPECT_NEAR(actual.red, expected.red, tolerance);
    EXPECT_NEAR(actual.green, expected.green, tolerance);
    EXPECT_NEAR(actual.blue, expected.blue, tolerance);
    EXPECT_NEAR(actual.alpha, expected.alpha, tolerance);
}

[[nodiscard]] trace2d::assets::SpriteAsset MakeSingleRegionAsset(
    const trace2d::assets::SpriteColorSpace colorSpace,
    const trace2d::assets::SpritePixelSize pageSize = {1U, 1U},
    const trace2d::assets::SpritePixelRect packedRect = {0U, 0U, 1U, 1U})
{
    using namespace trace2d::assets;

    SpriteAsset asset{};
    asset.id = "sprites/gpu-smoke.sprite.toml";
    asset.sampling = SpriteSampling::Nearest;
    asset.pages = {
        SpriteAtlasPage{
            "page",
            "textures/gpu-smoke.png",
            pageSize,
            colorSpace,
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
            packedRect,
            SpriteRationalPivot{0, 0, 1},
            SpritePackedRotation::None,
        },
    };
    return asset;
}

[[nodiscard]] trace2d::render::SpritePresentation2D BuildPresentation(
    const trace2d::assets::SpriteAsset& asset,
    const trace2d::render::SpriteAppearance2D& appearance)
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
        appearance,
        presentation).Succeeded());
    return presentation;
}

[[nodiscard]] trace2d::render::CapturedFrame CaptureSprite(
    trace2d::render::Renderer& renderer,
    const trace2d::render::OrthographicCamera& camera,
    const std::span<const trace2d::render::SpritePresentationRenderData> sprites,
    const std::filesystem::path& path,
    const std::uint64_t frame)
{
    const trace2d::render::CaptureRequest request{
        frame,
        path,
        trace2d::render::CaptureImageFormat::Bmp,
    };
    return renderer.CaptureFrame(request, camera, sprites);
}

TEST(SpriteGpuSmokeTests, Sr3ColorSamplingBlendAndCachesMatchFrozenContract)
{
    if (!GpuSmokeEnabled())
    {
        GTEST_SKIP() << "Set TRACE2D_RUN_GPU_SMOKE=1 on a Windows machine with a presentation GPU to run this test.";
    }

    using namespace trace2d;

    platform::PlatformConfig platformConfig{};
    platformConfig.mode = platform::StartupMode::Windowed;
    platformConfig.windowWidth = 64;
    platformConfig.windowHeight = 64;
    platformConfig.windowTitle = "Trace2D Sprite SR3 GPU smoke";
    platform::Platform platform{platformConfig};

    render::RendererConfig rendererConfig{};
    rendererConfig.enableDebugValidation = true;
    render::Renderer renderer{rendererConfig, platform};

    const render::OrthographicCamera camera{{0.0F, 0.0F}, 2.0F};
    const assets::SpriteAsset linearAsset =
        MakeSingleRegionAsset(assets::SpriteColorSpace::Linear);
    const assets::SpriteAsset srgbAsset =
        MakeSingleRegionAsset(assets::SpriteColorSpace::Srgb);

    constexpr std::array<std::uint8_t, 4> DestinationPixel{102U, 128U, 153U, 255U};
    constexpr std::array<std::uint8_t, 4> SourcePixel{204U, 64U, 128U, 128U};
    constexpr std::array<std::uint8_t, 4> MidGrayOpaque{128U, 128U, 128U, 255U};
    constexpr std::array<std::uint8_t, 8> AtlasPixels{
        255U, 0U, 0U, 255U,
        0U, 255U, 0U, 255U,
    };

    const render::TextureHandle destinationTexture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{1U, 1U, DestinationPixel},
        render::SpriteTextureEncoding::Linear);
    const render::TextureHandle sourceTexture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{1U, 1U, SourcePixel},
        render::SpriteTextureEncoding::Linear);
    const render::TextureHandle srgbMidGrayTexture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{1U, 1U, MidGrayOpaque},
        render::SpriteTextureEncoding::Srgb);

    render::SpriteAppearance2D destinationAppearance{};
    const render::SpritePresentationRenderData destination{
        BuildPresentation(linearAsset, destinationAppearance),
        destinationTexture,
    };

    // Ordinary presentation must reuse the startup cache and perform no explicit readback/wait.
    renderer.RenderFrame(camera, destination);
    const render::RenderMetrics afterFirstNormalFrame = renderer.Metrics();
    ASSERT_EQ(afterFirstNormalFrame.spriteSamplerCreations, 2U);
    ASSERT_EQ(afterFirstNormalFrame.spritePipelineCreations, 4U);
    ASSERT_GE(afterFirstNormalFrame.spriteVertexCapacitySprites, 1U);
    EXPECT_EQ(afterFirstNormalFrame.explicitGpuReadbacks, 0U);
    EXPECT_EQ(afterFirstNormalFrame.explicitGpuFenceWaits, 0U);

    renderer.RenderFrame(camera, destination);
    const render::RenderMetrics afterSecondNormalFrame = renderer.Metrics();
    EXPECT_EQ(afterSecondNormalFrame.spriteSamplerCreations, 2U);
    EXPECT_EQ(afterSecondNormalFrame.spritePipelineCreations, 4U);
    EXPECT_EQ(
        afterSecondNormalFrame.spriteVertexCapacitySprites,
        afterFirstNormalFrame.spriteVertexCapacitySprites);
    EXPECT_EQ(afterSecondNormalFrame.explicitGpuReadbacks, 0U);
    EXPECT_EQ(afterSecondNormalFrame.explicitGpuFenceWaits, 0U);

    const std::filesystem::path temp = std::filesystem::temp_directory_path();
    const std::filesystem::path calibrationPath = temp / "trace2d_sprite_sr3_calibration.bmp";
    const render::CapturedFrame calibration = CaptureSprite(
        renderer,
        camera,
        std::span<const render::SpritePresentationRenderData>{&destination, 1U},
        calibrationPath,
        10U);
    ASSERT_EQ(calibration.width, 64U);
    ASSERT_EQ(calibration.height, 64U);

    const Rgba8 actualCalibration = PixelAt(calibration, 32U, 32U);
    const render::SpriteLinearRgba destinationLinear{
        DestinationPixel[0] / 255.0F,
        DestinationPixel[1] / 255.0F,
        DestinationPixel[2] / 255.0F,
        1.0F,
    };
    const Rgba8 expectedLinearTarget = EncodeTarget(destinationLinear, TargetEncoding::Linear);
    const Rgba8 expectedSrgbTarget = EncodeTarget(destinationLinear, TargetEncoding::Srgb);
    const TargetEncoding targetEncoding =
        RgbError(actualCalibration, expectedSrgbTarget) < RgbError(actualCalibration, expectedLinearTarget)
        ? TargetEncoding::Srgb
        : TargetEncoding::Linear;
    ExpectRgbaNear(
        actualCalibration,
        EncodeTarget(destinationLinear, targetEncoding),
        3);

    // sRGB source must decode once before the same target encoding. A double decode does not match.
    const render::SpritePresentationRenderData srgbMidGray{
        BuildPresentation(srgbAsset, render::SpriteAppearance2D{}),
        srgbMidGrayTexture,
    };
    const std::filesystem::path srgbPath = temp / "trace2d_sprite_sr3_srgb.bmp";
    const render::CapturedFrame srgbCapture = CaptureSprite(
        renderer,
        camera,
        std::span<const render::SpritePresentationRenderData>{&srgbMidGray, 1U},
        srgbPath,
        11U);
    const float decodedMidGray = SrgbDecode(128.0F / 255.0F);
    const Rgba8 expectedSrgbSample = EncodeTarget(
        render::SpriteLinearRgba{decodedMidGray, decodedMidGray, decodedMidGray, 1.0F},
        targetEncoding);
    ExpectRgbaNear(PixelAt(srgbCapture, 32U, 32U), expectedSrgbSample, 4);

    // Atlas edge: the left 1px region must remain pure red with both filters even next to green.
    const assets::SpriteAsset atlasAsset = MakeSingleRegionAsset(
        assets::SpriteColorSpace::Linear,
        assets::SpritePixelSize{2U, 1U},
        assets::SpritePixelRect{0U, 0U, 1U, 1U});
    const render::TextureHandle atlasTexture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{2U, 1U, AtlasPixels},
        render::SpriteTextureEncoding::Linear);

    for (const render::SpriteAppearanceSampling sampling : {
             render::SpriteAppearanceSampling::Nearest,
             render::SpriteAppearanceSampling::Linear})
    {
        render::SpriteAppearance2D appearance{};
        appearance.sampling = sampling;
        const render::SpritePresentationRenderData atlasSprite{
            BuildPresentation(atlasAsset, appearance),
            atlasTexture,
        };
        const std::filesystem::path atlasPath =
            temp / (sampling == render::SpriteAppearanceSampling::Nearest
                ? "trace2d_sprite_sr3_atlas_nearest.bmp"
                : "trace2d_sprite_sr3_atlas_linear.bmp");
        const render::CapturedFrame atlasCapture = CaptureSprite(
            renderer,
            camera,
            std::span<const render::SpritePresentationRenderData>{&atlasSprite, 1U},
            atlasPath,
            sampling == render::SpriteAppearanceSampling::Nearest ? 12U : 13U);

        // x=46 is near the right rasterized edge where unclamped linear filtering would pull green.
        const Rgba8 edge = PixelAt(atlasCapture, 46U, 32U);
        EXPECT_GE(edge.red, 250U);
        EXPECT_LE(edge.green, 5U);
        EXPECT_LE(edge.blue, 5U);
        EXPECT_GE(edge.alpha, 250U);
        std::error_code removeError{};
        std::filesystem::remove(atlasPath, removeError);
    }

    render::SpriteAppearance2D sourceAppearance{};
    sourceAppearance.tint = render::SpriteLinearRgba{0.5F, 1.0F, 0.25F, 0.5F};
    sourceAppearance.opacity = 0.75F;

    const render::SpriteLinearRgba destinationFragment =
        render::EvaluateSpritePremultipliedFragment(destinationLinear, destination.presentation.appearance);
    const render::SpriteLinearRgba sourceStraight{
        SourcePixel[0] / 255.0F,
        SourcePixel[1] / 255.0F,
        SourcePixel[2] / 255.0F,
        SourcePixel[3] / 255.0F,
    };

    std::uint64_t captureFrame = 20U;
    for (const render::SpriteBlendMode mode : {
             render::SpriteBlendMode::Normal,
             render::SpriteBlendMode::Additive,
             render::SpriteBlendMode::Multiply,
             render::SpriteBlendMode::Screen})
    {
        sourceAppearance.blend = mode;
        const render::SpritePresentationRenderData source{
            BuildPresentation(linearAsset, sourceAppearance),
            sourceTexture,
        };
        const std::array<render::SpritePresentationRenderData, 2> draws{destination, source};
        const std::filesystem::path blendPath =
            temp / (std::string{"trace2d_sprite_sr3_blend_"} +
                std::string{render::ToString(source.presentation.appearance.blend)} + ".bmp");
        const render::CapturedFrame capture = CaptureSprite(
            renderer,
            camera,
            draws,
            blendPath,
            captureFrame++);

        const render::SpriteLinearRgba sourceFragment =
            render::EvaluateSpritePremultipliedFragment(
                sourceStraight,
                source.presentation.appearance);
        render::SpriteLinearRgba expectedLinear{};
        ASSERT_TRUE(render::TryEvaluateSpriteBlend(
            sourceFragment,
            destinationFragment,
            source.presentation.appearance.blend,
            expectedLinear));
        ExpectRgbaNear(
            PixelAt(capture, 32U, 32U),
            EncodeTarget(expectedLinear, targetEncoding),
            5);

        std::error_code removeError{};
        std::filesystem::remove(blendPath, removeError);
    }

    const render::RenderMetrics finalMetrics = renderer.Metrics();
    EXPECT_EQ(finalMetrics.spriteSamplerCreations, 2U);
    EXPECT_EQ(finalMetrics.spritePipelineCreations, 4U);
    EXPECT_GE(finalMetrics.spriteVertexCapacitySprites, 2U);
    EXPECT_GE(finalMetrics.spritePresentationDrawCalls, 13U);
    EXPECT_GE(finalMetrics.spritePresentationSprites, 13U);
    EXPECT_GT(finalMetrics.explicitGpuReadbacks, 0U);
    EXPECT_EQ(finalMetrics.explicitGpuReadbacks, finalMetrics.explicitGpuFenceWaits);

    renderer.DestroyTexture(atlasTexture);
    renderer.DestroyTexture(srgbMidGrayTexture);
    renderer.DestroyTexture(sourceTexture);
    renderer.DestroyTexture(destinationTexture);

    for (const std::filesystem::path& path : {calibrationPath, srgbPath})
    {
        std::error_code removeError{};
        std::filesystem::remove(path, removeError);
    }
}
} // namespace
