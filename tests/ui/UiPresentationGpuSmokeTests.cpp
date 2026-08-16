#include <trace2d/agent/Inspection.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/ui/UiPresentation2D.hpp>
#include <trace2d/ui/UiRaster.hpp>
#include <trace2d/ui/UiText.hpp>
#include <trace2d/ui/UiTextLayout.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace trace2d::ui
{
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

[[nodiscard]] std::vector<std::uint8_t> LoadGpuSmokeFont()
{
    std::ifstream input(std::filesystem::path{TRACE2D_UI_TEST_FONT_PATH}, std::ios::binary);
    EXPECT_TRUE(input.is_open());
    return std::vector<std::uint8_t>(
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

[[nodiscard]] render::TextureHandle PublishLinearTexture(
    assets::ResourceRegistry& resources,
    const std::string_view reference,
    const std::uint32_t width,
    const std::uint32_t height,
    const std::span<const std::uint8_t> rgba,
    const assets::CpuRetentionPolicy retention = assets::CpuRetentionPolicy::Releasable)
{
    assets::TextureResource texture{};
    texture.width = width;
    texture.height = height;
    texture.colorSpace = assets::TextureResourceColorSpace::Linear;
    texture.alphaMode = assets::TextureResourceAlphaMode::Straight;
    texture.cpuRetention = retention;
    texture.retentionReason = "U15 GPU smoke canonical texture";
    texture.canonicalRgba8.assign(rgba.begin(), rgba.end());
    const auto published = resources.PublishTexture(reference, std::move(texture));
    EXPECT_TRUE(published.Succeeded());
    return published.handle;
}

[[nodiscard]] text::GlyphAtlasPrepareResult PrepareGpuSmokeAtlas(
    assets::ResourceRegistry& resources)
{
    assets::FontResource font{};
    font.canonicalBytes = LoadGpuSmokeFont();
    const auto published = resources.PublishFont("content/fonts/u15-gpu.ttf", std::move(font));
    EXPECT_TRUE(published.Succeeded());
    if (!published.Succeeded())
    {
        return {};
    }
    return text::PrepareGlyphAtlas(
        resources,
        published.handle,
        text::GlyphAtlasConfig{512U, 256U, 20U, 1U, 128U});
}

[[nodiscard]] std::size_t ElementIndex(const UiDocument& document, const std::string_view id)
{
    const std::span<const UiElement> elements = document.Elements();
    const UiElement* const element = document.Find(id);
    EXPECT_NE(element, nullptr);
    if (element == nullptr)
    {
        return InvalidUiElementIndex;
    }
    return static_cast<std::size_t>(element - elements.data());
}

[[nodiscard]] UiTextLayoutCachePrepareResult PrepareGpuTextCache()
{
    return PrepareUiTextLayoutCache(
        UiTextLayoutCacheConfig{
            .text = text::TextLayoutCacheConfig{
                .layout = text::TextLayoutRunConfig{64U, 8U},
                .maxFallbackFonts = 1U,
            },
            .maxComposedUtf8Bytes = 128U,
        });
}

struct CaptureEvidence final
{
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    std::vector<std::uint8_t> rgba{};
};

void CaptureHud(
    const int requestedWidth,
    const int requestedHeight,
    const std::uint64_t frameNumber,
    assets::ResourceRegistry& resources,
    UiDocument& document,
    UiPresentationCache2D& presentationCache,
    const UiSolidTextureBinding2D solid,
    const std::span<const UiTextPresentationInput2D> textInputs,
    const render::TextureHandle imageTexture,
    const std::span<const std::uint8_t> imagePixels,
    const text::GlyphAtlasTextureBinding2D& glyphBinding,
    const std::span<const std::uint8_t> glyphPixels,
    const std::uint32_t glyphWidth,
    const std::uint32_t glyphHeight,
    const bool releaseImageCpuPayload,
    const bool proveUnchangedReuse,
    CaptureEvidence& outEvidence)
{
    platform::PlatformConfig platformConfig{};
    platformConfig.mode = platform::StartupMode::Windowed;
    platformConfig.windowWidth = requestedWidth;
    platformConfig.windowHeight = requestedHeight;
    platformConfig.windowTitle = "Trace2D U15 UI presentation GPU smoke";
    platform::Platform platform{platformConfig};

    render::RendererConfig rendererConfig{};
    rendererConfig.enableDebugValidation = true;
    rendererConfig.clearColor = render::ClearColor{0.0F, 0.0F, 0.0F, 1.0F};
    render::Renderer renderer{rendererConfig, platform};
    renderer.RenderFrame();
    const render::RenderMetrics initial = renderer.Metrics();
    ASSERT_GT(initial.lastTargetWidth, 0U);
    ASSERT_GT(initial.lastTargetHeight, 0U);

    constexpr std::array<std::uint8_t, 4U> White{255U, 255U, 255U, 255U};
    EXPECT_EQ(
        renderer.CreateSpriteTextureRgba8(
            solid.texture,
            render::Rgba8TextureData{1U, 1U, White},
            render::SpriteTextureEncoding::Linear),
        solid.texture);
    EXPECT_EQ(
        renderer.CreateSpriteTextureRgba8(
            imageTexture,
            render::Rgba8TextureData{2U, 2U, imagePixels},
            render::SpriteTextureEncoding::Linear),
        imageTexture);
    EXPECT_EQ(
        renderer.CreateSpriteTextureRgba8(
            glyphBinding.texture,
            render::Rgba8TextureData{glyphWidth, glyphHeight, glyphPixels},
            render::SpriteTextureEncoding::Linear),
        glyphBinding.texture);

    if (releaseImageCpuPayload)
    {
        ASSERT_TRUE(resources.SetTextureRendererResidency(
            imageTexture,
            true,
            imagePixels.size()).Succeeded());
        ASSERT_TRUE(resources.ReleaseTextureCpuPayload(imageTexture).Succeeded());
        const assets::TextureResource* const released = resources.Resolve(imageTexture);
        ASSERT_NE(released, nullptr);
        EXPECT_TRUE(released->canonicalRgba8.empty());
    }

    render::Viewport2D authoredViewport{};
    authoredViewport.semanticId = "hud";
    authoredViewport.logicalWidth = document.Width();
    authoredViewport.logicalHeight = document.Height();
    authoredViewport.scaleMode = render::ViewportScaleMode2D::Fit;
    const render::ViewportResolveResult2D resolved = render::ResolveViewport2D(
        authoredViewport,
        initial.lastTargetWidth,
        initial.lastTargetHeight);
    ASSERT_TRUE(resolved.Succeeded());

    const UiPresentationUpdateResult update = presentationCache.Update(
        document,
        resources,
        resolved.viewport,
        solid,
        textInputs);
    ASSERT_TRUE(update.Succeeded());
    EXPECT_FALSE(update.reused);
    const UiPresentationFrame2D frame = presentationCache.Frame();
    ASSERT_FALSE(frame.presentations.empty());

    const std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("trace2d_u15_ui_" + std::to_string(frameNumber) + ".bmp");
    const render::CapturedFrame captured = renderer.CaptureFrame(
        render::CaptureRequest{frameNumber, path, render::CaptureImageFormat::Bmp},
        frame.camera,
        frame.presentations);
    EXPECT_EQ(captured.width, initial.lastTargetWidth);
    EXPECT_EQ(captured.height, initial.lastTargetHeight);

    bool sawImageHandle = false;
    for (const render::SpritePresentationRenderData& presentation : frame.presentations)
    {
        sawImageHandle = sawImageHandle || presentation.texture == imageTexture;
    }
    EXPECT_TRUE(sawImageHandle);

    const render::Float2 imageCenterLogical{272.0F, 40.0F};
    const float sampleXf = resolved.viewport.contentRect.origin.x +
        imageCenterLogical.x * resolved.viewport.viewportToPresentationScale.x;
    const float sampleYf = resolved.viewport.contentRect.origin.y +
        imageCenterLogical.y * resolved.viewport.viewportToPresentationScale.y;
    const std::uint32_t sampleX = static_cast<std::uint32_t>(sampleXf);
    const std::uint32_t sampleY = static_cast<std::uint32_t>(sampleYf);
    ASSERT_LT(sampleX, captured.width);
    ASSERT_LT(sampleY, captured.height);
    const std::size_t sampleOffset =
        (static_cast<std::size_t>(sampleY) * captured.width + sampleX) * 4U;
    ASSERT_LT(sampleOffset + 3U, captured.rgba8Pixels.size());
    EXPECT_GT(captured.rgba8Pixels[sampleOffset], 220U);
    EXPECT_LT(captured.rgba8Pixels[sampleOffset + 1U], 32U);
    EXPECT_LT(captured.rgba8Pixels[sampleOffset + 2U], 32U);

    if (proveUnchangedReuse)
    {
        const UiPresentationMetrics beforeReuse = presentationCache.Metrics();
        const std::uint64_t atlasRevision = glyphBinding.pixelRevision;
        const UiPresentationUpdateResult unchanged = presentationCache.Update(
            document,
            resources,
            resolved.viewport,
            solid,
            textInputs);
        ASSERT_TRUE(unchanged.Succeeded());
        EXPECT_TRUE(unchanged.reused);
        EXPECT_EQ(presentationCache.Metrics().rebuilds, beforeReuse.rebuilds);
        EXPECT_EQ(presentationCache.Metrics().cacheHits, beforeReuse.cacheHits + 1U);
        EXPECT_EQ(glyphBinding.pixelRevision, atlasRevision);

        const std::filesystem::path secondPath = std::filesystem::temp_directory_path() /
            ("trace2d_u15_ui_reuse_" + std::to_string(frameNumber) + ".bmp");
        const UiPresentationFrame2D retainedFrame = presentationCache.Frame();
        const render::CapturedFrame second = renderer.CaptureFrame(
            render::CaptureRequest{frameNumber + 1U, secondPath, render::CaptureImageFormat::Bmp},
            retainedFrame.camera,
            retainedFrame.presentations);
        EXPECT_EQ(second.rgba8Pixels, captured.rgba8Pixels);
        std::error_code removeError{};
        std::filesystem::remove(secondPath, removeError);
    }

    std::error_code removeError{};
    std::filesystem::remove(path, removeError);
    outEvidence = CaptureEvidence{captured.width, captured.height, captured.rgba8Pixels};
}
} // namespace

TEST(UiPresentationGpuSmokeTests, AuthoredHudUsesProductionRendererAtTwoTargetsAndSurvivesImageCpuRelease)
{
    if (!GpuSmokeEnabled())
    {
        GTEST_SKIP() << "Set TRACE2D_RUN_GPU_SMOKE=1 on a Windows machine with a presentation GPU to run this test.";
    }

    assets::ResourceRegistry resources{"."};
    constexpr std::array<std::uint8_t, 4U> White{255U, 255U, 255U, 255U};
    constexpr std::array<std::uint8_t, 16U> RedImage{
        255U, 0U, 0U, 255U,
        255U, 0U, 0U, 255U,
        255U, 0U, 0U, 255U,
        255U, 0U, 0U, 255U,
    };
    const render::TextureHandle solidHandle = PublishLinearTexture(
        resources,
        "generated/ui/u15-white",
        1U,
        1U,
        White);
    const render::TextureHandle imageHandle = PublishLinearTexture(
        resources,
        "content/ui/u15-portrait",
        2U,
        2U,
        RedImage);
    UiSolidTextureBinding2D solid{};
    ASSERT_TRUE(ResolveUiSolidTextureBinding2D(resources, solidHandle, solid));

    constexpr std::string_view AuthoredHud = R"toml(
format_version = 1

[canvas]
width = 320
height = 180

[[elements]]
id = "root"
kind = "panel"
bounds = [0, 0, 320, 180]
name = "HUD"

[[elements]]
id = "health"
kind = "progress"
parent = "root"
bounds = [8, 8, 120, 12]
name = "Health"
progress_value = 50
progress_maximum = 100

[[elements]]
id = "portrait"
kind = "image"
parent = "root"
bounds = [240, 8, 64, 64]
name = "Portrait"
texture = "content/ui/u15-portrait"

[[elements]]
id = "resume"
kind = "button"
parent = "root"
bounds = [8, 32, 100, 28]
name = "Resume"
text = "Resume"

[[elements]]
id = "chat"
kind = "text_input"
parent = "root"
bounds = [116, 32, 116, 28]
name = "Chat"
text = "Hello"

[[elements]]
id = "scroll"
kind = "panel"
parent = "root"
bounds = [8, 80, 200, 60]
name = "Inventory"
scroll_content_size = [200, 100]

[[elements]]
id = "scrolled_label"
kind = "label"
parent = "scroll"
bounds = [8, 72, 120, 24]
name = "Scrolled"
text = "More items"
)toml";

    UiLoadResult loaded = LoadUiToml(AuthoredHud, resources, "u15-hud.toml");
    ASSERT_TRUE(loaded.Succeeded());
    ASSERT_TRUE(loaded.document.has_value());
    UiDocument& document = *loaded.document;
    ASSERT_EQ(document.Focus("chat"), UiActionResult::Success);
    ASSERT_EQ(document.ScrollTo("scroll", 0U, 40U), UiActionResult::Success);

    UiRasterImage cpuEvidence{};
    UiRasterMetrics cpuMetrics{};
    ASSERT_TRUE(RasterizeUi(document, resources, cpuEvidence, &cpuMetrics));
    EXPECT_EQ(cpuEvidence.width, 320U);
    EXPECT_EQ(cpuEvidence.height, 180U);
    EXPECT_GT(cpuMetrics.elementsRasterized, 0U);
    agent::AgentFacade facade(nullptr, nullptr, &document);
    const agent::UiTreeResult tree = facade.InspectUi();
    ASSERT_TRUE(tree.Succeeded());
    ASSERT_TRUE(tree.tree.has_value());
    EXPECT_EQ(tree.tree->elements.size(), document.Elements().size());
    const UiElement* const scrolled = document.Find("scrolled_label");
    ASSERT_NE(scrolled, nullptr);
    EXPECT_TRUE(scrolled->clipActive);
    EXPECT_EQ(scrolled->clipBounds, (UiRect{8U, 80U, 200U, 60U}));
    EXPECT_EQ(scrolled->presentationBounds.y, 112);

    text::GlyphAtlasPrepareResult atlas = PrepareGpuSmokeAtlas(resources);
    ASSERT_TRUE(atlas.Succeeded());
    const text::TextFontAtlasRef atlasRef{atlas.atlas.get()};
    const std::span<const text::TextFontAtlasRef> fallback(&atlasRef, 1U);

    UiTextLayoutCachePrepareResult resumeCache = PrepareGpuTextCache();
    UiTextLayoutCachePrepareResult chatCache = PrepareGpuTextCache();
    UiTextLayoutCachePrepareResult scrolledCache = PrepareGpuTextCache();
    ASSERT_TRUE(resumeCache.Succeeded());
    ASSERT_TRUE(chatCache.Succeeded());
    ASSERT_TRUE(scrolledCache.Succeeded());
    ASSERT_TRUE(resumeCache.cache->Update(document, "resume", fallback).Succeeded());
    ASSERT_TRUE(chatCache.cache->Update(document, "chat", fallback).Succeeded());
    ASSERT_TRUE(scrolledCache.cache->Update(document, "scrolled_label", fallback).Succeeded());

    const text::GlyphAtlasConfig atlasConfig = atlas.atlas->Config();
    std::vector<std::uint8_t> atlasPixels(
        static_cast<std::size_t>(atlasConfig.width) * atlasConfig.height * 4U);
    std::size_t requiredBytes = 0U;
    ASSERT_TRUE(text::WriteGlyphAtlasRgba8(*atlas.atlas, atlasPixels, requiredBytes).Succeeded());
    ASSERT_EQ(requiredBytes, atlasPixels.size());
    const render::TextureHandle glyphTexture = PublishLinearTexture(
        resources,
        "generated/ui/u15-glyph-atlas",
        atlasConfig.width,
        atlasConfig.height,
        atlasPixels);
    text::GlyphAtlasTextureBinding2D glyphBinding{};
    ASSERT_TRUE(text::ResolveGlyphAtlasTextureBinding2D(
        *atlas.atlas,
        resources,
        glyphTexture,
        glyphBinding).Succeeded());
    const std::uint64_t glyphPixelRevision = text::GlyphAtlasPixelRevision(*atlas.atlas);

    const std::size_t resumeIndex = ElementIndex(document, "resume");
    const std::size_t chatIndex = ElementIndex(document, "chat");
    const std::size_t scrolledIndex = ElementIndex(document, "scrolled_label");
    ASSERT_LT(resumeIndex, chatIndex);
    ASSERT_LT(chatIndex, scrolledIndex);
    const std::array<UiTextPresentationInput2D, 3U> textInputs{
        UiTextPresentationInput2D{
            resumeIndex,
            resumeCache.cache->Layout(),
            fallback,
            std::span<const text::GlyphAtlasTextureBinding2D>(&glyphBinding, 1U),
            document.Elements()[resumeIndex].displayTextRevision,
        },
        UiTextPresentationInput2D{
            chatIndex,
            chatCache.cache->Layout(),
            fallback,
            std::span<const text::GlyphAtlasTextureBinding2D>(&glyphBinding, 1U),
            document.Elements()[chatIndex].displayTextRevision,
        },
        UiTextPresentationInput2D{
            scrolledIndex,
            scrolledCache.cache->Layout(),
            fallback,
            std::span<const text::GlyphAtlasTextureBinding2D>(&glyphBinding, 1U),
            document.Elements()[scrolledIndex].displayTextRevision,
        },
    };

    UiPresentationCachePrepareResult prepared =
        PrepareUiPresentationCache(UiPresentationCacheConfig{512U});
    ASSERT_TRUE(prepared.Succeeded());

    CaptureEvidence first{};
    CaptureHud(
        640,
        360,
        1500U,
        resources,
        document,
        *prepared.cache,
        solid,
        textInputs,
        imageHandle,
        RedImage,
        glyphBinding,
        atlasPixels,
        atlasConfig.width,
        atlasConfig.height,
        true,
        true,
        first);
    ASSERT_GT(first.width, 0U);
    ASSERT_GT(first.height, 0U);
    EXPECT_EQ(text::GlyphAtlasPixelRevision(*atlas.atlas), glyphPixelRevision);

    const std::uint32_t imageGeneration = imageHandle.generation;
    ASSERT_EQ(document.SetProgress("health", 75U, 100U), UiProgressResult::Success);
    CaptureEvidence second{};
    CaptureHud(
        960,
        540,
        1600U,
        resources,
        document,
        *prepared.cache,
        solid,
        textInputs,
        imageHandle,
        RedImage,
        glyphBinding,
        atlasPixels,
        atlasConfig.width,
        atlasConfig.height,
        false,
        false,
        second);
    ASSERT_GT(second.width, 0U);
    ASSERT_GT(second.height, 0U);
    EXPECT_NE(first.width, second.width);
    EXPECT_NE(first.height, second.height);
    EXPECT_EQ(imageHandle.generation, imageGeneration);
    EXPECT_EQ(glyphBinding.pixelRevision, glyphPixelRevision);
    ASSERT_NE(resources.Resolve(imageHandle), nullptr);
    EXPECT_TRUE(resources.Resolve(imageHandle)->canonicalRgba8.empty());
}
} // namespace trace2d::ui
