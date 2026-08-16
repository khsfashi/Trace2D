#include <trace2d/ui/UiPresentation2D.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace trace2d::ui
{
namespace
{
constexpr std::size_t MaxPreparedPresentations = 1U << 20U;
constexpr std::size_t MaxPreparedElements = 1U << 20U;
constexpr std::uint64_t FnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t FnvPrime = 1099511628211ULL;

struct UiColor final
{
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
};

constexpr UiColor PanelColor{28U, 32U, 42U, 255U};
constexpr UiColor TextColor{236U, 240U, 248U, 255U};
constexpr UiColor DisabledTextColor{140U, 144U, 152U, 255U};
constexpr UiColor ButtonColor{58U, 84U, 136U, 255U};
constexpr UiColor DisabledButtonColor{66U, 68U, 74U, 255U};
constexpr UiColor InputColor{20U, 23U, 30U, 255U};
constexpr UiColor ProgressTrackColor{36U, 40U, 52U, 255U};
constexpr UiColor ProgressFillColor{76U, 166U, 106U, 255U};
constexpr UiColor BorderColor{154U, 166U, 188U, 255U};
constexpr UiColor FocusBorderColor{246U, 196U, 76U, 255U};

[[nodiscard]] render::SpriteLinearRgba ToLinearRgba(const UiColor color) noexcept
{
    constexpr float Scale = 1.0F / 255.0F;
    return render::SpriteLinearRgba{
        static_cast<float>(color.r) * Scale,
        static_cast<float>(color.g) * Scale,
        static_cast<float>(color.b) * Scale,
        static_cast<float>(color.a) * Scale,
    };
}

[[nodiscard]] bool IsLiveTextureHandle(const render::TextureHandle texture) noexcept
{
    return texture.generation != 0U && texture.domain == assets::ResourceTypeDomain::Texture;
}

void Mix(std::uint64_t& hash, const std::uint64_t value) noexcept
{
    hash ^= value;
    hash *= FnvPrime;
}

void MixFloat(std::uint64_t& hash, const float value) noexcept
{
    Mix(hash, std::bit_cast<std::uint32_t>(value));
}

void MixHandle(std::uint64_t& hash, const render::TextureHandle value) noexcept
{
    Mix(hash, value.slot);
    Mix(hash, value.generation);
    Mix(hash, static_cast<std::uint64_t>(value.domain));
}

void MixRect(std::uint64_t& hash, const UiRect value) noexcept
{
    Mix(hash, value.x);
    Mix(hash, value.y);
    Mix(hash, value.width);
    Mix(hash, value.height);
}

void MixRect(std::uint64_t& hash, const UiPresentationRect value) noexcept
{
    Mix(hash, static_cast<std::uint32_t>(value.x));
    Mix(hash, static_cast<std::uint32_t>(value.y));
    Mix(hash, value.width);
    Mix(hash, value.height);
}

[[nodiscard]] UiRect IntersectRect(const UiRect lhs, const UiRect rhs) noexcept
{
    const std::uint64_t lhsRight = static_cast<std::uint64_t>(lhs.x) + lhs.width;
    const std::uint64_t lhsBottom = static_cast<std::uint64_t>(lhs.y) + lhs.height;
    const std::uint64_t rhsRight = static_cast<std::uint64_t>(rhs.x) + rhs.width;
    const std::uint64_t rhsBottom = static_cast<std::uint64_t>(rhs.y) + rhs.height;
    const std::uint64_t left = std::max<std::uint64_t>(lhs.x, rhs.x);
    const std::uint64_t top = std::max<std::uint64_t>(lhs.y, rhs.y);
    const std::uint64_t right = std::min(lhsRight, rhsRight);
    const std::uint64_t bottom = std::min(lhsBottom, rhsBottom);
    if (right <= left || bottom <= top)
    {
        return UiRect{
            static_cast<std::uint32_t>(std::min<std::uint64_t>(left, std::numeric_limits<std::uint32_t>::max())),
            static_cast<std::uint32_t>(std::min<std::uint64_t>(top, std::numeric_limits<std::uint32_t>::max())),
            0U,
            0U,
        };
    }
    return UiRect{
        static_cast<std::uint32_t>(left),
        static_cast<std::uint32_t>(top),
        static_cast<std::uint32_t>(right - left),
        static_cast<std::uint32_t>(bottom - top),
    };
}

[[nodiscard]] UiRect PresentationExtent(const UiPresentationRect bounds) noexcept
{
    const std::int64_t left = std::max<std::int64_t>(bounds.x, 0);
    const std::int64_t top = std::max<std::int64_t>(bounds.y, 0);
    const std::int64_t right = std::max<std::int64_t>(
        left,
        static_cast<std::int64_t>(bounds.x) + static_cast<std::int64_t>(bounds.width));
    const std::int64_t bottom = std::max<std::int64_t>(
        top,
        static_cast<std::int64_t>(bounds.y) + static_cast<std::int64_t>(bounds.height));
    return UiRect{
        static_cast<std::uint32_t>(std::min<std::int64_t>(left, std::numeric_limits<std::uint32_t>::max())),
        static_cast<std::uint32_t>(std::min<std::int64_t>(top, std::numeric_limits<std::uint32_t>::max())),
        static_cast<std::uint32_t>(std::min<std::int64_t>(right - left, std::numeric_limits<std::uint32_t>::max())),
        static_cast<std::uint32_t>(std::min<std::int64_t>(bottom - top, std::numeric_limits<std::uint32_t>::max())),
    };
}

[[nodiscard]] bool IsValidViewportForDocument(
    const UiDocument& document,
    const render::ResolvedViewport2D& viewport) noexcept
{
    return viewport.logicalWidth == document.Width() && viewport.logicalHeight == document.Height() &&
        viewport.targetWidth > 0U && viewport.targetHeight > 0U &&
        std::isfinite(viewport.contentRect.origin.x) && std::isfinite(viewport.contentRect.origin.y) &&
        std::isfinite(viewport.contentRect.size.x) && std::isfinite(viewport.contentRect.size.y) &&
        std::isfinite(viewport.viewportToPresentationScale.x) &&
        std::isfinite(viewport.viewportToPresentationScale.y) &&
        viewport.contentRect.size.x > 0.0F && viewport.contentRect.size.y > 0.0F &&
        viewport.viewportToPresentationScale.x > 0.0F && viewport.viewportToPresentationScale.y > 0.0F;
}

[[nodiscard]] render::OrthographicCamera BuildUiCamera(
    const UiDocument& document,
    const render::ResolvedViewport2D& viewport) noexcept
{
    const float logicalWidth = static_cast<float>(document.Width());
    const float logicalHeight = static_cast<float>(document.Height());
    const float logicalAspect = logicalWidth / logicalHeight;
    const float targetAspect =
        static_cast<float>(viewport.targetWidth) / static_cast<float>(viewport.targetHeight);
    const render::Float2 contentToTarget{
        viewport.contentRect.size.x / static_cast<float>(viewport.targetWidth),
        viewport.contentRect.size.y / static_cast<float>(viewport.targetHeight),
    };

    render::OrthographicCamera camera{};
    camera.center = render::Float2{logicalWidth * 0.5F, logicalHeight * 0.5F};
    camera.verticalSize = logicalHeight;
    camera.presentationScale = render::Float2{
        (targetAspect / logicalAspect) * contentToTarget.x,
        contentToTarget.y,
    };
    camera.rasterViewport.enabled = true;
    camera.rasterViewport.targetWidth = viewport.targetWidth;
    camera.rasterViewport.targetHeight = viewport.targetHeight;
    camera.rasterViewport.origin = viewport.contentRect.origin;
    camera.rasterViewport.size = viewport.contentRect.size;
    return camera;
}

[[nodiscard]] render::SpriteTextureEncoding ResolveEncoding(
    const assets::TextureResource& texture) noexcept
{
    return texture.colorSpace == assets::TextureResourceColorSpace::Srgb
        ? render::SpriteTextureEncoding::Srgb
        : render::SpriteTextureEncoding::Linear;
}

[[nodiscard]] render::SpriteSampleBounds SampleBounds(
    const std::uint32_t width,
    const std::uint32_t height) noexcept
{
    const float halfU = 0.5F / static_cast<float>(width);
    const float halfV = 0.5F / static_cast<float>(height);
    return render::SpriteSampleBounds{
        render::Float2{halfU, halfV},
        render::Float2{1.0F - halfU, 1.0F - halfV},
    };
}

[[nodiscard]] bool ClipScreenQuad(
    render::SpriteDrawQuad& quad,
    const UiRect clip) noexcept
{
    const float originalLeft = quad.topLeft.position.x;
    const float originalRight = quad.topRight.position.x;
    const float originalTop = quad.topLeft.position.y;
    const float originalBottom = quad.bottomLeft.position.y;
    if (!(originalRight > originalLeft) || !(originalBottom > originalTop))
    {
        return false;
    }

    const float clipLeft = static_cast<float>(clip.x);
    const float clipTop = static_cast<float>(clip.y);
    const float clipRight = clipLeft + static_cast<float>(clip.width);
    const float clipBottom = clipTop + static_cast<float>(clip.height);
    const float left = std::max(originalLeft, clipLeft);
    const float top = std::max(originalTop, clipTop);
    const float right = std::min(originalRight, clipRight);
    const float bottom = std::min(originalBottom, clipBottom);
    if (!(right > left) || !(bottom > top))
    {
        return false;
    }

    const float inverseWidth = 1.0F / (originalRight - originalLeft);
    const float inverseHeight = 1.0F / (originalBottom - originalTop);
    const float leftT = (left - originalLeft) * inverseWidth;
    const float rightT = (right - originalLeft) * inverseWidth;
    const float topT = (top - originalTop) * inverseHeight;
    const float bottomT = (bottom - originalTop) * inverseHeight;

    const float originalLeftU = quad.topLeft.uv.x;
    const float originalRightU = quad.topRight.uv.x;
    const float originalTopV = quad.topLeft.uv.y;
    const float originalBottomV = quad.bottomLeft.uv.y;
    const float leftU = originalLeftU + (originalRightU - originalLeftU) * leftT;
    const float rightU = originalLeftU + (originalRightU - originalLeftU) * rightT;
    const float topV = originalTopV + (originalBottomV - originalTopV) * topT;
    const float bottomV = originalTopV + (originalBottomV - originalTopV) * bottomT;

    quad.topLeft = render::SpriteDrawVertex{{left, top}, {leftU, topV}};
    quad.topRight = render::SpriteDrawVertex{{right, top}, {rightU, topV}};
    quad.bottomRight = render::SpriteDrawVertex{{right, bottom}, {rightU, bottomV}};
    quad.bottomLeft = render::SpriteDrawVertex{{left, bottom}, {leftU, bottomV}};
    return true;
}

void FlipScreenQuadToWorld(render::SpriteDrawQuad& quad, const float logicalHeight) noexcept
{
    quad.topLeft.position.y = logicalHeight - quad.topLeft.position.y;
    quad.topRight.position.y = logicalHeight - quad.topRight.position.y;
    quad.bottomRight.position.y = logicalHeight - quad.bottomRight.position.y;
    quad.bottomLeft.position.y = logicalHeight - quad.bottomLeft.position.y;

    // After the y flip, semantic top vertices still carry top UVs but now have the larger world y.
    // The Sprite renderer consumes explicit triangle vertices, so no vertex relabeling is required.
}

[[nodiscard]] render::SpriteDrawQuad MakeScreenQuad(
    const UiPresentationRect bounds) noexcept
{
    const float left = static_cast<float>(bounds.x);
    const float top = static_cast<float>(bounds.y);
    const float right = left + static_cast<float>(bounds.width);
    const float bottom = top + static_cast<float>(bounds.height);
    return render::SpriteDrawQuad{
        render::SpriteDrawVertex{{left, top}, {0.0F, 0.0F}},
        render::SpriteDrawVertex{{right, top}, {1.0F, 0.0F}},
        render::SpriteDrawVertex{{right, bottom}, {1.0F, 1.0F}},
        render::SpriteDrawVertex{{left, bottom}, {0.0F, 1.0F}},
    };
}

[[nodiscard]] UiPresentationDiagnostic Diagnostic(
    const UiPresentationErrorCode code,
    const std::size_t elementIndex = InvalidUiElementIndex) noexcept
{
    UiPresentationDiagnostic diagnostic{};
    diagnostic.code = code;
    diagnostic.elementIndex = elementIndex;
    return diagnostic;
}

[[nodiscard]] std::uint64_t BuildSignature(
    const UiDocument& document,
    const render::ResolvedViewport2D& viewport,
    const UiSolidTextureBinding2D solidTexture,
    const std::span<const UiTextPresentationInput2D> textInputs,
    const bool includeProgressState) noexcept
{
    std::uint64_t hash = FnvOffset;
    Mix(hash, document.Width());
    Mix(hash, document.Height());
    Mix(hash, viewport.logicalWidth);
    Mix(hash, viewport.logicalHeight);
    Mix(hash, viewport.targetWidth);
    Mix(hash, viewport.targetHeight);
    Mix(hash, static_cast<std::uint64_t>(viewport.scaleMode));
    MixFloat(hash, viewport.contentRect.origin.x);
    MixFloat(hash, viewport.contentRect.origin.y);
    MixFloat(hash, viewport.contentRect.size.x);
    MixFloat(hash, viewport.contentRect.size.y);
    MixFloat(hash, viewport.viewportToPresentationScale.x);
    MixFloat(hash, viewport.viewportToPresentationScale.y);
    MixHandle(hash, solidTexture.texture);
    Mix(hash, static_cast<std::uint64_t>(solidTexture.encoding));

    const UiElement* const focused = document.FocusedElement();
    const std::span<const UiElement> elements = document.Elements();
    Mix(hash, elements.size());
    for (std::size_t index = 0U; index < elements.size(); ++index)
    {
        const UiElement& element = elements[index];
        Mix(hash, index);
        Mix(hash, static_cast<std::uint64_t>(element.kind));
        Mix(hash, element.visible ? 1U : 0U);
        Mix(hash, element.enabled ? 1U : 0U);
        Mix(hash, focused == &element ? 1U : 0U);
        MixRect(hash, element.presentationBounds);
        Mix(hash, element.clipActive ? 1U : 0U);
        if (element.clipActive)
        {
            MixRect(hash, element.clipBounds);
        }
        Mix(hash, element.displayTextRevision);
        Mix(hash, element.scroll.revision);
        if (includeProgressState)
        {
            Mix(hash, element.progress.Revision());
            Mix(hash, element.progress.Value());
            Mix(hash, element.progress.Maximum());
        }
        Mix(hash, element.image.Revision());
        MixHandle(hash, element.image.Texture());
    }

    Mix(hash, textInputs.size());
    for (const UiTextPresentationInput2D& input : textInputs)
    {
        Mix(hash, input.elementIndex);
        Mix(hash, input.presentationRevision);
        Mix(hash, input.fallbackAtlases.size());
        Mix(hash, input.bindings.size());
        for (const text::GlyphAtlasTextureBinding2D& binding : input.bindings)
        {
            MixHandle(hash, binding.texture);
            Mix(hash, binding.pixelRevision);
            Mix(hash, binding.width);
            Mix(hash, binding.height);
        }
    }
    return hash;
}

[[nodiscard]] bool AppendProgressSolid(
    std::vector<render::SpritePresentationRenderData>& output,
    const std::size_t maxPresentations,
    const UiPresentationRect rect,
    const UiRect clip,
    const UiSolidTextureBinding2D solidTexture,
    const UiColor color,
    const float logicalHeight,
    std::uint64_t& stableOrder) noexcept
{
    render::SpriteDrawQuad quad = MakeScreenQuad(rect);
    if (ClipScreenQuad(quad, clip))
    {
        if (output.size() >= maxPresentations)
        {
            return false;
        }
        FlipScreenQuadToWorld(quad, logicalHeight);
        render::SpritePresentationRenderData presentation{};
        presentation.presentation.quad = quad;
        presentation.presentation.appearance.tint = ToLinearRgba(color);
        presentation.presentation.appearance.opacity = 1.0F;
        presentation.presentation.appearance.sampler = render::SpriteSamplerCompatibility::Nearest;
        presentation.presentation.appearance.blend = render::SpriteBlendCompatibility::Normal;
        presentation.presentation.appearance.textureEncoding = solidTexture.encoding;
        presentation.presentation.appearance.sourceAlphaMode = assets::SpriteAlphaMode::Straight;
        presentation.presentation.appearance.sampleBounds = render::SpriteSampleBounds{
            render::Float2{0.5F, 0.5F},
            render::Float2{0.5F, 0.5F},
        };
        presentation.texture = solidTexture.texture;
        presentation.order.stableOrder = stableOrder;
        output.push_back(presentation);
    }
    ++stableOrder;
    return true;
}

[[nodiscard]] bool AppendProgressBorder(
    std::vector<render::SpritePresentationRenderData>& output,
    const std::size_t maxPresentations,
    const UiPresentationRect bounds,
    const UiRect clip,
    const UiSolidTextureBinding2D solidTexture,
    const float logicalHeight,
    std::uint64_t& stableOrder) noexcept
{
    if (bounds.width == 0U || bounds.height == 0U)
    {
        return true;
    }
    const UiPresentationRect top{bounds.x, bounds.y, bounds.width, 1U};
    const UiPresentationRect bottom{
        bounds.x,
        static_cast<std::int32_t>(static_cast<std::int64_t>(bounds.y) + bounds.height - 1U),
        bounds.width,
        1U,
    };
    const UiPresentationRect left{bounds.x, bounds.y, 1U, bounds.height};
    const UiPresentationRect right{
        static_cast<std::int32_t>(static_cast<std::int64_t>(bounds.x) + bounds.width - 1U),
        bounds.y,
        1U,
        bounds.height,
    };
    return AppendProgressSolid(
               output,
               maxPresentations,
               top,
               clip,
               solidTexture,
               BorderColor,
               logicalHeight,
               stableOrder) &&
        AppendProgressSolid(
               output,
               maxPresentations,
               bottom,
               clip,
               solidTexture,
               BorderColor,
               logicalHeight,
               stableOrder) &&
        AppendProgressSolid(
               output,
               maxPresentations,
               left,
               clip,
               solidTexture,
               BorderColor,
               logicalHeight,
               stableOrder) &&
        AppendProgressSolid(
               output,
               maxPresentations,
               right,
               clip,
               solidTexture,
               BorderColor,
               logicalHeight,
               stableOrder);
}

[[nodiscard]] bool BuildProgressSegment(
    const UiElement& element,
    const UiRect clip,
    const UiSolidTextureBinding2D solidTexture,
    const float logicalHeight,
    const std::uint64_t stableOrderBase,
    const std::size_t maxPresentations,
    std::vector<render::SpritePresentationRenderData>& output,
    std::uint64_t& outStableOrderSpan) noexcept
{
    output.clear();
    std::uint64_t stableOrder = stableOrderBase;
    if (!AppendProgressSolid(
            output,
            maxPresentations,
            element.presentationBounds,
            clip,
            solidTexture,
            ProgressTrackColor,
            logicalHeight,
            stableOrder))
    {
        return false;
    }

    const std::uint64_t scaledWidth =
        static_cast<std::uint64_t>(element.presentationBounds.width) * element.progress.Value();
    const std::uint32_t fillWidth = static_cast<std::uint32_t>(
        scaledWidth / static_cast<std::uint64_t>(element.progress.Maximum()));
    if (fillWidth > 0U &&
        !AppendProgressSolid(
            output,
            maxPresentations,
            UiPresentationRect{
                element.presentationBounds.x,
                element.presentationBounds.y,
                fillWidth,
                element.presentationBounds.height,
            },
            clip,
            solidTexture,
            ProgressFillColor,
            logicalHeight,
            stableOrder))
    {
        return false;
    }
    if (!AppendProgressBorder(
            output,
            maxPresentations,
            element.presentationBounds,
            clip,
            solidTexture,
            logicalHeight,
            stableOrder))
    {
        return false;
    }
    outStableOrderSpan = stableOrder - stableOrderBase;
    return true;
}

[[nodiscard]] bool ValidateTextInputs(
    const std::span<const UiElement> elements,
    const std::span<const UiTextPresentationInput2D> textInputs,
    UiPresentationDiagnostic& diagnostic) noexcept
{
    std::size_t previous = InvalidUiElementIndex;
    for (const UiTextPresentationInput2D& input : textInputs)
    {
        if (input.elementIndex >= elements.size() || input.layout == nullptr ||
            input.presentationRevision == 0U || input.fallbackAtlases.empty() ||
            input.fallbackAtlases.size() != input.bindings.size() ||
            (previous != InvalidUiElementIndex && input.elementIndex <= previous))
        {
            diagnostic = Diagnostic(UiPresentationErrorCode::InvalidTextInput, input.elementIndex);
            return false;
        }
        const UiElementKind kind = elements[input.elementIndex].kind;
        if (kind != UiElementKind::Label && kind != UiElementKind::Button &&
            kind != UiElementKind::TextInput)
        {
            diagnostic = Diagnostic(UiPresentationErrorCode::InvalidTextInput, input.elementIndex);
            return false;
        }
        previous = input.elementIndex;
    }
    return true;
}
} // namespace

struct UiPresentationCache2D::Impl final
{
    struct ElementRetainedState final
    {
        std::size_t presentationOffset{0U};
        std::size_t presentationCount{0U};
        std::uint64_t stableOrderBase{0U};
        std::uint64_t stableOrderSpan{0U};
        std::uint64_t progressRevision{0U};
        bool progressActive{false};
    };

    UiPresentationCacheConfig config{};
    std::vector<render::SpritePresentationRenderData> presentations{};
    std::vector<render::SpritePresentationRenderData> scratch{};
    std::vector<ElementRetainedState> elementStates{};
    render::OrthographicCamera camera{};
    UiPresentationMetrics metrics{};
    const UiDocument* boundDocument{nullptr};
    const assets::ResourceRegistry* boundResources{nullptr};
    std::uint64_t signature{0U};
    std::uint64_t progressPatchSignature{0U};
    bool hasSignature{false};
};

UiPresentationCache2D::UiPresentationCache2D(std::unique_ptr<Impl> impl) noexcept
    : impl_{std::move(impl)}
{
}

UiPresentationCache2D::UiPresentationCache2D(UiPresentationCache2D&&) noexcept = default;
UiPresentationCache2D& UiPresentationCache2D::operator=(UiPresentationCache2D&&) noexcept = default;
UiPresentationCache2D::~UiPresentationCache2D() = default;

bool ResolveUiSolidTextureBinding2D(
    const assets::ResourceRegistry& resources,
    const render::TextureHandle texture,
    UiSolidTextureBinding2D& outBinding) noexcept
{
    outBinding = {};
    const assets::TextureResource* const resource = resources.Resolve(texture);
    if (resource == nullptr || resource->width != 1U || resource->height != 1U ||
        resource->colorSpace != assets::TextureResourceColorSpace::Linear ||
        resource->alphaMode != assets::TextureResourceAlphaMode::Straight ||
        resource->canonicalRgba8.size() != 4U ||
        resource->canonicalRgba8[0] != 255U || resource->canonicalRgba8[1] != 255U ||
        resource->canonicalRgba8[2] != 255U || resource->canonicalRgba8[3] != 255U)
    {
        return false;
    }
    outBinding.texture = texture;
    outBinding.encoding = render::SpriteTextureEncoding::Linear;
    return true;
}

UiPresentationUpdateResult UiPresentationCache2D::Update(
    const UiDocument& document,
    const assets::ResourceRegistry& resources,
    const render::ResolvedViewport2D& viewport,
    const UiSolidTextureBinding2D solidTexture,
    const std::span<const UiTextPresentationInput2D> textInputs)
{
    UiPresentationUpdateResult result{};
    if (impl_ == nullptr)
    {
        result.diagnostic = Diagnostic(UiPresentationErrorCode::InvalidConfig);
        return result;
    }
    ++impl_->metrics.updateCalls;
    if (!document.HasValidSize())
    {
        result.diagnostic = Diagnostic(UiPresentationErrorCode::InvalidDocument);
        return result;
    }
    if (!IsValidViewportForDocument(document, viewport))
    {
        result.diagnostic = Diagnostic(UiPresentationErrorCode::InvalidViewport);
        return result;
    }
    if (!IsLiveTextureHandle(solidTexture.texture) ||
        solidTexture.encoding != render::SpriteTextureEncoding::Linear)
    {
        result.diagnostic = Diagnostic(UiPresentationErrorCode::InvalidSolidTexture);
        return result;
    }

    const std::span<const UiElement> elements = document.Elements();
    if (elements.size() > impl_->config.maxElements)
    {
        result.diagnostic = Diagnostic(UiPresentationErrorCode::ElementCapacityExceeded, impl_->config.maxElements);
        return result;
    }
    UiPresentationDiagnostic textInputDiagnostic{};
    if (!ValidateTextInputs(elements, textInputs, textInputDiagnostic))
    {
        result.diagnostic = textInputDiagnostic;
        return result;
    }

    const std::uint64_t signature = BuildSignature(document, viewport, solidTexture, textInputs, true);
    const std::uint64_t progressPatchSignature =
        BuildSignature(document, viewport, solidTexture, textInputs, false);
    if (impl_->hasSignature && impl_->boundDocument == &document && impl_->boundResources == &resources &&
        impl_->signature == signature)
    {
        ++impl_->metrics.cacheHits;
        result.reused = true;
        return result;
    }

    // Fast bounded mutation path: when every presentation-relevant input except Progress state is
    // unchanged, patch only changed retained Progress segments. If a mutation changes segment count
    // or stable-order footprint (for example 0 -> non-zero fill), fall back to the normal full build.
    if (impl_->hasSignature && impl_->boundDocument == &document && impl_->boundResources == &resources &&
        impl_->progressPatchSignature == progressPatchSignature &&
        impl_->elementStates.size() == elements.size())
    {
        const float logicalHeight = static_cast<float>(document.Height());
        const UiRect canvas{0U, 0U, document.Width(), document.Height()};
        bool sawProgressChange = false;
        bool canPatch = true;
        std::uint64_t rebuiltElements = 0U;
        std::uint64_t rebuiltPresentations = 0U;
        for (std::size_t index = 0U; index < elements.size(); ++index)
        {
            const UiElement& element = elements[index];
            Impl::ElementRetainedState& retained = impl_->elementStates[index];
            if (element.progress.Revision() == retained.progressRevision)
            {
                continue;
            }
            sawProgressChange = true;

            if (!retained.progressActive || !element.progress.Active() || element.image.Active())
            {
                canPatch = false;
                break;
            }

            UiRect clip = canvas;
            if (element.clipActive)
            {
                clip = IntersectRect(clip, element.clipBounds);
            }
            const UiRect visibleBounds = IntersectRect(PresentationExtent(element.presentationBounds), clip);
            if (!element.visible || visibleBounds.width == 0U || visibleBounds.height == 0U)
            {
                // Structural/clip state is part of progressPatchSignature, so this element remains
                // invisible and its presentation segment remains correctly empty.
                retained.progressRevision = element.progress.Revision();
                continue;
            }

            std::uint64_t stableOrderSpan = 0U;
            if (!BuildProgressSegment(
                    element,
                    clip,
                    solidTexture,
                    logicalHeight,
                    retained.stableOrderBase,
                    impl_->config.maxPresentations,
                    impl_->scratch,
                    stableOrderSpan) ||
                impl_->scratch.size() != retained.presentationCount ||
                stableOrderSpan != retained.stableOrderSpan)
            {
                canPatch = false;
                break;
            }

            std::copy(
                impl_->scratch.begin(),
                impl_->scratch.end(),
                impl_->presentations.begin() + static_cast<std::ptrdiff_t>(retained.presentationOffset));
            retained.progressRevision = element.progress.Revision();
            ++rebuiltElements;
            rebuiltPresentations += impl_->scratch.size();
        }

        if (canPatch && sawProgressChange)
        {
            impl_->signature = signature;
            ++impl_->metrics.rebuilds;
            ++impl_->metrics.partialRebuilds;
            impl_->metrics.elementsRebuilt += rebuiltElements;
            impl_->metrics.presentationsRebuilt += rebuiltPresentations;
            impl_->metrics.solidPresentationsBuilt += rebuiltPresentations;
            impl_->metrics.lastPresentationCount = impl_->presentations.size();
            return result;
        }
    }

    // A full rebuild mutates retained storage in place. Invalidate the previous cache identity
    // before the first destructive write so any later diagnostic cannot leave an empty/partial frame
    // reachable through the old signature on a subsequent update.
    impl_->boundDocument = nullptr;
    impl_->boundResources = nullptr;
    impl_->signature = 0U;
    impl_->progressPatchSignature = 0U;
    impl_->hasSignature = false;

    impl_->presentations.clear();
    impl_->elementStates.clear();
    impl_->elementStates.resize(elements.size());
    impl_->metrics.lastPresentationCount = 0U;
    impl_->camera = BuildUiCamera(document, viewport);
    const float logicalHeight = static_cast<float>(document.Height());
    const UiRect canvas{0U, 0U, document.Width(), document.Height()};
    const UiElement* const focused = document.FocusedElement();
    std::uint64_t stableOrder = 0U;
    std::size_t textInputCursor = 0U;

    const auto failCapacity = [&](const std::size_t elementIndex, const std::size_t required)
    {
        UiPresentationDiagnostic diagnostic =
            Diagnostic(UiPresentationErrorCode::PresentationCapacityExceeded, elementIndex);
        diagnostic.requiredPresentations = required;
        result.diagnostic = diagnostic;
        impl_->presentations.clear();
    };

    const auto appendQuad = [&](const std::size_t elementIndex,
                                render::SpriteDrawQuad screenQuad,
                                const UiRect clip,
                                const render::TextureHandle texture,
                                const render::SpriteTextureEncoding encoding,
                                const render::SpriteSampleBounds sampleBounds,
                                const render::SpriteLinearRgba tint,
                                const render::SpriteSamplerCompatibility sampler) -> bool
    {
        if (!ClipScreenQuad(screenQuad, clip))
        {
            return true;
        }
        if (impl_->presentations.size() >= impl_->config.maxPresentations)
        {
            failCapacity(elementIndex, impl_->presentations.size() + 1U);
            return false;
        }
        FlipScreenQuadToWorld(screenQuad, logicalHeight);
        render::SpritePresentationRenderData presentation{};
        presentation.presentation.quad = screenQuad;
        presentation.presentation.appearance.tint = tint;
        presentation.presentation.appearance.opacity = 1.0F;
        presentation.presentation.appearance.sampler = sampler;
        presentation.presentation.appearance.blend = render::SpriteBlendCompatibility::Normal;
        presentation.presentation.appearance.textureEncoding = encoding;
        presentation.presentation.appearance.sourceAlphaMode = assets::SpriteAlphaMode::Straight;
        presentation.presentation.appearance.sampleBounds = sampleBounds;
        presentation.texture = texture;
        presentation.order.stableOrder = stableOrder;
        impl_->presentations.push_back(presentation);
        return true;
    };

    const render::SpriteSampleBounds solidSampleBounds{
        render::Float2{0.5F, 0.5F},
        render::Float2{0.5F, 0.5F},
    };
    const auto appendSolidRect = [&](const std::size_t elementIndex,
                                     const UiPresentationRect rect,
                                     const UiRect clip,
                                     const UiColor color) -> bool
    {
        const bool appended = appendQuad(
            elementIndex,
            MakeScreenQuad(rect),
            clip,
            solidTexture.texture,
            solidTexture.encoding,
            solidSampleBounds,
            ToLinearRgba(color),
            render::SpriteSamplerCompatibility::Nearest);
        ++stableOrder;
        return appended;
    };

    const auto appendBorder = [&](const std::size_t elementIndex,
                                  const UiPresentationRect bounds,
                                  const UiRect clip,
                                  const UiColor color) -> bool
    {
        if (bounds.width == 0U || bounds.height == 0U)
        {
            return true;
        }
        const UiPresentationRect top{bounds.x, bounds.y, bounds.width, 1U};
        const UiPresentationRect bottom{
            bounds.x,
            static_cast<std::int32_t>(static_cast<std::int64_t>(bounds.y) + bounds.height - 1U),
            bounds.width,
            1U,
        };
        const UiPresentationRect left{bounds.x, bounds.y, 1U, bounds.height};
        const UiPresentationRect right{
            static_cast<std::int32_t>(static_cast<std::int64_t>(bounds.x) + bounds.width - 1U),
            bounds.y,
            1U,
            bounds.height,
        };
        return appendSolidRect(elementIndex, top, clip, color) &&
            appendSolidRect(elementIndex, bottom, clip, color) &&
            appendSolidRect(elementIndex, left, clip, color) &&
            appendSolidRect(elementIndex, right, clip, color);
    };

    for (std::size_t index = 0U; index < elements.size(); ++index)
    {
        const UiElement& element = elements[index];
        const std::size_t elementPresentationStart = impl_->presentations.size();
        const std::uint64_t elementStableOrderStart = stableOrder;
        const auto finalizeElement = [&]() noexcept
        {
            Impl::ElementRetainedState& retained = impl_->elementStates[index];
            retained.presentationOffset = elementPresentationStart;
            retained.presentationCount = impl_->presentations.size() - elementPresentationStart;
            retained.stableOrderBase = elementStableOrderStart;
            retained.stableOrderSpan = stableOrder - elementStableOrderStart;
            retained.progressRevision = element.progress.Revision();
            retained.progressActive = element.progress.Active();
        };

        while (textInputCursor < textInputs.size() && textInputs[textInputCursor].elementIndex < index)
        {
            ++textInputCursor;
        }
        const UiTextPresentationInput2D* const textInput =
            textInputCursor < textInputs.size() && textInputs[textInputCursor].elementIndex == index
                ? &textInputs[textInputCursor]
                : nullptr;

        if (!element.visible)
        {
            finalizeElement();
            continue;
        }

        UiRect clip = canvas;
        if (element.clipActive)
        {
            clip = IntersectRect(clip, element.clipBounds);
        }
        const UiRect visibleBounds = IntersectRect(PresentationExtent(element.presentationBounds), clip);
        if (visibleBounds.width == 0U || visibleBounds.height == 0U)
        {
            finalizeElement();
            continue;
        }

        if (element.image.Active())
        {
            const assets::TextureResource* const texture = resources.Resolve(element.image.Texture());
            if (texture == nullptr || texture->width == 0U || texture->height == 0U ||
                texture->alphaMode != assets::TextureResourceAlphaMode::Straight)
            {
                result.diagnostic = Diagnostic(UiPresentationErrorCode::InvalidImageTexture, index);
                impl_->presentations.clear();
                return result;
            }
            if (!appendQuad(
                    index,
                    MakeScreenQuad(element.presentationBounds),
                    clip,
                    element.image.Texture(),
                    ResolveEncoding(*texture),
                    SampleBounds(texture->width, texture->height),
                    render::SpriteLinearRgba{},
                    render::SpriteSamplerCompatibility::Linear))
            {
                return result;
            }
            ++stableOrder;
            ++impl_->metrics.imagePresentationsBuilt;
            finalizeElement();
            continue;
        }

        if (element.progress.Active())
        {
            if (!appendSolidRect(index, element.presentationBounds, clip, ProgressTrackColor))
            {
                return result;
            }
            ++impl_->metrics.solidPresentationsBuilt;
            const std::uint64_t scaledWidth =
                static_cast<std::uint64_t>(element.presentationBounds.width) * element.progress.Value();
            const std::uint32_t fillWidth = static_cast<std::uint32_t>(
                scaledWidth / static_cast<std::uint64_t>(element.progress.Maximum()));
            if (fillWidth > 0U)
            {
                if (!appendSolidRect(
                        index,
                        UiPresentationRect{
                            element.presentationBounds.x,
                            element.presentationBounds.y,
                            fillWidth,
                            element.presentationBounds.height,
                        },
                        clip,
                        ProgressFillColor))
                {
                    return result;
                }
                ++impl_->metrics.solidPresentationsBuilt;
            }
            const std::size_t beforeBorder = impl_->presentations.size();
            if (!appendBorder(index, element.presentationBounds, clip, BorderColor))
            {
                return result;
            }
            impl_->metrics.solidPresentationsBuilt += impl_->presentations.size() - beforeBorder;
            finalizeElement();
            continue;
        }

        switch (element.kind)
        {
        case UiElementKind::Panel:
            if (!appendSolidRect(index, element.presentationBounds, clip, PanelColor))
            {
                return result;
            }
            ++impl_->metrics.solidPresentationsBuilt;
            break;
        case UiElementKind::Label:
            break;
        case UiElementKind::Button:
        {
            if (!appendSolidRect(
                    index,
                    element.presentationBounds,
                    clip,
                    element.enabled ? ButtonColor : DisabledButtonColor))
            {
                return result;
            }
            ++impl_->metrics.solidPresentationsBuilt;
            const std::size_t beforeBorder = impl_->presentations.size();
            if (!appendBorder(
                    index,
                    element.presentationBounds,
                    clip,
                    focused == &element ? FocusBorderColor : BorderColor))
            {
                return result;
            }
            impl_->metrics.solidPresentationsBuilt += impl_->presentations.size() - beforeBorder;
            break;
        }
        case UiElementKind::TextInput:
        {
            if (!appendSolidRect(index, element.presentationBounds, clip, InputColor))
            {
                return result;
            }
            ++impl_->metrics.solidPresentationsBuilt;
            const std::size_t beforeBorder = impl_->presentations.size();
            if (!appendBorder(
                    index,
                    element.presentationBounds,
                    clip,
                    focused == &element ? FocusBorderColor : BorderColor))
            {
                return result;
            }
            impl_->metrics.solidPresentationsBuilt += impl_->presentations.size() - beforeBorder;
            break;
        }
        }

        const bool requiresText = element.kind == UiElementKind::Label ||
            element.kind == UiElementKind::Button || element.kind == UiElementKind::TextInput;
        const bool hasDisplayGlyphs = !element.text.empty() ||
            (element.textLayout.valid && element.textLayout.glyphCount > 0U);
        if (!requiresText || !hasDisplayGlyphs)
        {
            finalizeElement();
            continue;
        }
        if (textInput == nullptr)
        {
            result.diagnostic = Diagnostic(UiPresentationErrorCode::MissingTextInput, index);
            impl_->presentations.clear();
            return result;
        }

        text::TextPresentationConfig2D textConfig{};
        textConfig.origin = render::Float2{
            static_cast<float>(element.presentationBounds.x) +
                (element.kind == UiElementKind::TextInput && element.presentationBounds.width > 8U ? 4.0F : 0.0F),
            static_cast<float>(element.presentationBounds.y),
        };
        textConfig.pixelsPerUnit = 1.0F;
        textConfig.stableOrderBase = stableOrder;
        textConfig.tint = ToLinearRgba(element.enabled ? TextColor : DisabledTextColor);
        textConfig.opacity = 1.0F;
        textConfig.sampler = render::SpriteSamplerCompatibility::Linear;

        std::size_t requiredCount = 0U;
        text::TextPresentationMeasurement2D measurement{};
        const text::TextPresentationStatus countStatus = text::BuildTextPresentation2D(
            *textInput->layout,
            textInput->fallbackAtlases,
            textInput->bindings,
            textConfig,
            {},
            requiredCount,
            measurement);
        if (!countStatus.Succeeded() &&
            countStatus.error != text::TextPresentationError::InsufficientOutputCapacity)
        {
            UiPresentationDiagnostic diagnostic =
                Diagnostic(UiPresentationErrorCode::TextPresentationFailed, index);
            diagnostic.textStatus = countStatus;
            result.diagnostic = diagnostic;
            impl_->presentations.clear();
            return result;
        }
        if (requiredCount > impl_->config.maxPresentations - impl_->presentations.size())
        {
            failCapacity(index, impl_->presentations.size() + requiredCount);
            return result;
        }
        if (textInput->layout->Glyphs().size() >
            static_cast<std::size_t>(std::numeric_limits<std::uint64_t>::max() - stableOrder))
        {
            failCapacity(index, impl_->config.maxPresentations + 1U);
            return result;
        }

        const std::size_t start = impl_->presentations.size();
        impl_->presentations.resize(start + requiredCount);
        if (requiredCount > 0U)
        {
            const text::TextPresentationStatus buildStatus = text::BuildTextPresentation2D(
                *textInput->layout,
                textInput->fallbackAtlases,
                textInput->bindings,
                textConfig,
                std::span<render::SpritePresentationRenderData>{
                    impl_->presentations.data() + start,
                    requiredCount,
                },
                requiredCount,
                measurement);
            if (!buildStatus.Succeeded())
            {
                UiPresentationDiagnostic diagnostic =
                    Diagnostic(UiPresentationErrorCode::TextPresentationFailed, index);
                diagnostic.textStatus = buildStatus;
                result.diagnostic = diagnostic;
                impl_->presentations.clear();
                return result;
            }
        }

        std::size_t write = start;
        for (std::size_t read = start; read < start + requiredCount; ++read)
        {
            render::SpritePresentationRenderData presentation = impl_->presentations[read];
            if (!ClipScreenQuad(presentation.presentation.quad, clip))
            {
                continue;
            }
            FlipScreenQuadToWorld(presentation.presentation.quad, logicalHeight);
            impl_->presentations[write++] = presentation;
        }
        impl_->presentations.resize(write);
        impl_->metrics.textPresentationsBuilt += write - start;
        stableOrder += static_cast<std::uint64_t>(textInput->layout->Glyphs().size());
        finalizeElement();
    }

    impl_->boundDocument = &document;
    impl_->boundResources = &resources;
    impl_->signature = signature;
    impl_->progressPatchSignature = progressPatchSignature;
    impl_->hasSignature = true;
    ++impl_->metrics.rebuilds;
    ++impl_->metrics.fullRebuilds;
    impl_->metrics.elementsRebuilt += elements.size();
    impl_->metrics.presentationsRebuilt += impl_->presentations.size();
    impl_->metrics.lastPresentationCount = impl_->presentations.size();
    return result;
}

UiPresentationFrame2D UiPresentationCache2D::Frame() const noexcept
{
    if (impl_ == nullptr)
    {
        return {};
    }
    return UiPresentationFrame2D{impl_->camera, impl_->presentations};
}

UiPresentationCacheConfig UiPresentationCache2D::Config() const noexcept
{
    return impl_ != nullptr ? impl_->config : UiPresentationCacheConfig{};
}

const UiPresentationMetrics& UiPresentationCache2D::Metrics() const noexcept
{
    static const UiPresentationMetrics Empty{};
    return impl_ != nullptr ? impl_->metrics : Empty;
}

void UiPresentationCache2D::Reset() noexcept
{
    if (impl_ == nullptr)
    {
        return;
    }
    impl_->presentations.clear();
    impl_->scratch.clear();
    impl_->elementStates.clear();
    impl_->camera = {};
    impl_->boundDocument = nullptr;
    impl_->boundResources = nullptr;
    impl_->signature = 0U;
    impl_->progressPatchSignature = 0U;
    impl_->hasSignature = false;
    impl_->metrics.lastPresentationCount = 0U;
}

UiPresentationCachePrepareResult PrepareUiPresentationCache(const UiPresentationCacheConfig config)
{
    UiPresentationCachePrepareResult output{};
    if (config.maxPresentations == 0U || config.maxPresentations > MaxPreparedPresentations ||
        config.maxElements == 0U || config.maxElements > MaxPreparedElements)
    {
        output.diagnostic = Diagnostic(UiPresentationErrorCode::InvalidConfig);
        return output;
    }
    try
    {
        auto impl = std::make_unique<UiPresentationCache2D::Impl>();
        impl->config = config;
        impl->presentations.reserve(config.maxPresentations);
        impl->scratch.reserve(config.maxPresentations);
        impl->elementStates.reserve(config.maxElements);
        output.cache = std::unique_ptr<UiPresentationCache2D>(new UiPresentationCache2D(std::move(impl)));
    }
    catch (const std::bad_alloc&)
    {
        output.diagnostic = Diagnostic(UiPresentationErrorCode::AllocationFailed);
    }
    return output;
}

std::string_view ToString(const UiPresentationErrorCode value) noexcept
{
    switch (value)
    {
    case UiPresentationErrorCode::InvalidConfig:
        return "invalid_config";
    case UiPresentationErrorCode::InvalidDocument:
        return "invalid_document";
    case UiPresentationErrorCode::InvalidViewport:
        return "invalid_viewport";
    case UiPresentationErrorCode::InvalidSolidTexture:
        return "invalid_solid_texture";
    case UiPresentationErrorCode::InvalidTextInput:
        return "invalid_text_input";
    case UiPresentationErrorCode::MissingTextInput:
        return "missing_text_input";
    case UiPresentationErrorCode::InvalidImageTexture:
        return "invalid_image_texture";
    case UiPresentationErrorCode::TextPresentationFailed:
        return "text_presentation_failed";
    case UiPresentationErrorCode::ElementCapacityExceeded:
        return "element_capacity_exceeded";
    case UiPresentationErrorCode::PresentationCapacityExceeded:
        return "presentation_capacity_exceeded";
    case UiPresentationErrorCode::AllocationFailed:
        return "allocation_failed";
    }
    return "unknown_ui_presentation_error";
}
} // namespace trace2d::ui
