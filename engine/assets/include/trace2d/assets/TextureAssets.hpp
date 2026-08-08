#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace trace2d::assets
{
enum class TextureAssetErrorCode : std::uint8_t
{
    InvalidReference,
    UnsupportedFormat,
    MissingFile,
    ReadFailure,
    DecodeFailure,
    SizeOverflow,
};

[[nodiscard]] std::string_view ToString(TextureAssetErrorCode code) noexcept;

struct TextureAssetDiagnostic final
{
    TextureAssetErrorCode code{TextureAssetErrorCode::InvalidReference};
    std::string reference{};
    std::string resolvedPath{};
    std::string message{};
};

struct TextureAssetData final
{
    std::string id{};
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> rgba8{};
};

struct TextureAssetLoadResult final
{
    std::shared_ptr<const TextureAssetData> asset{};
    std::optional<TextureAssetDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return asset != nullptr && !diagnostic.has_value();
    }
};

struct TextureAssetCacheMetrics final
{
    std::uint64_t requests{0};
    std::uint64_t cacheHits{0};
    std::uint64_t cacheMisses{0};
    std::uint64_t successfulImports{0};
    std::uint64_t failedImports{0};
    std::size_t cachedAssets{0};
};

class TextureAssetCache final
{
public:
    explicit TextureAssetCache(std::filesystem::path projectRoot);

    TextureAssetCache(const TextureAssetCache&) = delete;
    TextureAssetCache& operator=(const TextureAssetCache&) = delete;
    TextureAssetCache(TextureAssetCache&&) noexcept = default;
    TextureAssetCache& operator=(TextureAssetCache&&) noexcept = default;
    ~TextureAssetCache() = default;

    [[nodiscard]] TextureAssetLoadResult Load(std::string_view projectRelativeReference);
    [[nodiscard]] bool Invalidate(std::string_view projectRelativeReference);
    void Clear() noexcept;

    [[nodiscard]] const std::filesystem::path& ProjectRoot() const noexcept;
    [[nodiscard]] TextureAssetCacheMetrics Metrics() const noexcept;

private:
    std::filesystem::path projectRoot_{};
    std::unordered_map<std::string, std::shared_ptr<const TextureAssetData>> cache_{};
    std::uint64_t requests_{0};
    std::uint64_t cacheHits_{0};
    std::uint64_t cacheMisses_{0};
    std::uint64_t successfulImports_{0};
    std::uint64_t failedImports_{0};
};
} // namespace trace2d::assets
