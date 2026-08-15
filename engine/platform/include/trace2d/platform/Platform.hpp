#pragma once

#include <trace2d/input/Input.hpp>
#include <trace2d/input/TextInput.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace trace2d::platform
{
using WindowId = std::uint32_t;
inline constexpr WindowId InvalidWindowId = 0;

enum class StartupMode
{
    Headless,
    Windowed,
};

struct PlatformConfig
{
    StartupMode mode{StartupMode::Headless};
    int windowWidth{1280};
    int windowHeight{720};
    std::string windowTitle{"Trace2D"};
};

enum class PlatformEventType
{
    None,
    QuitRequested,
    Input,
    TextInput,
};

struct PlatformEvent
{
    PlatformEventType type{PlatformEventType::None};
    input::InputEvent input{};
    input::TextInputEvent textInput{};
};

class Platform final
{
public:
    explicit Platform(const PlatformConfig& config);
    ~Platform();

    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;
    Platform(Platform&&) = delete;
    Platform& operator=(Platform&&) = delete;

    [[nodiscard]] StartupMode Mode() const noexcept;
    [[nodiscard]] bool HasWindow() const noexcept;
    [[nodiscard]] WindowId WindowIdValue() const noexcept;

    // SDL3 text input is window-scoped and disabled by default. The host owns enabling it only
    // while a real text-entry target is focused; headless mode returns false and remains inert.
    [[nodiscard]] bool SetTextInputEnabled(bool enabled) noexcept;
    [[nodiscard]] bool TextInputEnabled() const noexcept;

    [[nodiscard]] bool PollEvent(PlatformEvent& event);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] const char* ToString(StartupMode mode) noexcept;
} // namespace trace2d::platform
