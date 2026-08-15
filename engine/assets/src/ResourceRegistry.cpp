#include <trace2d/assets/ResourceRegistry.hpp>

#include <algorithm>
#include <cctype>
#include <limits>
#include <utility>

namespace trace2d::assets
{
namespace
{
[[nodiscard]] bool IsControl(char value) noexcept
{
    return static_cast<unsigned char>(value) < 0x20U;
}

[[nodiscard]] bool LooksLikeDriveAbsolute(std::string_view value) noexcept
{
    return value.size() >= 2U && std::isalpha(static_cast<unsigned char>(value[0])) != 0 && value[1] == ':';
}

[[nodiscard]] std::size_t StringLogicalBytes(const std::string& value) noexcept
{
    return value.size();
}

[[nodiscard]] std::size_t StringCapacityBytes(const std::string& value) noexcept
{
    return value.capacity();
}
} // namespace

std::string_view ToString(ResourceTypeDomain value) noexcept
{
    switch (value)
    {
    case ResourceTypeDomain::Texture:
        return "texture";
    case ResourceTypeDomain::Sprite:
        return "sprite";
    case ResourceTypeDomain::SceneTemplate:
        return "scene_template";
    case ResourceTypeDomain::Font:
        return "font";
    }
    return "unknown";
}

std::string_view ToString(ResourceLoadState value) noexcept
{
    switch (value)
    {
    case ResourceLoadState::Unloaded:
        return "unloaded";
    case ResourceLoadState::Ready:
        return "ready";
    case ResourceLoadState::Error:
        return "error";
    }
    return "unknown";
}

std::string_view ToString(CpuRetentionPolicy value) noexcept
{
    switch (value)
    {
    case CpuRetentionPolicy::Required:
        return "required";
    case CpuRetentionPolicy::Releasable:
        return "releasable";
    case CpuRetentionPolicy::Reacquirable:
        return "reacquirable";
    }
    return "unknown";
}

std::string_view ToString(ResourceErrorCode value) noexcept
{
    switch (value)
    {
    case ResourceErrorCode::InvalidReference:
        return "invalid_reference";
    case ResourceErrorCode::InvalidPayload:
        return "invalid_payload";
    case ResourceErrorCode::TypeMismatch:
        return "type_mismatch";
    case ResourceErrorCode::InvalidHandle:
        return "invalid_handle";
    case ResourceErrorCode::StaleHandle:
        return "stale_handle";
    case ResourceErrorCode::DependencyNotReady:
        return "dependency_not_ready";
    case ResourceErrorCode::DependencyCycle:
        return "dependency_cycle";
    case ResourceErrorCode::HasDependents:
        return "has_dependents";
    case ResourceErrorCode::RetainedByCaller:
        return "retained_by_caller";
    case ResourceErrorCode::CpuRetentionRequired:
        return "cpu_retention_required";
    case ResourceErrorCode::RetryRequiresInvalidation:
        return "retry_requires_invalidation";
    }
    return "unknown";
}

ResourceRegistry::ResourceRegistry(std::filesystem::path projectRoot)
    : projectRoot_(std::move(projectRoot))
{
}

ResourcePublishResult<TextureResource> ResourceRegistry::PublishTexture(
    std::string_view projectRelativeReference,
    TextureResource resource)
{
    ResourceDiagnostic diagnostic{};
    const auto identity = Canonicalize(ResourceTypeDomain::Texture, projectRelativeReference, diagnostic);
    if (!identity.has_value())
    {
        return ResourcePublishResult<TextureResource>{{}, false, std::move(diagnostic)};
    }

    if (resource.width == 0U || resource.height == 0U)
    {
        diagnostic.code = ResourceErrorCode::InvalidPayload;
        diagnostic.identity = *identity;
        diagnostic.message = "texture dimensions must be non-zero";
        return ResourcePublishResult<TextureResource>{{}, false, std::move(diagnostic)};
    }

    constexpr std::size_t kChannels = 4U;
    const std::size_t width = static_cast<std::size_t>(resource.width);
    const std::size_t height = static_cast<std::size_t>(resource.height);
    if (height > std::numeric_limits<std::size_t>::max() / width ||
        width * height > std::numeric_limits<std::size_t>::max() / kChannels)
    {
        diagnostic.code = ResourceErrorCode::InvalidPayload;
        diagnostic.identity = *identity;
        diagnostic.message = "texture byte size overflows size_t";
        return ResourcePublishResult<TextureResource>{{}, false, std::move(diagnostic)};
    }

    const std::size_t expectedBytes = width * height * kChannels;
    if (resource.canonicalRgba8.size() != expectedBytes)
    {
        diagnostic.code = ResourceErrorCode::InvalidPayload;
        diagnostic.identity = *identity;
        diagnostic.message = "canonical texture payload must contain exactly width * height * 4 RGBA8 bytes";
        return ResourcePublishResult<TextureResource>{{}, false, std::move(diagnostic)};
    }

    return Publish<TextureResource>(
        *identity,
        std::move(resource),
        std::span<const ResourceHandleUntyped>{});
}

ResourcePublishResult<SpriteResource> ResourceRegistry::PublishSprite(
    std::string_view projectRelativeReference,
    SpriteResource resource,
    std::span<const ResourceHandleUntyped> strongDependencies)
{
    ResourceDiagnostic diagnostic{};
    const auto identity = Canonicalize(ResourceTypeDomain::Sprite, projectRelativeReference, diagnostic);
    if (!identity.has_value())
    {
        return ResourcePublishResult<SpriteResource>{{}, false, std::move(diagnostic)};
    }

    return Publish<SpriteResource>(*identity, std::move(resource), strongDependencies);
}

ResourcePublishResult<SceneTemplateResource> ResourceRegistry::PublishSceneTemplate(
    std::string_view projectRelativeReference,
    SceneTemplateResource resource,
    std::span<const ResourceHandleUntyped> strongDependencies)
{
    ResourceDiagnostic diagnostic{};
    const auto identity = Canonicalize(ResourceTypeDomain::SceneTemplate, projectRelativeReference, diagnostic);
    if (!identity.has_value())
    {
        return ResourcePublishResult<SceneTemplateResource>{{}, false, std::move(diagnostic)};
    }
    if (resource.canonicalToml.empty())
    {
        diagnostic.code = ResourceErrorCode::InvalidPayload;
        diagnostic.identity = *identity;
        diagnostic.message = "scene template canonical TOML must not be empty";
        return ResourcePublishResult<SceneTemplateResource>{{}, false, std::move(diagnostic)};
    }

    return Publish<SceneTemplateResource>(*identity, std::move(resource), strongDependencies);
}

ResourcePublishResult<FontResource> ResourceRegistry::PublishFont(
    std::string_view projectRelativeReference,
    FontResource resource)
{
    ResourceDiagnostic diagnostic{};
    const auto identity = Canonicalize(ResourceTypeDomain::Font, projectRelativeReference, diagnostic);
    if (!identity.has_value())
    {
        return ResourcePublishResult<FontResource>{{}, false, std::move(diagnostic)};
    }
    if (resource.canonicalBytes.empty())
    {
        diagnostic.code = ResourceErrorCode::InvalidPayload;
        diagnostic.identity = *identity;
        diagnostic.message = "canonical font payload must not be empty";
        return ResourcePublishResult<FontResource>{{}, false, std::move(diagnostic)};
    }
    if (resource.cpuRetention != CpuRetentionPolicy::Required)
    {
        diagnostic.code = ResourceErrorCode::InvalidPayload;
        diagnostic.identity = *identity;
        diagnostic.message = "font CPU retention must remain required for in-memory prepared faces";
        return ResourcePublishResult<FontResource>{{}, false, std::move(diagnostic)};
    }

    return Publish<FontResource>(
        *identity,
        std::move(resource),
        std::span<const ResourceHandleUntyped>{});
}

ResourceOperationResult ResourceRegistry::RecordLoadFailure(
    ResourceTypeDomain domain,
    std::string_view projectRelativeReference,
    ResourceErrorCode code,
    std::string message)
{
    ResourceDiagnostic canonicalDiagnostic{};
    const auto identity = Canonicalize(domain, projectRelativeReference, canonicalDiagnostic);
    if (!identity.has_value())
    {
        return ResourceOperationResult{std::move(canonicalDiagnostic)};
    }

    const std::string key = IdentityKey(*identity);
    const auto existing = identityToSlot_.find(key);
    if (existing != identityToSlot_.end())
    {
        Slot& slot = slots_[existing->second];
        if (slot.state == ResourceLoadState::Ready)
        {
            ResourceDiagnostic diagnostic{};
            diagnostic.code = ResourceErrorCode::InvalidHandle;
            diagnostic.identity = *identity;
            diagnostic.message = "cannot replace a ready resource with an error record";
            return ResourceOperationResult{std::move(diagnostic)};
        }
        if (slot.state == ResourceLoadState::Error)
        {
            slot.error = ResourceDiagnostic{code, *identity, std::move(message), {}};
            ++failedLoadRecords_;
            return {};
        }
    }

    const std::uint32_t slotIndex = AllocateSlot();
    Slot& slot = slots_[slotIndex];
    slot.state = ResourceLoadState::Error;
    slot.identity = *identity;
    slot.payload = std::monostate{};
    slot.dependencies.clear();
    slot.dependents.clear();
    slot.callerRetainCount = 0U;
    slot.rendererResident = false;
    slot.knownRendererGpuBytes = 0U;
    slot.error = ResourceDiagnostic{code, *identity, std::move(message), {}};
    identityToSlot_.emplace(key, slotIndex);
    ++failedLoadRecords_;
    ++errorResources_;
    return {};
}

ResourceOperationResult ResourceRegistry::Invalidate(
    ResourceTypeDomain domain,
    std::string_view projectRelativeReference)
{
    ResourceDiagnostic diagnostic{};
    const auto identity = Canonicalize(domain, projectRelativeReference, diagnostic);
    if (!identity.has_value())
    {
        return ResourceOperationResult{std::move(diagnostic)};
    }

    const auto found = identityToSlot_.find(IdentityKey(*identity));
    if (found == identityToSlot_.end())
    {
        return {};
    }

    Slot& slot = slots_[found->second];
    if (slot.state == ResourceLoadState::Error)
    {
        ClearErrorSlot(found->second);
        return {};
    }

    return Unload(ResourceHandleUntyped{found->second, slot.generation, slot.identity.domain});
}

ResourceOperationResult ResourceRegistry::SetStrongDependencies(
    ResourceHandleUntyped owner,
    std::span<const ResourceHandleUntyped> dependencies)
{
    const ResourceOperationResult ownerValidation = ValidateReadyHandle(owner);
    if (!ownerValidation.Succeeded())
    {
        return ownerValidation;
    }

    std::vector<ResourceHandleUntyped> validated{};
    validated.reserve(dependencies.size());

    for (const ResourceHandleUntyped dependency : dependencies)
    {
        const ResourceOperationResult dependencyValidation = ValidateReadyHandle(dependency);
        if (!dependencyValidation.Succeeded())
        {
            ResourceDiagnostic diagnostic = *dependencyValidation.diagnostic;
            diagnostic.code = ResourceErrorCode::DependencyNotReady;
            diagnostic.identity = IdentityOf(owner);
            diagnostic.message = "strong dependency is not a ready resource";
            return ResourceOperationResult{std::move(diagnostic)};
        }

        if (dependency == owner)
        {
            ResourceDiagnostic diagnostic{};
            diagnostic.code = ResourceErrorCode::DependencyCycle;
            diagnostic.identity = IdentityOf(owner);
            diagnostic.message = "resource cannot strongly depend on itself";
            diagnostic.chain = {IdentityOf(owner), IdentityOf(owner)};
            return ResourceOperationResult{std::move(diagnostic)};
        }

        if (std::find(validated.begin(), validated.end(), dependency) != validated.end())
        {
            continue;
        }

        std::vector<bool> visited(slots_.size(), false);
        std::vector<ResourceHandleUntyped> chain{};
        if (Reaches(dependency, owner, visited, chain))
        {
            ResourceDiagnostic diagnostic{};
            diagnostic.code = ResourceErrorCode::DependencyCycle;
            diagnostic.identity = IdentityOf(owner);
            diagnostic.message = "strong dependency cycle rejected";
            diagnostic.chain.push_back(IdentityOf(owner));
            for (const ResourceHandleUntyped chainHandle : chain)
            {
                diagnostic.chain.push_back(IdentityOf(chainHandle));
            }
            return ResourceOperationResult{std::move(diagnostic)};
        }

        validated.push_back(dependency);
    }

    Slot& ownerSlot = slots_[owner.slot];
    for (const ResourceHandleUntyped previous : ownerSlot.dependencies)
    {
        RemoveDependent(previous, owner);
    }

    ownerSlot.dependencies = std::move(validated);
    for (const ResourceHandleUntyped dependency : ownerSlot.dependencies)
    {
        AddDependent(dependency, owner);
    }
    return {};
}

ResourceOperationResult ResourceRegistry::Retain(ResourceHandleUntyped handle)
{
    const ResourceOperationResult validation = ValidateReadyHandle(handle);
    if (!validation.Succeeded())
    {
        return validation;
    }

    Slot& slot = slots_[handle.slot];
    if (slot.callerRetainCount == std::numeric_limits<std::uint32_t>::max())
    {
        ResourceDiagnostic diagnostic{};
        diagnostic.code = ResourceErrorCode::InvalidHandle;
        diagnostic.identity = slot.identity;
        diagnostic.message = "caller retain count overflow";
        return ResourceOperationResult{std::move(diagnostic)};
    }

    ++slot.callerRetainCount;
    return {};
}

ResourceOperationResult ResourceRegistry::Release(ResourceHandleUntyped handle)
{
    const ResourceOperationResult validation = ValidateReadyHandle(handle);
    if (!validation.Succeeded())
    {
        return validation;
    }

    Slot& slot = slots_[handle.slot];
    if (slot.callerRetainCount == 0U)
    {
        ResourceDiagnostic diagnostic{};
        diagnostic.code = ResourceErrorCode::InvalidHandle;
        diagnostic.identity = slot.identity;
        diagnostic.message = "resource has no caller retain to release";
        return ResourceOperationResult{std::move(diagnostic)};
    }

    --slot.callerRetainCount;
    return {};
}

ResourceOperationResult ResourceRegistry::Unload(ResourceHandleUntyped handle)
{
    const ResourceOperationResult validation = ValidateReadyHandle(handle);
    if (!validation.Succeeded())
    {
        return validation;
    }

    const Slot& slot = slots_[handle.slot];
    if (slot.callerRetainCount != 0U)
    {
        ResourceDiagnostic diagnostic{};
        diagnostic.code = ResourceErrorCode::RetainedByCaller;
        diagnostic.identity = slot.identity;
        diagnostic.message = "resource is explicitly retained by a caller";
        return ResourceOperationResult{std::move(diagnostic)};
    }
    if (!slot.dependents.empty())
    {
        ResourceDiagnostic diagnostic{};
        diagnostic.code = ResourceErrorCode::HasDependents;
        diagnostic.identity = slot.identity;
        diagnostic.message = "resource cannot unload while live strong dependents exist";
        for (const ResourceHandleUntyped dependent : slot.dependents)
        {
            diagnostic.chain.push_back(IdentityOf(dependent));
        }
        return ResourceOperationResult{std::move(diagnostic)};
    }

    UnloadSlot(handle.slot, false, nullptr);
    return {};
}

std::size_t ResourceRegistry::ReleaseUnused()
{
    std::size_t released = 0U;
    bool progress = true;
    while (progress)
    {
        progress = false;
        for (std::uint32_t slotIndex = 0U; slotIndex < static_cast<std::uint32_t>(slots_.size()); ++slotIndex)
        {
            const Slot& slot = slots_[slotIndex];
            if (slot.state == ResourceLoadState::Ready && slot.callerRetainCount == 0U && slot.dependents.empty())
            {
                UnloadSlot(slotIndex, false, nullptr);
                ++released;
                progress = true;
            }
        }
    }
    return released;
}

ResourceClearReport ResourceRegistry::ClearProjectResources()
{
    ResourceClearReport report{};
    while (readyResources_ != 0U)
    {
        bool progress = false;
        for (std::uint32_t slotIndex = 0U; slotIndex < static_cast<std::uint32_t>(slots_.size()); ++slotIndex)
        {
            if (slots_[slotIndex].state == ResourceLoadState::Ready && slots_[slotIndex].dependents.empty())
            {
                UnloadSlot(slotIndex, true, &report);
                progress = true;
                break;
            }
        }
        if (!progress)
        {
            break;
        }
    }

    for (std::uint32_t slotIndex = 0U; slotIndex < static_cast<std::uint32_t>(slots_.size()); ++slotIndex)
    {
        if (slots_[slotIndex].state == ResourceLoadState::Error)
        {
            ClearErrorSlot(slotIndex);
            ++report.clearedErrors;
        }
    }
    return report;
}

ResourceOperationResult ResourceRegistry::SetTextureRendererResidency(
    ResourceHandle<TextureResource> handle,
    bool resident,
    std::size_t knownGpuBytes)
{
    const ResourceOperationResult validation = ValidateReadyHandle(handle.Untyped());
    if (!validation.Succeeded())
    {
        return validation;
    }

    Slot& slot = slots_[handle.slot];
    if (handle.domain != ResourceTypeDomain::Texture || std::get_if<TextureResource>(&slot.payload) == nullptr)
    {
        ResourceDiagnostic diagnostic{};
        diagnostic.code = ResourceErrorCode::TypeMismatch;
        diagnostic.identity = slot.identity;
        diagnostic.message = "resource handle is not backed by a texture resource";
        return ResourceOperationResult{std::move(diagnostic)};
    }

    slot.rendererResident = resident;
    slot.knownRendererGpuBytes = resident ? knownGpuBytes : 0U;
    return {};
}

ResourceOperationResult ResourceRegistry::ReleaseTextureCpuPayload(ResourceHandle<TextureResource> handle)
{
    const ResourceOperationResult validation = ValidateReadyHandle(handle.Untyped());
    if (!validation.Succeeded())
    {
        return validation;
    }

    TextureResource* texture = std::get_if<TextureResource>(&slots_[handle.slot].payload);
    if (texture == nullptr)
    {
        ResourceDiagnostic diagnostic{};
        diagnostic.code = ResourceErrorCode::TypeMismatch;
        diagnostic.identity = IdentityOf(handle.Untyped());
        diagnostic.message = "resource payload is not a texture";
        return ResourceOperationResult{std::move(diagnostic)};
    }
    if (texture->cpuRetention == CpuRetentionPolicy::Required)
    {
        ResourceDiagnostic diagnostic{};
        diagnostic.code = ResourceErrorCode::CpuRetentionRequired;
        diagnostic.identity = IdentityOf(handle.Untyped());
        diagnostic.message = "texture CPU payload is required by its retention policy";
        return ResourceOperationResult{std::move(diagnostic)};
    }

    std::vector<std::uint8_t>{}.swap(texture->canonicalRgba8);
    return {};
}

std::optional<ResourceSnapshot> ResourceRegistry::Inspect(ResourceHandleUntyped handle) const
{
    if (handle.slot >= slots_.size())
    {
        return std::nullopt;
    }

    const Slot& slot = slots_[handle.slot];
    if (slot.state == ResourceLoadState::Unloaded || slot.generation != handle.generation ||
        slot.identity.domain != handle.domain)
    {
        return std::nullopt;
    }
    return SnapshotOf(slot);
}

std::vector<ResourceSnapshot> ResourceRegistry::InspectAll() const
{
    std::vector<ResourceSnapshot> snapshots{};
    snapshots.reserve(readyResources_ + errorResources_);
    for (const Slot& slot : slots_)
    {
        if (slot.state != ResourceLoadState::Unloaded)
        {
            snapshots.push_back(SnapshotOf(slot));
        }
    }
    return snapshots;
}

ResourceRegistryStats ResourceRegistry::Stats() const noexcept
{
    return ResourceRegistryStats{
        canonicalizationCalls_,
        duplicateReadyLoads_,
        failedLoadRecords_,
        unloads_,
        0U,
        readyResources_,
        errorResources_};
}

const std::filesystem::path& ResourceRegistry::ProjectRoot() const noexcept
{
    return projectRoot_;
}

std::optional<ResourceIdentity> ResourceRegistry::Canonicalize(
    ResourceTypeDomain domain,
    std::string_view reference,
    ResourceDiagnostic& diagnostic)
{
    ++canonicalizationCalls_;
    diagnostic.code = ResourceErrorCode::InvalidReference;
    diagnostic.identity = ResourceIdentity{domain, std::string(reference)};

    if (reference.empty())
    {
        diagnostic.message = "resource reference must not be empty";
        return std::nullopt;
    }

    std::string normalized(reference);
    for (char& value : normalized)
    {
        if (value == '\\')
        {
            value = '/';
        }
        else if (IsControl(value))
        {
            diagnostic.message = "resource reference contains a control character";
            return std::nullopt;
        }
    }

    if (normalized.front() == '/' || LooksLikeDriveAbsolute(normalized))
    {
        diagnostic.message = "absolute resource references are invalid";
        return std::nullopt;
    }

    std::string canonical{};
    std::size_t cursor = 0U;
    while (cursor <= normalized.size())
    {
        const std::size_t separator = normalized.find('/', cursor);
        const std::size_t end = separator == std::string::npos ? normalized.size() : separator;
        const std::string_view segment(normalized.data() + cursor, end - cursor);

        if (segment == "..")
        {
            diagnostic.message = "resource reference traversal is invalid";
            return std::nullopt;
        }
        if (!segment.empty() && segment != ".")
        {
            if (segment.find(':') != std::string_view::npos)
            {
                diagnostic.message = "resource reference segment contains ':' and is not portable";
                return std::nullopt;
            }
            if (!canonical.empty())
            {
                canonical.push_back('/');
            }
            canonical.append(segment);
        }

        if (separator == std::string::npos)
        {
            break;
        }
        cursor = separator + 1U;
    }

    if (canonical.empty())
    {
        diagnostic.message = "resource reference does not identify a project-relative resource";
        return std::nullopt;
    }
    return ResourceIdentity{domain, std::move(canonical)};
}

std::string ResourceRegistry::IdentityKey(const ResourceIdentity& identity) const
{
    std::string key{};
    key.reserve(identity.canonicalReference.size() + 4U);
    key.append(std::to_string(static_cast<unsigned int>(identity.domain)));
    key.push_back(':');
    key.append(identity.canonicalReference);
    return key;
}

std::uint32_t ResourceRegistry::AllocateSlot()
{
    if (!freeSlots_.empty())
    {
        const std::uint32_t slotIndex = freeSlots_.back();
        freeSlots_.pop_back();
        return slotIndex;
    }

    slots_.emplace_back();
    return static_cast<std::uint32_t>(slots_.size() - 1U);
}

ResourceOperationResult ResourceRegistry::ValidateReadyHandle(ResourceHandleUntyped handle) const
{
    if (handle.slot >= slots_.size())
    {
        ResourceDiagnostic diagnostic{};
        diagnostic.code = ResourceErrorCode::InvalidHandle;
        diagnostic.message = "resource handle slot is out of range";
        return ResourceOperationResult{std::move(diagnostic)};
    }

    const Slot& slot = slots_[handle.slot];
    if (slot.generation != handle.generation || slot.state != ResourceLoadState::Ready)
    {
        ResourceDiagnostic diagnostic{};
        diagnostic.code = ResourceErrorCode::StaleHandle;
        diagnostic.identity = slot.identity;
        diagnostic.message = "resource handle generation is stale or the slot is not ready";
        return ResourceOperationResult{std::move(diagnostic)};
    }
    if (slot.identity.domain != handle.domain)
    {
        ResourceDiagnostic diagnostic{};
        diagnostic.code = ResourceErrorCode::TypeMismatch;
        diagnostic.identity = slot.identity;
        diagnostic.message = "resource handle type domain does not match the resolved slot";
        return ResourceOperationResult{std::move(diagnostic)};
    }
    return {};
}

bool ResourceRegistry::Reaches(
    ResourceHandleUntyped start,
    ResourceHandleUntyped target,
    std::vector<bool>& visited,
    std::vector<ResourceHandleUntyped>& chain) const
{
    if (start.slot >= slots_.size())
    {
        return false;
    }
    if (start == target)
    {
        chain.push_back(start);
        return true;
    }
    if (visited[start.slot])
    {
        return false;
    }

    visited[start.slot] = true;
    chain.push_back(start);
    for (const ResourceHandleUntyped dependency : slots_[start.slot].dependencies)
    {
        if (Reaches(dependency, target, visited, chain))
        {
            return true;
        }
    }
    chain.pop_back();
    return false;
}

ResourceIdentity ResourceRegistry::IdentityOf(ResourceHandleUntyped handle) const
{
    if (handle.slot >= slots_.size())
    {
        return ResourceIdentity{handle.domain, {}};
    }
    return slots_[handle.slot].identity;
}

ResourceMemoryEvidence ResourceRegistry::MemoryOf(const Slot& slot) const
{
    ResourceMemoryEvidence evidence{};
    evidence.rendererResident = slot.rendererResident;
    evidence.knownRendererGpuBytes = slot.knownRendererGpuBytes;

    if (const TextureResource* texture = std::get_if<TextureResource>(&slot.payload))
    {
        evidence.knownRetainedCpuBytes = sizeof(TextureResource) + texture->canonicalRgba8.size() +
                                         StringLogicalBytes(texture->retentionReason);
        evidence.retainedContainerCapacityBytes = texture->canonicalRgba8.capacity() +
                                                  StringCapacityBytes(texture->retentionReason);
        evidence.cpuRetention = texture->cpuRetention;
        evidence.cpuPayloadResident = !texture->canonicalRgba8.empty();
        evidence.retentionReason = texture->retentionReason;
        return evidence;
    }

    if (const SpriteResource* sprite = std::get_if<SpriteResource>(&slot.payload))
    {
        std::size_t logical = sizeof(SpriteResource) + StringLogicalBytes(sprite->retentionReason) +
                              StringLogicalBytes(sprite->asset.id) +
                              sprite->asset.pages.size() * sizeof(SpriteAtlasPage) +
                              sprite->asset.regions.size() * sizeof(SpriteRegion);
        std::size_t capacity = StringCapacityBytes(sprite->retentionReason) +
                               StringCapacityBytes(sprite->asset.id) +
                               sprite->asset.pages.capacity() * sizeof(SpriteAtlasPage) +
                               sprite->asset.regions.capacity() * sizeof(SpriteRegion);

        for (const SpriteAtlasPage& page : sprite->asset.pages)
        {
            logical += StringLogicalBytes(page.id) + StringLogicalBytes(page.textureReference);
            capacity += StringCapacityBytes(page.id) + StringCapacityBytes(page.textureReference);
        }
        for (const SpriteRegion& region : sprite->asset.regions)
        {
            logical += StringLogicalBytes(region.id) + StringLogicalBytes(region.pageId);
            capacity += StringCapacityBytes(region.id) + StringCapacityBytes(region.pageId);
        }

        evidence.knownRetainedCpuBytes = logical;
        evidence.retainedContainerCapacityBytes = capacity;
        evidence.cpuRetention = sprite->cpuRetention;
        evidence.cpuPayloadResident = true;
        evidence.retentionReason = sprite->retentionReason;
        return evidence;
    }

    if (const SceneTemplateResource* sceneTemplate = std::get_if<SceneTemplateResource>(&slot.payload))
    {
        evidence.knownRetainedCpuBytes = sizeof(SceneTemplateResource) +
                                         StringLogicalBytes(sceneTemplate->canonicalToml) +
                                         StringLogicalBytes(sceneTemplate->retentionReason);
        evidence.retainedContainerCapacityBytes = StringCapacityBytes(sceneTemplate->canonicalToml) +
                                                  StringCapacityBytes(sceneTemplate->retentionReason);
        evidence.cpuRetention = sceneTemplate->cpuRetention;
        evidence.cpuPayloadResident = !sceneTemplate->canonicalToml.empty();
        evidence.retentionReason = sceneTemplate->retentionReason;
        return evidence;
    }

    if (const FontResource* font = std::get_if<FontResource>(&slot.payload))
    {
        evidence.knownRetainedCpuBytes = sizeof(FontResource) + font->canonicalBytes.size() +
                                         StringLogicalBytes(font->retentionReason);
        evidence.retainedContainerCapacityBytes = font->canonicalBytes.capacity() +
                                                  StringCapacityBytes(font->retentionReason);
        evidence.cpuRetention = font->cpuRetention;
        evidence.cpuPayloadResident = !font->canonicalBytes.empty();
        evidence.retentionReason = font->retentionReason;
        return evidence;
    }
    return evidence;
}

ResourceSnapshot ResourceRegistry::SnapshotOf(const Slot& slot) const
{
    ResourceSnapshot snapshot{};
    snapshot.identity = slot.identity;
    snapshot.state = slot.state;
    snapshot.generation = slot.generation;
    snapshot.callerRetainCount = slot.callerRetainCount;
    snapshot.memory = MemoryOf(slot);
    snapshot.error = slot.error;
    snapshot.dependencies.reserve(slot.dependencies.size());
    snapshot.dependents.reserve(slot.dependents.size());
    for (const ResourceHandleUntyped dependency : slot.dependencies)
    {
        snapshot.dependencies.push_back(IdentityOf(dependency));
    }
    for (const ResourceHandleUntyped dependent : slot.dependents)
    {
        snapshot.dependents.push_back(IdentityOf(dependent));
    }
    return snapshot;
}

void ResourceRegistry::RemoveDependent(ResourceHandleUntyped dependency, ResourceHandleUntyped dependent)
{
    if (dependency.slot >= slots_.size())
    {
        return;
    }
    auto& dependents = slots_[dependency.slot].dependents;
    dependents.erase(std::remove(dependents.begin(), dependents.end(), dependent), dependents.end());
}

void ResourceRegistry::AddDependent(ResourceHandleUntyped dependency, ResourceHandleUntyped dependent)
{
    if (dependency.slot >= slots_.size())
    {
        return;
    }
    auto& dependents = slots_[dependency.slot].dependents;
    if (std::find(dependents.begin(), dependents.end(), dependent) == dependents.end())
    {
        dependents.push_back(dependent);
    }
}

void ResourceRegistry::UnloadSlot(std::uint32_t slotIndex, bool ignoreRetains, ResourceClearReport* clearReport)
{
    Slot& slot = slots_[slotIndex];
    if (slot.state != ResourceLoadState::Ready || (!ignoreRetains && slot.callerRetainCount != 0U))
    {
        return;
    }

    if (clearReport != nullptr)
    {
        clearReport->unloadOrder.push_back(slot.identity);
    }

    const ResourceHandleUntyped self{slotIndex, slot.generation, slot.identity.domain};
    for (const ResourceHandleUntyped dependency : slot.dependencies)
    {
        RemoveDependent(dependency, self);
    }

    identityToSlot_.erase(IdentityKey(slot.identity));
    slot.payload = std::monostate{};
    slot.dependencies.clear();
    slot.dependents.clear();
    slot.callerRetainCount = 0U;
    slot.rendererResident = false;
    slot.knownRendererGpuBytes = 0U;
    slot.error.reset();
    slot.state = ResourceLoadState::Unloaded;
    slot.generation = NextGeneration(slot.generation);
    freeSlots_.push_back(slotIndex);
    --readyResources_;
    ++unloads_;
}

void ResourceRegistry::ClearErrorSlot(std::uint32_t slotIndex)
{
    Slot& slot = slots_[slotIndex];
    if (slot.state != ResourceLoadState::Error)
    {
        return;
    }

    identityToSlot_.erase(IdentityKey(slot.identity));
    slot.payload = std::monostate{};
    slot.dependencies.clear();
    slot.dependents.clear();
    slot.callerRetainCount = 0U;
    slot.rendererResident = false;
    slot.knownRendererGpuBytes = 0U;
    slot.error.reset();
    slot.state = ResourceLoadState::Unloaded;
    slot.generation = NextGeneration(slot.generation);
    freeSlots_.push_back(slotIndex);
    --errorResources_;
}

std::uint32_t ResourceRegistry::NextGeneration(std::uint32_t current) noexcept
{
    ++current;
    return current == 0U ? 1U : current;
}
} // namespace trace2d::assets
