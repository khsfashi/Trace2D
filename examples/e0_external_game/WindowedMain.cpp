#include "ExampleGame.hpp"

#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>

#include <exception>

namespace
{
void Present(const trace2d::application::GameContext&, void* const userData)
{
    auto* const renderer = static_cast<trace2d::render::Renderer*>(userData);
    renderer->RenderFrame();
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

        trace2d::application::ApplicationConfig applicationConfig{};
        applicationConfig.scene.semanticId = "e0.scene";
        applicationConfig.scene.name = "E0 Scene";
        applicationConfig.uiWidth = 640;
        applicationConfig.uiHeight = 360;

        ExampleGame game{};
        trace2d::application::Application application{game, applicationConfig};
        application.SetPresentationCallback(&Present, &renderer);
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
                application.Input().ApplyEvent(event.input);
            }
        }

        if (!quitRequested)
        {
            application.StepFrames();
            static_cast<void>(application.Present());
        }

        application.Stop();
        return 0;
    }
    catch (const std::exception&)
    {
        return 1;
    }
}
