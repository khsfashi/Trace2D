#include <trace2d/platform/Platform.hpp>

#include <SDL3/SDL.h>

#include <cstddef>
#include <optional>
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

[[nodiscard]] std::optional<input::InputControl> TranslateScancode(const SDL_Scancode scancode) noexcept
{
    using input::InputControl;

    switch (scancode)
    {
    case SDL_SCANCODE_A:
        return InputControl::KeyA;
    case SDL_SCANCODE_B:
        return InputControl::KeyB;
    case SDL_SCANCODE_C:
        return InputControl::KeyC;
    case SDL_SCANCODE_D:
        return InputControl::KeyD;
    case SDL_SCANCODE_E:
        return InputControl::KeyE;
    case SDL_SCANCODE_F:
        return InputControl::KeyF;
    case SDL_SCANCODE_G:
        return InputControl::KeyG;
    case SDL_SCANCODE_H:
        return InputControl::KeyH;
    case SDL_SCANCODE_I:
        return InputControl::KeyI;
    case SDL_SCANCODE_J:
        return InputControl::KeyJ;
    case SDL_SCANCODE_K:
        return InputControl::KeyK;
    case SDL_SCANCODE_L:
        return InputControl::KeyL;
    case SDL_SCANCODE_M:
        return InputControl::KeyM;
    case SDL_SCANCODE_N:
        return InputControl::KeyN;
    case SDL_SCANCODE_O:
        return InputControl::KeyO;
    case SDL_SCANCODE_P:
        return InputControl::KeyP;
    case SDL_SCANCODE_Q:
        return InputControl::KeyQ;
    case SDL_SCANCODE_R:
        return InputControl::KeyR;
    case SDL_SCANCODE_S:
        return InputControl::KeyS;
    case SDL_SCANCODE_T:
        return InputControl::KeyT;
    case SDL_SCANCODE_U:
        return InputControl::KeyU;
    case SDL_SCANCODE_V:
        return InputControl::KeyV;
    case SDL_SCANCODE_W:
        return InputControl::KeyW;
    case SDL_SCANCODE_X:
        return InputControl::KeyX;
    case SDL_SCANCODE_Y:
        return InputControl::KeyY;
    case SDL_SCANCODE_Z:
        return InputControl::KeyZ;
    case SDL_SCANCODE_LEFT:
        return InputControl::ArrowLeft;
    case SDL_SCANCODE_RIGHT:
        return InputControl::ArrowRight;
    case SDL_SCANCODE_UP:
        return InputControl::ArrowUp;
    case SDL_SCANCODE_DOWN:
        return InputControl::ArrowDown;
    case SDL_SCANCODE_SPACE:
        return InputControl::Space;
    case SDL_SCANCODE_RETURN:
        return InputControl::Enter;
    case SDL_SCANCODE_ESCAPE:
        return InputControl::Escape;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] std::optional<input::InputControl> TranslateMouseButton(const std::uint8_t button) noexcept
{
    using input::InputControl;

    switch (button)
    {
    case SDL_BUTTON_LEFT:
        return InputControl::MouseLeft;
    case SDL_BUTTON_MIDDLE:
        return InputControl::MouseMiddle;
    case SDL_BUTTON_RIGHT:
        return InputControl::MouseRight;
    default:
        return std::nullopt;
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

    [[nodiscard]] WindowId WindowIdValue() const noexcept
    {
        if (window_ == nullptr)
        {
            return InvalidWindowId;
        }

        return static_cast<WindowId>(SDL_GetWindowID(window_));
    }

    [[nodiscard]] bool PollEvent(PlatformEvent& event) noexcept
    {
        event = {};

        SDL_Event sdlEvent{};
        if (!SDL_PollEvent(&sdlEvent))
        {
            return false;
        }

        if (sdlEvent.type == SDL_EVENT_QUIT || sdlEvent.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        {
            event.type = PlatformEventType::QuitRequested;
            return true;
        }

        if (sdlEvent.type == SDL_EVENT_KEY_DOWN || sdlEvent.type == SDL_EVENT_KEY_UP)
        {
            const std::optional<input::InputControl> control = TranslateScancode(sdlEvent.key.scancode);
            if (control.has_value())
            {
                event.type = PlatformEventType::Input;
                event.input = input::InputEvent{
                    .control = *control,
                    .type = sdlEvent.type == SDL_EVENT_KEY_DOWN ? input::InputEventType::Press
                                                               : input::InputEventType::Release,
                };
            }
            return true;
        }

        if (sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN || sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            const std::optional<input::InputControl> control = TranslateMouseButton(sdlEvent.button.button);
            if (control.has_value())
            {
                event.type = PlatformEventType::Input;
                event.input = input::InputEvent{
                    .control = *control,
                    .type = sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? input::InputEventType::Press
                                                                        : input::InputEventType::Release,
                };
            }
            return true;
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

WindowId Platform::WindowIdValue() const noexcept
{
    return impl_->WindowIdValue();
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
