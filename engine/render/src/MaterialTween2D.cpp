#include <trace2d/render/MaterialTween2D.hpp>

#include <algorithm>
#include <array>

namespace trace2d::render
{
namespace
{
[[nodiscard]] bool IsPreparedBlockValid(const MaterialParameterBlock2D& block) noexcept
{
    if (block.layoutIdentity == InvalidMaterial2DIdentity ||
        block.valueIdentity == InvalidMaterial2DIdentity ||
        static_cast<std::size_t>(block.parameterCount) > MaximumMaterial2DParameters)
    {
        return false;
    }
    for (std::size_t index = 0U; index < static_cast<std::size_t>(block.parameterCount); ++index)
    {
        switch (block.types[index])
        {
        case MaterialParameterType2D::Float:
        case MaterialParameterType2D::Float2:
        case MaterialParameterType2D::Color:
            break;
        default:
            return false;
        }
    }
    return true;
}

[[nodiscard]] runtime::TweenValueType2D ToTweenType(
    const MaterialParameterType2D type) noexcept
{
    switch (type)
    {
    case MaterialParameterType2D::Float:
        return runtime::TweenValueType2D::Float;
    case MaterialParameterType2D::Float2:
        return runtime::TweenValueType2D::Float2;
    case MaterialParameterType2D::Color:
        return runtime::TweenValueType2D::Color;
    }
    return runtime::TweenValueType2D::Float;
}

[[nodiscard]] MaterialParameterValue2D ToMaterialValue(
    const runtime::TweenValue2D& value) noexcept
{
    switch (value.type)
    {
    case runtime::TweenValueType2D::Float:
        return MaterialFloat2D(value.components[0]);
    case runtime::TweenValueType2D::Float2:
        return MaterialFloat2D(value.components[0], value.components[1]);
    case runtime::TweenValueType2D::Color:
        return MaterialColor2D(
            value.components[0], value.components[1], value.components[2], value.components[3]);
    }
    return MaterialParameterValue2D{};
}
} // namespace

void MaterialTweenTargetPool2D::Reserve(const std::size_t capacity)
{
    slots_.reserve(capacity);
}

MaterialTweenStatus2D MaterialTweenTargetPool2D::Create(
    const MaterialParameterBlock2D& preparedBlock,
    MaterialTweenTargetHandle2D& outHandle)
{
    outHandle = MaterialTweenTargetHandle2D{};
    if (!IsPreparedBlockValid(preparedBlock))
    {
        return {MaterialTweenError2D::InvalidPreparedBlock};
    }

    std::size_t index = slots_.size();
    bool reused = false;
    for (std::size_t candidate = 0U; candidate < slots_.size(); ++candidate)
    {
        if (!slots_[candidate].occupied)
        {
            index = candidate;
            reused = true;
            break;
        }
    }
    if (index == slots_.size())
    {
        slots_.push_back(Slot{});
    }

    Slot& slot = slots_[index];
    slot.generation = NextGeneration(slot.generation);
    slot.block = preparedBlock;
    slot.occupied = true;

    ++activeTargetCount_;
    highWaterActiveTargetCount_ = std::max(highWaterActiveTargetCount_, activeTargetCount_);
    ++createdTargetCount_;
    if (reused)
    {
        ++reusedTargetSlotCount_;
    }

    outHandle = MaterialTweenTargetHandle2D{
        static_cast<std::uint32_t>(index),
        slot.generation,
    };
    return {};
}

MaterialTweenStatus2D MaterialTweenTargetPool2D::Destroy(
    const MaterialTweenTargetHandle2D handle) noexcept
{
    Slot* const slot = ResolveMutable(handle.index, handle.generation);
    if (slot == nullptr)
    {
        return {MaterialTweenError2D::InvalidTarget};
    }
    slot->occupied = false;
    slot->block = MaterialParameterBlock2D{};
    if (activeTargetCount_ > 0U)
    {
        --activeTargetCount_;
    }
    return {};
}

const MaterialParameterBlock2D* MaterialTweenTargetPool2D::Resolve(
    const MaterialTweenTargetHandle2D handle) const noexcept
{
    const Slot* const slot = ResolveSlot(handle.index, handle.generation);
    return slot == nullptr ? nullptr : &slot->block;
}

scene::TweenExternalPropertyProvider2D MaterialTweenTargetPool2D::ExternalProvider() noexcept
{
    return scene::TweenExternalPropertyProvider2D{
        this,
        &MaterialTweenTargetPool2D::ValidateExternal,
        &MaterialTweenTargetPool2D::ReadExternal,
        &MaterialTweenTargetPool2D::WriteExternal,
    };
}

MaterialTweenStatus2D MaterialTweenTargetPool2D::ResolveBinding(
    scene::TweenBindingSystem2D& tweens,
    const scene::TweenExternalProviderHandle2D provider,
    const MaterialTweenTargetHandle2D target,
    const MaterialParameterBinding2D parameterBinding,
    scene::ResolvedTweenBinding2D& outBinding) const noexcept
{
    outBinding = scene::ResolvedTweenBinding2D{};
    const Slot* const slot = ResolveSlot(target.index, target.generation);
    if (slot == nullptr)
    {
        return {MaterialTweenError2D::InvalidTarget};
    }
    if (parameterBinding.layoutIdentity != slot->block.layoutIdentity)
    {
        return {MaterialTweenError2D::BindingLayoutMismatch};
    }
    if (parameterBinding.slot >= slot->block.parameterCount)
    {
        return {MaterialTweenError2D::BindingSlotOutOfRange};
    }
    if (slot->block.types[parameterBinding.slot] != parameterBinding.type)
    {
        return {MaterialTweenError2D::BindingTypeMismatch};
    }

    const scene::TweenBindingStatus2D status = tweens.ResolveExternal(
        provider,
        target.index,
        target.generation,
        parameterBinding.slot,
        ToTweenType(parameterBinding.type),
        outBinding);
    return status.Succeeded() ? MaterialTweenStatus2D{} :
        MaterialTweenStatus2D{MaterialTweenError2D::TweenBindingFailure, status.error};
}

MaterialTweenTargetMetrics2D MaterialTweenTargetPool2D::Metrics() const noexcept
{
    return MaterialTweenTargetMetrics2D{
        activeTargetCount_,
        static_cast<std::uint64_t>(slots_.size()),
        static_cast<std::uint64_t>(slots_.capacity()),
        highWaterActiveTargetCount_,
        createdTargetCount_,
        reusedTargetSlotCount_,
        appliedWriteCount_,
    };
}

MaterialTweenTargetPool2D::Slot* MaterialTweenTargetPool2D::ResolveMutable(
    const std::uint32_t index,
    const std::uint64_t generation) noexcept
{
    if (static_cast<std::size_t>(index) >= slots_.size())
    {
        return nullptr;
    }
    Slot& slot = slots_[index];
    return slot.occupied && generation != 0U && slot.generation == generation ? &slot : nullptr;
}

const MaterialTweenTargetPool2D::Slot* MaterialTweenTargetPool2D::ResolveSlot(
    const std::uint32_t index,
    const std::uint64_t generation) const noexcept
{
    if (static_cast<std::size_t>(index) >= slots_.size())
    {
        return nullptr;
    }
    const Slot& slot = slots_[index];
    return slot.occupied && generation != 0U && slot.generation == generation ? &slot : nullptr;
}

std::uint64_t MaterialTweenTargetPool2D::NextGeneration(const std::uint64_t generation) noexcept
{
    return generation == std::numeric_limits<std::uint64_t>::max() ? 1U : generation + 1U;
}

bool MaterialTweenTargetPool2D::ValidateExternal(
    void* const context,
    const std::uint32_t targetSlot,
    const std::uint64_t targetGeneration,
    const std::uint32_t propertyIndex,
    const runtime::TweenValueType2D valueType) noexcept
{
    auto* const pool = static_cast<MaterialTweenTargetPool2D*>(context);
    if (pool == nullptr)
    {
        return false;
    }
    const Slot* const slot = pool->ResolveSlot(targetSlot, targetGeneration);
    return slot != nullptr && propertyIndex < slot->block.parameterCount &&
        ToTweenType(slot->block.types[propertyIndex]) == valueType;
}

bool MaterialTweenTargetPool2D::ReadExternal(
    void* const context,
    const std::uint32_t targetSlot,
    const std::uint64_t targetGeneration,
    const std::uint32_t propertyIndex,
    runtime::TweenValue2D& outValue) noexcept
{
    auto* const pool = static_cast<MaterialTweenTargetPool2D*>(context);
    if (pool == nullptr)
    {
        return false;
    }
    const Slot* const slot = pool->ResolveSlot(targetSlot, targetGeneration);
    if (slot == nullptr || propertyIndex >= slot->block.parameterCount)
    {
        return false;
    }

    const std::size_t offset =
        static_cast<std::size_t>(propertyIndex) * Material2DParameterSlotFloatCount;
    switch (slot->block.types[propertyIndex])
    {
    case MaterialParameterType2D::Float:
        outValue = runtime::TweenValue2D::Float(slot->block.packed[offset]);
        return true;
    case MaterialParameterType2D::Float2:
        outValue = runtime::TweenValue2D::Float2(
            slot->block.packed[offset], slot->block.packed[offset + 1U]);
        return true;
    case MaterialParameterType2D::Color:
        outValue = runtime::TweenValue2D::Color(
            slot->block.packed[offset],
            slot->block.packed[offset + 1U],
            slot->block.packed[offset + 2U],
            slot->block.packed[offset + 3U]);
        return true;
    }
    return false;
}

bool MaterialTweenTargetPool2D::WriteExternal(
    void* const context,
    const std::uint32_t targetSlot,
    const std::uint64_t targetGeneration,
    const std::uint32_t propertyIndex,
    const runtime::TweenValue2D& value) noexcept
{
    auto* const pool = static_cast<MaterialTweenTargetPool2D*>(context);
    if (pool == nullptr)
    {
        return false;
    }
    Slot* const slot = pool->ResolveMutable(targetSlot, targetGeneration);
    if (slot == nullptr || propertyIndex >= slot->block.parameterCount ||
        ToTweenType(slot->block.types[propertyIndex]) != value.type)
    {
        return false;
    }

    const MaterialParameterBinding2D binding{
        slot->block.layoutIdentity,
        slot->block.types[propertyIndex],
        static_cast<std::uint8_t>(propertyIndex),
    };
    const ResolvedMaterialParameterOverride2D overrideValue{
        binding,
        ToMaterialValue(value),
    };
    MaterialParameterBlock2D updated{};
    const std::array<ResolvedMaterialParameterOverride2D, 1U> overrides{overrideValue};
    if (!ApplyMaterialParameterOverrides2D(slot->block, overrides, updated).Succeeded())
    {
        return false;
    }

    slot->block = updated;
    ++pool->appliedWriteCount_;
    return true;
}
} // namespace trace2d::render
