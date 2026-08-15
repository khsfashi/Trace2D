#include <trace2d/text/TextLayoutCache.hpp>

#include <cstddef>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace trace2d::text
{
namespace
{
constexpr std::size_t MaxCachedFallbackFonts = 64U;

[[nodiscard]] TextLayoutDiagnostic CacheDiagnostic(
    const TextLayoutErrorCode code,
    std::string message)
{
    TextLayoutDiagnostic output{};
    output.code = code;
    output.message = std::move(message);
    return output;
}

[[nodiscard]] bool SameOptions(
    const TextLayoutOptions& left,
    const TextLayoutOptions& right) noexcept
{
    return left.boxWidth26_6 == right.boxWidth26_6 &&
           left.boxHeight26_6 == right.boxHeight26_6 &&
           left.lineHeight26_6 == right.lineHeight26_6 &&
           left.baselineOffset26_6 == right.baselineOffset26_6 &&
           left.wrapMode == right.wrapMode &&
           left.horizontalAlignment == right.horizontalAlignment &&
           left.verticalAlignment == right.verticalAlignment;
}
} // namespace

struct TextLayoutCache::Impl final
{
    TextLayoutCacheConfig config{};
    std::unique_ptr<TextLayoutRun> run{};
    std::vector<const GlyphAtlas*> fallbackIdentity{};
    TextLayoutOptions options{};
    std::uint64_t sourceIdentity{0U};
    std::uint64_t sourceRevision{0U};
    bool published{false};
};

TextLayoutCache::TextLayoutCache(std::unique_ptr<Impl> impl) noexcept
    : impl_{std::move(impl)}
{
}

TextLayoutCache::TextLayoutCache(TextLayoutCache&&) noexcept = default;
TextLayoutCache& TextLayoutCache::operator=(TextLayoutCache&&) noexcept = default;
TextLayoutCache::~TextLayoutCache() = default;

bool TextLayoutCache::CanReuse(
    const std::span<const TextFontAtlasRef> fallbackAtlases,
    const TextSourceView source,
    const TextLayoutOptions options) const noexcept
{
    if (impl_ == nullptr || impl_->run == nullptr || !impl_->published ||
        fallbackAtlases.empty() || fallbackAtlases.size() > impl_->config.maxFallbackFonts ||
        fallbackAtlases.size() != impl_->fallbackIdentity.size() ||
        source.identity != impl_->sourceIdentity ||
        source.revision != impl_->sourceRevision ||
        !SameOptions(options, impl_->options))
    {
        return false;
    }

    for (std::size_t index = 0U; index < fallbackAtlases.size(); ++index)
    {
        if (fallbackAtlases[index].atlas != impl_->fallbackIdentity[index])
        {
            return false;
        }
    }
    return true;
}

TextLayoutCacheUpdateResult TextLayoutCache::Update(
    const std::span<const TextFontAtlasRef> fallbackAtlases,
    const TextSourceView source,
    const TextLayoutOptions options)
{
    TextLayoutCacheUpdateResult output{};
    if (impl_ == nullptr || impl_->run == nullptr)
    {
        output.diagnostic = CacheDiagnostic(
            TextLayoutErrorCode::InvalidConfig,
            "text layout cache is not prepared");
        return output;
    }

    if (fallbackAtlases.empty() || fallbackAtlases.size() > impl_->config.maxFallbackFonts)
    {
        output.diagnostic = CacheDiagnostic(
            TextLayoutErrorCode::InvalidConfig,
            "text fallback atlas chain exceeds the prepared cache capacity");
        return output;
    }

    if (CanReuse(fallbackAtlases, source, options))
    {
        output.metrics = impl_->run->Metrics();
        output.reused = true;
        return output;
    }

    const TextLayoutResult layout = impl_->run->LayoutUtf8(
        fallbackAtlases,
        source.utf8,
        options);
    if (!layout.Succeeded())
    {
        output.diagnostic = layout.diagnostic;
        return output;
    }

    impl_->fallbackIdentity.clear();
    for (const TextFontAtlasRef fallback : fallbackAtlases)
    {
        impl_->fallbackIdentity.push_back(fallback.atlas);
    }
    impl_->sourceIdentity = source.identity;
    impl_->sourceRevision = source.revision;
    impl_->options = options;
    impl_->published = true;

    output.metrics = layout.metrics;
    return output;
}

const TextLayoutRun* TextLayoutCache::Layout() const noexcept
{
    if (impl_ == nullptr || !impl_->published)
    {
        return nullptr;
    }
    return impl_->run.get();
}

bool TextLayoutCache::HasPublishedLayout() const noexcept
{
    return impl_ != nullptr && impl_->published;
}

TextLayoutCacheConfig TextLayoutCache::Config() const noexcept
{
    return impl_ != nullptr ? impl_->config : TextLayoutCacheConfig{};
}

void TextLayoutCache::Reset() noexcept
{
    if (impl_ != nullptr)
    {
        impl_->published = false;
        impl_->fallbackIdentity.clear();
    }
}

TextLayoutCachePrepareResult PrepareTextLayoutCache(const TextLayoutCacheConfig config)
{
    TextLayoutCachePrepareResult output{};
    if (config.maxFallbackFonts == 0U || config.maxFallbackFonts > MaxCachedFallbackFonts)
    {
        output.diagnostic = CacheDiagnostic(
            TextLayoutErrorCode::InvalidConfig,
            "text layout cache fallback capacity exceeds the supported bounded range");
        return output;
    }

    TextLayoutRunPrepareResult preparedRun = PrepareTextLayoutRun(config.layout);
    if (!preparedRun.Succeeded())
    {
        output.diagnostic = preparedRun.diagnostic;
        return output;
    }

    try
    {
        auto impl = std::make_unique<TextLayoutCache::Impl>();
        impl->config = config;
        impl->run = std::move(preparedRun.run);
        impl->fallbackIdentity.reserve(config.maxFallbackFonts);
        output.cache = std::unique_ptr<TextLayoutCache>(new TextLayoutCache(std::move(impl)));
    }
    catch (const std::bad_alloc&)
    {
        output.diagnostic = CacheDiagnostic(
            TextLayoutErrorCode::AllocationFailed,
            "failed to allocate bounded text layout cache storage");
    }
    return output;
}
} // namespace trace2d::text
