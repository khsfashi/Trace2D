#pragma once

#include <trace2d/scene/Scene.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::scene
{
struct SceneTextDiagnostic final
{
    std::string path{};
    std::string message{};
    std::size_t line{0};
    std::size_t column{0};
};

struct SceneLoadResult final
{
    std::optional<Scene> scene{};
    std::vector<SceneTextDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return scene.has_value() && diagnostics.empty();
    }
};

struct SceneSaveResult final
{
    std::string text{};
    std::vector<SceneTextDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return diagnostics.empty();
    }
};

[[nodiscard]] SceneLoadResult LoadSceneToml(
    std::string_view text,
    std::string_view sourceName = {});

[[nodiscard]] SceneSaveResult SaveSceneToml(const Scene& scene);
} // namespace trace2d::scene
