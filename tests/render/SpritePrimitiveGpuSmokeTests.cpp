#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
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

[[nodiscard]] trace2d::assets::SpriteAsset MakePrimitiveAsset()
{
    using namespace trace2d::assets;

    SpriteAsset asset{};
    asset.id = "sprites/sr5-gpu-smoke.sprite.toml";
    asset.sampling = SpriteSampling::Linear;
    asset.pages = {
        SpriteAtlasPage{
            "page",
            "textures/sr5-gpu-smoke.png",
            SpritePixelSize{3U, 3U},
            SpriteColorSpace::Linear,
            SpriteAlphaMode::Straight,
        },
    };
    asset.regions = {
        SpriteRegion{
            "panel",
            "page",
            SpritePixelSize{3U, 3U},
            SpritePixelOffset{0U, 0U},
            SpritePixelSize{3U, 3U},
            SpritePixelRect{0U, 0U, 3U, 3U},
            SpriteRationalPivot{3, 3, 2},
            SpritePackedRotation::None,
            SpritePixelBorder{1U, 1U, 1U, 1U},
        },
    };
    return asset;
}

[[nodiscard]] trace2d::assets::SpriteAsset MakeMaskAsset()
{
    using namespace trace2d::assets;

    SpriteAsset asset{};
    asset.id = "sprites/sr5-mask.sprite.toml";
    asset.sampling = SpriteSampling::Nearest;
    asset.pages = {
        SpriteAtlasPage{
            "page",
            "textures/sr5-mask.png",
            SpritePixelSize{1U, 1U},
            SpriteColorSpace::Linear,
            SpriteAlphaMode::Straight,
        },
    };
    asset.regions = {
        SpriteRegion{
            "mask",
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

[[nodiscard]] trace2d::render::ResolvedSpriteRegion ResolveOnlyRegion(
    const trace2d::assets::SpriteAsset& asset)
{
    trace2d::render::ResolvedSpriteRegion selection{};
    EXPECT_TRUE(trace2d::render::ResolveSpriteRegionByIndices(
        &asset, 0U, 0U, selection).Succeeded());
    return selection;
}

[[nodiscard]] trace2d::render::SpritePresentationRenderData BuildPrimitiveRenderData(
    const trace2d::assets::SpriteAsset& asset,
    const trace2d::render::TextureHandle texture,
    std::span<trace2d::render::SpritePrimitivePatch2D> scratch,
    std::size_t& outPatchCount)
{
    using namespace trace2d;

    const render::ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    render::SpriteAppearance2D appearance{};
    appearance.sampling = render::SpriteAppearanceSampling::Linear;

    render::SpriteAppearanceContractData appearanceContract{};
    EXPECT_TRUE(render::ExtractSpriteAppearanceContract(
        selection,
        appearance,
        appearanceContract).Succeeded());

    const render::SpritePrimitive2D primitive{
        render::SpritePrimitiveMode::Tiled,
        render::Float2{5.5F, 3.0F},
    };
    EXPECT_TRUE(render::BuildSpritePrimitivePatches(
        selection,
        scene::SpritePose2D{},
        3.0F,
        primitive,
        scratch,
        outPatchCount).Succeeded());
    EXPECT_EQ(outPatchCount, 18U);

    render::SpritePresentationRenderData data{};
    data.presentation.appearance = appearanceContract;
    data.texture = texture;
    data.geometryKind = render::SpritePresentationGeometryKind::PrimitivePatches;
    data.primitivePatches = scratch.first(outPatchCount);
    return data;
}

[[nodiscard]] trace2d::render::SpritePresentationRenderData BuildMaskWriter(
    const trace2d::assets::SpriteAsset& asset,
    const trace2d::render::TextureHandle texture)
{
    using namespace trace2d;

    const render::ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    scene::SpritePose2D pose{};
    pose.transform.position = scene::Vector2{-0.5F, 0.5F};
    pose.transform.scale = scene::Vector2{0.5F, 1.0F};

    render::SpritePresentation2D presentation{};
    EXPECT_TRUE(render::BuildSpritePresentation2D(
        selection,
        pose,
        1.0F,
        render::SpriteAppearance2D{},
        presentation).Succeeded());

    render::SpritePresentationRenderData data{};
    data.presentation = presentation;
    data.texture = texture;
    data.order.stableOrder = 0U;
    data.mask = render::SpriteMask2D{render::SpriteMaskMode::Write, 9U};
    return data;
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

TEST(SpritePrimitiveGpuSmokeTests, Sr5TiledLinearMaskAndCapacityReuseMatchContract)
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
    platformConfig.windowTitle = "Trace2D Sprite SR5 GPU smoke";
    platform::Platform platform{platformConfig};

    render::RendererConfig rendererConfig{};
    rendererConfig.enableDebugValidation = true;
    rendererConfig.clearColor = render::ClearColor{0.0F, 0.0F, 0.0F, 1.0F};
    render::Renderer renderer{rendererConfig, platform};
    const render::OrthographicCamera camera{{0.0F, 0.0F}, 2.0F};

    const assets::SpriteAsset primitiveAsset = MakePrimitiveAsset();
    const assets::SpriteAsset maskAsset = MakeMaskAsset();

    // 3x3 atlas: all border pixels are red and the one-pixel center cell is green. Linear tiled
    // sampling must clamp each repeated/partial center patch to that green texel rather than blend
    // with the neighboring red border texels.
    constexpr std::array<std::uint8_t, 36U> PanelPixels{
        255U, 0U, 0U, 255U,  255U, 0U, 0U, 255U,  255U, 0U, 0U, 255U,
        255U, 0U, 0U, 255U,    0U, 255U, 0U, 255U,  255U, 0U, 0U, 255U,
        255U, 0U, 0U, 255U,  255U, 0U, 0U, 255U,  255U, 0U, 0U, 255U,
    };
    constexpr std::array<std::uint8_t, 4U> White{255U, 255U, 255U, 255U};

    const render::TextureHandle panelTexture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{3U, 3U, PanelPixels},
        render::SpriteTextureEncoding::Linear);
    const render::TextureHandle maskTexture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{1U, 1U, White},
        render::SpriteTextureEncoding::Linear);

    std::array<render::SpritePrimitivePatch2D, 32U> patchScratch{};
    std::size_t patchCount = 0U;
    render::SpritePresentationRenderData tiled = BuildPrimitiveRenderData(
        primitiveAsset,
        panelTexture,
        patchScratch,
        patchCount);
    tiled.order.stableOrder = 1U;

    const std::filesystem::path temp = std::filesystem::temp_directory_path();
    const std::filesystem::path tiledPath = temp / "trace2d_sprite_sr5_tiled.bmp";
    const std::array<render::SpritePresentationRenderData, 1U> tiledOnly{tiled};
    const render::CapturedFrame tiledCapture = Capture(
        renderer,
        camera,
        tiledOnly,
        tiledPath,
        200U);

    // x=72 lies in the 0.5-source-pixel final partial center tile. Its per-patch sample clamp
    // collapses to the green texel center; x=84 lies in the fixed red right border.
    ExpectNear(PixelAt(tiledCapture, 72U, 48U), 0U, 255U, 0U);
    ExpectNear(PixelAt(tiledCapture, 84U, 48U), 255U, 0U, 0U);

    const render::SpritePresentationRenderData writer = BuildMaskWriter(maskAsset, maskTexture);
    tiled.mask = render::SpriteMask2D{render::SpriteMaskMode::TestInside, 9U};
    const std::array<render::SpritePresentationRenderData, 2U> masked{
        tiled,
        writer,
    };
    const std::filesystem::path maskedPath = temp / "trace2d_sprite_sr5_masked_tiled.bmp";
    const render::CapturedFrame maskedCapture = Capture(
        renderer,
        camera,
        masked,
        maskedPath,
        201U);

    // The mask writer covers x [-0.5, 0]. The same tiled primitive is therefore visible on the
    // left probe and rejected on the symmetric right probe.
    ExpectNear(PixelAt(maskedCapture, 40U, 48U), 0U, 255U, 0U);
    ExpectNear(PixelAt(maskedCapture, 56U, 48U), 0U, 0U, 0U);

    // Ordinary frames after warm-up prove one top-level primitive Sprite remains one draw while
    // persistent GPU capacity/samplers/pipelines/mask target are reused without readback/waits.
    renderer.RenderFrame(camera, tiledOnly);
    const render::RenderMetrics firstWarm = renderer.Metrics();
    const std::uint64_t drawCallsBeforeRepeat = firstWarm.spritePresentationDrawCalls;
    const std::uint64_t spritesBeforeRepeat = firstWarm.spritePresentationSprites;
    const std::uint64_t capacityBeforeRepeat = firstWarm.spriteVertexCapacitySprites;
    const std::uint64_t samplersBeforeRepeat = firstWarm.spriteSamplerCreations;
    const std::uint64_t pipelinesBeforeRepeat = firstWarm.spritePipelineCreations;
    const std::uint64_t maskTargetsBeforeRepeat = firstWarm.spriteMaskTargetCreations;
    const std::uint64_t readbacksBeforeRepeat = firstWarm.explicitGpuReadbacks;
    const std::uint64_t waitsBeforeRepeat = firstWarm.explicitGpuFenceWaits;

    renderer.RenderFrame(camera, tiledOnly);
    const render::RenderMetrics secondWarm = renderer.Metrics();
    EXPECT_EQ(secondWarm.spritePresentationDrawCalls, drawCallsBeforeRepeat + 1U);
    EXPECT_EQ(secondWarm.spritePresentationSprites, spritesBeforeRepeat + 1U);
    EXPECT_EQ(secondWarm.spriteVertexCapacitySprites, capacityBeforeRepeat);
    EXPECT_GE(secondWarm.spriteVertexCapacitySprites, patchCount);
    EXPECT_EQ(secondWarm.spriteSamplerCreations, samplersBeforeRepeat);
    EXPECT_EQ(secondWarm.spritePipelineCreations, pipelinesBeforeRepeat);
    EXPECT_EQ(secondWarm.spriteMaskTargetCreations, maskTargetsBeforeRepeat);
    EXPECT_EQ(secondWarm.explicitGpuReadbacks, readbacksBeforeRepeat);
    EXPECT_EQ(secondWarm.explicitGpuFenceWaits, waitsBeforeRepeat);

    renderer.DestroyTexture(maskTexture);
    renderer.DestroyTexture(panelTexture);
    RemoveFile(tiledPath);
    RemoveFile(maskedPath);
}
