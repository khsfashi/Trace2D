#pragma once

#include <trace2d/render/Material2D.hpp>
#include <trace2d/scene/TweenBinding2D.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace trace2d::render
{
struct MaterialTweenTargetHandle2D final
{
    static constexpr std::uint32_t InvalidIndex = std::numeric_limits<std::uint32_t>::max();

    std::uint32_t index{InvalidIndex};
    std::uint64_t generation{0U};

    [[nodiscard]] bool Valid() const noexcept
    {
        return index != InvalidIndex && generation != 0U;
    }

    [[nodiscard]] bool operator==(const MaterialTweenTargetHandle2D&) const noexcept = default;
};

enum class MaterialTweenError2D : std::uint8_t
{
    None = 0,
    InvalidPreparedBlock,
    InvalidTarget,
    BindingLayoutMismatch,
    BindingSlotOutOfRange,
    BindingTypeMismatch,
    TweenBindingFailure,
};

struct MaterialTweenStatus2D final
{
    MaterialTweenError2D error{MaterialTweenError2D::None};
    scene::TweenBindingError2D bindingError{scene::TweenBindingError2D::None};

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return error == MaterialTweenError2D::None;
    }

    [[nodiscard]] bool operator==(const MaterialTweenStatus2D&) const noexcept = default;
};

struct MaterialTweenTargetMetrics2D final
{
    std::uint64_t activeTargetCount{0U};
    std::uint64_t retainedTargetSlotCount{0U};
    std::uint64_t retainedTargetCapacity{0U};
    std::uint64_t highWaterActiveTargetCount{0U};
    std::uint64_t createdTargetCount{0U};
    std::uint64_t reusedTargetSlotCount{0U};
    std::uint64_t appliedWriteCount{0U};

    [[nodiscard]] bool operator==(const MaterialTweenTargetMetrics2D&) const noexcept = default;
};

// Owns per-instance prepared Material2D parameter state that can be consumed directly by renderer
// presentation data. Canonical #86 Material2D defaults remain immutable. Handles are generational,
// so a destroyed/reused target cannot satisfy a stale resolved Tween2D binding.
class MaterialTweenTargetPool2D final
{
public:
    void Reserve(std::size_t capacity);

    [[nodiscard]] MaterialTweenStatus2D Create(
        const MaterialParameterBlock2D& preparedBlock,
        MaterialTweenTargetHandle2D& outHandle);
    [[nodiscard]] MaterialTweenStatus2D Destroy(MaterialTweenTargetHandle2D handle) noexcept;

    [[nodiscard]] const MaterialParameterBlock2D* Resolve(
        MaterialTweenTargetHandle2D handle) const noexcept;

    // Register this provider once with TweenBindingSystem2D during setup. The pool must outlive the
    // binding system/provider registration. No target pointer is retained by Tween2D.
    [[nodiscard]] scene::TweenExternalPropertyProvider2D ExternalProvider() noexcept;

    // `parameterBinding` must already be resolved by ResolveMaterialParameterBinding2D. This call
    // therefore performs no semantic-name lookup and emits a compact T2 binding usable by T3.
    [[nodiscard]] MaterialTweenStatus2D ResolveBinding(
        scene::TweenBindingSystem2D& tweens,
        scene::TweenExternalProviderHandle2D provider,
        MaterialTweenTargetHandle2D target,
        MaterialParameterBinding2D parameterBinding,
        scene::ResolvedTweenBinding2D& outBinding) const noexcept;

    [[nodiscard]] MaterialTweenTargetMetrics2D Metrics() const noexcept;

private:
    struct Slot final
    {
        MaterialParameterBlock2D block{};
        std::uint64_t generation{0U};
        bool occupied{false};
    };

    [[nodiscard]] Slot* ResolveMutable(
        std::uint32_t index,
        std::uint64_t generation) noexcept;
    [[nodiscard]] const Slot* ResolveSlot(
        std::uint32_t index,
        std::uint64_t generation) const noexcept;
    [[nodiscard]] static std::uint64_t NextGeneration(std::uint64_t generation) noexcept;
    [[nodiscard]] static bool ValidateExternal(
        void* context,
        std::uint32_t targetSlot,
        std::uint64_t targetGeneration,
        std::uint32_t propertyIndex,
        runtime::TweenValueType2D valueType) noexcept;
    [[nodiscard]] static bool ReadExternal(
        void* context,
        std::uint32_t targetSlot,
        std::uint64_t targetGeneration,
        std::uint32_t propertyIndex,
        runtime::TweenValue2D& outValue) noexcept;
    [[nodiscard]] static bool WriteExternal(
        void* context,
        std::uint32_t targetSlot,
        std::uint64_t targetGeneration,
        std::uint32_t propertyIndex,
        const runtime::TweenValue2D& value) noexcept;

    std::vector<Slot> slots_{};
    std::uint64_t activeTargetCount_{0U};
    std::uint64_t highWaterActiveTargetCount_{0U};
    std::uint64_t createdTargetCount_{0U};
    std::uint64_t reusedTargetSlotCount_{0U};
    std::uint64_t appliedWriteCount_{0U};
};
} // namespace trace2d::render
