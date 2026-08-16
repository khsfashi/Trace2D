#pragma once

#include <trace2d/assets/SpriteAssets.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::agent
{
enum class SpriteAuthoringErrorCode
{
    InvalidRequest,
    AssetLoadFailed,
    UnknownRegion,
    ValidationFailed,
    WriteFailed,
};

[[nodiscard]] std::string_view ToString(SpriteAuthoringErrorCode code) noexcept;

struct SpriteRegionMutation final
{
    std::string regionId{};
    std::optional<assets::SpritePixelSize> sourceSize{};
    std::optional<assets::SpritePixelOffset> trimOffset{};
    std::optional<assets::SpritePixelSize> trimSize{};
    std::optional<assets::SpritePixelRect> packedRect{};
    std::optional<assets::SpriteRationalPivot> pivot{};

    [[nodiscard]] bool HasChanges() const noexcept;
};

struct SpriteMutation final
{
    std::optional<assets::SpriteSampling> sampling{};
    std::optional<SpriteRegionMutation> region{};

    [[nodiscard]] bool HasChanges() const noexcept;
};

struct SpriteAuthoringDiagnostic final
{
    SpriteAuthoringErrorCode code = SpriteAuthoringErrorCode::InvalidRequest;
    std::string path{};
    std::string message{};
};

struct SpriteAuthoringResult final
{
    std::string resource{};
    bool committed = false;
    bool validationPassed = false;
    std::vector<std::string> changedFields{};
    std::vector<SpriteAuthoringDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return diagnostics.empty();
    }
};

// Applies one typed Sprite mutation transaction. The canonical Sprite authority is parsed once,
// semantic identities are resolved once, the candidate is validated in memory, and only then is
// the resource replaced atomically. Validation or authoring failures leave the resource untouched.
[[nodiscard]] SpriteAuthoringResult MutateSpriteResource(
    const std::filesystem::path& projectRoot,
    std::string_view projectRelativeReference,
    const SpriteMutation& mutation);
} // namespace trace2d::agent
