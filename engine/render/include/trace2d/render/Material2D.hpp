#pragma once

#include <trace2d/assets/ResourceRegistry.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

namespace trace2d::render
{
// MAT2 promotes the authored numeric vocabulary/capacity into the canonical #86 resource layer.
// Keep these aliases so the prepared renderer API remains source-compatible while there is exactly
// one type/capacity authority shared by authored Material2D and steady-state prepared blocks.
inline constexpr std::size_t MaximumMaterial2DParameters = assets::MaximumMaterial2DParameters;
inline constexpr std::size_t MaximumMaterial2DParameterNameBytes =
    assets::MaximumMaterial2DParameterNameBytes;
inline constexpr std::size_t Material2DParameterSlotFloatCount =
    assets::Material2DParameterSlotFloatCount;
inline constexpr std::size_t Material2DParameterSlotBytes =
    Material2DParameterSlotFloatCount * sizeof(float);
inline constexpr std::uint64_t InvalidMaterial2DIdentity = 0U;

using MaterialParameterType2D = assets::MaterialParameterType2D;
using MaterialParameterValue2D = assets::MaterialParameterValue2D;

[[nodiscard]] std::string_view ToString(MaterialParameterType2D value) noexcept;

struct MaterialParameterDeclaration2D final
{
    std::string_view name{};
    MaterialParameterType2D type{MaterialParameterType2D::Float};

    [[nodiscard]] bool operator==(const MaterialParameterDeclaration2D&) const noexcept = default;
};

[[nodiscard]] constexpr MaterialParameterValue2D MaterialFloat2D(const float value) noexcept
{
    return MaterialParameterValue2D{MaterialParameterType2D::Float, {value, 0.0F, 0.0F, 0.0F}};
}

[[nodiscard]] constexpr MaterialParameterValue2D MaterialFloat2D(
    const float x,
    const float y) noexcept
{
    return MaterialParameterValue2D{MaterialParameterType2D::Float2, {x, y, 0.0F, 0.0F}};
}

[[nodiscard]] constexpr MaterialParameterValue2D MaterialColor2D(
    const float red,
    const float green,
    const float blue,
    const float alpha) noexcept
{
    return MaterialParameterValue2D{MaterialParameterType2D::Color, {red, green, blue, alpha}};
}

struct MaterialParameterLayoutEntry2D final
{
    MaterialParameterType2D type{MaterialParameterType2D::Float};
    std::uint8_t slot{0U};

    [[nodiscard]] bool operator==(const MaterialParameterLayoutEntry2D&) const noexcept = default;
};

// Fixed-capacity owned setup/tooling name. The prepared layout never keeps a string_view into
// authored/canonical storage, so unloading or replacing the source resource cannot leave dangling
// inspection/name-resolution state.
struct MaterialParameterName2D final
{
    std::array<char, MaximumMaterial2DParameterNameBytes + 1U> bytes{};
    std::uint8_t length{0U};

    [[nodiscard]] std::string_view View() const noexcept
    {
        return std::string_view{bytes.data(), length};
    }

    [[nodiscard]] bool operator==(const MaterialParameterName2D&) const noexcept = default;
};

// Setup-time prepared Shader2D parameter layout. Names are copied into fixed-capacity owned storage
// only for setup/tooling lookup and inspection. Steady draw paths consume compact bindings/blocks.
struct MaterialParameterLayout2D final
{
    assets::ResourceHandleUntyped shaderIdentity{};
    std::array<MaterialParameterName2D, MaximumMaterial2DParameters> names{};
    std::array<MaterialParameterLayoutEntry2D, MaximumMaterial2DParameters> entries{};
    std::uint8_t parameterCount{0U};
    std::uint64_t identity{InvalidMaterial2DIdentity};

    [[nodiscard]] bool operator==(const MaterialParameterLayout2D&) const noexcept = default;
};

struct MaterialParameterBinding2D final
{
    std::uint64_t layoutIdentity{InvalidMaterial2DIdentity};
    MaterialParameterType2D type{MaterialParameterType2D::Float};
    std::uint8_t slot{0U};

    [[nodiscard]] bool operator==(const MaterialParameterBinding2D&) const noexcept = default;
};

// Fixed-capacity std140-safe material parameter payload. Every authored parameter consumes one
// float4-sized 16-byte slot. This deliberately wastes a small bounded amount of space so layout is
// deterministic across backends and no per-instance heap container is required.
struct MaterialParameterBlock2D final
{
    std::array<float, MaximumMaterial2DParameters * Material2DParameterSlotFloatCount> packed{};
    std::array<MaterialParameterType2D, MaximumMaterial2DParameters> types{};
    std::uint8_t parameterCount{0U};
    std::uint64_t layoutIdentity{InvalidMaterial2DIdentity};
    std::uint64_t valueIdentity{InvalidMaterial2DIdentity};

    [[nodiscard]] bool operator==(const MaterialParameterBlock2D&) const noexcept = default;

    [[nodiscard]] std::span<const float> ActivePackedFloats() const noexcept
    {
        return std::span<const float>{
            packed.data(),
            static_cast<std::size_t>(parameterCount) * Material2DParameterSlotFloatCount};
    }

    [[nodiscard]] std::size_t ActivePackedBytes() const noexcept
    {
        return static_cast<std::size_t>(parameterCount) * Material2DParameterSlotBytes;
    }
};

struct ResolvedMaterialParameterOverride2D final
{
    MaterialParameterBinding2D binding{};
    MaterialParameterValue2D value{};

    [[nodiscard]] bool operator==(const ResolvedMaterialParameterOverride2D&) const noexcept = default;
};

enum class MaterialPrepareError2D : std::uint8_t
{
    None = 0,
    InvalidShaderIdentity,
    TooManyParameters,
    EmptyParameterName,
    ParameterNameTooLong,
    InvalidParameterName,
    DuplicateParameterName,
    UnsupportedParameterType,
    UnknownParameterName,
    ParameterCountMismatch,
    ParameterTypeMismatch,
    NonFiniteParameterValue,
    InvalidPreparedLayout,
    BindingLayoutMismatch,
    BindingSlotOutOfRange,
    DuplicateOverride,
};

[[nodiscard]] std::string_view ToString(MaterialPrepareError2D value) noexcept;

struct MaterialPrepareStatus2D final
{
    MaterialPrepareError2D error{MaterialPrepareError2D::None};
    std::uint8_t parameterIndex{0U};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == MaterialPrepareError2D::None;
    }

    [[nodiscard]] bool operator==(const MaterialPrepareStatus2D&) const noexcept = default;
};

// Explicit preparation boundary. O(N^2) only for duplicate-name validation with N <= 16, no heap
// allocation, no filesystem/GPU work. Resource generation/domain bits participate in identity so a
// re-prepared newer generation cannot silently accept bindings from an older layout.
[[nodiscard]] MaterialPrepareStatus2D PrepareMaterialParameterLayout2D(
    assets::ResourceHandleUntyped shaderIdentity,
    std::span<const MaterialParameterDeclaration2D> declarations,
    MaterialParameterLayout2D& outLayout) noexcept;

// Setup/tooling lookup only. O(N), allocation-free. Store the returned binding and never resolve the
// parameter name again in steady rendering/tween code.
[[nodiscard]] MaterialPrepareStatus2D ResolveMaterialParameterBinding2D(
    const MaterialParameterLayout2D& layout,
    std::string_view name,
    MaterialParameterBinding2D& outBinding) noexcept;

// Material-default preparation. Defaults must already be in declaration order. O(N), bounded and
// allocation-free; each value is canonicalized into a complete float4 slot.
[[nodiscard]] MaterialPrepareStatus2D PrepareMaterialParameterBlock2D(
    const MaterialParameterLayout2D& layout,
    std::span<const MaterialParameterValue2D> defaults,
    MaterialParameterBlock2D& outBlock) noexcept;

// Per-instance override path. Takes only pre-resolved bindings, copies one fixed-capacity block and
// updates selected slots. O(N + overrides), no strings, maps, filesystem, reflection or heap work.
[[nodiscard]] MaterialPrepareStatus2D ApplyMaterialParameterOverrides2D(
    const MaterialParameterBlock2D& base,
    std::span<const ResolvedMaterialParameterOverride2D> overrides,
    MaterialParameterBlock2D& outBlock) noexcept;

static_assert(sizeof(float) == 4U);
static_assert(Material2DParameterSlotBytes == 16U);
static_assert(MaximumMaterial2DParameterNameBytes <= 255U);
static_assert(std::is_trivially_copyable_v<MaterialParameterDeclaration2D>);
static_assert(std::is_trivially_copyable_v<MaterialParameterValue2D>);
static_assert(std::is_trivially_copyable_v<MaterialParameterLayoutEntry2D>);
static_assert(std::is_trivially_copyable_v<MaterialParameterName2D>);
static_assert(std::is_trivially_copyable_v<MaterialParameterLayout2D>);
static_assert(std::is_trivially_copyable_v<MaterialParameterBinding2D>);
static_assert(std::is_trivially_copyable_v<MaterialParameterBlock2D>);
static_assert(std::is_trivially_copyable_v<ResolvedMaterialParameterOverride2D>);
} // namespace trace2d::render
