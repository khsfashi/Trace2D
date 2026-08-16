#include <trace2d/ui/Ui.hpp>

#include <limits>

namespace trace2d::ui
{
std::string_view ToString(const UiImageResult result) noexcept
{
    switch (result)
    {
    case UiImageResult::Success:
        return "success";
    case UiImageResult::NotFound:
        return "not_found";
    case UiImageResult::InvalidTarget:
        return "invalid_target";
    case UiImageResult::NotImage:
        return "not_image";
    case UiImageResult::InvalidTexture:
        return "invalid_texture";
    }

    return "unknown";
}

UiImageResult UiDocument::ConfigureImage(
    const std::string_view id,
    const assets::ResourceHandle<assets::TextureResource> texture,
    const assets::ResourceRegistry& resources) noexcept
{
    UiElement* const element = FindMutable(id);
    if (element == nullptr)
    {
        return UiImageResult::NotFound;
    }
    if (element->kind != UiElementKind::Panel || element->scroll.viewport ||
        element->progress.Active() || element->image.active_)
    {
        return UiImageResult::InvalidTarget;
    }
    if (resources.Resolve(texture) == nullptr)
    {
        return UiImageResult::InvalidTexture;
    }

    element->image.active_ = true;
    element->image.texture_ = texture;
    element->image.revision_ = 1U;
    return UiImageResult::Success;
}

UiImageResult UiDocument::SetImage(
    const std::string_view id,
    const assets::ResourceHandle<assets::TextureResource> texture,
    const assets::ResourceRegistry& resources) noexcept
{
    UiElement* const element = FindMutable(id);
    if (element == nullptr)
    {
        return UiImageResult::NotFound;
    }
    if (!element->image.active_)
    {
        return UiImageResult::NotImage;
    }
    if (resources.Resolve(texture) == nullptr)
    {
        return UiImageResult::InvalidTexture;
    }
    if (element->image.texture_ == texture)
    {
        return UiImageResult::Success;
    }

    element->image.texture_ = texture;
    element->image.revision_ = element->image.revision_ == std::numeric_limits<std::uint64_t>::max()
        ? 1U
        : element->image.revision_ + 1U;
    return UiImageResult::Success;
}
} // namespace trace2d::ui
