#include <trace2d/render/Material2D.hpp>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace trace2d::render
{
namespace
{
constexpr std::uint64_t FnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t FnvPrime = 1099511628211ULL;

[[nodiscard]] bool IsAsciiAlpha(const char value) noexcept
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

[[nodiscard]] bool IsAsciiDigit(const char value) noexcept
{
    return value >= '0' && value <= '9';
}

[[nodiscard]] bool IsValidParameterName(const std::string_view name) noexcept
{
    if (name.empty())
    {
        return false;
    }
    if (!(IsAsciiAlpha(name.front()) || name.front() == '_'))
    {
        return false;
    }
    for (const char value : name.substr(1U))
    {
        if (!(IsAsciiAlpha(value) || IsAsciiDigit(value) || value == '_'))
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool IsSupportedType(const MaterialParameterType2D type) noexcept
{
    switch (type)
    {
    case MaterialParameterType2D::Float:
    case MaterialParameterType2D::Float2:
    case MaterialParameterType2D::Color:
        return true;
    }
    return false;
}

[[nodiscard]] std::size_t ActiveLaneCount(const MaterialParameterType2D type) noexcept
{
    switch (type)
    {
    case MaterialParameterType2D::Float:
        return 1U;
    case MaterialParameterType2D::Float2:
        return 2U;
    case MaterialParameterType2D::Color:
        return 4U;
    }
    return 0U;
}

void HashByte(std::uint64_t& hash, const std::uint8_t value) noexcept
{
    hash ^= value;
    hash *= FnvPrime;
}

void HashU32(std::uint64_t& hash, const std::uint32_t value) noexcept
{
    for (std::uint32_t shift = 0U; shift < 32U; shift += 8U)
    {
        HashByte(hash, static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void HashU64(std::uint64_t& hash, const std::uint64_t value) noexcept
{
    for (std::uint32_t shift = 0U; shift < 64U; shift += 8U)
    {
        HashByte(hash, static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void HashString(std::uint64_t& hash, const std::string_view value) noexcept
{
    HashU64(hash, static_cast<std::uint64_t>(value.size()));
    for (const char character : value)
    {
        HashByte(hash, static_cast<std::uint8_t>(character));
    }
}

[[nodiscard]] std::uint64_t FinishIdentity(const std::uint64_t hash) noexcept
{
    return hash == InvalidMaterial2DIdentity ? 1U : hash;
}

[[nodiscard]] std::uint64_t LayoutIdentity(
    const assets::ResourceHandleUntyped shaderIdentity,
    const std::span<const MaterialParameterDeclaration2D> declarations) noexcept
{
    std::uint64_t hash = FnvOffsetBasis;
    HashU32(hash, shaderIdentity.slot);
    HashU32(hash, shaderIdentity.generation);
    HashByte(hash, static_cast<std::uint8_t>(shaderIdentity.domain));
    HashU64(hash, static_cast<std::uint64_t>(declarations.size()));
    for (const MaterialParameterDeclaration2D& declaration : declarations)
    {
        HashString(hash, declaration.name);
        HashByte(hash, static_cast<std::uint8_t>(declaration.type));
    }
    return FinishIdentity(hash);
}

[[nodiscard]] std::uint64_t ValueIdentity(const MaterialParameterBlock2D& block) noexcept
{
    std::uint64_t hash = FnvOffsetBasis;
    HashU64(hash, block.layoutIdentity);
    HashByte(hash, block.parameterCount);
    const std::size_t activeFloatCount =
        static_cast<std::size_t>(block.parameterCount) * Material2DParameterSlotFloatCount;
    for (std::size_t index = 0U; index < activeFloatCount; ++index)
    {
        HashU32(hash, std::bit_cast<std::uint32_t>(block.packed[index]));
    }
    return FinishIdentity(hash);
}

[[nodiscard]] bool HasValidShaderIdentity(
    const assets::ResourceHandleUntyped shaderIdentity) noexcept
{
    return shaderIdentity.generation != 0U;
}

[[nodiscard]] bool HasValidPreparedLayout(const MaterialParameterLayout2D& layout) noexcept
{
    return HasValidShaderIdentity(layout.shaderIdentity) &&
        layout.identity != InvalidMaterial2DIdentity &&
        static_cast<std::size_t>(layout.parameterCount) <= MaximumMaterial2DParameters;
}

[[nodiscard]] bool IsFiniteValue(const MaterialParameterValue2D& value) noexcept
{
    const std::size_t activeLaneCount = ActiveLaneCount(value.type);
    if (activeLaneCount == 0U)
    {
        return false;
    }
    for (std::size_t lane = 0U; lane < activeLaneCount; ++lane)
    {
        if (!std::isfinite(value.lanes[lane]))
        {
            return false;
        }
    }
    return true;
}

void WriteCanonicalSlot(
    MaterialParameterBlock2D& block,
    const std::size_t slot,
    const MaterialParameterValue2D& value) noexcept
{
    const std::size_t offset = slot * Material2DParameterSlotFloatCount;
    for (std::size_t lane = 0U; lane < Material2DParameterSlotFloatCount; ++lane)
    {
        block.packed[offset + lane] = 0.0F;
    }
    const std::size_t activeLaneCount = ActiveLaneCount(value.type);
    for (std::size_t lane = 0U; lane < activeLaneCount; ++lane)
    {
        block.packed[offset + lane] = value.lanes[lane];
    }
}

[[nodiscard]] MaterialPrepareStatus2D Failure(
    const MaterialPrepareError2D error,
    const std::size_t parameterIndex = 0U) noexcept
{
    return MaterialPrepareStatus2D{
        error,
        static_cast<std::uint8_t>(
            parameterIndex <= static_cast<std::size_t>(UINT8_MAX) ? parameterIndex : UINT8_MAX)};
}
} // namespace

std::string_view ToString(const MaterialParameterType2D value) noexcept
{
    switch (value)
    {
    case MaterialParameterType2D::Float:
        return "float";
    case MaterialParameterType2D::Float2:
        return "float2";
    case MaterialParameterType2D::Color:
        return "color";
    }
    return "unknown";
}

std::string_view ToString(const MaterialPrepareError2D value) noexcept
{
    switch (value)
    {
    case MaterialPrepareError2D::None:
        return "none";
    case MaterialPrepareError2D::InvalidShaderIdentity:
        return "invalid_shader_identity";
    case MaterialPrepareError2D::TooManyParameters:
        return "too_many_parameters";
    case MaterialPrepareError2D::EmptyParameterName:
        return "empty_parameter_name";
    case MaterialPrepareError2D::InvalidParameterName:
        return "invalid_parameter_name";
    case MaterialPrepareError2D::DuplicateParameterName:
        return "duplicate_parameter_name";
    case MaterialPrepareError2D::UnsupportedParameterType:
        return "unsupported_parameter_type";
    case MaterialPrepareError2D::UnknownParameterName:
        return "unknown_parameter_name";
    case MaterialPrepareError2D::ParameterCountMismatch:
        return "parameter_count_mismatch";
    case MaterialPrepareError2D::ParameterTypeMismatch:
        return "parameter_type_mismatch";
    case MaterialPrepareError2D::NonFiniteParameterValue:
        return "non_finite_parameter_value";
    case MaterialPrepareError2D::InvalidPreparedLayout:
        return "invalid_prepared_layout";
    case MaterialPrepareError2D::BindingLayoutMismatch:
        return "binding_layout_mismatch";
    case MaterialPrepareError2D::BindingSlotOutOfRange:
        return "binding_slot_out_of_range";
    case MaterialPrepareError2D::DuplicateOverride:
        return "duplicate_override";
    }
    return "unknown";
}

MaterialPrepareStatus2D PrepareMaterialParameterLayout2D(
    const assets::ResourceHandleUntyped shaderIdentity,
    const std::span<const MaterialParameterDeclaration2D> declarations,
    MaterialParameterLayout2D& outLayout) noexcept
{
    outLayout = MaterialParameterLayout2D{};
    if (!HasValidShaderIdentity(shaderIdentity))
    {
        return Failure(MaterialPrepareError2D::InvalidShaderIdentity);
    }
    if (declarations.size() > MaximumMaterial2DParameters)
    {
        return Failure(MaterialPrepareError2D::TooManyParameters, declarations.size());
    }

    for (std::size_t index = 0U; index < declarations.size(); ++index)
    {
        const MaterialParameterDeclaration2D& declaration = declarations[index];
        if (declaration.name.empty())
        {
            return Failure(MaterialPrepareError2D::EmptyParameterName, index);
        }
        if (!IsValidParameterName(declaration.name))
        {
            return Failure(MaterialPrepareError2D::InvalidParameterName, index);
        }
        if (!IsSupportedType(declaration.type))
        {
            return Failure(MaterialPrepareError2D::UnsupportedParameterType, index);
        }
        for (std::size_t previous = 0U; previous < index; ++previous)
        {
            if (declarations[previous].name == declaration.name)
            {
                return Failure(MaterialPrepareError2D::DuplicateParameterName, index);
            }
        }
    }

    outLayout.shaderIdentity = shaderIdentity;
    outLayout.parameterCount = static_cast<std::uint8_t>(declarations.size());
    outLayout.identity = LayoutIdentity(shaderIdentity, declarations);
    for (std::size_t index = 0U; index < declarations.size(); ++index)
    {
        outLayout.names[index] = declarations[index].name;
        outLayout.entries[index] = MaterialParameterLayoutEntry2D{
            declarations[index].type,
            static_cast<std::uint8_t>(index)};
    }
    return {};
}

MaterialPrepareStatus2D ResolveMaterialParameterBinding2D(
    const MaterialParameterLayout2D& layout,
    const std::string_view name,
    MaterialParameterBinding2D& outBinding) noexcept
{
    outBinding = MaterialParameterBinding2D{};
    if (!HasValidPreparedLayout(layout))
    {
        return Failure(MaterialPrepareError2D::InvalidPreparedLayout);
    }

    for (std::size_t index = 0U; index < static_cast<std::size_t>(layout.parameterCount); ++index)
    {
        if (layout.names[index] == name)
        {
            outBinding = MaterialParameterBinding2D{
                layout.identity,
                layout.entries[index].type,
                layout.entries[index].slot};
            return {};
        }
    }
    return Failure(MaterialPrepareError2D::UnknownParameterName);
}

MaterialPrepareStatus2D PrepareMaterialParameterBlock2D(
    const MaterialParameterLayout2D& layout,
    const std::span<const MaterialParameterValue2D> defaults,
    MaterialParameterBlock2D& outBlock) noexcept
{
    outBlock = MaterialParameterBlock2D{};
    if (!HasValidPreparedLayout(layout))
    {
        return Failure(MaterialPrepareError2D::InvalidPreparedLayout);
    }
    if (defaults.size() != static_cast<std::size_t>(layout.parameterCount))
    {
        return Failure(MaterialPrepareError2D::ParameterCountMismatch, defaults.size());
    }

    outBlock.parameterCount = layout.parameterCount;
    outBlock.layoutIdentity = layout.identity;
    for (std::size_t index = 0U; index < defaults.size(); ++index)
    {
        const MaterialParameterType2D expectedType = layout.entries[index].type;
        const MaterialParameterValue2D& value = defaults[index];
        if (value.type != expectedType)
        {
            outBlock = MaterialParameterBlock2D{};
            return Failure(MaterialPrepareError2D::ParameterTypeMismatch, index);
        }
        if (!IsFiniteValue(value))
        {
            outBlock = MaterialParameterBlock2D{};
            return Failure(MaterialPrepareError2D::NonFiniteParameterValue, index);
        }
        outBlock.types[index] = expectedType;
        WriteCanonicalSlot(outBlock, index, value);
    }
    outBlock.valueIdentity = ValueIdentity(outBlock);
    return {};
}

MaterialPrepareStatus2D ApplyMaterialParameterOverrides2D(
    const MaterialParameterBlock2D& base,
    const std::span<const ResolvedMaterialParameterOverride2D> overrides,
    MaterialParameterBlock2D& outBlock) noexcept
{
    outBlock = MaterialParameterBlock2D{};
    if (base.layoutIdentity == InvalidMaterial2DIdentity ||
        base.valueIdentity == InvalidMaterial2DIdentity ||
        static_cast<std::size_t>(base.parameterCount) > MaximumMaterial2DParameters)
    {
        return Failure(MaterialPrepareError2D::InvalidPreparedLayout);
    }

    outBlock = base;
    std::uint32_t overriddenSlots = 0U;
    for (std::size_t index = 0U; index < overrides.size(); ++index)
    {
        const ResolvedMaterialParameterOverride2D& overrideValue = overrides[index];
        const MaterialParameterBinding2D& binding = overrideValue.binding;
        if (binding.layoutIdentity != base.layoutIdentity)
        {
            outBlock = MaterialParameterBlock2D{};
            return Failure(MaterialPrepareError2D::BindingLayoutMismatch, index);
        }
        if (binding.slot >= base.parameterCount)
        {
            outBlock = MaterialParameterBlock2D{};
            return Failure(MaterialPrepareError2D::BindingSlotOutOfRange, index);
        }

        const std::uint32_t slotBit = 1U << binding.slot;
        if ((overriddenSlots & slotBit) != 0U)
        {
            outBlock = MaterialParameterBlock2D{};
            return Failure(MaterialPrepareError2D::DuplicateOverride, index);
        }
        overriddenSlots |= slotBit;

        const MaterialParameterType2D expectedType = base.types[binding.slot];
        if (binding.type != expectedType || overrideValue.value.type != expectedType)
        {
            outBlock = MaterialParameterBlock2D{};
            return Failure(MaterialPrepareError2D::ParameterTypeMismatch, index);
        }
        if (!IsFiniteValue(overrideValue.value))
        {
            outBlock = MaterialParameterBlock2D{};
            return Failure(MaterialPrepareError2D::NonFiniteParameterValue, index);
        }
        WriteCanonicalSlot(outBlock, binding.slot, overrideValue.value);
    }

    outBlock.valueIdentity = ValueIdentity(outBlock);
    return {};
}
} // namespace trace2d::render
