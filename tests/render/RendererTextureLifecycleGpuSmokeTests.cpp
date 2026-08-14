#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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

[[nodiscard]] trace2d::assets::TextureResource MakeCanonicalTexture(
    const std::array<std::uint8_t, 4>& pixel)
{
    trace2d::assets::TextureResource resource{};
    resource.width = 1U;
    resource.height = 1U;
    resource.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
    resource.alphaMode = trace2d::assets::TextureResourceAlphaMode::Straight;
    resource.cpuRetention = trace2d::assets::CpuRetentionPolicy::Reacquirable;
    resource.retentionReason = "GPU smoke fixture can recreate canonical pixels";
    resource.canonicalRgba8.assign(pixel.begin(), pixel.end());
    return resource;
}

TEST(RendererTextureLifecycleGpuSmokeTests, ReusedCanonicalSlotRejectsStaleGenerationWithoutDestroyingReplacement)
{
    if (!GpuSmokeEnabled())
    {
        GTEST_SKIP() << "Set TRACE2D_RUN_GPU_SMOKE=1 on a machine with a presentation GPU to run this test.";
    }

    using namespace trace2d;

    platform::PlatformConfig platformConfig{};
    platformConfig.mode = platform::StartupMode::Windowed;
    platformConfig.windowWidth = 64;
    platformConfig.windowHeight = 64;
    platformConfig.windowTitle = "Trace2D R0 renderer texture lifecycle GPU smoke";
    platform::Platform platform{platformConfig};

    render::RendererConfig rendererConfig{};
    rendererConfig.enableDebugValidation = true;
    render::Renderer renderer{rendererConfig, platform};
    assets::ResourceRegistry resources{"."};

    constexpr std::array<std::uint8_t, 4> WhitePixel{255U, 255U, 255U, 255U};
    const render::Rgba8TextureData textureData{1U, 1U, WhitePixel};

    const auto firstPublished = resources.PublishTexture(
        "tests/render/lifecycle/first.rgba8",
        MakeCanonicalTexture(WhitePixel));
    ASSERT_TRUE(firstPublished.Succeeded());
    const render::TextureHandle first = firstPublished.handle;
    ASSERT_EQ(renderer.CreateTextureRgba8(first, textureData), first);

    renderer.DestroyTexture(first);
    ASSERT_TRUE(resources.Unload(first.Untyped()).Succeeded());
    EXPECT_EQ(resources.Resolve(first), nullptr);

    const auto replacementPublished = resources.PublishTexture(
        "tests/render/lifecycle/replacement.rgba8",
        MakeCanonicalTexture(WhitePixel));
    ASSERT_TRUE(replacementPublished.Succeeded());
    const render::TextureHandle replacement = replacementPublished.handle;
    ASSERT_EQ(replacement.slot, first.slot);
    ASSERT_NE(replacement.generation, first.generation);
    ASSERT_EQ(renderer.CreateTextureRgba8(replacement, textureData), replacement);

    // Stale destruction must not release the newer generation now occupying the canonical slot.
    renderer.DestroyTexture(first);

    const render::OrthographicCamera camera{{0.0F, 0.0F}, 2.0F};
    const render::SpriteRenderData staleSprite{
        {0.0F, 0.0F},
        {0.5F, 0.5F},
        first,
        0,
        0U,
    };
    const render::SpriteRenderData liveSprite{
        {0.0F, 0.0F},
        {0.5F, 0.5F},
        replacement,
        0,
        1U,
    };

    EXPECT_THROW(renderer.RenderFrame(camera, staleSprite), std::invalid_argument);
    EXPECT_NO_THROW(renderer.RenderFrame(camera, liveSprite));

    renderer.DestroyTexture(replacement);
    EXPECT_TRUE(resources.Unload(replacement.Untyped()).Succeeded());
}
} // namespace
