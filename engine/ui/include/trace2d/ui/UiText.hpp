#pragma once

#include <trace2d/ui/Ui.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::ui
{
struct UiTextDiagnostic final
{
    std::string path{};
    std::string message{};
    std::size_t line{0};
    std::size_t column{0};
};

struct UiLoadResult final
{
    std::optional<UiDocument> document{};
    std::vector<UiTextDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return document.has_value() && diagnostics.empty();
    }
};

[[nodiscard]] UiLoadResult LoadUiToml(
    std::string_view text,
    std::string_view sourceName = {});

// U14 resource-aware authored loading is required only when kind = "image" is present. Progress and
// every legacy kind remain loadable through the resource-free overload. Image lookup resolves an
// already-ready project-relative texture through the canonical #86 ResourceRegistry; it never loads
// or republishes resource bytes.
[[nodiscard]] UiLoadResult LoadUiToml(
    std::string_view text,
    assets::ResourceRegistry& resources,
    std::string_view sourceName = {});
} // namespace trace2d::ui
