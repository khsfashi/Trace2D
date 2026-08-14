#pragma once

#include <trace2d/assets/SpriteAssets.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace trace2d::assets
{
enum class ResourceTypeDomain : std::uint8_t
{
    Texture = 1,
    Sprite = 2,
};

[[nodiscard]] std::string_view ToString(ResourceTypeDomain value) noexcept;

enum class ResourceLoadState : std::uint8_t
{
    Unloaded = 0,
    Ready,
    Error,
};

[[nodiscard]] std::string_view ToString(ResourceLoadState value) noexcept;

enum class CpuRetentionPolicy : std::uint8_t
{
    Required = 0,
    Releasable,
    Reacquirable,
};

[[nodiscard]] std::string_view ToString(CpuRetentionPolicy value) noexcept;

enum class TextureResourceColorSpace : std::uint8_t
{
    Srgb = 0,
    Linear,
};

enum class TextureResourceAlphaMode : std::uint8_t
{
    Straight = 0,
    Premultiplied,
};

struct ResourceIdentity final
{
    ResourceTypeDomain domain{ResourceTypeDomain::Texture};
    std::string canonicalReference{};

    [[nodiscard]] bool operator==(const ResourceIdentity&) const noexcept = default;
};

struct ResourceHandleUntyped final
{
    std::uint32_t slot{0};
    std::uint32_t generation{0};
    ResourceTypeDomain domain{ResourceTypeDomain::Texture};

    [[nodiscard]] bool operator==(const ResourceHandleUntyped&) const noexcept = default;
};

template <typename T>
struct ResourceTraits;

template <typename T>
struct ResourceHandle final
{
    std::uint32_t slot{0};
    std::uint32_t generation{0};
    ResourceTypeDomain domain{ResourceTraits<T>::Domain};

    [[nodiscard]] bool operator==(const ResourceHandle&) const noexcept = default;

    [[nodiscard]] ResourceHandleUntyped Untyped() const noexcept
    {
        return ResourceHandleUntyped{slot, generation, domain};
    }
};

// Canonical CPU-side texture truth. Renderer/backend residency is intentionally not stored here.
struct TextureResource final
{
    std::uint32_t width{0};
    std::uint32_t height{0};
    TextureResourceColorSpace colorSpace{TextureResourceColorSpace::Srgb};
    TextureResourceAlphaMode alphaMode{TextureResourceAlphaMode::Straight};
    CpuRetentionPolicy cpuRetention{CpuRetentionPolicy::Reacquirable};
    std::string retentionReason{"source/package can be reacquired"};
    std::vector<std::uint8_t> canonicalRgba8{};
};

struct SpriteResource final
{
    SpriteAsset asset{};
    CpuRetentionPolicy cpuRetention{CpuRetentionPolicy::Required};
    std::string retentionReason{"canonical sprite metadata is required for runtime lookup"};
};

template <>
struct ResourceTraits<TextureResource> final
{
    static constexpr ResourceTypeDomain Domain = ResourceTypeDomain::Texture;
};

template <>
struct ResourceTraits<SpriteResource> final
{
    static constexpr ResourceTypeDomain Domain = ResourceTypeDomain::Sprite;
};

enum class ResourceErrorCode : std::uint8_t
{
    InvalidReference = 0,
    InvalidPayload,
    TypeMismatch,
    InvalidHandle,
    StaleHandle,
    DependencyNotReady,
    DependencyCycle,
    HasDependents,
    RetainedByCaller,
    CpuRetentionRequired,
    RetryRequiresInvalidation,
};

[[nodiscard]] std::string_view ToString(ResourceErrorCode value) noexcept;

struct ResourceDiagnostic final
{
    ResourceErrorCode code{ResourceErrorCode::InvalidReference};
    ResourceIdentity identity{};
    std::string message{};
    std::vector<ResourceIdentity> chain{};
};

template <typename T>
struct ResourcePublishResult final
{
    ResourceHandle<T> handle{};
    bool reusedExisting{false};
    std::optional<ResourceDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return !diagnostic.has_value();
    }
};

struct ResourceOperationResult final
{
    std::optional<ResourceDiagnostic> diagnostic{};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return !diagnostic.has_value();
    }
};

struct ResourceMemoryEvidence final
{
    std::size_t knownRetainedCpuBytes{0};
    std::size_t retainedContainerCapacityBytes{0};
    std::size_t knownRendererGpuBytes{0};
    CpuRetentionPolicy cpuRetention{CpuRetentionPolicy::Required};
    bool cpuPayloadResident{false};
    bool rendererResident{false};
    std::string retentionReason{};
};

struct ResourceSnapshot final
{
    ResourceIdentity identity{};
    ResourceLoadState state{ResourceLoadState::Unloaded};
    std::uint32_t generation{0};
    std::uint32_t callerRetainCount{0};
    std::vector<ResourceIdentity> dependencies{};
    std::vector<ResourceIdentity> dependents{};
    ResourceMemoryEvidence memory{};
    std::optional<ResourceDiagnostic> error{};
};

struct ResourceRegistryStats final
{
    std::uint64_t canonicalizationCalls{0};
    std::uint64_t duplicateReadyLoads{0};
    std::uint64_t failedLoadRecords{0};
    std::uint64_t unloads{0};
    std::uint64_t filesystemQueries{0};
    std::size_t readyResources{0};
    std::size_t errorResources{0};
};

struct ResourceClearReport final
{
    std::vector<ResourceIdentity> unloadOrder{};
    std::size_t clearedErrors{0};
};

class ResourceRegistry final
{
public:
    explicit ResourceRegistry(std::filesystem::path projectRoot);

    ResourceRegistry(const ResourceRegistry&) = delete;
    ResourceRegistry& operator=(const ResourceRegistry&) = delete;
    ResourceRegistry(ResourceRegistry&&) noexcept = default;
    ResourceRegistry& operator=(ResourceRegistry&&) noexcept = default;
    ~ResourceRegistry() = default;

    [[nodiscard]] ResourcePublishResult<TextureResource> PublishTexture(
        std::string_view projectRelativeReference,
        TextureResource resource);
    [[nodiscard]] ResourcePublishResult<SpriteResource> PublishSprite(
        std::string_view projectRelativeReference,
        SpriteResource resource,
        std::span<const ResourceHandleUntyped> strongDependencies = {});

    [[nodiscard]] ResourceOperationResult RecordLoadFailure(
        ResourceTypeDomain domain,
        std::string_view projectRelativeReference,
        ResourceErrorCode code,
        std::string message);
    [[nodiscard]] ResourceOperationResult Invalidate(
        ResourceTypeDomain domain,
        std::string_view projectRelativeReference);

    [[nodiscard]] ResourceOperationResult SetStrongDependencies(
        ResourceHandleUntyped owner,
        std::span<const ResourceHandleUntyped> dependencies);
    [[nodiscard]] ResourceOperationResult Retain(ResourceHandleUntyped handle);
    [[nodiscard]] ResourceOperationResult Release(ResourceHandleUntyped handle);
    [[nodiscard]] ResourceOperationResult Unload(ResourceHandleUntyped handle);
    [[nodiscard]] std::size_t ReleaseUnused();
    [[nodiscard]] ResourceClearReport ClearProjectResources();

    // Records only renderer-owned residency evidence; no backend handle enters canonical CPU state.
    [[nodiscard]] ResourceOperationResult SetTextureRendererResidency(
        ResourceHandle<TextureResource> handle,
        bool resident,
        std::size_t knownGpuBytes);
    [[nodiscard]] ResourceOperationResult ReleaseTextureCpuPayload(ResourceHandle<TextureResource> handle);

    template <typename T>
    [[nodiscard]] const T* Resolve(ResourceHandle<T> handle) const noexcept
    {
        return Resolve<T>(handle.Untyped());
    }

    template <typename T>
    [[nodiscard]] const T* Resolve(ResourceHandleUntyped handle) const noexcept
    {
        if (handle.domain != ResourceTraits<T>::Domain || handle.slot >= slots_.size())
        {
            return nullptr;
        }

        const Slot& slot = slots_[handle.slot];
        if (slot.state != ResourceLoadState::Ready || slot.generation != handle.generation ||
            slot.identity.domain != ResourceTraits<T>::Domain)
        {
            return nullptr;
        }

        return std::get_if<T>(&slot.payload);
    }

    [[nodiscard]] std::optional<ResourceSnapshot> Inspect(ResourceHandleUntyped handle) const;
    [[nodiscard]] std::vector<ResourceSnapshot> InspectAll() const;
    [[nodiscard]] ResourceRegistryStats Stats() const noexcept;
    [[nodiscard]] const std::filesystem::path& ProjectRoot() const noexcept;

private:
    using Payload = std::variant<std::monostate, TextureResource, SpriteResource>;

    struct Slot final
    {
        std::uint32_t generation{1};
        ResourceLoadState state{ResourceLoadState::Unloaded};
        ResourceIdentity identity{};
        Payload payload{};
        std::vector<ResourceHandleUntyped> dependencies{};
        std::vector<ResourceHandleUntyped> dependents{};
        std::uint32_t callerRetainCount{0};
        bool rendererResident{false};
        std::size_t knownRendererGpuBytes{0};
        std::optional<ResourceDiagnostic> error{};
    };

    [[nodiscard]] std::optional<ResourceIdentity> Canonicalize(
        ResourceTypeDomain domain,
        std::string_view reference,
        ResourceDiagnostic& diagnostic);
    [[nodiscard]] std::string IdentityKey(const ResourceIdentity& identity) const;
    [[nodiscard]] std::uint32_t AllocateSlot();
    [[nodiscard]] ResourceOperationResult ValidateReadyHandle(ResourceHandleUntyped handle) const;
    [[nodiscard]] bool Reaches(
        ResourceHandleUntyped start,
        ResourceHandleUntyped target,
        std::vector<bool>& visited,
        std::vector<ResourceHandleUntyped>& chain) const;
    [[nodiscard]] ResourceIdentity IdentityOf(ResourceHandleUntyped handle) const;
    [[nodiscard]] ResourceMemoryEvidence MemoryOf(const Slot& slot) const;
    [[nodiscard]] ResourceSnapshot SnapshotOf(const Slot& slot) const;
    void RemoveDependent(ResourceHandleUntyped dependency, ResourceHandleUntyped dependent);
    void AddDependent(ResourceHandleUntyped dependency, ResourceHandleUntyped dependent);
    void UnloadSlot(std::uint32_t slotIndex, bool ignoreRetains, ResourceClearReport* clearReport);
    void ClearErrorSlot(std::uint32_t slotIndex);
    [[nodiscard]] static std::uint32_t NextGeneration(std::uint32_t current) noexcept;

    template <typename T>
    [[nodiscard]] ResourcePublishResult<T> Publish(
        const ResourceIdentity& identity,
        T resource,
        std::span<const ResourceHandleUntyped> dependencies)
    {
        const std::string key = IdentityKey(identity);
        const auto existing = identityToSlot_.find(key);
        if (existing != identityToSlot_.end())
        {
            Slot& slot = slots_[existing->second];
            if (slot.state == ResourceLoadState::Ready)
            {
                ++duplicateReadyLoads_;
                return ResourcePublishResult<T>{
                    ResourceHandle<T>{existing->second, slot.generation, identity.domain},
                    true,
                    std::nullopt};
            }

            if (slot.state == ResourceLoadState::Error)
            {
                ResourceDiagnostic diagnostic{};
                diagnostic.code = ResourceErrorCode::RetryRequiresInvalidation;
                diagnostic.identity = identity;
                diagnostic.message = "resource has a recorded load error; invalidate it before retrying";
                return ResourcePublishResult<T>{{}, false, std::move(diagnostic)};
            }
        }

        const std::uint32_t slotIndex = AllocateSlot();
        Slot& slot = slots_[slotIndex];
        slot.state = ResourceLoadState::Ready;
        slot.identity = identity;
        slot.payload = std::move(resource);
        slot.dependencies.clear();
        slot.dependents.clear();
        slot.callerRetainCount = 0;
        slot.rendererResident = false;
        slot.knownRendererGpuBytes = 0U;
        slot.error.reset();
        identityToSlot_.emplace(key, slotIndex);
        ++readyResources_;

        ResourceHandle<T> typed{slotIndex, slot.generation, identity.domain};
        const ResourceOperationResult dependencyResult = SetStrongDependencies(typed.Untyped(), dependencies);
        if (!dependencyResult.Succeeded())
        {
            ResourceDiagnostic diagnostic = *dependencyResult.diagnostic;
            UnloadSlot(slotIndex, true, nullptr);
            return ResourcePublishResult<T>{{}, false, std::move(diagnostic)};
        }

        return ResourcePublishResult<T>{typed, false, std::nullopt};
    }

    std::filesystem::path projectRoot_{};
    std::vector<Slot> slots_{};
    std::vector<std::uint32_t> freeSlots_{};
    std::unordered_map<std::string, std::uint32_t> identityToSlot_{};
    std::uint64_t canonicalizationCalls_{0};
    std::uint64_t duplicateReadyLoads_{0};
    std::uint64_t failedLoadRecords_{0};
    std::uint64_t unloads_{0};
    std::size_t readyResources_{0};
    std::size_t errorResources_{0};
};
} // namespace trace2d::assets
