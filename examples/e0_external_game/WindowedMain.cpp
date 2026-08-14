#include "ExampleGame.hpp"

#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>

#include <array>
#include <cstdint>
#include <exception>
#include <span>
#include <stdexcept>

namespace
{
struct PresentationState final
{
    trace2d::render::Renderer* renderer{nullptr};
    trace2d::render::TextureHandle texture{trace2d::render::InvalidTextureHandle};
};

void Present(const trace2d::application::GameContext& context, void* const userData)
{
    auto* const presentation = static_cast<PresentationState*>(userData);
    if (presentation == nullptr || presentation->renderer == nullptr)
    {
        throw std::runtime_error{"E0 windowed presentation state is unavailable."};
    }

    const std::optional<trace2d::scene::EntityId> playerId =
        context.Scene().FindBySemanticId("game.player");
    if (!playerId.has_value())
    {
        throw std::runtime_error{"E0 windowed presentation cannot find game.player."};
    }

    const trace2d::scene::Entity* const player = context.Scene().TryGet(*playerId);
    if (player == nullptr)
    {
        throw std::runtime_error{"E0 windowed presentation resolved a stale player handle."};
    }

    trace2d::render::OrthographicCamera camera{};
    trace2d::render::SpriteRenderData sprite{};
    sprite.center = trace2d::render::Float2{
        .x = player->Transform().position.x,
        .y = player->Transform().position.y,
    };
    sprite.halfExtents = trace2d::render::Float2{.x = 0.5F, .y = 0.5F};
    sprite.texture = presentation->texture;
    presentation->renderer->RenderFrame(camera, sprite);
}
} // namespace

int main()
{
    try
    {
        trace2d::platform::PlatformConfig platformConfig{};
        platformConfig.mode = trace2d::platform::StartupMode::Windowed;
        platformConfig.windowWidth = 640;
        platformConfig.windowHeight = 360;
        platformConfig.windowTitle = "Trace2D E0 External Game";

        trace2d::platform::Platform platform{platformConfig};
        trace2d::render::Renderer renderer{{}, platform};

        constexpr std::array<std::uint8_t, 4> whitePixel{255U, 255U, 255U, 255U};
        trace2d::render::Rgba8TextureData textureData{};
        textureData.width = 1;
        textureData.height = 1;
        textureData.pixels = std::span<const std::uint8_t>{whitePixel};
        const trace2d::render::TextureHandle texture = renderer.CreateTextureRgba8(textureData);
        PresentationState presentation{.renderer = &renderer, .texture = texture};

        trace2d::application::ApplicationConfig applicationConfig{};
        applicationConfig.scene.semanticId = "e0.scene";
        applicationConfig.scene.name = "E0 Scene";
        applicationConfig.uiWidth = 640;
        applicationConfig.uiHeight = 360;

        ExampleGame game{};
        trace2d::application::Application application{game, applicationConfig};
        application.SetPresentationCallback(&Present, &presentation);
        application.Start();

        bool quitRequested = false;
        trace2d::platform::PlatformEvent event{};
        while (platform.PollEvent(event))
        {
            if (event.type == trace2d::platform::PlatformEventType::QuitRequested)
            {
                quitRequested = true;
            }
            else if (event.type == trace2d::platform::PlatformEventType::Input)
            {
                application.ApplyInput(event.input);
            }
        }

        if (!quitRequested)
        {
            application.StepFrames();
            static_cast<void>(application.Present());
        }

        application.Stop();
        renderer.DestroyTexture(texture);
        return 0;
    }
    catch (const std::exception&)
    {
        return 1;
    }
}
