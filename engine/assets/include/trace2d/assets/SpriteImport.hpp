#pragma once

#include <trace2d/assets/SpriteAssets.hpp>
#include <trace2d/assets/SpriteExtraction.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace trace2d::assets
{
enum class SpriteImportSourceKind : std::uint8_t
{
    AsepriteSheetJson = 0,
    GenericSheet,
    LooseFrames,
};

enum class SpriteImportAnimationDirection : std::uint8_t
{
    Forward = 0,
    Reverse,
    PingPong,
    PingPongReverse,
};

enum class SpriteAsepriteRotatedFramePolicy : std::uint8_t
{
    Reject = 0,
    InterpretAsCw90,
};

enum class SpriteGenericSheetMode : std::uint8_t
{
    ExplicitRegions = 0,
    UniformGrid,
};

enum class SpriteImportErrorCode : std::uint8_t
{
    EmptyAssetId = 0,
    EmptyPageId,
    EmptyTextureReference,
    InvalidDimensions,
    InvalidByteCount,
    SizeOverflow,
    JsonParseError,
    UnsupportedManifest,
    MissingField,
    InvalidField,
    EmptyRegionId,
    DuplicateRegionId,
    InvalidRectangle,
    RectangleOutOfBounds,
    UnsupportedRotation,
    InvalidDuration,
    DuplicateTag,
    InvalidTagRange,
    UnsupportedTagDirection,
    InvalidGenericSpec,
    ExpectedFrameCountMismatch,
    DuplicatePageId,
    CanonicalValidationFailed,
};

[[nodiscard]] std::string_view ToString(SpriteImportSourceKind value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteImportAnimationDirection value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteAsepriteRotatedFramePolicy value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteGenericSheetMode value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteImportErrorCode value) noexcept;

struct SpriteImportDecodedImageView final
{
    std::string_view id{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::span<const std::uint8_t> rgba8{};
};

struct SpriteImportedFrame final
{
    std::string regionId{};
    std::optional<std::int64_t> durationNanoseconds{};

    [[nodiscard]] bool operator==(const SpriteImportedFrame&) const noexcept = default;
};

struct SpriteImportedTag final
{
    std::string name{};
    std::uint32_t firstFrame{0};
    std::uint32_t lastFrame{0};
    SpriteImportAnimationDirection direction{SpriteImportAnimationDirection::Forward};

    [[nodiscard]] bool operator==(const SpriteImportedTag&) const noexcept = default;
};

struct SpriteImportDiagnostic final
{
    SpriteImportErrorCode code{SpriteImportErrorCode::InvalidField};
    std::string path{};
    std::string message{};

    [[nodiscard]] bool operator==(const SpriteImportDiagnostic&) const noexcept = default;
};

struct SpriteImportResult final
{
    std::uint32_t schemaVersion{1};
    SpriteImportSourceKind sourceKind{SpriteImportSourceKind::GenericSheet};
    SpriteAsset asset{};
    std::vector<SpriteImportedFrame> frames{};
    std::vector<SpriteImportedTag> tags{};
    std::vector<SpriteImportDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return diagnostics.empty() && !asset.pages.empty() && !asset.regions.empty();
    }
};

struct SpriteAsepriteSheetImportOptions final
{
    std::string_view canonicalAssetId{};
    std::string_view pageId{"main"};
    std::string_view textureReference{};
    SpriteSampling sampling{SpriteSampling::Nearest};
    SpriteColorSpace colorSpace{SpriteColorSpace::Srgb};
    SpriteRationalPivot defaultPivot{};
    SpriteAsepriteRotatedFramePolicy rotatedFramePolicy{
        SpriteAsepriteRotatedFramePolicy::Reject};
};

struct SpriteGenericRegionView final
{
    std::string_view id{};
    SpritePixelRect packedRect{};
    std::optional<SpritePixelSize> sourceSize{};
    std::optional<SpritePixelOffset> trimOffset{};
    std::optional<SpritePixelSize> trimSize{};
    std::optional<SpriteRationalPivot> pivot{};
    SpritePackedRotation packedRotation{SpritePackedRotation::None};
};

struct SpriteGenericSheetImportSpec final
{
    SpriteGenericSheetMode mode{SpriteGenericSheetMode::ExplicitRegions};
    std::span<const SpriteGenericRegionView> explicitRegions{};
    SpriteExtractionGridSpec grid{};
    std::span<const std::string_view> gridRegionIds{};
    std::uint32_t expectedFrameCount{0};
    SpriteRationalPivot defaultPivot{};
};

struct SpriteGenericSheetImportOptions final
{
    std::string_view canonicalAssetId{};
    std::string_view pageId{"main"};
    std::string_view textureReference{};
    SpriteSampling sampling{SpriteSampling::Nearest};
    SpriteColorSpace colorSpace{SpriteColorSpace::Srgb};
};

struct SpriteLooseFrameView final
{
    std::string_view pageId{};
    std::string_view regionId{};
    std::string_view textureReference{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::span<const std::uint8_t> rgba8{};
    std::optional<SpriteRationalPivot> pivot{};
};

struct SpriteLooseFrameImportOptions final
{
    std::string_view canonicalAssetId{};
    SpriteSampling sampling{SpriteSampling::Nearest};
    SpriteColorSpace colorSpace{SpriteColorSpace::Srgb};
    SpriteRationalPivot defaultPivot{};
};

[[nodiscard]] SpriteImportResult ImportAsepriteSpriteSheetJson(
    std::string_view jsonText,
    const SpriteImportDecodedImageView& decodedSheet,
    const SpriteAsepriteSheetImportOptions& options);

[[nodiscard]] SpriteImportResult ImportGenericSpriteSheet(
    const SpriteImportDecodedImageView& decodedSheet,
    const SpriteGenericSheetImportSpec& spec,
    const SpriteGenericSheetImportOptions& options);

[[nodiscard]] SpriteImportResult ImportLooseSpriteFrames(
    std::span<const SpriteLooseFrameView> frames,
    const SpriteLooseFrameImportOptions& options);

[[nodiscard]] std::string SerializeSpriteImportResultJson(const SpriteImportResult& result);
} // namespace trace2d::assets
