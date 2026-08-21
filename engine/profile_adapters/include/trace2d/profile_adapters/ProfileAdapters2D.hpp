#pragma once

#include <trace2d/assets/ResourceRegistry.hpp>
#include <trace2d/audio/AudioOutput2D.hpp>
#include <trace2d/audio/AudioSystem2D.hpp>
#include <trace2d/particles/ParticleReference.hpp>
#include <trace2d/physics/PhysicsWorld2D.hpp>
#include <trace2d/profile/StructuralProfile2D.hpp>
#include <trace2d/render/Renderer.hpp>

#include <cstddef>
#include <cstdint>
#include <span>

namespace trace2d::profile_adapters
{
inline constexpr std::size_t StructuralProfileAdapterMetricCount2D = 97U;

struct ParticleReferenceProfileInput2D final
{
    const particles::ParticleReferenceCounters* counters{nullptr};
    const particles::ParticleReferenceMemoryReport* memory{nullptr};
    std::uint32_t aliveCount{0U};
    bool measured{false};
};

struct StructuralProfileInputs2D final
{
    const render::RenderMetrics* renderer{nullptr};
    const physics::PhysicsMetrics2D* physics{nullptr};
    const audio::AudioMetrics2D* audio{nullptr};
    const audio::AudioOutputMetrics2D* audioOutput{nullptr};
    const assets::ResourceRegistryStats* resources{nullptr};
    std::span<const assets::ResourceSnapshot> resourceSnapshots{};
    bool resourceMemoryMeasured{false};
    ParticleReferenceProfileInput2D particleReference{};
};

// Cross-subsystem composition is explicit diagnostic work. It reads existing cheap metric structs
// and writes only into already-prepared Profile storage. Source systems remain unaware of this
// adapter target and never construct profile names or JSON in their normal frame paths.
//
// The call is transactional for expected data failures: insufficient snapshot capacity and resource
// byte-sum overflow are detected before Clear(), preserving the previously committed snapshot.
[[nodiscard]] profile::StructuralProfileResult2D ComposeStructuralProfile2D(
    profile::StructuralProfileSnapshot2D& snapshot,
    const StructuralProfileInputs2D& inputs) noexcept;
} // namespace trace2d::profile_adapters
