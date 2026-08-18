#include "TinyPlayableGame.hpp"

#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>
#include <trace2d/ui/Ui.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace
{
using Clock = std::chrono::steady_clock;
using TextureHandle = trace2d::render::TextureHandle;

struct PresentationState final
{
    trace2d::render::Renderer* renderer{nullptr};
    TextureHandle player{};
    TextureHandle hazard{};
    TextureHandle beacon{};
    TextureHandle neutral{};
    TextureHandle health{};
};

TextureHandle CreateSolidTexture(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    std::string reference,
    const std::array<std::uint8_t, 4>& rgba)
{
    trace2d::assets::TextureResource canonical{};
    canonical.width = 1U;
    canonical.height = 1U;
    canonical.colorSpace = trace2d::assets::TextureResourceColorSpace::Linear;
    canonical.canonicalRgba8.assign(rgba.begin(), rgba.end());
    const auto published = resources.PublishTexture(std::move(reference), std::move(canonical));
    if (!published.Succeeded()) throw std::runtime_error{"P0 could not publish a solid presentation texture."};

    trace2d::render::Rgba8TextureData data{};
    data.width = 1U;
    data.height = 1U;
    data.pixels = std::span<const std::uint8_t>{rgba.data(), rgba.size()};
    return renderer.CreateTextureRgba8(published.handle, data);
}

void ReleaseTexture(
    trace2d::assets::ResourceRegistry& resources,
    trace2d::render::Renderer& renderer,
    const TextureHandle texture) noexcept
{
    renderer.DestroyTexture(texture);
    static_cast<void>(resources.Unload(texture.Untyped()));
}

void AddRect(
    std::array<trace2d::render::SpriteRenderData, 16>& sprites,
    std::size_t& count,
    const float centerX,
    const float centerY,
    const float halfWidth,
    const float halfHeight,
    const TextureHandle texture,
    const std::int32_t layer,
    const std::uint64_t order)
{
    if (count >= sprites.size()) throw std::runtime_error{"P0 presentation sprite capacity exceeded."};
    sprites[count++] = trace2d::render::SpriteRenderData{
        .center = {.x = centerX, .y = centerY},
        .halfExtents = {.x = halfWidth, .y = halfHeight},
        .texture = texture,
        .layer = layer,
        .stableOrder = order,
    };
}

void AddProgress(
    std::array<trace2d::render::SpriteRenderData, 16>& sprites,
    std::size_t& count,
    const trace2d::ui::UiElement& element,
    const TextureHandle foreground,
    const TextureHandle background,
    std::uint64_t& order)
{
    const float width = static_cast<float>(element.bounds.width);
    const float height = static_cast<float>(element.bounds.height);
    const float x = static_cast<float>(element.bounds.x) + width * 0.5F;
    const float y = TinyPlayableGame::CanvasHeight -
        (static_cast<float>(element.bounds.y) + height * 0.5F);
    AddRect(sprites, count, x, y, width * 0.5F, height * 0.5F, background, 10, order++);

    if (!element.progress.Active() || element.progress.Maximum() == 0U || element.progress.Value() == 0U) return;
    const float ratio = static_cast<float>(element.progress.Value()) /
        static_cast<float>(element.progress.Maximum());
    const float fillWidth = width * std::clamp(ratio, 0.0F, 1.0F);
    const float fillCenterX = static_cast<float>(element.bounds.x) + fillWidth * 0.5F;
    AddRect(sprites, count, fillCenterX, y, fillWidth * 0.5F, height * 0.5F, foreground, 11, order++);
}

void Present(const trace2d::application::GameContext& context, void* const userData)
{
    auto* const state = static_cast<PresentationState*>(userData);
    if (state == nullptr || state->renderer == nullptr) throw std::runtime_error{"P0 presentation state is unavailable."};

    const auto playerId = context.Scene().FindBySemanticId("game.player");
    const auto hazardId = context.Scene().FindBySemanticId("game.hazard");
    const auto beaconId = context.Scene().FindBySemanticId("game.beacon");
    const trace2d::ui::UiElement* const health = context.Ui().Find("hud.health");
    const trace2d::ui::UiElement* const objective = context.Ui().Find("hud.objective");
    if (!playerId.has_value() || !hazardId.has_value() || !beaconId.has_value() || health == nullptr || objective == nullptr)
        throw std::runtime_error{"P0 presentation could not resolve canonical gameplay state."};

    const trace2d::scene::Entity* const player = context.Scene().TryGet(*playerId);
    const trace2d::scene::Entity* const hazard = context.Scene().TryGet(*hazardId);
    const trace2d::scene::Entity* const beacon = context.Scene().TryGet(*beaconId);
    if (player == nullptr || hazard == nullptr || beacon == nullptr)
        throw std::runtime_error{"P0 presentation resolved a stale gameplay entity."};

    std::array<trace2d::render::SpriteRenderData, 16> sprites{};
    std::size_t count = 0U;
    std::uint64_t order = 0U;

    AddRect(sprites, count, TinyPlayableGame::CanvasWidth * 0.5F, TinyPlayableGame::GroundY + 31.0F,
        TinyPlayableGame::CanvasWidth * 0.5F, 2.0F, state->neutral, 0, order++);
    AddRect(sprites, count, player->LocalTransform().position.x, player->LocalTransform().position.y,
        18.0F, 18.0F, state->player, 2, order++);
    AddRect(sprites, count, hazard->LocalTransform().position.x, hazard->LocalTransform().position.y,
        8.0F, 58.0F, state->hazard, 1, order++);
    AddRect(sprites, count, beacon->LocalTransform().position.x, beacon->LocalTransform().position.y,
        15.0F, 30.0F, state->beacon, 1, order++);
    AddProgress(sprites, count, *health, state->health, state->neutral, order);
    AddProgress(sprites, count, *objective, state->beacon, state->neutral, order);

    trace2d::render::OrthographicCamera camera{};
    camera.center = {.x = TinyPlayableGame::CanvasWidth * 0.5F, .y = TinyPlayableGame::CanvasHeight * 0.5F};
    camera.verticalSize = TinyPlayableGame::CanvasHeight;
    state->renderer->RenderFrame(camera, std::span<const trace2d::render::SpriteRenderData>{sprites.data(), count});
}
} // namespace

int main()
{
    using namespace std::chrono_literals;
    try
    {
        trace2d::platform::PlatformConfig platformConfig{};
        platformConfig.mode = trace2d::platform::StartupMode::Windowed;
        platformConfig.windowWidth = static_cast<int>(TinyPlayableGame::CanvasWidth);
        platformConfig.windowHeight = static_cast<int>(TinyPlayableGame::CanvasHeight);
        platformConfig.windowTitle = "Trace2D P0 | A/D move | Space/Enter claim | Esc quit";

        trace2d::platform::Platform platform{platformConfig};
        trace2d::render::RendererConfig rendererConfig{};
        rendererConfig.clearColor = {.red = 0.035F, .green = 0.045F, .blue = 0.070F, .alpha = 1.0F};
        trace2d::render::Renderer renderer{rendererConfig, platform};
        trace2d::assets::ResourceRegistry resources{"."};

        const TextureHandle playerTexture = CreateSolidTexture(resources, renderer, "p0/player.rgba8", {74U, 210U, 255U, 255U});
        const TextureHandle hazardTexture = CreateSolidTexture(resources, renderer, "p0/hazard.rgba8", {255U, 82U, 92U, 255U});
        const TextureHandle beaconTexture = CreateSolidTexture(resources, renderer, "p0/beacon.rgba8", {255U, 200U, 76U, 255U});
        const TextureHandle neutralTexture = CreateSolidTexture(resources, renderer, "p0/neutral.rgba8", {62U, 73U, 92U, 255U});
        const TextureHandle healthTexture = CreateSolidTexture(resources, renderer, "p0/health.rgba8", {92U, 220U, 126U, 255U});

        PresentationState presentation{
            .renderer = &renderer,
            .player = playerTexture,
            .hazard = hazardTexture,
            .beacon = beaconTexture,
            .neutral = neutralTexture,
            .health = healthTexture,
        };

        trace2d::application::ApplicationConfig applicationConfig{};
        applicationConfig.runtime.fixedTimestep = std::chrono::nanoseconds{16'666'667};
        applicationConfig.runtime.seed = 315U;
        applicationConfig.scene.semanticId = "p0.tiny-playable";
        applicationConfig.scene.name = "P0 Tiny Playable";
        applicationConfig.uiWidth = static_cast<std::uint32_t>(TinyPlayableGame::CanvasWidth);
        applicationConfig.uiHeight = static_cast<std::uint32_t>(TinyPlayableGame::CanvasHeight);

        TinyPlayableGame game{};
        trace2d::application::Application application{game, applicationConfig};
        application.SetPresentationCallback(&Present, &presentation);
        application.Start();

        bool quitRequested = false;
        Clock::time_point previous = Clock::now();
        while (!quitRequested)
        {
            trace2d::platform::PlatformEvent event{};
            while (platform.PollEvent(event))
            {
                if (event.type == trace2d::platform::PlatformEventType::QuitRequested)
                {
                    quitRequested = true;
                }
                else if (event.type == trace2d::platform::PlatformEventType::Input)
                {
                    if (event.input.control == trace2d::input::InputControl::Escape &&
                        event.input.type == trace2d::input::InputEventType::Press)
                        quitRequested = true;
                    else
                        application.ApplyInput(event.input);
                }
            }

            if (quitRequested) break;
            const Clock::time_point now = Clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous);
            previous = now;
            elapsed = std::min(elapsed, 250ms);
            static_cast<void>(application.AdvanceElapsed(elapsed));
            static_cast<void>(application.Present());
            std::this_thread::sleep_for(1ms);
        }
        application.Stop();

        ReleaseTexture(resources, renderer, healthTexture);
        ReleaseTexture(resources, renderer, neutralTexture);
        ReleaseTexture(resources, renderer, beaconTexture);
        ReleaseTexture(resources, renderer, hazardTexture);
        ReleaseTexture(resources, renderer, playerTexture);
        return 0;
    }
    catch (const std::exception&)
    {
        return 1;
    }
}
