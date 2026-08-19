#pragma once

#include <trace2d/runtime/Tween2D.hpp>
#include <trace2d/scene/Scene.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace trace2d::scene
{
enum class TransformTweenProperty2D : std::uint8_t
{
    Position = 0,
    RotationRadians,
    Scale,
};

enum class TweenBindingTargetKind2D : std::uint8_t
{
    EntityTransform = 0,
    ComponentProperty,
    ExternalProperty,
};

inline constexpr std::uint32_t InvalidTweenExternalProviderIndex2D =
    std::numeric_limits<std::uint32_t>::max();

struct TweenExternalProviderHandle2D final
{
    std::uint32_t index{InvalidTweenExternalProviderIndex2D};

    [[nodiscard]] bool Valid() const noexcept
    {
        return index != InvalidTweenExternalProviderIndex2D;
    }

    [[nodiscard]] bool operator==(const TweenExternalProviderHandle2D&) const noexcept = default;
};

// Setup-time registered adapter for a retained, generation-safe typed presentation target. The
// provider object is copied into TweenBindingSystem2D; context must outlive that binding system.
// Property-name resolution stays provider-owned setup work. Steady stepping receives compact
// provider/target/property indices only and never executes arbitrary timeline completion callbacks.
struct TweenExternalPropertyProvider2D final
{
    using ValidateFunction = bool (*)(
        void* context,
        std::uint32_t targetSlot,
        std::uint64_t targetGeneration,
        std::uint32_t propertyIndex,
        runtime::TweenValueType2D valueType) noexcept;
    using ReadFunction = bool (*)(
        void* context,
        std::uint32_t targetSlot,
        std::uint64_t targetGeneration,
        std::uint32_t propertyIndex,
        runtime::TweenValue2D& outValue) noexcept;
    using WriteFunction = bool (*)(
        void* context,
        std::uint32_t targetSlot,
        std::uint64_t targetGeneration,
        std::uint32_t propertyIndex,
        const runtime::TweenValue2D& value) noexcept;

    void* context{nullptr};
    ValidateFunction validate{nullptr};
    ReadFunction read{nullptr};
    WriteFunction write{nullptr};
};

struct ResolvedTweenBinding2D final
{
    EntityId entity{};
    TweenBindingTargetKind2D targetKind{TweenBindingTargetKind2D::EntityTransform};
    ComponentTypeIndex componentType{InvalidComponentTypeIndex};
    std::uint32_t propertyIndex{0U};
    runtime::TweenValueType2D valueType{runtime::TweenValueType2D::Float};

    // ExternalProperty uses deterministic setup-order provider identity plus target slot/generation.
    // Entity/component fields remain unused for this target kind.
    std::uint32_t externalProviderIndex{InvalidTweenExternalProviderIndex2D};
    std::uint32_t externalTargetSlot{0U};
    std::uint64_t externalTargetGeneration{0U};

    [[nodiscard]] bool operator==(const ResolvedTweenBinding2D&) const noexcept = default;
};

enum class TweenStartMode2D : std::uint8_t
{
    Explicit = 0,
    CaptureCurrent,
};

enum class TweenEndMode2D : std::uint8_t
{
    Absolute = 0,
    Relative,
};

enum class TweenConflictPolicy2D : std::uint8_t
{
    Reject = 0,
    Replace,
};

struct TweenBindingSpec2D final
{
    runtime::TweenSpec2D tween{};
    TweenStartMode2D startMode{TweenStartMode2D::Explicit};
    TweenEndMode2D endMode{TweenEndMode2D::Absolute};
    TweenConflictPolicy2D conflictPolicy{TweenConflictPolicy2D::Reject};
};

enum class TweenBindingError2D : std::uint8_t
{
    None = 0,
    RegistryUnavailable,
    InvalidBinding,
    EntityNotFound,
    ComponentTypeNotFound,
    ComponentNotFound,
    PropertyNotFound,
    ValueTypeMismatch,
    ConflictRejected,
    TweenFailure,
    PropertyWriteRejected,
    InvalidExternalProvider,
    ExternalProviderUnavailable,
};

struct TweenBindingStatus2D final
{
    TweenBindingError2D error{TweenBindingError2D::None};
    runtime::Tween2DError tweenError{runtime::Tween2DError::None};

    [[nodiscard]] bool Succeeded() const noexcept { return error == TweenBindingError2D::None; }
    [[nodiscard]] bool operator==(const TweenBindingStatus2D&) const noexcept = default;
};

struct TweenBindingMetrics2D final
{
    std::uint64_t createdCount{0U};
    std::uint64_t appliedWriteCount{0U};
    std::uint64_t capturedStartCount{0U};
    std::uint64_t conflictRejectedCount{0U};
    std::uint64_t conflictReplacedCount{0U};
    std::uint64_t targetInvalidatedCount{0U};
    std::uint64_t propertyWriteRejectedCount{0U};
    std::uint64_t retainedBindingSlotCount{0U};
    std::uint64_t retainedBindingCapacity{0U};
    std::uint64_t retainedExternalProviderCount{0U};
    std::uint64_t retainedExternalProviderCapacity{0U};

    [[nodiscard]] bool operator==(const TweenBindingMetrics2D&) const noexcept = default;
};

class TweenBindingSystem2D final
{
public:
    explicit TweenBindingSystem2D(Scene& scene) noexcept;

    void Reserve(std::size_t capacity);
    void ReserveExternalProviders(std::size_t capacity);

    // Provider registration is setup-only. Provider indices are stable for the lifetime of this
    // binding system and are never removed/reordered, so resolved bindings stay deterministic.
    [[nodiscard]] TweenBindingStatus2D RegisterExternalProvider(
        TweenExternalPropertyProvider2D provider,
        TweenExternalProviderHandle2D& outHandle);

    [[nodiscard]] TweenBindingStatus2D ResolveTransform(
        EntityId entity,
        TransformTweenProperty2D property,
        ResolvedTweenBinding2D& outBinding) const noexcept;

    template <typename T>
    [[nodiscard]] TweenBindingStatus2D ResolveComponent(
        EntityId entity,
        ComponentTypeHandle<T> componentType,
        std::string_view propertyName,
        ResolvedTweenBinding2D& outBinding) const noexcept
    {
        if (!scene_.Contains(entity))
        {
            return {TweenBindingError2D::EntityNotFound};
        }
        if (scene_.Registry() == nullptr)
        {
            return {TweenBindingError2D::RegistryUnavailable};
        }
        if (scene_.TryGetComponent(entity, componentType) == nullptr)
        {
            return {TweenBindingError2D::ComponentNotFound};
        }
        return ResolveComponentByIndex(entity, componentType.Index(), propertyName, outBinding);
    }

    [[nodiscard]] TweenBindingStatus2D ResolveComponent(
        EntityId entity,
        std::string_view componentTypeId,
        std::string_view propertyName,
        ResolvedTweenBinding2D& outBinding) const noexcept;

    [[nodiscard]] TweenBindingStatus2D ResolveExternal(
        TweenExternalProviderHandle2D provider,
        std::uint32_t targetSlot,
        std::uint64_t targetGeneration,
        std::uint32_t propertyIndex,
        runtime::TweenValueType2D valueType,
        ResolvedTweenBinding2D& outBinding) const noexcept;

    [[nodiscard]] TweenBindingStatus2D Create(
        const ResolvedTweenBinding2D& binding,
        const TweenBindingSpec2D& spec,
        runtime::TweenHandle2D& outHandle);
    [[nodiscard]] TweenBindingStatus2D Inspect(
        runtime::TweenHandle2D handle,
        runtime::TweenState2D& outState) const noexcept;
    [[nodiscard]] TweenBindingStatus2D InspectBinding(
        runtime::TweenHandle2D handle,
        ResolvedTweenBinding2D& outBinding) const noexcept;
    [[nodiscard]] TweenBindingStatus2D Pause(runtime::TweenHandle2D handle) noexcept;
    [[nodiscard]] TweenBindingStatus2D Resume(runtime::TweenHandle2D handle) noexcept;
    [[nodiscard]] TweenBindingStatus2D Restart(runtime::TweenHandle2D handle) noexcept;
    [[nodiscard]] TweenBindingStatus2D Cancel(runtime::TweenHandle2D handle) noexcept;
    [[nodiscard]] TweenBindingStatus2D Step(
        runtime::TweenTimeDomain2D domain,
        runtime::TweenTime2D delta) noexcept;

    [[nodiscard]] TweenBindingMetrics2D Metrics() const noexcept;
    [[nodiscard]] runtime::TweenPoolMetrics2D PoolMetrics() const noexcept;

private:
    struct BindingSlot final
    {
        ResolvedTweenBinding2D binding{};
        TweenBindingSpec2D authoredSpec{};
        runtime::TweenHandle2D tween{};
        runtime::TweenValue2D lastAppliedValue{};
        bool capturePending{false};
        bool hasAppliedValue{false};
        bool occupied{false};
    };

    [[nodiscard]] TweenBindingStatus2D ResolveComponentByIndex(
        EntityId entity,
        ComponentTypeIndex componentType,
        std::string_view propertyName,
        ResolvedTweenBinding2D& outBinding) const noexcept;
    [[nodiscard]] TweenBindingStatus2D ValidateBinding(
        const ResolvedTweenBinding2D& binding) const noexcept;
    [[nodiscard]] const TweenExternalPropertyProvider2D* FindExternalProvider(
        std::uint32_t providerIndex) const noexcept;
    [[nodiscard]] const ComponentInstance* FindComponent(
        const ResolvedTweenBinding2D& binding) const noexcept;
    [[nodiscard]] ComponentInstance* FindComponent(
        const ResolvedTweenBinding2D& binding) noexcept;
    [[nodiscard]] bool ReadBinding(
        const ResolvedTweenBinding2D& binding,
        runtime::TweenValue2D& outValue) const noexcept;
    [[nodiscard]] bool WriteBinding(
        const ResolvedTweenBinding2D& binding,
        const runtime::TweenValue2D& value) noexcept;
    [[nodiscard]] BindingSlot* FindSlot(runtime::TweenHandle2D handle) noexcept;
    [[nodiscard]] const BindingSlot* FindSlot(runtime::TweenHandle2D handle) const noexcept;
    [[nodiscard]] BindingSlot* FindActiveConflict(
        const ResolvedTweenBinding2D& binding,
        runtime::TweenHandle2D except = {}) noexcept;
    [[nodiscard]] BindingSlot& AcquireSlot(runtime::TweenHandle2D newHandle);
    [[nodiscard]] TweenBindingStatus2D PrepareCapture(
        BindingSlot& slot,
        runtime::TweenHandle2D handle) noexcept;
    [[nodiscard]] TweenBindingStatus2D ApplyInitialValue(BindingSlot& slot) noexcept;
    [[nodiscard]] TweenBindingStatus2D Wrap(runtime::Tween2DStatus status) const noexcept;

    Scene& scene_;
    runtime::TweenPool2D pool_{};
    std::vector<BindingSlot> bindings_{};
    std::vector<TweenExternalPropertyProvider2D> externalProviders_{};
    std::uint64_t createdCount_{0U};
    std::uint64_t appliedWriteCount_{0U};
    std::uint64_t capturedStartCount_{0U};
    std::uint64_t conflictRejectedCount_{0U};
    std::uint64_t conflictReplacedCount_{0U};
    std::uint64_t targetInvalidatedCount_{0U};
    std::uint64_t propertyWriteRejectedCount_{0U};
};
} // namespace trace2d::scene
