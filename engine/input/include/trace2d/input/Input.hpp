#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace trace2d::input
{
using InputDeviceId = std::uint64_t;
inline constexpr InputDeviceId InvalidInputDeviceId = 0;

enum class InputDeviceType : std::uint8_t
{
    Gamepad,
};

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
    GamepadSouth,
    GamepadEast,
    GamepadWest,
    GamepadNorth,
    GamepadBack,
    GamepadGuide,
    GamepadStart,
    GamepadLeftStick,
    GamepadRightStick,
    GamepadLeftShoulder,
    GamepadRightShoulder,
    GamepadDpadUp,
    GamepadDpadDown,
    GamepadDpadLeft,
    GamepadDpadRight,
    Count,
};

enum class InputAxis : std::uint8_t
{
    Unknown = 0,
    GamepadLeftX,
    GamepadLeftY,
    GamepadRightX,
    GamepadRightY,
    GamepadLeftTrigger,
    GamepadRightTrigger,
    Count,
};

enum class InputEventType : std::uint8_t
{
    Press,
    Release,
    AxisMotion,
    PointerMotion,
    PointerWheel,
    DeviceConnected,
    DeviceDisconnected,
};

struct InputEvent
{
    InputControl control{InputControl::Unknown};
    InputEventType type{InputEventType::Press};
    InputAxis axis{InputAxis::Unknown};
    InputDeviceId device{InvalidInputDeviceId};
    InputDeviceType deviceType{InputDeviceType::Gamepad};
    float value{0.0F};
    float x{0.0F};
    float y{0.0F};
    float deltaX{0.0F};
    float deltaY{0.0F};
    float wheelX{0.0F};
    float wheelY{0.0F};

    [[nodiscard]] bool operator==(const InputEvent&) const noexcept = default;
};

struct InputControlState
{
    bool held{false};
    bool pressed{false};
    bool released{false};

    [[nodiscard]] bool operator==(const InputControlState&) const noexcept = default;
};

struct PointerState
{
    float x{0.0F};
    float y{0.0F};
    float deltaX{0.0F};
    float deltaY{0.0F};
    float wheelX{0.0F};
    float wheelY{0.0F};

    [[nodiscard]] bool operator==(const PointerState&) const noexcept = default;
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
    [[nodiscard]] float Axis(InputAxis axis) const noexcept;
    [[nodiscard]] PointerState Pointer() const noexcept;
    [[nodiscard]] std::optional<InputDeviceId> PrimaryGamepad() const noexcept;
    [[nodiscard]] bool IsGamepadConnected(InputDeviceId device) const noexcept;
    [[nodiscard]] std::size_t ConnectedGamepadCount() const noexcept;
    [[nodiscard]] std::size_t PendingScheduledEventCount() const noexcept;

private:
    struct ScheduledInputEvent
    {
        std::uint64_t frame{0};
        InputEvent event{};
    };

    static constexpr std::size_t kControlCount = static_cast<std::size_t>(InputControl::Count);
    static constexpr std::size_t kAxisCount = static_cast<std::size_t>(InputAxis::Count);

    struct GamepadState final
    {
        InputDeviceId device{InvalidInputDeviceId};
        std::array<bool, kControlCount> held{};
        std::array<float, kAxisCount> axes{};
    };

    [[nodiscard]] static std::size_t ControlIndex(InputControl control) noexcept;
    [[nodiscard]] static std::size_t AxisIndex(InputAxis axis) noexcept;
    [[nodiscard]] GamepadState* FindGamepad(InputDeviceId device) noexcept;
    [[nodiscard]] const GamepadState* FindGamepad(InputDeviceId device) const noexcept;
    void ApplyButtonTransition(InputControl control, InputEventType type) noexcept;
    void ClearCanonicalGamepadState() noexcept;
    void SyncPrimaryGamepadState() noexcept;
    void ClearTransientState() noexcept;

    std::array<InputControlState, kControlCount> states_{};
    std::array<float, kAxisCount> axes_{};
    PointerState pointer_{};
    std::vector<GamepadState> gamepads_{};
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

    void ConnectGamepad(InputDeviceId device) noexcept;
    void DisconnectGamepad(InputDeviceId device) noexcept;
    void PressGamepad(InputDeviceId device, InputControl control) noexcept;
    void ReleaseGamepad(InputDeviceId device, InputControl control) noexcept;
    void SetGamepadAxis(InputDeviceId device, InputAxis axis, float value) noexcept;
    void MovePointer(float x, float y, float deltaX, float deltaY) noexcept;
    void ScrollPointer(float wheelX, float wheelY) noexcept;

private:
    InputSystem& input_;
};
} // namespace trace2d::input
