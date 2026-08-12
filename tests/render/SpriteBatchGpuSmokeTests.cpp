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

[[nodiscard]] trace2d::assets::SpriteAsset MakeAsset()
{
    using namespace trace2d::assets;

    SpriteAsset asset{};
    asset.id = "sprites/sr7-gpu-smoke.sprite.toml";
    asset.sampling = SpriteSampling::Nearest;
    asset.pages = {
        SpriteAtlasPage{
            "page",
            "textures/sr7-gpu-smoke.png",
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

[[nodiscard]] trace2d::render::SpriteAppearance2D MakeAppearance(
    const float red,
    const float green,
    const float blue,
    const float opacity)
{
    trace2d::render::SpriteAppearance2D appearance{};
    appearance.tint = trace2d::render::SpriteLinearRgba{red, green, blue, 1.0F};
    appearance.opacity = opacity;
    appearance.sampling = trace2d::render::SpriteAppearanceSampling::Nearest;
    return appearance;
}

[[nodiscard]] trace2d::render::SpritePresentationRenderData BuildExactQuad(
    const trace2d::render::ResolvedSpriteRegion& selection,
    const trace2d::render::TextureHandle texture,
    const trace2d::render::SpritePixelPerfectViewport2D& viewport,
    const trace2d::scene::Vector2 position,
    const trace2d::render::SpriteAppearance2D& appearance,
    const std::uint64_t stableOrder)
{
    using namespace trace2d;

    scene::SpritePose2D pose{};
    pose.transform.position = position;
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
        appearance,
        presentation).Succeeded());
    EXPECT_EQ(presentation.appearance.sampler, render::SpriteSamplerCompatibility::Nearest);

    render::SpritePresentationRenderData data{};
    data.presentation = presentation;
    data.texture = texture;
    data.order.stableOrder = stableOrder;
    data.pixelPerfectViewport = &viewport;
    return data;
}

[[nodiscard]] trace2d::render::SpritePresentationRenderData BuildMaskWriter(
    const trace2d::render::ResolvedSpriteRegion& selection,
    const trace2d::render::TextureHandle texture,
    const trace2d::render::SpritePixelPerfectViewport2D& viewport)
{
    trace2d::render::SpritePresentationRenderData data = BuildExactQuad(
        selection,
        texture,
        viewport,
        trace2d::scene::Vector2{-1.0F, 0.5F},
        MakeAppearance(1.0F, 1.0F, 1.0F, 1.0F),
        0U);
    data.mask = trace2d::render::SpriteMask2D{
        trace2d::render::SpriteMaskMode::Write,
        7U,
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
        MakeAppearance(1.0F, 1.0F, 1.0F, 1.0F),
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
    data.mask = render::SpriteMask2D{render::SpriteMaskMode::TestInside, 7U};
    data.geometryKind = render::SpritePresentationGeometryKind::PrimitivePatches;
    data.primitivePatches = scratch.first(outPatchCount);
    data.pixelPerfectViewport = &viewport;
    return data;
}

[[nodiscard]] trace2d::render::CapturedFrame Capture(
    trace2d::render::Renderer& renderer,
    const trace2d::render::OrthographicCamera& camera,
    const std::span<const trace2d::render::SpritePresentationRenderData> sprites,
    const std::filesystem::path& path,
    const std::uint64_t simulationFrame)
{
    return renderer.CaptureFrame(
        trace2d::render::CaptureRequest{
            simulationFrame,
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

TEST(SpriteBatchGpuSmokeTests, Sr7BatchesCullsPreservesAppearanceMaskPrimitivePixelPerfectAndReuse)
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
    platformConfig.windowTitle = "Trace2D Sprite SR7 GPU smoke";
    platform::Platform platform{platformConfig};

    render::RendererConfig rendererConfig{};
    rendererConfig.enableDebugValidation = true;
    rendererConfig.clearColor = render::ClearColor{0.0F, 0.0F, 0.0F, 1.0F};
    render::Renderer renderer{rendererConfig, platform};

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

    const assets::SpriteAsset asset = MakeAsset();
    const render::ResolvedSpriteRegion selection = ResolveOnlyRegion(asset);
    constexpr std::array<std::uint8_t, 8U> RedGreen{
        255U, 0U, 0U, 255U,
        0U, 255U, 0U, 255U,
    };
    constexpr std::array<std::uint8_t, 8U> WhiteWhite{
        255U, 255U, 255U, 255U,
        255U, 255U, 255U, 255U,
    };
    const render::TextureHandle textureA = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{2U, 1U, RedGreen},
        render::SpriteTextureEncoding::Linear);
    const render::TextureHandle textureB = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{2U, 1U, WhiteWhite},
        render::SpriteTextureEncoding::Linear);

    const render::SpritePresentationRenderData red = BuildExactQuad(
        selection,
        textureA,
        viewport,
        scene::Vector2{-5.0F, 1.5F},
        MakeAppearance(1.0F, 0.0F, 0.0F, 0.5F),
        0U);
    const render::SpritePresentationRenderData culled = BuildExactQuad(
        selection,
        textureA,
        viewport,
        scene::Vector2{20.0F, 1.5F},
        MakeAppearance(0.0F, 0.0F, 1.0F, 1.0F),
        1U);
    const render::SpritePresentationRenderData green = BuildExactQuad(
        selection,
        textureA,
        viewport,
        scene::Vector2{1.0F, 1.5F},
        MakeAppearance(0.0F, 1.0F, 0.0F, 1.0F),
        2U);
    const render::SpritePresentationRenderData blueDifferentTexture = BuildExactQuad(
        selection,
        textureB,
        viewport,
        scene::Vector2{4.0F, 1.5F},
        MakeAppearance(0.0F, 0.0F, 1.0F, 1.0F),
        3U);
    const std::array<render::SpritePresentationRenderData, 4U> batched{
        green,
        blueDifferentTexture,
        culled,
        red,
    };

    const std::filesystem::path temp = std::filesystem::temp_directory_path();
    const std::filesystem::path batchPath = temp / "trace2d_sprite_sr7_batch.bmp";
    const render::RenderMetrics beforeBatchCapture = renderer.Metrics();
    const render::CapturedFrame batchCapture = Capture(
        renderer,
        camera,
        batched,
        batchPath,
        700U);
    const render::RenderMetrics afterBatchCapture = renderer.Metrics();

    EXPECT_EQ(
        afterBatchCapture.spritePresentationSprites,
        beforeBatchCapture.spritePresentationSprites + 4U);
    EXPECT_EQ(
        afterBatchCapture.spritePresentationVisibleSprites,
        beforeBatchCapture.spritePresentationVisibleSprites + 3U);
    EXPECT_EQ(
        afterBatchCapture.spritePresentationCulledSprites,
        beforeBatchCapture.spritePresentationCulledSprites + 1U);
    EXPECT_EQ(
        afterBatchCapture.spritePresentationDrawCalls,
        beforeBatchCapture.spritePresentationDrawCalls + 2U);
    EXPECT_EQ(
        afterBatchCapture.spritePresentationCompatibilityRuns,
        beforeBatchCapture.spritePresentationCompatibilityRuns + 2U);
    EXPECT_EQ(
        afterBatchCapture.spritePresentationUploadedQuads,
        beforeBatchCapture.spritePresentationUploadedQuads + 3U);
    EXPECT_EQ(
        afterBatchCapture.spritePresentationUploadedVertexBytes,
        beforeBatchCapture.spritePresentationUploadedVertexBytes + 3U * 6U * 48U);
    EXPECT_GE(afterBatchCapture.spriteVertexCapacitySprites, 3U);
    EXPECT_EQ(
        afterBatchCapture.spriteVertexCapacityBytes,
        afterBatchCapture.spriteVertexCapacitySprites * 6U * 48U);

    const std::uint32_t scale = viewport.integerScale;
    const std::uint32_t y = viewport.contentRect.y + 3U * scale + scale / 2U;
    const std::uint32_t redX = viewport.contentRect.x + 3U * scale + scale / 2U;
    // The green Sprite shares textureA with the red Sprite, so probe its second source texel
    // (green) rather than its first source texel (red) when validating the green tint payload.
    const std::uint32_t greenX = viewport.contentRect.x + 10U * scale + scale / 2U;
    const std::uint32_t blueX = viewport.contentRect.x + 12U * scale + scale / 2U;
    ExpectNear(PixelAt(batchCapture, redX, y), 128U, 0U, 0U);
    ExpectNear(PixelAt(batchCapture, greenX, y), 0U, 255U, 0U);
    ExpectNear(PixelAt(batchCapture, blueX, y), 0U, 0U, 255U);

    const std::uint64_t capacityBeforeRepeat = afterBatchCapture.spriteVertexCapacitySprites;
    const std::uint64_t capacityBytesBeforeRepeat = afterBatchCapture.spriteVertexCapacityBytes;
    const std::uint64_t samplersBeforeRepeat = afterBatchCapture.spriteSamplerCreations;
    const std::uint64_t pipelinesBeforeRepeat = afterBatchCapture.spritePipelineCreations;
    const std::uint64_t readbacksBeforeRepeat = afterBatchCapture.explicitGpuReadbacks;
    const std::uint64_t waitsBeforeRepeat = afterBatchCapture.explicitGpuFenceWaits;
    const std::uint64_t drawsBeforeRepeat = afterBatchCapture.spritePresentationDrawCalls;

    renderer.RenderFrame(camera, batched);
    renderer.RenderFrame(camera, batched);
    const render::RenderMetrics afterRepeats = renderer.Metrics();
    EXPECT_EQ(afterRepeats.spritePresentationDrawCalls, drawsBeforeRepeat + 4U);
    EXPECT_EQ(afterRepeats.spriteVertexCapacitySprites, capacityBeforeRepeat);
    EXPECT_EQ(afterRepeats.spriteVertexCapacityBytes, capacityBytesBeforeRepeat);
    EXPECT_EQ(afterRepeats.spriteSamplerCreations, samplersBeforeRepeat);
    EXPECT_EQ(afterRepeats.spritePipelineCreations, pipelinesBeforeRepeat);
    EXPECT_EQ(afterRepeats.explicitGpuReadbacks, readbacksBeforeRepeat);
    EXPECT_EQ(afterRepeats.explicitGpuFenceWaits, waitsBeforeRepeat);

    std::array<render::SpritePrimitivePatch2D, 8U> patchScratch{};
    std::size_t patchCount = 0U;
    const render::SpritePresentationRenderData writer =
        BuildMaskWriter(selection, textureA, viewport);
    const render::SpritePresentationRenderData tiled = BuildMaskedTiled(
        selection,
        textureA,
        viewport,
        patchScratch,
        patchCount);
    const std::array<render::SpritePresentationRenderData, 2U> maskedTiled{
        tiled,
        writer,
    };

    const std::filesystem::path maskedPath = temp / "trace2d_sprite_sr7_masked_tiled.bmp";
    const render::RenderMetrics beforeMaskedCapture = renderer.Metrics();
    const render::CapturedFrame maskedCapture = Capture(
        renderer,
        camera,
        maskedTiled,
        maskedPath,
        701U);
    const render::RenderMetrics afterMaskedCapture = renderer.Metrics();
    EXPECT_EQ(
        afterMaskedCapture.spritePresentationDrawCalls,
        beforeMaskedCapture.spritePresentationDrawCalls + 2U);
    EXPECT_EQ(
        afterMaskedCapture.spritePresentationUploadedQuads,
        beforeMaskedCapture.spritePresentationUploadedQuads + patchCount + 1U);
    EXPECT_GE(afterMaskedCapture.spriteMaskTargetCreations, 1U);

    const std::uint32_t insideX = viewport.contentRect.x + 7U * scale + scale / 2U;
    const std::uint32_t outsideX = viewport.contentRect.x + 9U * scale + scale / 2U;
    const std::uint32_t maskedY = viewport.contentRect.y + 4U * scale + scale / 2U;
    ExpectNear(PixelAt(maskedCapture, insideX, maskedY), 255U, 0U, 0U);
    ExpectNear(PixelAt(maskedCapture, outsideX, maskedY), 0U, 0U, 0U);

    renderer.DestroyTexture(textureB);
    renderer.DestroyTexture(textureA);
    RemoveFile(batchPath);
    RemoveFile(maskedPath);
}