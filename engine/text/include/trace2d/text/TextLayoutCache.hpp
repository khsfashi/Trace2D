#pragma once

#include <trace2d/text/TextLayout.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace trace2d::text
{
// Caller-owned resolved UTF-8 source. identity identifies the semantic source (for example a
// localization key or UI element) and revision must change whenever the resolved bytes change.
// The cache intentionally trusts this revision contract so a hit does not need to scan utf8.
struct TextSourceView final
{
    std::uint64_t identity{0U};
    std::uint64_t revision{0U};
    std::string_view utf8{};
};

struct TextLayoutCacheConfig final
{
    TextLayoutRunConfig layout{};
    std::size_t maxFallbackFonts{8U};
};

struct TextLayoutCacheUpdateResult final
{
    std::optional<TextLayoutMetrics> metrics{};
    std::optional<TextLayoutDiagnostic> diagnostic{};
    bool reused{false};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return metrics.has_value() && !diagnostic.has_value();
    }
};

struct TextLayoutCachePrepareResult;

// Event/revision-driven cache above TextLayoutRun. A hit compares only fixed-size source/options
// state plus the bounded fallback pointer chain, then reuses the already-published layout.
class TextLayoutCache final
{
public:
    TextLayoutCache(const TextLayoutCache&) = delete;
    TextLayoutCache& operator=(const TextLayoutCache&) = delete;
    TextLayoutCache(TextLayoutCache&&) noexcept;
    TextLayoutCache& operator=(TextLayoutCache&&) noexcept;
    ~TextLayoutCache();

    [[nodiscard]] TextLayoutCacheUpdateResult Update(
        std::span<const TextFontAtlasRef> fallbackAtlases,
        TextSourceView source,
        TextLayoutOptions options = {});

    [[nodiscard]] const TextLayoutRun* Layout() const noexcept;
    [[nodiscard]] bool HasPublishedLayout() const noexcept;
    [[nodiscard]] TextLayoutCacheConfig Config() const noexcept;

    // Invalidates only the cache key. Prepared capacities and the last published TextLayoutRun
    // storage are retained for reuse by the next Update.
    void Reset() noexcept;

private:
    struct Impl;
    explicit TextLayoutCache(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_{};

    friend TextLayoutCachePrepareResult PrepareTextLayoutCache(TextLayoutCacheConfig config);
};

struct TextLayoutCachePrepareResult final
{
    std::unique_ptr<TextLayoutCache> cache{};
    std::optional<TextLayoutDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return cache != nullptr && !diagnostic.has_value();
    }
};

[[nodiscard]] TextLayoutCachePrepareResult PrepareTextLayoutCache(TextLayoutCacheConfig config = {});
} // namespace trace2d::text
