#include <trace2d/platform/Platform.hpp>

#include <SDL3/SDL.h>

#include <cstddef>
#include <stdexcept>
#include <string>

namespace trace2d::platform
{
namespace
{
std::size_t gSdlOwnerCount = 0;

void ReleaseSdl(const SDL_InitFlags initializedSubsystems) noexcept
{
    SDL_QuitSubSystem(initializedSubsystems);

    if (gSdlOwnerCount > 0)
    {
        --gSdlOwnerCount;
    }

    if (gSdlOwnerCount == 0)
    {
        SDL_Quit();
    }
}
} // namespace

class Platform::Impl final
{
public:
    explicit Impl(const PlatformConfig& config)
        : mode_{config.mode}
        , initializedSubsystems_{config.mode == StartupMode::Windowed ? SDL_INIT_VIDEO : SDL_INIT_EVENTS}
    {
        if (!SDL_Init(initializedSubsystems_))
        {
            const std::string error{SDL_GetError()};
            if (gSdlOwnerCount == 0)
            {
                SDL_Quit();
            }
            throw std::runtime_error{"SDL initialization failed: " + error};
        }

        ++gSdlOwnerCount;

        if (mode_ == StartupMode::Windowed)
        {
            window_ = SDL_CreateWindow(
                config.windowTitle.c_str(), config.windowWidth, config.windowHeight, SDL_WINDOW_RESIZABLE);

            if (window_ == nullptr)
            {
                const std::string error{SDL_GetError()};
                ReleaseSdl(initializedSubsystems_);
                initializedSubsystems_ = 0;
                throw std::runtime_error{"SDL window creation failed: " + error};
            }
        }
    }

    ~Impl()
    {
        if (window_ != nullptr)
        {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        if (initializedSubsystems_ != 0)
        {
            ReleaseSdl(initializedSubsystems_);
            initializedSubsystems_ = 0;
        }
    }

    [[nodiscard]] StartupMode Mode() const noexcept
    {
        return mode_;
    }

    [[nodiscard]] bool HasWindow() const noexcept
    {
        return window_ != nullptr;
    }

    [[nodiscard]] bool PollEvent(PlatformEvent& event) noexcept
    {
        event.type = PlatformEventType::None;

        SDL_Event sdlEvent{};
        if (!SDL_PollEvent(&sdlEvent))
        {
            return false;
        }

        if (sdlEvent.type == SDL_EVENT_QUIT || sdlEvent.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        {
            event.type = PlatformEventType::QuitRequested;
        }

        return true;
    }

private:
    StartupMode mode_{StartupMode::Headless};
    SDL_InitFlags initializedSubsystems_{0};
    SDL_Window* window_{nullptr};
};

Platform::Platform(const PlatformConfig& config)
    : impl_{std::make_unique<Impl>(config)}
{
}

Platform::~Platform() = default;

StartupMode Platform::Mode() const noexcept
{
    return impl_->Mode();
}

bool Platform::HasWindow() const noexcept
{
    return impl_->HasWindow();
}

bool Platform::PollEvent(PlatformEvent& event) noexcept
{
    return impl_->PollEvent(event);
}

const char* ToString(const StartupMode mode) noexcept
{
    switch (mode)
    {
    case StartupMode::Headless:
        return "headless";
    case StartupMode::Windowed:
        return "windowed";
    }

    return "unknown";
}
} // namespace trace2d::platform
