#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/render/SpritePresentation2D.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
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

[[nodiscard]] trace2d::assets::SpriteAsset MakeCw90Asset()
{
    using namespace trace2d::assets;

    SpriteAsset asset{};
    asset.id = "sprites/sr8-gpu-conformance.sprite.toml";
    asset.sampling = SpriteSampling::Nearest;
    asset.pages = {
        SpriteAtlasPage{
            "page",
            "textures/sr8-gpu-conformance.png",
            SpritePixelSize{1U, 2U},
            SpriteColorSpace::Linear,
            SpriteAlphaMode::Straight,
        },
    };
    asset.regions = {
        SpriteRegion{
            "frame",
            "page",
            SpritePixelSize{4U, 3U},
            SpritePixelOffset{1U, 1U},
            SpritePixelSize{2U, 1U},
            SpritePixelRect{0U, 0U, 1U, 2U},
            SpriteRationalPivot{4, 3, 2},
            SpritePackedRotation::Cw90,
        },
    };
    return asset;
}

void RemoveFile(const std::filesystem::path& path)
{
    std::error_code error{};
    std::filesystem::remove(path, error);
}
} // namespace

TEST(SpriteRendererGpuConformanceTests, Sr8TrimPivotCw90PresentationMatchesCanonicalGeometry)
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
    platformConfig.windowTitle = "Trace2D Sprite SR8 GPU conformance";
    platform::Platform platform{platformConfig};

    render::RendererConfig rendererConfig{};
    rendererConfig.enableDebugValidation = true;
    rendererConfig.clearColor = render::ClearColor{0.0F, 0.0F, 0.0F, 1.0F};
    render::Renderer renderer{rendererConfig, platform};

    const assets::SpriteAsset asset = MakeCw90Asset();
    render::ResolvedSpriteRegion selection{};
    ASSERT_TRUE(render::ResolveSpriteRegionByIndices(
        &asset, 0U, 0U, selection).Succeeded());

    scene::SpritePose2D pose{};
    render::SpriteDrawQuad canonicalDraw{};
    ASSERT_TRUE(render::BuildSpriteDrawQuad(
        selection, pose, 1.0F, canonicalDraw).Succeeded());
    EXPECT_EQ(canonicalDraw.topLeft.position, (render::Float2{-1.0F, 0.5F}));
    EXPECT_EQ(canonicalDraw.bottomRight.position, (render::Float2{1.0F, -0.5F}));
    EXPECT_EQ(canonicalDraw.topLeft.uv, (render::Float2{1.0F, 0.0F}));
    EXPECT_EQ(canonicalDraw.bottomRight.uv, (render::Float2{0.0F, 1.0F}));

    render::SpriteAppearance2D appearance{};
    appearance.sampling = render::SpriteAppearanceSampling::Nearest;
    render::SpritePresentation2D presentation{};
    ASSERT_TRUE(render::BuildSpritePresentation2D(
        selection, pose, 1.0F, appearance, presentation).Succeeded());

    // Physical storage is 1x2 after clockwise packing: top red, bottom green. SR2's UV
    // permutation must reconstruct a logical 2x1 Sprite with red on the left and green on the right.
    constexpr std::array<std::uint8_t, 8U> PackedPixels{
        255U, 0U, 0U, 255U,
        0U, 255U, 0U, 255U,
    };
    const render::TextureHandle texture = renderer.CreateSpriteTextureRgba8(
        render::Rgba8TextureData{1U, 2U, PackedPixels},
        render::SpriteTextureEncoding::Linear);

    render::SpritePresentationRenderData sprite{};
    sprite.presentation = presentation;
    sprite.texture = texture;

    const render::OrthographicCamera camera{{0.0F, 0.0F}, 4.0F};
    renderer.RenderFrame(camera, sprite);
    const render::RenderMetrics beforeCapture = renderer.Metrics();
    EXPECT_EQ(beforeCapture.explicitGpuReadbacks, 0U);
    EXPECT_EQ(beforeCapture.explicitGpuFenceWaits, 0U);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "trace2d_sprite_sr8_cw90.bmp";
    const render::CapturedFrame frame = renderer.CaptureFrame(
        render::CaptureRequest{800U, path, render::CaptureImageFormat::Bmp},
        camera,
        sprite);
    ASSERT_EQ(frame.width, 64U);
    ASSERT_EQ(frame.height, 64U);

    ExpectNear(PixelAt(frame, 24U, 32U), 255U, 0U, 0U);
    ExpectNear(PixelAt(frame, 40U, 32U), 0U, 255U, 0U);

    const render::RenderMetrics afterCapture = renderer.Metrics();
    EXPECT_GT(afterCapture.explicitGpuReadbacks, beforeCapture.explicitGpuReadbacks);
    EXPECT_GT(afterCapture.explicitGpuFenceWaits, beforeCapture.explicitGpuFenceWaits);

    RemoveFile(path);
}
