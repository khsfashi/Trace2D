#include "PresentationPlayground.hpp"

#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/input/Input.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/render/SpritePresentation2D.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
using namespace std::chrono_literals;
using namespace trace2d;

struct PixelArt final
{
    std::uint32_t width{0U};
    std::uint32_t height{0U};
    std::vector<std::uint8_t> rgba{};
};

void Put(PixelArt& art, const int x, const int y, const std::array<std::uint8_t, 4U>& color)
{
    if (x < 0 || y < 0 || x >= static_cast<int>(art.width) || y >= static_cast<int>(art.height)) return;
    const std::size_t index =
        (static_cast<std::size_t>(y) * art.width + static_cast<std::size_t>(x)) * 4U;
    std::copy(color.begin(), color.end(), art.rgba.begin() + static_cast<std::ptrdiff_t>(index));
}

void Fill(
    PixelArt& art,
    const int x,
    const int y,
    const int width,
    const int height,
    const std::array<std::uint8_t, 4U>& color)
{
    for (int py = y; py < y + height; ++py)
        for (int px = x; px < x + width; ++px)
            Put(art, px, py, color);
}

[[nodiscard]] PixelArt MakeTargetArt()
{
    constexpr std::array<std::uint8_t, 4U> Transparent{0U, 0U, 0U, 0U};
    constexpr std::array<std::uint8_t, 4U> Ink{13U, 18U, 28U, 255U};
    constexpr std::array<std::uint8_t, 4U> Dark{33U, 73U, 91U, 255U};
    constexpr std::array<std::uint8_t, 4U> Mid{55U, 153U, 164U, 255U};
    constexpr std::array<std::uint8_t, 4U> Light{143U, 230U, 216U, 255U};
    constexpr std::array<std::uint8_t, 4U> Eye{255U, 211U, 98U, 255U};

    PixelArt art{};
    art.width = 16U;
    art.height = 16U;
    art.rgba.resize(16U * 16U * 4U);
    for (std::size_t index = 0U; index < art.rgba.size(); index += 4U)
        std::copy(Transparent.begin(), Transparent.end(), art.rgba.begin() + static_cast<std::ptrdiff_t>(index));

    Fill(art, 4, 2, 8, 2, Ink);
    Fill(art, 2, 4, 12, 8, Ink);
    Fill(art, 4, 12, 8, 2, Ink);
    Fill(art, 4, 4, 8, 8, Dark);
    Fill(art, 5, 4, 6, 5, Mid);
    Fill(art, 6, 5, 4, 2, Light);
    Fill(art, 4, 9, 8, 3, Dark);
    Put(art, 5, 8, Eye);
    Put(art, 10, 8, Eye);
    Put(art, 6, 11, Light);
    Put(art, 9, 11, Light);
    return art;
}

[[nodiscard]] assets::SpriteAsset MakeSpriteAsset(
    const std::string& id,
    const std::string& textureReference,
    const std::uint32_t width,
    const std::uint32_t height)
{
    assets::SpriteAsset asset{};
    asset.id = id;
    asset.sampling = assets::SpriteSampling::Nearest;
    asset.pages = {
        assets::SpriteAtlasPage{
            "page",
            textureReference,
            assets::SpritePixelSize{width, height},
            assets::SpriteColorSpace::Linear,
            assets::SpriteAlphaMode::Straight,
        },
    };
    asset.regions = {
        assets::SpriteRegion{
            "region",
            "page",
            assets::SpritePixelSize{width, height},
            assets::SpritePixelOffset{0U, 0U},
            assets::SpritePixelSize{width, height},
            assets::SpritePixelRect{0U, 0U, width, height},
            assets::SpriteRationalPivot{1, 1, 2},
            assets::SpritePackedRotation::None,
        },
    };
    return asset;
}

[[nodiscard]] render::TextureHandle PublishTexture(
    assets::ResourceRegistry& resources,
    render::Renderer& renderer,
    const std::string& reference,
    const PixelArt& art)
{
    assets::TextureResource canonical{};
    canonical.width = art.width;
    canonical.height = art.height;
    canonical.colorSpace = assets::TextureResourceColorSpace::Linear;
    canonical.alphaMode = assets::TextureResourceAlphaMode::Straight;
    canonical.canonicalRgba8 = art.rgba;
    const auto published = resources.PublishTexture(reference, std::move(canonical));
    if (!published.Succeeded()) throw std::runtime_error{"P1 could not publish a canonical presentation texture."};

    const render::Rgba8TextureData data{
        art.width,
        art.height,
        std::span<const std::uint8_t>{art.rgba.data(), art.rgba.size()},
    };
    return renderer.CreateSpriteTextureRgba8(published.handle, data, render::SpriteTextureEncoding::Linear);
}

[[nodiscard]] assets::Shader2DResource MakeFlashShader()
{
    assets::Shader2DResource shader{};
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

[[nodiscard]] assets::Material2DResource MakeFlashMaterial(
    const assets::ResourceHandle<assets::Shader2DResource> shader)
{
    assets::Material2DResource material{};
    material.shader = shader.Untyped();
    material.sampler = assets::MaterialSampler2D::Nearest;
    material.blend = assets::MaterialBlend2D::Normal;
    material.parameters = {
        assets::MaterialParameterDefault2D{
            "flashColor",
            assets::MaterialParameterValue2D{
                assets::MaterialParameterType2D::Color,
                {1.0F, 0.35F, 0.10F, 1.0F}}},
        assets::MaterialParameterDefault2D{
            "flashAmount",
            assets::MaterialParameterValue2D{
                assets::MaterialParameterType2D::Float,
                {0.0F, 0.0F, 0.0F, 0.0F}}},
    };
    return material;
}

[[nodiscard]] render::SpritePresentationRenderData BuildDraw(
    const assets::SpriteAsset& asset,
    const render::TextureHandle texture,
    const float x,
    const float y,
    const float scaleX,
    const float scaleY,
    const render::SpriteLinearRgba tint,
    const std::uint64_t stableOrder)
{
    render::ResolvedSpriteRegion selection{};
    if (!render::ResolveSpriteRegionByIndices(&asset, 0U, 0U, selection).Succeeded())
        throw std::runtime_error{"P1 could not resolve its presentation sprite region."};

    scene::SpritePose2D pose{};
    pose.transform.position = {x, y};
    pose.transform.scale = {scaleX, scaleY};

    render::SpriteAppearance2D appearance{};
    appearance.tint = tint;
    appearance.sampling = render::SpriteAppearanceSampling::Nearest;

    render::SpritePresentationRenderData draw{};
    if (!render::BuildSpritePresentation2D(selection, pose, 1.0F, appearance, draw.presentation).Succeeded())
        throw std::runtime_error{"P1 could not build Sprite presentation data."};
    draw.texture = texture;
    draw.order.stableOrder = stableOrder;
    return draw;
}

void RenderShowcase(
    render::Renderer& renderer,
    const render::OrthographicCamera& camera,
    const assets::SpriteAsset& targetAsset,
    const assets::SpriteAsset& whiteAsset,
    const render::TextureHandle targetTexture,
    const render::TextureHandle whiteTexture,
    const render::PreparedMaterial2D& preparedMaterial,
    const examples::PresentationPlayground& proof,
    const bool capture,
    const std::uint64_t captureFrame)
{
    const scene::Transform2D* const button = proof.ButtonTransform();
    const scene::Transform2D* const panel = proof.PanelTransform();
    const render::MaterialParameterBlock2D* const parameters = proof.MaterialParameters();
    if (button == nullptr || panel == nullptr || parameters == nullptr)
        throw std::runtime_error{"P1 presentation target became unavailable."};

    std::array<render::SpritePresentationRenderData, 8U> draws{};
    std::size_t count = 0U;
    draws[count++] = BuildDraw(whiteAsset, whiteTexture, 0.0F, 0.0F, 640.0F, 360.0F,
        render::SpriteLinearRgba{0.055F, 0.075F, 0.12F, 1.0F}, 0U);
    draws[count++] = BuildDraw(whiteAsset, whiteTexture, 0.0F, 8.0F, 230.0F, 180.0F,
        render::SpriteLinearRgba{0.08F, 0.12F, 0.19F, 1.0F}, 1U);

    draws[count] = BuildDraw(targetAsset, targetTexture, 0.0F, 5.0F, 6.0F, 6.0F,
        render::SpriteLinearRgba{}, 2U);
    draws[count].presentation.appearance.sampler = preparedMaterial.sampler;
    draws[count].presentation.appearance.blend = preparedMaterial.blend;
    draws[count].materialPipeline = preparedMaterial.pipelineIdentity;
    draws[count].materialParameters = parameters;
    ++count;

    draws[count++] = BuildDraw(whiteAsset, whiteTexture, button->position.x, button->position.y,
        115.0F * button->scale.x, 42.0F * button->scale.y,
        render::SpriteLinearRgba{0.82F, 0.52F, 0.13F, 1.0F}, 3U);
    draws[count++] = BuildDraw(whiteAsset, whiteTexture, button->position.x, button->position.y,
        94.0F * button->scale.x, 26.0F * button->scale.y,
        render::SpriteLinearRgba{0.98F, 0.77F, 0.28F, 1.0F}, 4U);
    draws[count++] = BuildDraw(whiteAsset, whiteTexture, panel->position.x, panel->position.y,
        135.0F, 46.0F, render::SpriteLinearRgba{0.12F, 0.34F, 0.47F, 0.96F}, 5U);
    draws[count++] = BuildDraw(whiteAsset, whiteTexture, panel->position.x - 88.0F, panel->position.y,
        28.0F, 28.0F, render::SpriteLinearRgba{0.28F, 0.83F, 0.73F, 1.0F}, 6U);

    const std::span<const render::SpritePresentationRenderData> view{draws.data(), count};
    if (capture)
    {
        const std::filesystem::path output{"trace2d-p1-presentation-proof.bmp"};
        static_cast<void>(renderer.CaptureFrame(
            render::CaptureRequest{captureFrame, output, render::CaptureImageFormat::Bmp}, camera, view));
        std::cout << "Captured " << output.string() << '\n';
    }
    else
    {
        renderer.RenderFrame(camera, view);
    }
}
} // namespace

int main()
{
    try
    {
        platform::PlatformConfig platformConfig{};
        platformConfig.mode = platform::StartupMode::Windowed;
        platformConfig.windowWidth = 640;
        platformConfig.windowHeight = 360;
        platformConfig.windowTitle =
            "Trace2D P1 Presentation Playground | Space step | Enter autoplay | R restart | C capture | Esc quit";
        platform::Platform platform{platformConfig};

        render::RendererConfig rendererConfig{};
        rendererConfig.clearColor = {.red = 0.02F, .green = 0.025F, .blue = 0.04F, .alpha = 1.0F};
        rendererConfig.enableDebugValidation = true;
        render::Renderer renderer{rendererConfig, platform};
        assets::ResourceRegistry resources{"."};

        const PixelArt targetArt = MakeTargetArt();
        PixelArt whiteArt{};
        whiteArt.width = 1U;
        whiteArt.height = 1U;
        whiteArt.rgba = {255U, 255U, 255U, 255U};

        const render::TextureHandle targetTexture = PublishTexture(resources, renderer, "p1/target.rgba8", targetArt);
        const render::TextureHandle whiteTexture = PublishTexture(resources, renderer, "p1/white.rgba8", whiteArt);
        const assets::SpriteAsset targetAsset = MakeSpriteAsset("p1/target.sprite", "p1/target.rgba8", 16U, 16U);
        const assets::SpriteAsset whiteAsset = MakeSpriteAsset("p1/white.sprite", "p1/white.rgba8", 1U, 1U);

        const auto shader = resources.PublishShader2D("p1/hit-flash.shader2d", MakeFlashShader());
        if (!shader.Succeeded()) throw std::runtime_error{"P1 could not publish hit_flash Shader2D."};
        const auto material = resources.PublishMaterial2D(
            "p1/hit-flash.material2d", MakeFlashMaterial(shader.handle));
        if (!material.Succeeded()) throw std::runtime_error{"P1 could not publish hit_flash Material2D."};
        const render::MaterialGpuPrepareResult2D prepared = renderer.PrepareMaterial2D(resources, material.handle);
        if (!prepared.Succeeded())
            throw std::runtime_error{"P1 could not prepare hit_flash Material2D on the active renderer."};

        examples::PresentationPlayground proof{prepared.material.parameterLayout, prepared.material.defaultParameters};
        if (!proof.StartShowcase()) throw std::runtime_error{"P1 could not start its bounded presentation recipe set."};

        render::OrthographicCamera camera{};
        camera.center = {0.0F, 0.0F};
        camera.verticalSize = 360.0F;

        bool quit = false;
        bool autoplay = true;
        std::uint32_t holdFrames = 0U;
        std::uint64_t captureFrame = 1U;
        while (!quit)
        {
            bool captureRequested = false;
            platform::PlatformEvent event{};
            while (platform.PollEvent(event))
            {
                if (event.type == platform::PlatformEventType::QuitRequested)
                {
                    quit = true;
                    continue;
                }
                if (event.type != platform::PlatformEventType::Input ||
                    event.input.type != input::InputEventType::Press)
                    continue;

                switch (event.input.control)
                {
                case input::InputControl::Escape:
                    quit = true;
                    break;
                case input::InputControl::Space:
                    autoplay = false;
                    static_cast<void>(proof.Step(40ms));
                    break;
                case input::InputControl::Enter:
                    autoplay = !autoplay;
                    break;
                case input::InputControl::KeyR:
                    static_cast<void>(proof.RestartShowcase());
                    holdFrames = 0U;
                    break;
                case input::InputControl::KeyC:
                    captureRequested = true;
                    break;
                default:
                    break;
                }
            }

            if (quit) break;
            if (autoplay)
            {
                if (!proof.AllCompleted())
                {
                    if (!proof.Step(16'666'667ns)) throw std::runtime_error{"P1 explicit presentation stepping failed."};
                }
                else if (++holdFrames >= 36U)
                {
                    if (!proof.RestartShowcase()) throw std::runtime_error{"P1 deterministic presentation restart failed."};
                    holdFrames = 0U;
                }
            }

            RenderShowcase(renderer, camera, targetAsset, whiteAsset, targetTexture, whiteTexture,
                prepared.material, proof, captureRequested, captureFrame++);
            std::this_thread::sleep_for(16ms);
        }

        const examples::PresentationProofMetrics metrics = proof.Metrics();
        const render::RenderMetrics renderMetrics = renderer.Metrics();
        std::cout
            << "P1 metrics: tween_writes=" << metrics.bindings.appliedWriteCount
            << ", material_writes=" << metrics.materialTargets.appliedWriteCount
            << ", material_pipeline_switches=" << renderMetrics.materialPipelineSwitches
            << ", fragment_uniform_uploads=" << renderMetrics.fragmentUniformUploads
            << ", draw_calls=" << renderMetrics.spritePresentationDrawCalls
            << '\n';

        renderer.DestroyTexture(whiteTexture);
        renderer.DestroyTexture(targetTexture);
        static_cast<void>(resources.Unload(whiteTexture.Untyped()));
        static_cast<void>(resources.Unload(targetTexture.Untyped()));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "P1 windowed presentation proof failed: " << error.what() << '\n';
        return 1;
    }
}
