#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace trace2d::input
{
enum class InputControl : std::uint16_t
{
    Unknown = 0,
    KeyA,
    KeyB,
    KeyC,
    KeyD,
    KeyE,
    KeyF,
    KeyG,
    KeyH,
    KeyI,
    KeyJ,
    KeyK,
    KeyL,
    KeyM,
    KeyN,
    KeyO,
    KeyP,
    KeyQ,
    KeyR,
    KeyS,
    KeyT,
    KeyU,
    KeyV,
    KeyW,
    KeyX,
    KeyY,
    KeyZ,
    ArrowLeft,
    ArrowRight,
    ArrowUp,
    ArrowDown,
    Space,
    Enter,
    Escape,
    MouseLeft,
    MouseMiddle,
    MouseRight,
    Count,
};

enum class InputEventType : std::uint8_t
{
    Press,
    Release,
};

struct InputEvent
{
    InputControl control{InputControl::Unknown};
    InputEventType type{InputEventType::Press};

    [[nodiscard]] bool operator==(const InputEvent&) const noexcept = default;
};

struct InputControlState
{
    bool held{false};
    bool pressed{false};
    bool released{false};

    [[nodiscard]] bool operator==(const InputControlState&) const noexcept = default;
};

class InputSystem final
{
public:
    InputSystem() = default;

    void Reset() noexcept;
    void ApplyEvent(const InputEvent& event) noexcept;
    void Schedule(std::uint64_t frame, const InputEvent& event);
    void AdvanceToFrame(std::uint64_t frame);

    [[nodiscard]] std::uint64_t CurrentFrame() const noexcept;
    [[nodiscard]] InputControlState State(InputControl control) const noexcept;
    [[nodiscard]] bool Held(InputControl control) const noexcept;
    [[nodiscard]] bool Pressed(InputControl control) const noexcept;
    [[nodiscard]] bool Released(InputControl control) const noexcept;
    [[nodiscard]] std::size_t PendingScheduledEventCount() const noexcept;

private:
    struct ScheduledInputEvent
    {
        std::uint64_t frame{0};
        InputEvent event{};
    };

    static constexpr std::size_t kControlCount = static_cast<std::size_t>(InputControl::Count);

    [[nodiscard]] static std::size_t ControlIndex(InputControl control) noexcept;
    void ClearTransientState() noexcept;

    std::array<InputControlState, kControlCount> states_{};
    std::vector<ScheduledInputEvent> scheduledEvents_{};
    std::size_t nextScheduledEvent_{0};
    std::uint64_t currentFrame_{0};
};

class VirtualInputSource final
{
public:
    explicit VirtualInputSource(InputSystem& input) noexcept;

    void Reset() noexcept;
    void Press(InputControl control) noexcept;
    void Release(InputControl control) noexcept;
    void SchedulePress(std::uint64_t frame, InputControl control);
    void ScheduleRelease(std::uint64_t frame, InputControl control);

private:
    InputSystem& input_;
};
} // namespace trace2d::input
