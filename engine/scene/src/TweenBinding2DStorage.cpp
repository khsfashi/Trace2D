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

[[nodiscard]] bool IsTerminal(const runtime::TweenPlaybackState2D state) noexcept
{
    return state == runtime::TweenPlaybackState2D::Completed ||
        state == runtime::TweenPlaybackState2D::Cancelled;
}

[[nodiscard]] runtime::TweenValue2D AddValue(
    const runtime::TweenValue2D& base,
    const runtime::TweenValue2D& delta) noexcept
{
    runtime::TweenValue2D result = base;
    if (base.type != delta.type || !IsValidValueType(base.type)) return result;
    std::size_t componentCount = 1U;
    if (base.type == runtime::TweenValueType2D::Float2) componentCount = 2U;
    else if (base.type == runtime::TweenValueType2D::Color) componentCount = 4U;
    for (std::size_t index = 0U; index < componentCount; ++index)
        result.components[index] += delta.components[index];
    return result;
}
} // namespace

TweenBindingStatus2D TweenBindingSystem2D::ResolveComponentByIndex(
    const EntityId entity,
    const ComponentTypeIndex componentType,
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
    const ComponentTypeDescriptor* const descriptor = registry->Descriptor(componentType);
    if (descriptor == nullptr)
    {
        return {TweenBindingError2D::ComponentTypeNotFound};
    }

    ResolvedTweenBinding2D probe{
        entity,
        TweenBindingTargetKind2D::ComponentProperty,
        componentType,
        0U,
        runtime::TweenValueType2D::Float,
    };
    if (FindComponent(probe) == nullptr)
    {
        return {TweenBindingError2D::ComponentNotFound};
    }

    for (std::size_t index = 0U; index < descriptor->TweenPropertyCount(); ++index)
    {
        if (descriptor->TweenPropertyName(index) == propertyName)
        {
            outBinding = ResolvedTweenBinding2D{
                entity,
                TweenBindingTargetKind2D::ComponentProperty,
                componentType,
                static_cast<std::uint32_t>(index),
                descriptor->TweenPropertyValueType(index),
            };
            return {};
        }
    }
    return {TweenBindingError2D::PropertyNotFound};
}

const TweenExternalPropertyProvider2D* TweenBindingSystem2D::FindExternalProvider(
    const std::uint32_t providerIndex) const noexcept
{
    return providerIndex < externalProviders_.size() ? &externalProviders_[providerIndex] : nullptr;
}

TweenBindingStatus2D TweenBindingSystem2D::ValidateBinding(
    const ResolvedTweenBinding2D& binding) const noexcept
{
    if (!IsValidValueType(binding.valueType))
    {
        return {TweenBindingError2D::InvalidBinding};
    }

    if (binding.targetKind == TweenBindingTargetKind2D::ExternalProperty)
    {
        if (binding.externalTargetGeneration == 0U ||
            binding.externalProviderIndex == InvalidTweenExternalProviderIndex2D ||
            binding.componentType != InvalidComponentTypeIndex)
        {
            return {TweenBindingError2D::InvalidBinding};
        }
        const TweenExternalPropertyProvider2D* const provider =
            FindExternalProvider(binding.externalProviderIndex);
        if (provider == nullptr || provider->validate == nullptr)
        {
            return {TweenBindingError2D::ExternalProviderUnavailable};
        }
        return provider->validate(
                   provider->context,
                   binding.externalTargetSlot,
                   binding.externalTargetGeneration,
                   binding.propertyIndex,
                   binding.valueType) ?
            TweenBindingStatus2D{} : TweenBindingStatus2D{TweenBindingError2D::InvalidBinding};
    }

    if (!scene_.Contains(binding.entity))
    {
        return {TweenBindingError2D::EntityNotFound};
    }

    if (binding.targetKind == TweenBindingTargetKind2D::EntityTransform)
    {
        const auto property = static_cast<TransformTweenProperty2D>(binding.propertyIndex);
        runtime::TweenValueType2D expected{};
        switch (property)
        {
        case TransformTweenProperty2D::Position:
        case TransformTweenProperty2D::Scale:
            expected = runtime::TweenValueType2D::Float2;
            break;
        case TransformTweenProperty2D::RotationRadians:
            expected = runtime::TweenValueType2D::Float;
            break;
        default:
            return {TweenBindingError2D::InvalidBinding};
        }
        if (binding.componentType != InvalidComponentTypeIndex)
        {
            return {TweenBindingError2D::InvalidBinding};
        }
        return expected == binding.valueType ? TweenBindingStatus2D{} :
            TweenBindingStatus2D{TweenBindingError2D::ValueTypeMismatch};
    }

    if (binding.targetKind != TweenBindingTargetKind2D::ComponentProperty)
    {
        return {TweenBindingError2D::InvalidBinding};
    }
    const ComponentInstance* const instance = FindComponent(binding);
    if (instance == nullptr || instance->descriptor_ == nullptr)
    {
        return {TweenBindingError2D::ComponentNotFound};
    }
    if (binding.propertyIndex >= instance->descriptor_->TweenPropertyCount())
    {
        return {TweenBindingError2D::PropertyNotFound};
    }
    return instance->descriptor_->TweenPropertyValueType(binding.propertyIndex) == binding.valueType ?
        TweenBindingStatus2D{} : TweenBindingStatus2D{TweenBindingError2D::ValueTypeMismatch};
}

const ComponentInstance* TweenBindingSystem2D::FindComponent(
    const ResolvedTweenBinding2D& binding) const noexcept
{
    const Entity* const entity = static_cast<const Scene&>(scene_).TryGet(binding.entity);
    if (entity == nullptr || binding.componentType == InvalidComponentTypeIndex)
    {
        return nullptr;
    }
    const auto iterator = std::lower_bound(
        entity->components_.begin(), entity->components_.end(), binding.componentType,
        [](const ComponentInstance& instance, const ComponentTypeIndex index)
        {
            return instance.index_ < index;
        });
    return iterator == entity->components_.end() || iterator->index_ != binding.componentType ?
        nullptr : &*iterator;
}

ComponentInstance* TweenBindingSystem2D::FindComponent(
    const ResolvedTweenBinding2D& binding) noexcept
{
    Entity* const entity = scene_.TryGet(binding.entity);
    if (entity == nullptr || binding.componentType == InvalidComponentTypeIndex)
    {
        return nullptr;
    }
    const auto iterator = std::lower_bound(
        entity->components_.begin(), entity->components_.end(), binding.componentType,
        [](const ComponentInstance& instance, const ComponentTypeIndex index)
        {
            return instance.index_ < index;
        });
    return iterator == entity->components_.end() || iterator->index_ != binding.componentType ?
        nullptr : &*iterator;
}

bool TweenBindingSystem2D::ReadBinding(
    const ResolvedTweenBinding2D& binding,
    runtime::TweenValue2D& outValue) const noexcept
{
    if (binding.targetKind == TweenBindingTargetKind2D::ExternalProperty)
    {
        const TweenExternalPropertyProvider2D* const provider =
            FindExternalProvider(binding.externalProviderIndex);
        return provider != nullptr && provider->read != nullptr && provider->read(
            provider->context,
            binding.externalTargetSlot,
            binding.externalTargetGeneration,
            binding.propertyIndex,
            outValue);
    }

    if (binding.targetKind == TweenBindingTargetKind2D::EntityTransform)
    {
        const Entity* const entity = static_cast<const Scene&>(scene_).TryGet(binding.entity);
        if (entity == nullptr)
        {
            return false;
        }
        const Transform2D& transform = entity->LocalTransform();
        switch (static_cast<TransformTweenProperty2D>(binding.propertyIndex))
        {
        case TransformTweenProperty2D::Position:
            outValue = runtime::TweenValue2D::Float2(transform.position.x, transform.position.y);
            return true;
        case TransformTweenProperty2D::RotationRadians:
            outValue = runtime::TweenValue2D::Float(transform.rotationRadians);
            return true;
        case TransformTweenProperty2D::Scale:
            outValue = runtime::TweenValue2D::Float2(transform.scale.x, transform.scale.y);
            return true;
        }
        return false;
    }

    const ComponentInstance* const instance = FindComponent(binding);
    return instance != nullptr && instance->descriptor_ != nullptr &&
        instance->descriptor_->ReadTweenProperty(instance->data_, binding.propertyIndex, outValue);
}

bool TweenBindingSystem2D::WriteBinding(
    const ResolvedTweenBinding2D& binding,
    const runtime::TweenValue2D& value) noexcept
{
    if (value.type != binding.valueType)
    {
        return false;
    }

    if (binding.targetKind == TweenBindingTargetKind2D::ExternalProperty)
    {
        const TweenExternalPropertyProvider2D* const provider =
            FindExternalProvider(binding.externalProviderIndex);
        return provider != nullptr && provider->write != nullptr && provider->write(
            provider->context,
            binding.externalTargetSlot,
            binding.externalTargetGeneration,
            binding.propertyIndex,
            value);
    }

    if (binding.targetKind == TweenBindingTargetKind2D::EntityTransform)
    {
        Entity* const entity = scene_.TryGet(binding.entity);
        if (entity == nullptr)
        {
            return false;
        }
        Transform2D& transform = entity->LocalTransform();
        switch (static_cast<TransformTweenProperty2D>(binding.propertyIndex))
        {
        case TransformTweenProperty2D::Position:
            transform.position = {value.components[0], value.components[1]};
            return true;
        case TransformTweenProperty2D::RotationRadians:
            transform.rotationRadians = value.components[0];
            return true;
        case TransformTweenProperty2D::Scale:
            transform.scale = {value.components[0], value.components[1]};
            return true;
        }
        return false;
    }

    ComponentInstance* const instance = FindComponent(binding);
    return instance != nullptr && instance->descriptor_ != nullptr &&
        instance->descriptor_->WriteTweenProperty(instance->data_, binding.propertyIndex, value);
}

TweenBindingSystem2D::BindingSlot* TweenBindingSystem2D::FindSlot(
    const runtime::TweenHandle2D handle) noexcept
{
    for (BindingSlot& slot : bindings_)
    {
        if (slot.occupied && slot.tween == handle)
        {
            return &slot;
        }
    }
    return nullptr;
}

const TweenBindingSystem2D::BindingSlot* TweenBindingSystem2D::FindSlot(
    const runtime::TweenHandle2D handle) const noexcept
{
    for (const BindingSlot& slot : bindings_)
    {
        if (slot.occupied && slot.tween == handle)
        {
            return &slot;
        }
    }
    return nullptr;
}

TweenBindingSystem2D::BindingSlot* TweenBindingSystem2D::FindActiveConflict(
    const ResolvedTweenBinding2D& binding,
    const runtime::TweenHandle2D except) noexcept
{
    for (BindingSlot& slot : bindings_)
    {
        if (!slot.occupied || slot.tween == except || slot.binding != binding)
        {
            continue;
        }
        runtime::TweenState2D state{};
        if (pool_.Inspect(slot.tween, state).Succeeded() && !IsTerminal(state.playback))
        {
            return &slot;
        }
    }
    return nullptr;
}

TweenBindingSystem2D::BindingSlot& TweenBindingSystem2D::AcquireSlot(
    const runtime::TweenHandle2D newHandle)
{
    for (BindingSlot& slot : bindings_)
    {
        if (!slot.occupied)
        {
            return slot;
        }
        runtime::TweenState2D state{};
        if (!pool_.Inspect(slot.tween, state).Succeeded() && slot.tween != newHandle)
        {
            return slot;
        }
    }
    bindings_.push_back(BindingSlot{});
    return bindings_.back();
}

TweenBindingStatus2D TweenBindingSystem2D::PrepareCapture(
    BindingSlot& slot,
    const runtime::TweenHandle2D handle) noexcept
{
    runtime::TweenValue2D captured{};
    if (!ReadBinding(slot.binding, captured) || captured.type != slot.binding.valueType)
    {
        return {TweenBindingError2D::InvalidBinding};
    }

    runtime::TweenValue2D end = slot.authoredSpec.tween.end;
    if (slot.authoredSpec.endMode == TweenEndMode2D::Relative)
    {
        end = AddValue(captured, slot.authoredSpec.tween.end);
    }
    const runtime::Tween2DStatus rebaseStatus = pool_.Rebase(handle, captured, end);
    if (!rebaseStatus.Succeeded())
    {
        return Wrap(rebaseStatus);
    }
    slot.capturePending = false;
    ++capturedStartCount_;
    return {};
}

TweenBindingStatus2D TweenBindingSystem2D::ApplyInitialValue(BindingSlot& slot) noexcept
{
    runtime::TweenState2D state{};
    const runtime::Tween2DStatus inspectStatus = pool_.Inspect(slot.tween, state);
    if (!inspectStatus.Succeeded())
    {
        return Wrap(inspectStatus);
    }
    if (state.delayElapsed < slot.authoredSpec.tween.delay || slot.capturePending)
    {
        return {};
    }
    if (!WriteBinding(slot.binding, state.currentValue))
    {
        (void)pool_.Cancel(
            slot.tween,
            runtime::TweenCancellationReason2D::PropertyWriteRejected);
        ++propertyWriteRejectedCount_;
        return {TweenBindingError2D::PropertyWriteRejected};
    }
    slot.lastAppliedValue = state.currentValue;
    slot.hasAppliedValue = true;
    ++appliedWriteCount_;
    return {};
}

TweenBindingStatus2D TweenBindingSystem2D::Wrap(const runtime::Tween2DStatus status) const noexcept
{
    return status.Succeeded() ? TweenBindingStatus2D{} :
        TweenBindingStatus2D{TweenBindingError2D::TweenFailure, status.error};
}
} // namespace trace2d::scene
