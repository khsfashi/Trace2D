#include <trace2d/platform/Platform.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

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

    // Platform owns only the subsystem references it acquired. A separately alive AudioOutput2D
    // can hold SDL_INIT_AUDIO after the last Platform instance is gone; never globally tear that
    // state down. SDL_Quit is safe here only when no SDL subsystem remains initialized at all.
    if (gSdlOwnerCount == 0 && SDL_WasInit(0) == 0U)
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

[[nodiscard]] std::optional<input::InputControl> TranslateGamepadButton(const SDL_GamepadButton button) noexcept
{
    using input::InputControl;

    switch (button)
    {
    case SDL_GAMEPAD_BUTTON_SOUTH:
        return InputControl::GamepadSouth;
    case SDL_GAMEPAD_BUTTON_EAST:
        return InputControl::GamepadEast;
    case SDL_GAMEPAD_BUTTON_WEST:
        return InputControl::GamepadWest;
    case SDL_GAMEPAD_BUTTON_NORTH:
        return InputControl::GamepadNorth;
    case SDL_GAMEPAD_BUTTON_BACK:
        return InputControl::GamepadBack;
    case SDL_GAMEPAD_BUTTON_GUIDE:
        return InputControl::GamepadGuide;
    case SDL_GAMEPAD_BUTTON_START:
        return InputControl::GamepadStart;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        return InputControl::GamepadLeftStick;
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
        return InputControl::GamepadRightStick;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        return InputControl::GamepadLeftShoulder;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        return InputControl::GamepadRightShoulder;
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        return InputControl::GamepadDpadUp;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        return InputControl::GamepadDpadDown;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        return InputControl::GamepadDpadLeft;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        return InputControl::GamepadDpadRight;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] std::optional<input::InputAxis> TranslateGamepadAxis(const SDL_GamepadAxis axis) noexcept
{
    using input::InputAxis;

    switch (axis)
    {
    case SDL_GAMEPAD_AXIS_LEFTX:
        return InputAxis::GamepadLeftX;
    case SDL_GAMEPAD_AXIS_LEFTY:
        return InputAxis::GamepadLeftY;
    case SDL_GAMEPAD_AXIS_RIGHTX:
        return InputAxis::GamepadRightX;
    case SDL_GAMEPAD_AXIS_RIGHTY:
        return InputAxis::GamepadRightY;
    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
        return InputAxis::GamepadLeftTrigger;
    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
        return InputAxis::GamepadRightTrigger;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] float NormalizeGamepadAxis(const input::InputAxis axis, const Sint16 value) noexcept
{
    if (axis == input::InputAxis::GamepadLeftTrigger || axis == input::InputAxis::GamepadRightTrigger)
    {
        if (value <= 0)
        {
            return 0.0F;
        }
        return static_cast<float>(value) / 32767.0F;
    }

    if (value < 0)
    {
        return static_cast<float>(value) / 32768.0F;
    }
    return static_cast<float>(value) / 32767.0F;
}
} // namespace

class Platform::Impl final
{
public:
    explicit Impl(const PlatformConfig& config)
        : mode_{config.mode}
        , initializedSubsystems_{static_cast<SDL_InitFlags>(
              SDL_INIT_EVENTS | SDL_INIT_GAMEPAD | (config.mode == StartupMode::Windowed ? SDL_INIT_VIDEO : 0U))}
    {
        if (!SDL_Init(initializedSubsystems_))
        {
            const std::string error{SDL_GetError()};
            if (gSdlOwnerCount == 0 && SDL_WasInit(0) == 0U)
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
        for (OpenGamepad& gamepad : gamepads_)
        {
            if (gamepad.handle != nullptr)
            {
                SDL_CloseGamepad(gamepad.handle);
                gamepad.handle = nullptr;
            }
        }
        gamepads_.clear();

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

    [[nodiscard]] bool SetTextInputEnabled(const bool enabled) noexcept
    {
        if (window_ == nullptr)
        {
            return false;
        }

        if (SDL_TextInputActive(window_) == enabled)
        {
            return true;
        }

        return enabled ? SDL_StartTextInput(window_) : SDL_StopTextInput(window_);
    }

    [[nodiscard]] bool TextInputEnabled() const noexcept
    {
        return window_ != nullptr && SDL_TextInputActive(window_);
    }

    [[nodiscard]] bool PollEvent(PlatformEvent& event)
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

        if (sdlEvent.type == SDL_EVENT_TEXT_INPUT)
        {
            event.type = PlatformEventType::TextInput;
            event.textInput = input::TextInputEvent{
                .type = input::TextInputEventType::Committed,
                .text = sdlEvent.text.text != nullptr ? sdlEvent.text.text : "",
            };
            return true;
        }

        if (sdlEvent.type == SDL_EVENT_TEXT_EDITING)
        {
            event.type = PlatformEventType::TextInput;
            event.textInput = input::TextInputEvent{
                .type = input::TextInputEventType::Composition,
                .text = sdlEvent.edit.text != nullptr ? sdlEvent.edit.text : "",
                .selectionStart = sdlEvent.edit.start,
                .selectionLength = sdlEvent.edit.length,
            };
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

        if (sdlEvent.type == SDL_EVENT_MOUSE_MOTION)
        {
            event.type = PlatformEventType::Input;
            event.input = input::InputEvent{
                .type = input::InputEventType::PointerMotion,
                .x = sdlEvent.motion.x,
                .y = sdlEvent.motion.y,
                .deltaX = sdlEvent.motion.xrel,
                .deltaY = sdlEvent.motion.yrel,
            };
            return true;
        }

        if (sdlEvent.type == SDL_EVENT_MOUSE_WHEEL)
        {
            const float direction = sdlEvent.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0F : 1.0F;
            event.type = PlatformEventType::Input;
            event.input = input::InputEvent{
                .type = input::InputEventType::PointerWheel,
                .x = sdlEvent.wheel.mouse_x,
                .y = sdlEvent.wheel.mouse_y,
                .wheelX = sdlEvent.wheel.x * direction,
                .wheelY = sdlEvent.wheel.y * direction,
            };
            return true;
        }

        if (sdlEvent.type == SDL_EVENT_GAMEPAD_ADDED)
        {
            const SDL_JoystickID id = sdlEvent.gdevice.which;
            const auto existing = std::find_if(
                gamepads_.begin(),
                gamepads_.end(),
                [id](const OpenGamepad& gamepad) { return gamepad.id == id; });
            if (existing == gamepads_.end())
            {
                SDL_Gamepad* const handle = SDL_OpenGamepad(id);
                if (handle != nullptr)
                {
                    gamepads_.push_back(OpenGamepad{.id = id, .handle = handle});
                    event.type = PlatformEventType::Input;
                    event.input = input::InputEvent{
                        .type = input::InputEventType::DeviceConnected,
                        .device = static_cast<input::InputDeviceId>(id),
                    };
                }
            }
            return true;
        }

        if (sdlEvent.type == SDL_EVENT_GAMEPAD_REMOVED)
        {
            const SDL_JoystickID id = sdlEvent.gdevice.which;
            const auto gamepad = std::find_if(
                gamepads_.begin(),
                gamepads_.end(),
                [id](const OpenGamepad& candidate) { return candidate.id == id; });
            if (gamepad != gamepads_.end())
            {
                event.type = PlatformEventType::Input;
                event.input = input::InputEvent{
                    .type = input::InputEventType::DeviceDisconnected,
                    .device = static_cast<input::InputDeviceId>(id),
                };
                SDL_CloseGamepad(gamepad->handle);
                gamepads_.erase(gamepad);
            }
            return true;
        }

        if (sdlEvent.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || sdlEvent.type == SDL_EVENT_GAMEPAD_BUTTON_UP)
        {
            const std::optional<input::InputControl> control =
                TranslateGamepadButton(static_cast<SDL_GamepadButton>(sdlEvent.gbutton.button));
            if (control.has_value())
            {
                event.type = PlatformEventType::Input;
                event.input = input::InputEvent{
                    .control = *control,
                    .type = sdlEvent.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? input::InputEventType::Press
                                                                         : input::InputEventType::Release,
                    .device = static_cast<input::InputDeviceId>(sdlEvent.gbutton.which),
                };
            }
            return true;
        }

        if (sdlEvent.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
        {
            const std::optional<input::InputAxis> axis =
                TranslateGamepadAxis(static_cast<SDL_GamepadAxis>(sdlEvent.gaxis.axis));
            if (axis.has_value())
            {
                event.type = PlatformEventType::Input;
                event.input = input::InputEvent{
                    .type = input::InputEventType::AxisMotion,
                    .axis = *axis,
                    .device = static_cast<input::InputDeviceId>(sdlEvent.gaxis.which),
                    .value = NormalizeGamepadAxis(*axis, sdlEvent.gaxis.value),
                };
            }
            return true;
        }

        return true;
    }

private:
    struct OpenGamepad final
    {
        SDL_JoystickID id{0};
        SDL_Gamepad* handle{nullptr};
    };

    StartupMode mode_{StartupMode::Headless};
    SDL_InitFlags initializedSubsystems_{0};
    SDL_Window* window_{nullptr};
    std::vector<OpenGamepad> gamepads_{};
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

bool Platform::SetTextInputEnabled(const bool enabled) noexcept
{
    return impl_->SetTextInputEnabled(enabled);
}

bool Platform::TextInputEnabled() const noexcept
{
    return impl_->TextInputEnabled();
}

bool Platform::PollEvent(PlatformEvent& event)
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
