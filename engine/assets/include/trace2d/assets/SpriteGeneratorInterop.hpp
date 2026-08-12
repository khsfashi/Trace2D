#pragma once

#include <trace2d/assets/SpriteImport.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::assets
{
enum class SpriteGeneratorManifestKind : std::uint8_t
{
    SpriteGenComponentRow = 0,
    PerfectPixelV2,
};

[[nodiscard]] std::string_view ToString(SpriteGeneratorManifestKind value) noexcept;

struct SpriteGeneratorAnimationEvidence final
{
    std::string name{};
    std::uint32_t row{0};
    std::uint32_t firstFrame{0};
    std::uint32_t frameCount{0};
    std::uint32_t declaredFps{0};
    bool loop{false};

    [[nodiscard]] bool operator==(const SpriteGeneratorAnimationEvidence&) const noexcept = default;
};

struct SpriteGeneratorManifestImportOptions final
{
    std::string_view canonicalAssetId{};
    std::string_view pageId{"main"};
    std::string_view textureReference{};
    SpriteSampling sampling{SpriteSampling::Nearest};
    SpriteColorSpace colorSpace{SpriteColorSpace::Srgb};

    // sprite-gen's runtime manifest does not author an S1 pivot. Supplying the
    // pivot is therefore mandatory for that adapter; SPP4 never infers it from pixels.
    std::optional<SpriteRationalPivot> spriteGenDefaultPivot{};
};

struct SpriteGeneratorImportResult final
{
    std::uint32_t schemaVersion{1};
    SpriteGeneratorManifestKind manifestKind{SpriteGeneratorManifestKind::SpriteGenComponentRow};
    SpriteImportResult canonicalImport{};
    std::vector<SpriteGeneratorAnimationEvidence> animations{};
    std::vector<SpriteImportDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return diagnostics.empty() && canonicalImport.Succeeded();
    }
};

[[nodiscard]] SpriteGeneratorImportResult ImportSpriteGeneratorManifestJson(
    SpriteGeneratorManifestKind kind,
    std::string_view jsonText,
    const SpriteImportDecodedImageView& decodedSheet,
    const SpriteGeneratorManifestImportOptions& options);

[[nodiscard]] std::string SerializeSpriteGeneratorImportResultJson(
    const SpriteGeneratorImportResult& result);
} // namespace trace2d::assets
