#pragma once

#include <trace2d/assets/TextureAssets.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace trace2d::assets
{
enum class SpriteColorSpace : std::uint8_t
{
    Srgb = 0,
    Linear = 1,
};

enum class SpriteAlphaMode : std::uint8_t
{
    Straight = 0,
};

enum class SpriteSampling : std::uint8_t
{
    Nearest = 0,
    Linear = 1,
};

enum class SpritePackedRotation : std::uint8_t
{
    None = 0,
    Cw90 = 1,
};

[[nodiscard]] std::string_view ToString(SpriteColorSpace value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteAlphaMode value) noexcept;
[[nodiscard]] std::string_view ToString(SpriteSampling value) noexcept;
[[nodiscard]] std::string_view ToString(SpritePackedRotation value) noexcept;

struct SpritePixelSize final
{
    std::uint32_t width{0};
    std::uint32_t height{0};

    [[nodiscard]] bool operator==(const SpritePixelSize&) const noexcept = default;
};

struct SpritePixelOffset final
{
    std::uint32_t x{0};
    std::uint32_t y{0};

    [[nodiscard]] bool operator==(const SpritePixelOffset&) const noexcept = default;
};

struct SpritePixelRect final
{
    std::uint32_t x{0};
    std::uint32_t y{0};
    std::uint32_t width{0};
    std::uint32_t height{0};

    [[nodiscard]] bool operator==(const SpritePixelRect&) const noexcept = default;
};

// Exact untrimmed source-space 9-slice border metadata. The zero value means that the
// canonical region has no authored border. SR5 presentation may still render that region as
// a normal quad; sliced/tiled presentation validates the border against source_size.
struct SpritePixelBorder final
{
    std::uint32_t left{0};
    std::uint32_t top{0};
    std::uint32_t right{0};
    std::uint32_t bottom{0};

    [[nodiscard]] bool operator==(const SpritePixelBorder&) const noexcept = default;
};

struct SpriteRationalPivot final
{
    std::int64_t xNumerator{0};
    std::int64_t yNumerator{0};
    std::int64_t denominator{1};

    [[nodiscard]] bool operator==(const SpriteRationalPivot&) const noexcept = default;
};

struct SpriteAtlasPage final
{
    std::string id{};
    std::string textureReference{};
    SpritePixelSize size{};
    SpriteColorSpace colorSpace{SpriteColorSpace::Srgb};
    SpriteAlphaMode alphaMode{SpriteAlphaMode::Straight};

    [[nodiscard]] bool operator==(const SpriteAtlasPage&) const noexcept = default;
};

struct SpriteRegion final
{
    std::string id{};
    std::string pageId{};
    SpritePixelSize sourceSize{};
    SpritePixelOffset trimOffset{};
    SpritePixelSize trimSize{};
    SpritePixelRect packedRect{};
    SpriteRationalPivot pivot{};
    SpritePackedRotation packedRotation{SpritePackedRotation::None};
    SpritePixelBorder border{};

    [[nodiscard]] bool operator==(const SpriteRegion&) const noexcept = default;
};

struct SpriteAsset final
{
    std::string id{};
    std::uint32_t schemaVersion{1};
    SpriteSampling sampling{SpriteSampling::Nearest};
    std::vector<SpriteAtlasPage> pages{};
    std::vector<SpriteRegion> regions{};

    [[nodiscard]] bool operator==(const SpriteAsset&) const noexcept = default;
};

enum class SpriteAssetErrorCode : std::uint8_t
{
    InvalidReference = 0,
    UnsupportedFormat,
    MissingFile,
    ReadFailure,
    ParseError,
    SchemaError,
    TextureValidationError,
};

[[nodiscard]] std::string_view ToString(SpriteAssetErrorCode code) noexcept;

struct SpriteAssetDiagnostic final
{
    SpriteAssetErrorCode code{SpriteAssetErrorCode::SchemaError};
    std::string reference{};
    std::string resolvedPath{};
    std::string path{};
    std::string message{};
    std::size_t line{0};
    std::size_t column{0};
};

struct SpriteAssetLoadResult final
{
    std::shared_ptr<const SpriteAsset> asset{};
    std::vector<SpriteAssetDiagnostic> diagnostics{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return asset != nullptr && diagnostics.empty();
    }
};

struct SpriteAssetCacheMetrics final
{
    std::uint64_t requests{0};
    std::uint64_t cacheHits{0};
    std::uint64_t cacheMisses{0};
    std::uint64_t successfulImports{0};
    std::uint64_t failedImports{0};
    std::size_t cachedAssets{0};
};

[[nodiscard]] SpriteAssetLoadResult ParseSpriteAssetToml(
    std::string_view text,
    std::string_view canonicalAssetId,
    std::string_view sourceName = {});

[[nodiscard]] std::string SaveSpriteAssetToml(const SpriteAsset& asset);

class SpriteAssetCache final
{
public:
    explicit SpriteAssetCache(std::filesystem::path projectRoot);

    SpriteAssetCache(const SpriteAssetCache&) = delete;
    SpriteAssetCache& operator=(const SpriteAssetCache&) = delete;
    SpriteAssetCache(SpriteAssetCache&&) noexcept = default;
    SpriteAssetCache& operator=(SpriteAssetCache&&) noexcept = default;
    ~SpriteAssetCache() = default;

    [[nodiscard]] SpriteAssetLoadResult Load(std::string_view projectRelativeReference);
    [[nodiscard]] bool Invalidate(std::string_view projectRelativeReference);
    void Clear() noexcept;

    [[nodiscard]] const std::filesystem::path& ProjectRoot() const noexcept;
    [[nodiscard]] SpriteAssetCacheMetrics Metrics() const noexcept;
    [[nodiscard]] TextureAssetCacheMetrics TextureMetrics() const noexcept;

private:
    std::filesystem::path projectRoot_{};
    TextureAssetCache textureCache_;
    std::unordered_map<std::string, std::shared_ptr<const SpriteAsset>> cache_{};
    std::uint64_t requests_{0};
    std::uint64_t cacheHits_{0};
    std::uint64_t cacheMisses_{0};
    std::uint64_t successfulImports_{0};
    std::uint64_t failedImports_{0};
};
} // namespace trace2d::assets
