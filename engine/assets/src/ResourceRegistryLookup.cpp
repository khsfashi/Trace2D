#include <trace2d/assets/ResourceRegistry.hpp>

namespace trace2d::assets
{
std::optional<ResourceHandle<TextureResource>> ResourceRegistry::FindReadyTexture(
    const std::string_view projectRelativeReference)
{
    ResourceDiagnostic diagnostic{};
    const std::optional<ResourceIdentity> identity =
        Canonicalize(ResourceTypeDomain::Texture, projectRelativeReference, diagnostic);
    if (!identity.has_value())
    {
        return std::nullopt;
    }

    const auto existing = identityToSlot_.find(IdentityKey(*identity));
    if (existing == identityToSlot_.end() || existing->second >= slots_.size())
    {
        return std::nullopt;
    }

    const Slot& slot = slots_[existing->second];
    if (slot.state != ResourceLoadState::Ready || slot.identity != *identity ||
        slot.identity.domain != ResourceTypeDomain::Texture ||
        std::get_if<TextureResource>(&slot.payload) == nullptr)
    {
        return std::nullopt;
    }

    return ResourceHandle<TextureResource>{
        existing->second,
        slot.generation,
        ResourceTypeDomain::Texture};
}
} // namespace trace2d::assets
