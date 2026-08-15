#pragma once

#include <trace2d/text/TextLayoutCache.hpp>
#include <trace2d/ui/Ui.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace trace2d::ui
{
enum class UiTextLayoutErrorCode : std::uint8_t
{
    InvalidConfig = 0,
    ElementNotFound,
    ElementNotTextBearing,
    ComposedTextCapacityExceeded,
    TextLayoutFailed,
    AllocationFailed,
};

[[nodiscard]] std::string_view ToString(UiTextLayoutErrorCode value) noexcept;

struct UiTextLayoutDiagnostic final
{
    UiTextLayoutErrorCode code{UiTextLayoutErrorCode::InvalidConfig};
    std::string message{};
    std::size_t requiredUtf8Bytes{0U};
    std::optional<text::TextLayoutDiagnostic> textDiagnostic{};
};

struct UiTextLayoutCacheConfig final
{
    text::TextLayoutCacheConfig text{};
    std::size_t maxComposedUtf8Bytes{16U * 1024U};
};

struct UiTextLayoutUpdateResult final
{
    std::optional<text::TextLayoutMetrics> metrics{};
    std::optional<UiTextLayoutDiagnostic> diagnostic{};
    bool reused{false};
    bool includesComposition{false};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return metrics.has_value() && !diagnostic.has_value();
    }
};

struct UiTextLayoutCachePrepareResult;

class UiTextLayoutCache final
{
public:
    UiTextLayoutCache(const UiTextLayoutCache&) = delete;
    UiTextLayoutCache& operator=(const UiTextLayoutCache&) = delete;
    UiTextLayoutCache(UiTextLayoutCache&&) noexcept;
    UiTextLayoutCache& operator=(UiTextLayoutCache&&) noexcept;
    ~UiTextLayoutCache();

    [[nodiscard]] UiTextLayoutUpdateResult Update(
        UiDocument& document,
        std::string_view elementId,
        std::span<const text::TextFontAtlasRef> fallbackAtlases,
        text::TextLayoutOptions options = {});

    [[nodiscard]] const text::TextLayoutRun* Layout() const noexcept;
    [[nodiscard]] UiTextLayoutCacheConfig Config() const noexcept;
    void Reset() noexcept;

private:
    struct Impl;
    explicit UiTextLayoutCache(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_{};

    friend UiTextLayoutCachePrepareResult PrepareUiTextLayoutCache(UiTextLayoutCacheConfig config);
};

struct UiTextLayoutCachePrepareResult final
{
    std::unique_ptr<UiTextLayoutCache> cache{};
    std::optional<UiTextLayoutDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return cache != nullptr && !diagnostic.has_value();
    }
};

[[nodiscard]] text::TextLayoutOptions UiTextLayoutOptionsForElement(
    const UiElement& element,
    text::TextLayoutOptions options = {}) noexcept;

[[nodiscard]] UiTextLayoutCachePrepareResult PrepareUiTextLayoutCache(
    UiTextLayoutCacheConfig config = {});
} // namespace trace2d::ui
