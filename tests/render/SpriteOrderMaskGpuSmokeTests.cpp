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

void ExpectPrimaryNear(
    const Rgba8 actual,
    const std::uint8_t red,
    const std::uint8_t green,
    const std::uint8_t blue)
{
    constexpr int Tolerance = 4;
    EXPECT_NEAR(actual.red, red, Tolerance);
    EXPECT_NEAR(actual.green, green, Tolerance);
    EXPECT_NEAR(actual.blue, blue, Tolerance);
    EXPECT_NEAR(actual.alpha, 255U, Tolerance);
}

[[nodiscard]] trace2d::assets::SpriteAsset MakeAsset()
{
    using namespace trace2d::assets;

    SpriteAsset asset{};
    asset.id = "sprites/sr4-gpu-smoke.sprite.toml";
    asset.sampling = SpriteSampling::Nearest;
    asset.pages = {
        SpriteAtlasPage{
            "page",
            "textures/sr4-gpu-smoke.png",
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
    const trace2d::assets::SpriteAsset& asset,
    const trace2d::scene::Vector2 scale = {1.0F, 1.0F})
{
    using namespace trace2d;

    render::ResolvedSpriteRegion selection{};
    EXPECT_TRUE(render::ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection).Succeeded());

    scene::SpritePose2D pose{};
    pose.transform.position = scene::Vector2{-0.5F, 0.5F};
    pose.transform.scale = scale;

    render::SpritePresentation2D presentation{};
    EXPECT_TRUE(render::BuildSpritePresentation2D(
        selection,
        pose,
        1.0F,
        render::SpriteAppearance2D{},
        presentation).Succeeded());
    return presentation;
}

[[nodiscard]] trace2d::render::CapturedFrame Capture(
    trace2d::render::Renderer& renderer,
    const trace2d::render::OrthographicCamera& camera,
    const std::span<const trace2d::render::SpritePresentationRenderData> sprites,
    const std::filesystem::path& path,
    const std::uint64_t frame)
{
    return renderer.CaptureFrame(
        trace2d::render::CaptureRequest{
            frame,
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

TEST(SpriteOrderMaskGpuSmokeTests, Sr4PainterGroupsAndMasksMatchFrozenContract)
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
    platformConfig.windowTitle = "Trace2D Sprite SR4 GPU smoke";
    platform::Platform platform{platformConfig};

    render::RendererConfig rendererConfig{};
    rendererConfig.enableDebugValidation = true;
    rendererConfig.clearColor = render::ClearColor{0.0F, 0.0F, 0.0F, 1.0F};
    render::Renderer renderer{rendererConfig, platform};

    const render::OrthographicCamera camera{{0.0F, 0.0F}, 2.0F};
    const assets::SpriteAsset asset = MakeAsset();
    const render::SpritePresentation2D full = BuildPresentation(asset);
    const render::SpritePresentation2D leftHalf =
        BuildPresentation(asset, scene::Vector2{0.5F, 1.0F});

    constexpr std::array<std::uint8_t, 4U> Red{255U, 0U, 0U, 255U};
    constexpr std::array<std::uint8_t, 4U> Green{0U, 255U, 0U, 255U};
    constexpr std::array<std::uint8_t, 4U> Blue{0U, 0U, 255U, 255U};
    constexpr std::array<std::uint8_t, 4U> White{255U, 255U, 255U, 255U};
    constexpr std::array<std::uint8_t, 4U> Transparent{0U, 0U, 0U, 0U};

    const render::TextureHandle redTexture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{1U, 1U, Red},
        render::SpriteTextureEncoding::Linear);
    const render::TextureHandle greenTexture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{1U, 1U, Green},
        render::SpriteTextureEncoding::Linear);
    const render::TextureHandle blueTexture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{1U, 1U, Blue},
        render::SpriteTextureEncoding::Linear);
    const render::TextureHandle whiteTexture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{1U, 1U, White},
        render::SpriteTextureEncoding::Linear);
    const render::TextureHandle transparentTexture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{1U, 1U, Transparent},
        render::SpriteTextureEncoding::Linear);

    const std::filesystem::path temp = std::filesystem::temp_directory_path();

    // Input order is green then red. Semantic stable order must redraw as red(10) then green(20),
    // proving presentation order is semantic rather than caller/resource order.
    render::SpritePresentationRenderData red{full, redTexture};
    red.order.stableOrder = 10U;
    render::SpritePresentationRenderData green{full, greenTexture};
    green.order.stableOrder = 20U;
    const std::array<render::SpritePresentationRenderData, 2U> painterDraws{green, red};
    const std::filesystem::path painterPath = temp / "trace2d_sprite_sr4_painter.bmp";
    const render::CapturedFrame painter = Capture(
        renderer,
        camera,
        painterDraws,
        painterPath,
        100U);
    ExpectPrimaryNear(PixelAt(painter, 32U, 32U), 0U, 255U, 0U);

    // The group is one top-level unit at stable 10. Blue is top-level stable 5, so it draws first;
    // group children then draw by local order: green(1) followed by red(2).
    render::SpritePresentationRenderData groupRed{full, redTexture};
    groupRed.order.order = 2;
    groupRed.order.group = render::SpriteSortingGroup2D{7U, 0, 0, 10U};
    render::SpritePresentationRenderData groupGreen{full, greenTexture};
    groupGreen.order.order = 1;
    groupGreen.order.group = render::SpriteSortingGroup2D{7U, 0, 0, 10U};
    render::SpritePresentationRenderData blue{full, blueTexture};
    blue.order.stableOrder = 5U;
    const std::array<render::SpritePresentationRenderData, 3U> groupDraws{
        groupRed,
        groupGreen,
        blue,
    };
    const std::filesystem::path groupPath = temp / "trace2d_sprite_sr4_group.bmp";
    const render::CapturedFrame grouped = Capture(
        renderer,
        camera,
        groupDraws,
        groupPath,
        101U);
    ExpectPrimaryNear(PixelAt(grouped, 32U, 32U), 255U, 0U, 0U);

    // Unmasked SR3/SR4 presentations must not pay for a depth/stencil target or clear.
    EXPECT_EQ(renderer.Metrics().spriteMaskTargetCreations, 0U);

    render::SpritePresentationRenderData writer{leftHalf, whiteTexture};
    writer.order.stableOrder = 0U;
    writer.mask = render::SpriteMask2D{render::SpriteMaskMode::Write, 3U};

    // Transparent but otherwise ordinary Sprite proves `None` remains valid inside a masked pass.
    // It must use the stencil-target-compatible unmasked pipeline without changing stencil state.
    render::SpritePresentationRenderData unmaskedInMaskedPass{full, transparentTexture};
    unmaskedInMaskedPass.order.stableOrder = 1U;

    render::SpritePresentationRenderData inside{full, greenTexture};
    inside.order.stableOrder = 2U;
    inside.mask = render::SpriteMask2D{render::SpriteMaskMode::TestInside, 3U};
    const std::array<render::SpritePresentationRenderData, 3U> insideDraws{
        inside,
        unmaskedInMaskedPass,
        writer,
    };
    const std::filesystem::path insidePath = temp / "trace2d_sprite_sr4_mask_inside.bmp";
    const render::CapturedFrame insideCapture = Capture(
        renderer,
        camera,
        insideDraws,
        insidePath,
        102U);
    ExpectPrimaryNear(PixelAt(insideCapture, 24U, 32U), 0U, 255U, 0U);
    ExpectPrimaryNear(PixelAt(insideCapture, 40U, 32U), 0U, 0U, 0U);

    render::SpritePresentationRenderData outside{full, blueTexture};
    outside.order.stableOrder = 2U;
    outside.mask = render::SpriteMask2D{render::SpriteMaskMode::TestOutside, 3U};
    const std::array<render::SpritePresentationRenderData, 3U> outsideDraws{
        outside,
        unmaskedInMaskedPass,
        writer,
    };
    const std::filesystem::path outsidePath = temp / "trace2d_sprite_sr4_mask_outside.bmp";
    const render::CapturedFrame outsideCapture = Capture(
        renderer,
        camera,
        outsideDraws,
        outsidePath,
        103U);
    ExpectPrimaryNear(PixelAt(outsideCapture, 24U, 32U), 0U, 0U, 0U);
    ExpectPrimaryNear(PixelAt(outsideCapture, 40U, 32U), 0U, 0U, 255U);

    // Warm resources must remain stable and normal rendering must not introduce observation stalls.
    renderer.RenderFrame(camera, outsideDraws);
    const render::RenderMetrics firstWarmMetrics = renderer.Metrics();
    EXPECT_EQ(firstWarmMetrics.spriteSamplerCreations, 2U);
    EXPECT_EQ(firstWarmMetrics.spritePipelineCreations, 17U);
    EXPECT_EQ(firstWarmMetrics.spriteMaskTargetCreations, 1U);
    const std::uint64_t readbacksBeforeNormalRepeat = firstWarmMetrics.explicitGpuReadbacks;
    const std::uint64_t waitsBeforeNormalRepeat = firstWarmMetrics.explicitGpuFenceWaits;

    renderer.RenderFrame(camera, outsideDraws);
    const render::RenderMetrics secondWarmMetrics = renderer.Metrics();
    EXPECT_EQ(secondWarmMetrics.spriteSamplerCreations, firstWarmMetrics.spriteSamplerCreations);
    EXPECT_EQ(secondWarmMetrics.spritePipelineCreations, firstWarmMetrics.spritePipelineCreations);
    EXPECT_EQ(secondWarmMetrics.spriteMaskTargetCreations, firstWarmMetrics.spriteMaskTargetCreations);
    EXPECT_EQ(secondWarmMetrics.spriteVertexCapacitySprites, firstWarmMetrics.spriteVertexCapacitySprites);
    EXPECT_EQ(secondWarmMetrics.explicitGpuReadbacks, readbacksBeforeNormalRepeat);
    EXPECT_EQ(secondWarmMetrics.explicitGpuFenceWaits, waitsBeforeNormalRepeat);

    renderer.DestroyTexture(transparentTexture);
    renderer.DestroyTexture(whiteTexture);
    renderer.DestroyTexture(blueTexture);
    renderer.DestroyTexture(greenTexture);
    renderer.DestroyTexture(redTexture);

    for (const std::filesystem::path& path : {painterPath, groupPath, insidePath, outsidePath})
    {
        RemoveFile(path);
    }
}
