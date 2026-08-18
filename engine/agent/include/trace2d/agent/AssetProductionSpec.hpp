#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::agent
{
struct WorkSpec;

struct AssetProductionDiagnostic final
{
    std::string path{};
    std::string message{};
    std::size_t line{0};
    std::size_t column{0};
};

struct AssetProductionItem final
{
    std::string id{};
    std::string assetClass{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::string> requiredAnimations{};
    std::vector<std::string> requiredDirections{};
    std::vector<std::string> constraints{};
};

struct ArtProfile final
{
    std::string id{};
    std::string description{};
    std::vector<std::string> creativeConstraints{};
    std::vector<std::string> approvedReferences{};
};

struct AssetProductionSpec final
{
    std::string id{};
    std::string workSpecId{};
    std::string ownerReviewAcceptanceId{};
    std::string artProfileId{};
    std::uint32_t candidatesPerItem{0};
    std::uint32_t maxProviderCalls{0};
    std::vector<AssetProductionItem> items{};
    std::vector<ArtProfile> artProfiles{};
};

struct AssetProductionSpecParseResult final
{
    std::optional<AssetProductionSpec> spec{};
    std::vector<AssetProductionDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return spec.has_value() && diagnostics.empty();
    }
};

// AssetProductionSpec is a project-owned creative-intent extension anchored to
// the existing WorkSpec identity. It intentionally owns no completion state,
// provider configuration, candidate lifecycle, or aesthetic pass/fail truth.
[[nodiscard]] AssetProductionSpecParseResult ParseAssetProductionSpecToml(
    std::string_view text,
    std::string_view sourceName = {});

// Cross-contract validation is explicit tooling/setup work. The production
// spec references, rather than duplicates, WorkSpec human-review authority.
[[nodiscard]] std::vector<AssetProductionDiagnostic> ValidateAssetProductionSpecAgainstWorkSpec(
    const AssetProductionSpec& spec,
    const WorkSpec& workSpec);
} // namespace trace2d::agent
