#pragma once

#include <trace2d/input/Input.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::input
{
struct ButtonActionId final
{
    std::uint32_t value{std::numeric_limits<std::uint32_t>::max()};

    [[nodiscard]] bool operator==(const ButtonActionId&) const noexcept = default;
};

struct Axis1DActionId final
{
    std::uint32_t value{std::numeric_limits<std::uint32_t>::max()};

    [[nodiscard]] bool operator==(const Axis1DActionId&) const noexcept = default;
};

struct ButtonActionState final
{
    bool held{false};
    bool pressed{false};
    bool released{false};

    [[nodiscard]] bool operator==(const ButtonActionState&) const noexcept = default;
};

class ActionMap final
{
public:
    ActionMap() = default;

    [[nodiscard]] ButtonActionId AddButtonAction(std::string semanticId);
    void BindButton(ButtonActionId action, InputControl control);

    [[nodiscard]] Axis1DActionId AddAxis1DAction(
        std::string semanticId,
        InputControl negative,
        InputControl positive);

    // Finalization freezes semantic identities/bindings before fixed-step gameplay starts.
    // Resolve() performs no string lookup and no allocation after this point.
    void Finalize();
    [[nodiscard]] bool IsFinalized() const noexcept;

    // Resolve the semantic state from the already-authoritative low-level InputSystem for the
    // current fixed frame. Physical, test, and Agent input therefore converge before gameplay.
    void Resolve(const InputSystem& input);
    void ResetState() noexcept;

    [[nodiscard]] std::optional<ButtonActionId> FindButtonAction(std::string_view semanticId) const noexcept;
    [[nodiscard]] std::optional<Axis1DActionId> FindAxis1DAction(std::string_view semanticId) const noexcept;

    [[nodiscard]] ButtonActionState ButtonState(ButtonActionId action) const;
    [[nodiscard]] bool Held(ButtonActionId action) const;
    [[nodiscard]] bool Pressed(ButtonActionId action) const;
    [[nodiscard]] bool Released(ButtonActionId action) const;
    [[nodiscard]] float Axis1D(Axis1DActionId action) const;

    [[nodiscard]] std::size_t ButtonActionCount() const noexcept;
    [[nodiscard]] std::size_t Axis1DActionCount() const noexcept;

private:
    struct ButtonActionRecord final
    {
        std::string semanticId{};
        std::vector<InputControl> controls{};
        ButtonActionState state{};
    };

    struct Axis1DActionRecord final
    {
        std::string semanticId{};
        InputControl negative{InputControl::Unknown};
        InputControl positive{InputControl::Unknown};
        float value{0.0F};
    };

    void RequireMutable() const;
    void RequireUniqueSemanticId(std::string_view semanticId) const;
    [[nodiscard]] ButtonActionRecord& ButtonRecord(ButtonActionId action);
    [[nodiscard]] const ButtonActionRecord& ButtonRecord(ButtonActionId action) const;
    [[nodiscard]] const Axis1DActionRecord& AxisRecord(Axis1DActionId action) const;

    std::vector<ButtonActionRecord> buttonActions_{};
    std::vector<Axis1DActionRecord> axis1DActions_{};
    bool finalized_{false};
};
} // namespace trace2d::input
