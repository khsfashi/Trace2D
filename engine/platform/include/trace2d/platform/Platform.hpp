#pragma once

#include <memory>
#include <string>

namespace trace2d::platform
{
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
};

struct PlatformEvent
{
    PlatformEventType type{PlatformEventType::None};
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
    [[nodiscard]] bool PollEvent(PlatformEvent& event) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

[[nodiscard]] const char* ToString(StartupMode mode) noexcept;
} // namespace trace2d::platform
