#pragma once

#include <trace2d/input/ActionMap.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::input
{
enum class InputMapErrorCode : std::uint8_t
{
    InvalidReference = 0,
    UnsupportedFormat,
    MissingFile,
    ReadFailure,
    ParseError,
    SchemaError,
    StaleBinding,
    WriteFailure,
};

[[nodiscard]] std::string_view ToString(InputMapErrorCode code) noexcept;
[[nodiscard]] std::string_view ToString(InputControl control) noexcept;
[[nodiscard]] std::string_view ToString(InputAxis axis) noexcept;
[[nodiscard]] std::optional<InputControl> ParseInputControl(std::string_view value) noexcept;
[[nodiscard]] std::optional<InputAxis> ParseInputAxis(std::string_view value) noexcept;

struct InputMapDiagnostic final
{
    InputMapErrorCode code{InputMapErrorCode::SchemaError};
    std::string reference{};
    std::string resolvedPath{};
    std::string path{};
    std::string message{};
    std::size_t line{0};
    std::size_t column{0};
};

struct InputMapAnalogBinding final
{
    InputAxis axis{InputAxis::Unknown};
    float deadzone{0.0F};
    float scale{1.0F};

    [[nodiscard]] bool operator==(const InputMapAnalogBinding&) const noexcept = default;
};

struct InputMapButtonAction final
{
    std::string semanticId{};
    std::vector<InputControl> controls{};

    [[nodiscard]] bool operator==(const InputMapButtonAction&) const noexcept = default;
};

struct InputMapAxis1DAction final
{
    std::string semanticId{};
    InputControl negative{InputControl::Unknown};
    InputControl positive{InputControl::Unknown};
    bool hasDigitalBinding{false};
    std::vector<InputMapAnalogBinding> analogBindings{};

    [[nodiscard]] bool operator==(const InputMapAxis1DAction&) const noexcept = default;
};

struct InputMapDocument final
{
    static constexpr std::int64_t FormatVersion = 1;

    std::vector<InputMapButtonAction> buttonActions{};
    std::vector<InputMapAxis1DAction> axis1DActions{};

    [[nodiscard]] bool operator==(const InputMapDocument&) const noexcept = default;
};

struct InputMapLoadResult final
{
    std::optional<InputMapDocument> document{};
    std::vector<InputMapDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return document.has_value() && diagnostics.empty();
    }
};

struct InputMapBuildResult final
{
    std::optional<ActionMap> actionMap{};
    std::vector<InputMapDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return actionMap.has_value() && diagnostics.empty();
    }
};

struct InputMapEditResult final
{
    bool changed{false};
    std::vector<InputMapDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return diagnostics.empty();
    }
};

[[nodiscard]] InputMapLoadResult ParseInputMapToml(
    std::string_view text,
    std::string_view sourceName = {});
[[nodiscard]] std::string SaveInputMapToml(const InputMapDocument& document);
[[nodiscard]] InputMapBuildResult BuildActionMap(const InputMapDocument& document);

// Rebinding edits the authored/persistence representation only. A finalized runtime ActionMap is
// rebuilt and committed explicitly; it is never mutated opportunistically in the gameplay path.
[[nodiscard]] InputMapEditResult RebindControl(
    InputMapDocument& document,
    std::string_view semanticId,
    InputControl expectedCurrent,
    InputControl replacement);
[[nodiscard]] InputMapEditResult RebindAnalogAxis(
    InputMapDocument& document,
    std::string_view semanticId,
    InputAxis expectedCurrent,
    InputAxis replacement);

class InputMapStore final
{
public:
    explicit InputMapStore(std::filesystem::path projectRoot);

    [[nodiscard]] const std::filesystem::path& ProjectRoot() const noexcept;
    [[nodiscard]] InputMapLoadResult Load(std::string_view projectRelativeReference) const;
    [[nodiscard]] std::vector<InputMapDiagnostic> Save(
        std::string_view projectRelativeReference,
        const InputMapDocument& document) const;

private:
    std::filesystem::path projectRoot_{};
};
} // namespace trace2d::input
