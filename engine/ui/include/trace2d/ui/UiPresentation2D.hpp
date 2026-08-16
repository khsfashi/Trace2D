#pragma once

#include <trace2d/render/CameraViewport2D.hpp>
#include <trace2d/text/TextPresentation2D.hpp>
#include <trace2d/ui/Ui.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace trace2d::ui
{
enum class UiPresentationErrorCode : std::uint8_t
{
    InvalidConfig = 0,
    InvalidDocument,
    InvalidViewport,
    InvalidSolidTexture,
    InvalidTextInput,
    MissingTextInput,
    InvalidImageTexture,
    TextPresentationFailed,
    ElementCapacityExceeded,
    PresentationCapacityExceeded,
    AllocationFailed,
};

[[nodiscard]] std::string_view ToString(UiPresentationErrorCode value) noexcept;

// One-time binding for the ordinary #86 TextureResource used to tint solid UI rectangles. Resolve
// this before releasing a releasable CPU payload. Steady UI presentation keeps only canonical
// generation-safe texture identity and the already-resolved Sprite texture encoding.
struct UiSolidTextureBinding2D final
{
    render::TextureHandle texture{render::InvalidTextureHandle};
    render::SpriteTextureEncoding encoding{render::SpriteTextureEncoding::Linear};

    [[nodiscard]] bool operator==(const UiSolidTextureBinding2D&) const noexcept = default;
};

struct UiTextPresentationInput2D final
{
    // Setup-time resolved UiDocument::Elements() index. U15 never performs a semantic-id lookup on
    // the steady presentation path. Inputs must be strictly increasing by elementIndex.
    std::size_t elementIndex{InvalidUiElementIndex};
    const text::TextLayoutRun* layout{nullptr};
    std::span<const text::TextFontAtlasRef> fallbackAtlases{};
    std::span<const text::GlyphAtlasTextureBinding2D> bindings{};

    // Stable caller-owned revision for the already-built layout/binding chain. Change this whenever
    // glyph positions, layout options, font selection or another presentation-relevant input changes.
    // The UiElement displayTextRevision is mixed independently.
    std::uint64_t presentationRevision{0U};
};

struct UiPresentationCacheConfig final
{
    // All retained element metadata and command/scratch storage are reserved once. Rebuilds inside
    // the prepared bounds never allocate.
    std::size_t maxPresentations{4096U};
    std::size_t maxElements{1024U};
};

struct UiPresentationDiagnostic final
{
    UiPresentationErrorCode code{UiPresentationErrorCode::InvalidConfig};
    std::size_t elementIndex{InvalidUiElementIndex};
    std::size_t requiredPresentations{0U};
    text::TextPresentationStatus textStatus{};
};

struct UiPresentationUpdateResult final
{
    bool reused{false};
    std::optional<UiPresentationDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return !diagnostic.has_value();
    }
};

struct UiPresentationMetrics final
{
    std::uint64_t updateCalls{0U};
    // Any presentation-command rebuild. Unchanged frames leave this unchanged.
    std::uint64_t rebuilds{0U};
    std::uint64_t fullRebuilds{0U};
    std::uint64_t partialRebuilds{0U};
    std::uint64_t cacheHits{0U};
    // Amount of retained state touched by rebuild work. A bounded Progress mutation can therefore
    // prove that it patched one element instead of rebuilding the full document command list.
    std::uint64_t elementsRebuilt{0U};
    std::uint64_t presentationsRebuilt{0U};
    std::uint64_t solidPresentationsBuilt{0U};
    std::uint64_t imagePresentationsBuilt{0U};
    std::uint64_t textPresentationsBuilt{0U};
    std::size_t lastPresentationCount{0U};

    [[nodiscard]] bool operator==(const UiPresentationMetrics&) const noexcept = default;
};

struct UiPresentationFrame2D final
{
    // Fixed UI-overlay camera. World geometry is derived once from logical top-left UI coordinates;
    // #88 rasterViewport then maps that logical surface to the current presentation target.
    render::OrthographicCamera camera{};
    std::span<const render::SpritePresentationRenderData> presentations{};
};

class UiPresentationCache2D final
{
public:
    UiPresentationCache2D(const UiPresentationCache2D&) = delete;
    UiPresentationCache2D& operator=(const UiPresentationCache2D&) = delete;
    UiPresentationCache2D(UiPresentationCache2D&&) noexcept;
    UiPresentationCache2D& operator=(UiPresentationCache2D&&) noexcept;
    ~UiPresentationCache2D();

    [[nodiscard]] UiPresentationUpdateResult Update(
        const UiDocument& document,
        const assets::ResourceRegistry& resources,
        const render::ResolvedViewport2D& viewport,
        UiSolidTextureBinding2D solidTexture,
        std::span<const UiTextPresentationInput2D> textInputs = {});

    [[nodiscard]] UiPresentationFrame2D Frame() const noexcept;
    [[nodiscard]] UiPresentationCacheConfig Config() const noexcept;
    [[nodiscard]] const UiPresentationMetrics& Metrics() const noexcept;
    void Reset() noexcept;

private:
    struct Impl;
    explicit UiPresentationCache2D(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> impl_{};

    friend struct UiPresentationCachePrepareResult;
    friend UiPresentationCachePrepareResult PrepareUiPresentationCache(UiPresentationCacheConfig config);
};

struct UiPresentationCachePrepareResult final
{
    std::unique_ptr<UiPresentationCache2D> cache{};
    std::optional<UiPresentationDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return cache != nullptr && !diagnostic.has_value();
    }
};

// Validates an ordinary 1x1 opaque white, straight-alpha TextureResource exactly once. U15 does not
// create a private texture identity or GPU upload path; Renderer residency remains caller-owned.
[[nodiscard]] bool ResolveUiSolidTextureBinding2D(
    const assets::ResourceRegistry& resources,
    render::TextureHandle texture,
    UiSolidTextureBinding2D& outBinding) noexcept;

[[nodiscard]] UiPresentationCachePrepareResult PrepareUiPresentationCache(
    UiPresentationCacheConfig config = {});
} // namespace trace2d::ui
