#include <trace2d/assets/ResourceRegistry.hpp>

#include <optional>
#include <span>
#include <utility>

namespace trace2d::assets
{
std::string_view ToString(const AudioClipLoadPolicy value) noexcept
{
    switch (value)
    {
    case AudioClipLoadPolicy::Preload:
        return "preload";
    case AudioClipLoadPolicy::Stream:
        return "stream";
    }
    return "unknown";
}

ResourcePublishResult<AudioClipResource> ResourceRegistry::PublishAudioClip(
    const std::string_view projectRelativeReference,
    AudioClipResource resource)
{
    ResourceDiagnostic diagnostic{};
    const auto identity = Canonicalize(ResourceTypeDomain::AudioClip, projectRelativeReference, diagnostic);
    if (!identity.has_value())
    {
        return ResourcePublishResult<AudioClipResource>{{}, false, std::move(diagnostic)};
    }

    switch (resource.loadPolicy)
    {
    case AudioClipLoadPolicy::Preload:
    case AudioClipLoadPolicy::Stream:
        break;
    default:
        diagnostic.code = ResourceErrorCode::InvalidPayload;
        diagnostic.identity = *identity;
        diagnostic.message = "audio clip load policy must be preload or stream";
        return ResourcePublishResult<AudioClipResource>{{}, false, std::move(diagnostic)};
    }

    if (resource.sampleRateHz == 0U || resource.sampleRateHz > MaximumAudioClipSampleRateHz)
    {
        diagnostic.code = ResourceErrorCode::InvalidPayload;
        diagnostic.identity = *identity;
        diagnostic.message = "audio clip sample rate must be in 1..384000 Hz";
        return ResourcePublishResult<AudioClipResource>{{}, false, std::move(diagnostic)};
    }
    if (resource.channelCount == 0U || resource.channelCount > MaximumAudioClipChannelCount)
    {
        diagnostic.code = ResourceErrorCode::InvalidPayload;
        diagnostic.identity = *identity;
        diagnostic.message = "audio clip channel count must be in 1..8";
        return ResourcePublishResult<AudioClipResource>{{}, false, std::move(diagnostic)};
    }
    if (resource.frameCount == 0U)
    {
        diagnostic.code = ResourceErrorCode::InvalidPayload;
        diagnostic.identity = *identity;
        diagnostic.message = "audio clip frame count must be non-zero";
        return ResourcePublishResult<AudioClipResource>{{}, false, std::move(diagnostic)};
    }
    if (resource.encodedByteSize == 0U)
    {
        diagnostic.code = ResourceErrorCode::InvalidPayload;
        diagnostic.identity = *identity;
        diagnostic.message = "audio clip encoded byte size must be non-zero";
        return ResourcePublishResult<AudioClipResource>{{}, false, std::move(diagnostic)};
    }
    if (resource.cpuRetention != CpuRetentionPolicy::Required)
    {
        diagnostic.code = ResourceErrorCode::InvalidPayload;
        diagnostic.identity = *identity;
        diagnostic.message = "audio clip canonical metadata retention must remain required";
        return ResourcePublishResult<AudioClipResource>{{}, false, std::move(diagnostic)};
    }

    return Publish<AudioClipResource>(
        *identity,
        std::move(resource),
        std::span<const ResourceHandleUntyped>{});
}

std::optional<ResourceHandle<AudioClipResource>> ResourceRegistry::FindReadyAudioClip(
    const std::string_view projectRelativeReference)
{
    ResourceDiagnostic diagnostic{};
    const std::optional<ResourceIdentity> identity =
        Canonicalize(ResourceTypeDomain::AudioClip, projectRelativeReference, diagnostic);
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
        slot.identity.domain != ResourceTypeDomain::AudioClip ||
        std::get_if<AudioClipResource>(&slot.payload) == nullptr)
    {
        return std::nullopt;
    }

    return ResourceHandle<AudioClipResource>{
        existing->second,
        slot.generation,
        ResourceTypeDomain::AudioClip};
}
} // namespace trace2d::assets
