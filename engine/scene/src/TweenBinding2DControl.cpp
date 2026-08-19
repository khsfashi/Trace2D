#include <trace2d/scene/TweenBinding2D.hpp>

#include <algorithm>

namespace trace2d::scene
{
namespace
{
[[nodiscard]] bool IsValidValueType(const runtime::TweenValueType2D type) noexcept
{
    return type == runtime::TweenValueType2D::Float ||
        type == runtime::TweenValueType2D::Float2 ||
        type == runtime::TweenValueType2D::Color;
}

[[nodiscard]] bool IsValidStartMode(const TweenStartMode2D mode) noexcept
{
    return mode == TweenStartMode2D::Explicit || mode == TweenStartMode2D::CaptureCurrent;
}

[[nodiscard]] bool IsValidEndMode(const TweenEndMode2D mode) noexcept
{
    return mode == TweenEndMode2D::Absolute || mode == TweenEndMode2D::Relative;
}

[[nodiscard]] bool IsValidConflictPolicy(const TweenConflictPolicy2D policy) noexcept
{
    return policy == TweenConflictPolicy2D::Reject || policy == TweenConflictPolicy2D::Replace;
}

[[nodiscard]] runtime::TweenValue2D ZeroValue(const runtime::TweenValueType2D type) noexcept
{
    switch (type)
    {
    case runtime::TweenValueType2D::Float:
        return runtime::TweenValue2D::Float(0.0F);
    case runtime::TweenValueType2D::Float2:
        return runtime::TweenValue2D::Float2(0.0F, 0.0F);
    case runtime::TweenValueType2D::Color:
        return runtime::TweenValue2D::Color(0.0F, 0.0F, 0.0F, 0.0F);
    }
    return runtime::TweenValue2D{};
}

[[nodiscard]] runtime::TweenValue2D AddValue(
    const runtime::TweenValue2D& base,
    const runtime::TweenValue2D& delta) noexcept
{
    runtime::TweenValue2D result = base;
    if (base.type != delta.type || !IsValidValueType(base.type))
    {
        return result;
    }

    std::size_t componentCount = 1U;
    if (base.type == runtime::TweenValueType2D::Float2)
    {
        componentCount = 2U;
    }
    else if (base.type == runtime::TweenValueType2D::Color)
    {
        componentCount = 4U;
    }

    for (std::size_t index = 0U; index < componentCount; ++index)
    {
        result.components[index] += delta.components[index];
    }
    return result;
}
} // namespace

TweenBindingSystem2D::TweenBindingSystem2D(Scene& scene) noexcept
    : scene_{scene}
{
}

void TweenBindingSystem2D::Reserve(const std::size_t capacity)
{
    pool_.Reserve(capacity);
    bindings_.reserve(capacity);
}

void TweenBindingSystem2D::ReserveExternalProviders(const std::size_t capacity)
{
    externalProviders_.reserve(capacity);
}

TweenBindingStatus2D TweenBindingSystem2D::RegisterExternalProvider(
    const TweenExternalPropertyProvider2D provider,
    TweenExternalProviderHandle2D& outHandle)
{
    outHandle = TweenExternalProviderHandle2D{};
    if (provider.context == nullptr || provider.validate == nullptr ||
        provider.read == nullptr || provider.write == nullptr)
    {
        return {TweenBindingError2D::InvalidExternalProvider};
    }
    if (externalProviders_.size() >=
        static_cast<std::size_t>(InvalidTweenExternalProviderIndex2D))
    {
        return {TweenBindingError2D::InvalidExternalProvider};
    }

    outHandle.index = static_cast<std::uint32_t>(externalProviders_.size());
    externalProviders_.push_back(provider);
    return {};
}

TweenBindingStatus2D TweenBindingSystem2D::ResolveTransform(
    const EntityId entity,
    const TransformTweenProperty2D property,
    ResolvedTweenBinding2D& outBinding) const noexcept
{
    if (!scene_.Contains(entity))
    {
        return {TweenBindingError2D::EntityNotFound};
    }

    runtime::TweenValueType2D valueType{};
    switch (property)
    {
    case TransformTweenProperty2D::Position:
    case TransformTweenProperty2D::Scale:
        valueType = runtime::TweenValueType2D::Float2;
        break;
    case TransformTweenProperty2D::RotationRadians:
        valueType = runtime::TweenValueType2D::Float;
        break;
    default:
        return {TweenBindingError2D::InvalidBinding};
    }

    outBinding = ResolvedTweenBinding2D{
        entity,
        TweenBindingTargetKind2D::EntityTransform,
        InvalidComponentTypeIndex,
        static_cast<std::uint32_t>(property),
        valueType,
    };
    return {};
}

TweenBindingStatus2D TweenBindingSystem2D::ResolveComponent(
    const EntityId entity,
    const std::string_view componentTypeId,
    const std::string_view propertyName,
    ResolvedTweenBinding2D& outBinding) const noexcept
{
    if (!scene_.Contains(entity))
    {
        return {TweenBindingError2D::EntityNotFound};
    }
    const ComponentRegistry* const registry = scene_.Registry();
    if (registry == nullptr)
    {
        return {TweenBindingError2D::RegistryUnavailable};
    }
    const std::optional<ComponentTypeIndex> componentType = registry->FindIndexById(componentTypeId);
    if (!componentType.has_value())
    {
        return {TweenBindingError2D::ComponentTypeNotFound};
    }
    return ResolveComponentByIndex(entity, *componentType, propertyName, outBinding);
}

TweenBindingStatus2D TweenBindingSystem2D::ResolveExternal(
    const TweenExternalProviderHandle2D provider,
    const std::uint32_t targetSlot,
    const std::uint64_t targetGeneration,
    const std::uint32_t propertyIndex,
    const runtime::TweenValueType2D valueType,
    ResolvedTweenBinding2D& outBinding) const noexcept
{
    outBinding = ResolvedTweenBinding2D{};
    if (!provider.Valid() || targetGeneration == 0U || !IsValidValueType(valueType))
    {
        return {TweenBindingError2D::InvalidBinding};
    }
    const TweenExternalPropertyProvider2D* const resolvedProvider =
        FindExternalProvider(provider.index);
    if (resolvedProvider == nullptr)
    {
        return {TweenBindingError2D::ExternalProviderUnavailable};
    }
    if (!resolvedProvider->validate(
            resolvedProvider->context,
            targetSlot,
            targetGeneration,
            propertyIndex,
            valueType))
    {
        return {TweenBindingError2D::InvalidBinding};
    }

    outBinding.targetKind = TweenBindingTargetKind2D::ExternalProperty;
    outBinding.propertyIndex = propertyIndex;
    outBinding.valueType = valueType;
    outBinding.externalProviderIndex = provider.index;
    outBinding.externalTargetSlot = targetSlot;
    outBinding.externalTargetGeneration = targetGeneration;
    return {};
}

TweenBindingStatus2D TweenBindingSystem2D::Create(
    const ResolvedTweenBinding2D& binding,
    const TweenBindingSpec2D& spec,
    runtime::TweenHandle2D& outHandle)
{
    const TweenBindingStatus2D bindingStatus = ValidateBinding(binding);
    if (!bindingStatus.Succeeded())
    {
        return bindingStatus;
    }
    if (!IsValidStartMode(spec.startMode) || !IsValidEndMode(spec.endMode) ||
        !IsValidConflictPolicy(spec.conflictPolicy))
    {
        return {TweenBindingError2D::InvalidBinding};
    }
    if (spec.tween.end.type != binding.valueType ||
        (spec.startMode == TweenStartMode2D::Explicit && spec.tween.start.type != binding.valueType))
    {
        return {TweenBindingError2D::ValueTypeMismatch};
    }

    runtime::TweenSpec2D prepared = spec.tween;
    bool capturedAtCreate = false;
    if (spec.startMode == TweenStartMode2D::CaptureCurrent)
    {
        prepared.start = ZeroValue(binding.valueType);
        if (prepared.delay.count() == 0)
        {
            runtime::TweenValue2D captured{};
            if (!ReadBinding(binding, captured) || captured.type != binding.valueType)
            {
                return {TweenBindingError2D::InvalidBinding};
            }
            prepared.start = captured;
            capturedAtCreate = true;
        }
    }

    if (spec.endMode == TweenEndMode2D::Relative)
    {
        prepared.end = AddValue(prepared.start, spec.tween.end);
    }

    const runtime::Tween2DStatus validation = runtime::ValidateTweenSpec2D(prepared);
    if (!validation.Succeeded())
    {
        return Wrap(validation);
    }

    BindingSlot* const conflict = FindActiveConflict(binding);
    if (conflict != nullptr && spec.conflictPolicy == TweenConflictPolicy2D::Reject)
    {
        ++conflictRejectedCount_;
        return {TweenBindingError2D::ConflictRejected};
    }

    runtime::TweenHandle2D newHandle{};
    const runtime::Tween2DStatus createStatus = pool_.Create(prepared, newHandle);
    if (!createStatus.Succeeded())
    {
        return Wrap(createStatus);
    }

    if (conflict != nullptr)
    {
        const runtime::Tween2DStatus cancelStatus = pool_.Cancel(
            conflict->tween,
            runtime::TweenCancellationReason2D::Replaced);
        if (!cancelStatus.Succeeded())
        {
            (void)pool_.Cancel(newHandle);
            return Wrap(cancelStatus);
        }
        ++conflictReplacedCount_;
    }

    BindingSlot& slot = AcquireSlot(newHandle);
    slot.binding = binding;
    slot.authoredSpec = spec;
    slot.tween = newHandle;
    slot.lastAppliedValue = {};
    slot.capturePending = spec.startMode == TweenStartMode2D::CaptureCurrent &&
        prepared.delay.count() > 0;
    slot.hasAppliedValue = false;
    slot.occupied = true;
    ++createdCount_;
    if (capturedAtCreate)
    {
        ++capturedStartCount_;
    }

    outHandle = newHandle;
    return ApplyInitialValue(slot);
}

TweenBindingStatus2D TweenBindingSystem2D::Inspect(
    const runtime::TweenHandle2D handle,
    runtime::TweenState2D& outState) const noexcept
{
    return Wrap(pool_.Inspect(handle, outState));
}

TweenBindingStatus2D TweenBindingSystem2D::InspectBinding(
    const runtime::TweenHandle2D handle,
    ResolvedTweenBinding2D& outBinding) const noexcept
{
    runtime::TweenState2D state{};
    const runtime::Tween2DStatus tweenStatus = pool_.Inspect(handle, state);
    if (!tweenStatus.Succeeded())
    {
        return Wrap(tweenStatus);
    }
    const BindingSlot* const slot = FindSlot(handle);
    if (slot == nullptr)
    {
        return {TweenBindingError2D::InvalidBinding};
    }
    outBinding = slot->binding;
    return {};
}

TweenBindingStatus2D TweenBindingSystem2D::Pause(const runtime::TweenHandle2D handle) noexcept
{
    return Wrap(pool_.Pause(handle));
}

TweenBindingStatus2D TweenBindingSystem2D::Resume(const runtime::TweenHandle2D handle) noexcept
{
    return Wrap(pool_.Resume(handle));
}

TweenBindingStatus2D TweenBindingSystem2D::Restart(const runtime::TweenHandle2D handle) noexcept
{
    BindingSlot* const slot = FindSlot(handle);
    if (slot == nullptr)
    {
        return {TweenBindingError2D::InvalidBinding};
    }
    runtime::TweenState2D current{};
    const runtime::Tween2DStatus inspectStatus = pool_.Inspect(handle, current);
    if (!inspectStatus.Succeeded())
    {
        return Wrap(inspectStatus);
    }
    const TweenBindingStatus2D bindingStatus = ValidateBinding(slot->binding);
    if (!bindingStatus.Succeeded())
    {
        return bindingStatus;
    }

    BindingSlot* const conflict = FindActiveConflict(slot->binding, handle);
    if (conflict != nullptr && slot->authoredSpec.conflictPolicy == TweenConflictPolicy2D::Reject)
    {
        ++conflictRejectedCount_;
        return {TweenBindingError2D::ConflictRejected};
    }

    const runtime::Tween2DStatus restartStatus = pool_.Restart(handle);
    if (!restartStatus.Succeeded())
    {
        return Wrap(restartStatus);
    }
    if (conflict != nullptr)
    {
        const runtime::Tween2DStatus cancelStatus = pool_.Cancel(
            conflict->tween,
            runtime::TweenCancellationReason2D::Replaced);
        if (!cancelStatus.Succeeded())
        {
            (void)pool_.Cancel(handle);
            return Wrap(cancelStatus);
        }
        ++conflictReplacedCount_;
    }

    slot->capturePending = slot->authoredSpec.startMode == TweenStartMode2D::CaptureCurrent &&
        slot->authoredSpec.tween.delay.count() > 0;
    slot->hasAppliedValue = false;
    if (slot->authoredSpec.startMode == TweenStartMode2D::CaptureCurrent && !slot->capturePending)
    {
        const TweenBindingStatus2D captureStatus = PrepareCapture(*slot, handle);
        if (!captureStatus.Succeeded())
        {
            return captureStatus;
        }
    }
    return ApplyInitialValue(*slot);
}

TweenBindingStatus2D TweenBindingSystem2D::Cancel(const runtime::TweenHandle2D handle) noexcept
{
    return Wrap(pool_.Cancel(handle));
}

} // namespace trace2d::scene
