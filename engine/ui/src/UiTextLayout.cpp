#include <trace2d/ui/UiTextLayout.hpp>

#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <utility>

namespace trace2d::ui
{
namespace
{
constexpr std::size_t MaxComposedUtf8Bytes = 1U << 20U;

[[nodiscard]] UiTextLayoutDiagnostic LayoutDiagnostic(
    const UiTextLayoutErrorCode code,
    std::string message)
{
    UiTextLayoutDiagnostic output{};
    output.code = code;
    output.message = std::move(message);
    return output;
}

[[nodiscard]] bool IsTextBearing(const UiElementKind kind) noexcept
{
    return kind == UiElementKind::Label || kind == UiElementKind::Button ||
           kind == UiElementKind::TextInput;
}
} // namespace

struct UiTextLayoutCache::Impl final
{
    UiTextLayoutCacheConfig config{};
    std::unique_ptr<text::TextLayoutCache> textCache{};
    std::string composedUtf8{};
    const UiDocument* boundDocument{nullptr};
};

UiTextLayoutCache::UiTextLayoutCache(std::unique_ptr<Impl> impl) noexcept
    : impl_{std::move(impl)}
{
}

UiTextLayoutCache::UiTextLayoutCache(UiTextLayoutCache&&) noexcept = default;
UiTextLayoutCache& UiTextLayoutCache::operator=(UiTextLayoutCache&&) noexcept = default;
UiTextLayoutCache::~UiTextLayoutCache() = default;

UiTextLayoutUpdateResult UiTextLayoutCache::Update(
    UiDocument& document,
    const std::string_view elementId,
    const std::span<const text::TextFontAtlasRef> fallbackAtlases,
    const text::TextLayoutOptions options)
{
    UiTextLayoutUpdateResult output{};
    if (impl_ == nullptr || impl_->textCache == nullptr)
    {
        output.diagnostic = LayoutDiagnostic(
            UiTextLayoutErrorCode::InvalidConfig,
            "UI text layout cache is not prepared");
        return output;
    }

    if (impl_->boundDocument != &document)
    {
        impl_->textCache->Reset();
        impl_->boundDocument = &document;
    }

    const UiElement* const element = document.Find(elementId);
    if (element == nullptr)
    {
        impl_->textCache->Reset();
        output.diagnostic = LayoutDiagnostic(
            UiTextLayoutErrorCode::ElementNotFound,
            "UI text layout target does not exist");
        return output;
    }
    if (!IsTextBearing(element->kind))
    {
        impl_->textCache->Reset();
        output.diagnostic = LayoutDiagnostic(
            UiTextLayoutErrorCode::ElementNotTextBearing,
            "UI text layout target is not a label, button, or text input");
        return output;
    }

    const UiTextCompositionState composition = document.TextComposition();
    const bool includesComposition = element->kind == UiElementKind::TextInput &&
        document.IsFocused(elementId) && composition.active;

    std::string_view displayUtf8 = element->text;
    if (includesComposition)
    {
        const bool sizeOverflow = element->text.size() >
            std::numeric_limits<std::size_t>::max() - composition.text.size();
        const std::size_t requiredBytes = sizeOverflow
            ? std::numeric_limits<std::size_t>::max()
            : element->text.size() + composition.text.size();
        if (sizeOverflow || requiredBytes > impl_->config.maxComposedUtf8Bytes)
        {
            impl_->textCache->Reset();
            UiTextLayoutDiagnostic diagnostic = LayoutDiagnostic(
                UiTextLayoutErrorCode::ComposedTextCapacityExceeded,
                "committed text plus IME composition exceeds prepared UI text scratch capacity");
            diagnostic.requiredUtf8Bytes = requiredBytes;
            document.PublishTextLayoutEvidence(elementId, {});
            output.diagnostic = std::move(diagnostic);
            return output;
        }

        impl_->composedUtf8.clear();
        impl_->composedUtf8.append(element->text);
        impl_->composedUtf8.append(composition.text);
        displayUtf8 = impl_->composedUtf8;
    }

    const text::TextLayoutOptions boundedOptions =
        UiTextLayoutOptionsForElement(*element, options);
    const text::TextLayoutCacheUpdateResult updated = impl_->textCache->Update(
        fallbackAtlases,
        text::TextSourceView{
            .identity = element->textSourceIdentity,
            .revision = element->displayTextRevision,
            .utf8 = displayUtf8,
        },
        boundedOptions);
    if (!updated.Succeeded())
    {
        impl_->textCache->Reset();
        UiTextLayoutDiagnostic diagnostic = LayoutDiagnostic(
            UiTextLayoutErrorCode::TextLayoutFailed,
            "production Text layout rejected the UI display text");
        diagnostic.textDiagnostic = updated.diagnostic;
        document.PublishTextLayoutEvidence(elementId, {});
        output.diagnostic = std::move(diagnostic);
        return output;
    }

    const text::TextLayoutMetrics metrics = *updated.metrics;
    document.PublishTextLayoutEvidence(
        elementId,
        UiTextLayoutEvidence{
            .valid = true,
            .sourceRevision = element->displayTextRevision,
            .includesComposition = includesComposition,
            .glyphCount = metrics.glyphCount,
            .lineCount = metrics.lineCount,
            .contentWidth26_6 = metrics.contentWidth26_6,
            .contentHeight26_6 = metrics.contentHeight26_6,
            .layoutWidth26_6 = metrics.layoutWidth26_6,
            .layoutHeight26_6 = metrics.layoutHeight26_6,
        });

    output.metrics = metrics;
    output.reused = updated.reused;
    output.includesComposition = includesComposition;
    return output;
}

const text::TextLayoutRun* UiTextLayoutCache::Layout() const noexcept
{
    return impl_ != nullptr && impl_->textCache != nullptr
        ? impl_->textCache->Layout()
        : nullptr;
}

UiTextLayoutCacheConfig UiTextLayoutCache::Config() const noexcept
{
    return impl_ != nullptr ? impl_->config : UiTextLayoutCacheConfig{};
}

void UiTextLayoutCache::Reset() noexcept
{
    if (impl_ != nullptr && impl_->textCache != nullptr)
    {
        impl_->textCache->Reset();
        impl_->boundDocument = nullptr;
        impl_->composedUtf8.clear();
    }
}

text::TextLayoutOptions UiTextLayoutOptionsForElement(
    const UiElement& element,
    text::TextLayoutOptions options) noexcept
{
    options.boxWidth26_6 = static_cast<std::int64_t>(element.bounds.width) * 64;
    options.boxHeight26_6 = static_cast<std::int64_t>(element.bounds.height) * 64;
    return options;
}

UiTextLayoutCachePrepareResult PrepareUiTextLayoutCache(const UiTextLayoutCacheConfig config)
{
    UiTextLayoutCachePrepareResult output{};
    if (config.maxComposedUtf8Bytes == 0U || config.maxComposedUtf8Bytes > MaxComposedUtf8Bytes)
    {
        output.diagnostic = LayoutDiagnostic(
            UiTextLayoutErrorCode::InvalidConfig,
            "UI composed-text scratch capacity exceeds the supported bounded range");
        return output;
    }

    text::TextLayoutCachePrepareResult textCache = text::PrepareTextLayoutCache(config.text);
    if (!textCache.Succeeded())
    {
        UiTextLayoutDiagnostic diagnostic = LayoutDiagnostic(
            UiTextLayoutErrorCode::InvalidConfig,
            "failed to prepare the underlying production Text layout cache");
        diagnostic.textDiagnostic = textCache.diagnostic;
        output.diagnostic = std::move(diagnostic);
        return output;
    }

    try
    {
        auto impl = std::make_unique<UiTextLayoutCache::Impl>();
        impl->config = config;
        impl->textCache = std::move(textCache.cache);
        impl->composedUtf8.reserve(config.maxComposedUtf8Bytes);
        output.cache = std::unique_ptr<UiTextLayoutCache>(new UiTextLayoutCache(std::move(impl)));
    }
    catch (const std::bad_alloc&)
    {
        output.diagnostic = LayoutDiagnostic(
            UiTextLayoutErrorCode::AllocationFailed,
            "failed to allocate bounded UI text layout scratch storage");
    }
    return output;
}

std::string_view ToString(const UiTextLayoutErrorCode value) noexcept
{
    switch (value)
    {
    case UiTextLayoutErrorCode::InvalidConfig:
        return "invalid_config";
    case UiTextLayoutErrorCode::ElementNotFound:
        return "element_not_found";
    case UiTextLayoutErrorCode::ElementNotTextBearing:
        return "element_not_text_bearing";
    case UiTextLayoutErrorCode::ComposedTextCapacityExceeded:
        return "composed_text_capacity_exceeded";
    case UiTextLayoutErrorCode::TextLayoutFailed:
        return "text_layout_failed";
    case UiTextLayoutErrorCode::AllocationFailed:
        return "allocation_failed";
    }
    return "unknown_ui_text_layout_error";
}
} // namespace trace2d::ui
