#include "NightfallPresentation.hpp"
#include "NightfallProduct.hpp"
#include "NightfallSurvivorsGame.hpp"

#include <trace2d/audio/AudioOutput2D.hpp>
#include <trace2d/platform/Platform.hpp>
#include <trace2d/render/Renderer.hpp>

#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <thread>

#if !defined(_WIN32)
#include <iostream>
#endif

#ifndef TRACE2D_G1_RUNTIME_DIR
#define TRACE2D_G1_RUNTIME_DIR "."
#endif

namespace
{
using Clock = std::chrono::steady_clock;

struct RegistryTypes final
{
    trace2d::scene::ComponentRegistry registry{};
    trace2d::audio::AudioComponentTypes2D audio{};

    RegistryTypes()
        : audio{trace2d::audio::RegisterAudio2DComponents(registry)}
    {
        registry.Freeze();
    }
};

[[nodiscard]] trace2d::application::ApplicationConfig MakeConfig()
{
    trace2d::application::ApplicationConfig config{};
    config.runtime.fixedTimestep = std::chrono::nanoseconds{16'666'667};
    config.runtime.seed = 417U;
    config.scene.semanticId = "g1.nightfall-survivors";
    config.scene.name = "Nightfall Survivors";
    config.uiWidth = static_cast<std::uint32_t>(NightfallSurvivorsGame::CanvasWidth);
    config.uiHeight = static_cast<std::uint32_t>(NightfallSurvivorsGame::CanvasHeight);
    return config;
}

void RequireOutput(const trace2d::audio::AudioOutputResult2D result, const char* const message)
{
    if (result != trace2d::audio::AudioOutputResult2D::Success)
        throw std::runtime_error{message};
}

void WriteCrashReport(const char* const message) noexcept
{
    try
    {
        std::ofstream output{"nightfall_error.txt", std::ios::trunc};
        output << "Nightfall Survivors failed: " << message << '\n';
    }
    catch (...)
    {
    }
}

int RunNightfall()
{
    using namespace std::chrono_literals;
    try
    {
        trace2d::platform::PlatformConfig platformConfig{};
        platformConfig.mode = trace2d::platform::StartupMode::Windowed;
        platformConfig.windowWidth = static_cast<int>(NightfallSurvivorsGame::CanvasWidth);
        platformConfig.windowHeight = static_cast<int>(NightfallSurvivorsGame::CanvasHeight);
        platformConfig.windowTitle = "Nightfall Survivors";
        trace2d::platform::Platform platform{platformConfig};

        trace2d::render::RendererConfig rendererConfig{};
        rendererConfig.clearColor = {.red = 0.010F, .green = 0.014F, .blue = 0.028F, .alpha = 1.0F};
        trace2d::render::Renderer renderer{rendererConfig, platform};

        RegistryTypes types{};
        NightfallSurvivorsGame game{types.audio, TRACE2D_G1_RUNTIME_DIR};
        trace2d::application::Application application{game, types.registry, MakeConfig()};
        application.Start();

        NightfallProduct product{std::filesystem::path{"save"} / "nightfall_profile.v1"};
        product.Load();
        product.ApplySettings(game);

        NightfallPresentation presentation{renderer, game, product, TRACE2D_G1_RUNTIME_DIR};
        application.SetPresentationCallback(&NightfallPresentation::Present, &presentation);

        trace2d::audio::AudioOutputConfig2D audioConfig{};
        audioConfig.voiceCapacity = 32U;
        audioConfig.preloadCacheCapacity = 5U;
        audioConfig.preloadPcmByteBudget = 16U * 1024U * 1024U;
        audioConfig.refillBufferByteBudget = 2U * 1024U * 1024U;
        trace2d::audio::AudioOutput2D audioOutput{game.Resources(), audioConfig};
        RequireOutput(audioOutput.Start(), "Nightfall Survivors could not start physical audio output.");

        bool quit = false;
        auto previous = Clock::now();
        while (!quit)
        {
            trace2d::platform::PlatformEvent event{};
            while (platform.PollEvent(event))
            {
                if (event.type == trace2d::platform::PlatformEventType::QuitRequested)
                {
                    quit = true;
                    continue;
                }
                if (event.type != trace2d::platform::PlatformEventType::Input)
                    continue;

                const bool press = event.input.type == trace2d::input::InputEventType::Press;
                if (press)
                {
                    const NightfallProduct::InputDisposition disposition =
                        product.HandleInput(event.input.control, game);
                    if (disposition == NightfallProduct::InputDisposition::Quit)
                    {
                        quit = true;
                    }
                    else if (disposition == NightfallProduct::InputDisposition::PassToGame)
                    {
                        application.ApplyInput(event.input);
                    }
                }
                else if (event.input.type == trace2d::input::InputEventType::Release)
                {
                    // Always release canonical game controls so pausing while a key is held cannot
                    // leave movement or a level-up binding latched when gameplay resumes.
                    application.ApplyInput(event.input);
                }
                else if (product.CurrentScreen() == NightfallProduct::Screen::Playing)
                {
                    application.ApplyInput(event.input);
                }
            }
            if (quit) break;

            const auto now = Clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous);
            previous = now;
            elapsed = std::min(elapsed, std::chrono::duration_cast<std::chrono::nanoseconds>(250ms));

            if (product.CurrentScreen() == NightfallProduct::Screen::Playing)
            {
                static_cast<void>(application.AdvanceElapsed(elapsed));
                product.ObserveRun(game);
            }

            const auto sync = audioOutput.Sync(game.Audio(), game.Audio().Events());
            RequireOutput(sync.result, "Nightfall Survivors audio sync failed.");
            game.Audio().ClearEvents();
            const auto deviceEvents = audioOutput.PollDeviceEvents();
            RequireOutput(deviceEvents.result, "Nightfall Survivors audio device polling failed.");
            if (deviceEvents.recoveryRequested)
                RequireOutput(audioOutput.Recover(game.Audio()), "Nightfall Survivors audio recovery failed.");
            RequireOutput(audioOutput.Pump().result, "Nightfall Survivors audio pump failed.");

            static_cast<void>(application.Present());
            std::this_thread::sleep_for(1ms);
        }

        product.Save();
        audioOutput.Stop();
        application.Stop();
        return 0;
    }
    catch (const std::exception& error)
    {
        WriteCrashReport(error.what());
#if !defined(_WIN32)
        std::cerr << "Nightfall Survivors failed: " << error.what() << '\n';
#endif
        return 1;
    }
}
} // namespace

#if defined(_WIN32)
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return RunNightfall();
}
#else
int main()
{
    return RunNightfall();
}
#endif
