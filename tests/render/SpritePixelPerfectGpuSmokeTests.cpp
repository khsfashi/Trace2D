#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/render/SpritePixelPerfect2D.hpp>
#include <trace2d/render/SpritePrimitive2D.hpp>

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

void ExpectNear(
    const Rgba8 actual,
    const std::uint8_t red,
    const std::uint8_t green,
    const std::uint8_t blue,
    const std::uint8_t alpha = 255U)
{
    constexpr int Tolerance = 8;
    EXPECT_NEAR(actual.red, red, Tolerance);
    EXPECT_NEAR(actual.green, green, Tolerance);
    EXPECT_NEAR(actual.blue, blue, Tolerance);
    EXPECT_NEAR(actual.alpha, alpha, Tolerance);
}

[[nodiscard]] trace2d::assets::SpriteAsset MakePixelAsset()
{
    using namespace trace2d::assets;

    SpriteAsset asset{};
    asset.id = "sprites/sr6-gpu-smoke.sprite.toml";
    asset.sampling = SpriteSampling::Nearest;
    asset.pages = {
        SpriteAtlasPage{
            "page",
            "textures/sr6-gpu-smoke.png",
            SpritePixelSize{2U, 1U},
            SpriteColorSpace::Linear,
            SpriteAlphaMode::Straight,
        },
    };
    asset.regions = {
        SpriteRegion{
            "frame",
            "page",
            SpritePixelSize{2U, 1U},
            SpritePixelOffset{0U, 0U},
            SpritePixelSize{2U, 1U},
            SpritePixelRect{0U, 0U, 2U, 1U},
            SpriteRationalPivot{0, 0, 1},
            SpritePackedRotation::None,
        },
    };
    return asset;
}

[[nodiscard]] trace2d::render::ResolvedSpriteRegion ResolveOnlyRegion(
    const trace2d::assets::SpriteAsset& asset)
{
    trace2d::render::ResolvedSpriteRegion selection{};
    EXPECT_TRUE(trace2d::render::ResolveSpriteRegionByIndices(
        &asset, 0U, 0U, selection).Succeeded());
    return selection;
}

[[nodiscard]] trace2d::render::SpritePresentationRenderData BuildExactQuad(
    const trace2d::render::ResolvedSpriteRegion& selection,
    const trace2d::render::TextureHandle texture,
    const trace2d::render::SpritePixelPerfectViewport2D& viewport)
{
    using namespace trace2d;

    scene::SpritePose2D pose{};
    pose.transform.position = scene::Vector2{-1.0F, 0.5F};
    const scene::SpritePoseHistory2D history{pose, pose};
    render::SpritePixelPerfectMapping2D mapping{};
    EXPECT_TRUE(render::ResolveSpritePixelPerfectPose(
        selection,
        history,
        1.0F,
        viewport,
        {},
        mapping).Succeeded());

    render::SpritePresentation2D presentation{};
    EXPECT_TRUE(render::BuildSpritePresentation2D(
        selection,
        mapping.presentationPose,
        1.0F,
        render::SpriteAppearance2D{},
        presentation).Succeeded());
    EXPECT_EQ(presentation.appearance.sampler, render::SpriteSamplerCompatibility::Nearest);

    render::SpritePresentationRenderData data{};
    data.presentation = presentation;
    data.texture = texture;
    data.pixelPerfectViewport = &viewport;
    return data;
}

[[nodiscard]] trace2d::render::SpritePresentationRenderData BuildMaskWriter(
    const trace2d::render::ResolvedSpriteRegion& selection,
    const trace2d::render::TextureHandle texture,
    const trace2d::render::SpritePixelPerfectViewport2D& viewport)
{
    trace2d::render::SpritePresentationRenderData data =
        BuildExactQuad(selection, texture, viewport);
    data.order.stableOrder = 0U;
    data.mask = trace2d::render::SpriteMask2D{
        trace2d::render::SpriteMaskMode::Write,
        6U,
    };
    return data;
}

[[nodiscard]] trace2d::render::SpritePresentationRenderData BuildMaskedTiled(
    const trace2d::render::ResolvedSpriteRegion& selection,
    const trace2d::render::TextureHandle texture,
    const trace2d::render::SpritePixelPerfectViewport2D& viewport,
    std::span<trace2d::render::SpritePrimitivePatch2D> scratch,
    std::size_t& outPatchCount)
{
    using namespace trace2d;

    render::SpriteAppearanceContractData appearance{};
    EXPECT_TRUE(render::ExtractSpriteAppearanceContract(
        selection,
        render::SpriteAppearance2D{},
        appearance).Succeeded());

    scene::SpritePose2D pose{};
    pose.transform.position = scene::Vector2{-1.0F, 0.5F};
    const render::SpritePrimitive2D primitive{
        render::SpritePrimitiveMode::Tiled,
        render::Float2{4.0F, 1.0F},
    };
    EXPECT_TRUE(render::BuildSpritePrimitivePatches(
        selection,
        pose,
        1.0F,
        primitive,
        scratch,
        outPatchCount).Succeeded());
    EXPECT_EQ(outPatchCount, 2U);

    render::SpritePresentationRenderData data{};
    data.presentation.appearance = appearance;
    data.texture = texture;
    data.order.stableOrder = 1U;
    data.mask = render::SpriteMask2D{render::SpriteMaskMode::TestInside, 6U};
    data.geometryKind = render::SpritePresentationGeometryKind::PrimitivePatches;
    data.primitivePatches = scratch.first(outPatchCount);
    data.pixelPerfectViewport = &viewport;
    return data;
}

[[nodiscard]] trace2d::render::CapturedFrame Capture(
    trace2d::render::Renderer& renderer,
    const trace2d::render::OrthographicCamera& camera,
    const std::span<const trace2d::render::SpritePresentationRenderData> sprites,
    const std::filesystem::path& path)
{
    return renderer.CaptureFrame(
        trace2d::render::CaptureRequest{
            600U,
            path,
            trace2d::render::CaptureImageFormat::Bmp,
        },
        camera,
        sprites);
}

void RemoveFile(const std::filesystem::path& path)
{
    std::error_code error{};
    std::filesystem::remove(path, error);
}
} // namespace

TEST(SpritePixelPerfectGpuSmokeTests, Sr6IntegerViewportNearestMaskPrimitiveAndReuseMatchContract)
{
    if (!GpuSmokeEnabled())
    {
        GTEST_SKIP() << "Set TRACE2D_RUN_GPU_SMOKE=1 on a Windows machine with a presentation GPU to run this test.";
    }

    using namespace trace2d;

    platform::PlatformConfig platformConfig{};
    platformConfig.mode = platform::StartupMode::Windowed;
    platformConfig.windowWidth = 96;
    platformConfig.windowHeight = 96;
    platformConfig.windowTitle = "Trace2D Sprite SR6 GPU smoke";
    platform::Platform platform{platformConfig};

    render::RendererConfig rendererConfig{};
    rendererConfig.enableDebugValidation = true;
    rendererConfig.clearColor = render::ClearColor{0.0F, 0.0F, 0.0F, 1.0F};
    render::Renderer renderer{rendererConfig, platform};

    // Resolve the actual acquired pixel target first so the SR6 mapping is robust to platform
    // window pixel-size policy instead of assuming logical window points equal presentation pixels.
    renderer.RenderFrame();
    const render::RenderMetrics initialMetrics = renderer.Metrics();
    ASSERT_GE(initialMetrics.lastTargetWidth, 16U);
    ASSERT_GE(initialMetrics.lastTargetHeight, 9U);

    const render::OrthographicCamera camera{{0.0F, 0.0F}, 9.0F};
    render::SpritePixelPerfectViewport2D viewport{};
    ASSERT_TRUE(render::BuildSpritePixelPerfectViewport(
        camera,
        16U,
        9U,
        initialMetrics.lastTargetWidth,
        initialMetrics.lastTargetHeight,
        viewport).Succeeded());
    ASSERT_GT(viewport.contentRect.y, 0U);

    const assets::SpriteAsset asset = MakePixelAsset();
    const render::ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    constexpr std::array<std::uint8_t, 8U> RedGreen{
        255U, 0U, 0U, 255U,
        0U, 255U, 0U, 255U,
    };
    const render::TextureHandle texture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{2U, 1U, RedGreen},
        render::SpriteTextureEncoding::Linear);

    const render::SpritePresentationRenderData exact =
        BuildExactQuad(selection, texture, viewport);
    const std::array<render::SpritePresentationRenderData, 1U> exactOnly{exact};
    const std::filesystem::path capturePath =
        std::filesystem::temp_directory_path() / "trace2d_sprite_sr6_pixel_perfect.bmp";
    const render::CapturedFrame capture = Capture(
        renderer,
        camera,
        exactOnly,
        capturePath);

    const std::uint32_t scale = viewport.integerScale;
    // Sprite source origin is logical (7,4). Each source texel must occupy exactly scale x scale
    // final pixels inside the integer viewport.
    const std::uint32_t redX = viewport.contentRect.x + 7U * scale + scale / 2U;
    const std::uint32_t greenX = viewport.contentRect.x + 8U * scale + scale / 2U;
    const std::uint32_t spriteY = viewport.contentRect.y + 4U * scale + scale / 2U;
    ExpectNear(PixelAt(capture, redX, spriteY), 255U, 0U, 0U);
    ExpectNear(PixelAt(capture, greenX, spriteY), 0U, 255U, 0U);

    // Full-target clear precedes the restricted viewport/scissor, so a letterbox pixel remains
    // exactly the configured clear color.
    ExpectNear(PixelAt(capture, capture.width / 2U, viewport.contentRect.y - 1U), 0U, 0U, 0U);

    // SR4 stencil masking and SR5 primitive expansion execute under the same SR6 raster state.
    std::array<render::SpritePrimitivePatch2D, 8U> patchScratch{};
    std::size_t patchCount = 0U;
    const render::SpritePresentationRenderData writer =
        BuildMaskWriter(selection, texture, viewport);
    const render::SpritePresentationRenderData tiled = BuildMaskedTiled(
        selection,
        texture,
        viewport,
        patchScratch,
        patchCount);
    const std::array<render::SpritePresentationRenderData, 2U> maskedTiled{
        tiled,
        writer,
    };
    renderer.RenderFrame(camera, maskedTiled);
    const render::RenderMetrics firstWarm = renderer.Metrics();
    EXPECT_GE(firstWarm.spriteMaskTargetCreations, 1U);
    EXPECT_GE(firstWarm.spriteVertexCapacitySprites, patchCount + 1U);

    const std::uint64_t drawsBeforeRepeat = firstWarm.spritePresentationDrawCalls;
    const std::uint64_t spritesBeforeRepeat = firstWarm.spritePresentationSprites;
    const std::uint64_t capacityBeforeRepeat = firstWarm.spriteVertexCapacitySprites;
    const std::uint64_t samplersBeforeRepeat = firstWarm.spriteSamplerCreations;
    const std::uint64_t pipelinesBeforeRepeat = firstWarm.spritePipelineCreations;
    const std::uint64_t maskTargetsBeforeRepeat = firstWarm.spriteMaskTargetCreations;
    const std::uint64_t readbacksBeforeRepeat = firstWarm.explicitGpuReadbacks;
    const std::uint64_t waitsBeforeRepeat = firstWarm.explicitGpuFenceWaits;

    renderer.RenderFrame(camera, maskedTiled);
    const render::RenderMetrics secondWarm = renderer.Metrics();
    EXPECT_EQ(secondWarm.spritePresentationDrawCalls, drawsBeforeRepeat + 2U);
    EXPECT_EQ(secondWarm.spritePresentationSprites, spritesBeforeRepeat + 2U);
    EXPECT_EQ(secondWarm.spriteVertexCapacitySprites, capacityBeforeRepeat);
    EXPECT_EQ(secondWarm.spriteSamplerCreations, samplersBeforeRepeat);
    EXPECT_EQ(secondWarm.spritePipelineCreations, pipelinesBeforeRepeat);
    EXPECT_EQ(secondWarm.spriteMaskTargetCreations, maskTargetsBeforeRepeat);
    EXPECT_EQ(secondWarm.explicitGpuReadbacks, readbacksBeforeRepeat);
    EXPECT_EQ(secondWarm.explicitGpuFenceWaits, waitsBeforeRepeat);

    renderer.DestroyTexture(texture);
    RemoveFile(capturePath);
}
