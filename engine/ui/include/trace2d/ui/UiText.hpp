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
} // namespace trace2d::ui
