#include <trace2d/agent/Inspection.hpp>

#include <trace2d/particles/ParticleEffect.hpp>
#include <trace2d/particles/ParticleReference.hpp>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

namespace trace2d::agent
{
namespace
{
constexpr std::uint64_t FingerprintOffsetBasis = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t FingerprintPrime = 1'099'511'628'211ULL;
constexpr std::uint32_t ParticleFingerprintVersion = 1U;

class FingerprintBuilder final
{
public:
    void AddByte(const std::uint8_t value) noexcept
    {
        hash_ ^= value;
        hash_ *= FingerprintPrime;
    }

    void AddUInt32(const std::uint32_t value) noexcept
    {
        for (std::uint32_t shift = 0U; shift < 32U; shift += 8U)
        {
            AddByte(static_cast<std::uint8_t>((value >> shift) & 0xFFU));
        }
    }

    void AddUInt64(const std::uint64_t value) noexcept
    {
        for (std::uint32_t shift = 0U; shift < 64U; shift += 8U)
        {
            AddByte(static_cast<std::uint8_t>((value >> shift) & 0xFFULL));
        }
    }

    void AddFloat(const float value) noexcept
    {
        AddUInt32(std::bit_cast<std::uint32_t>(value));
    }

    [[nodiscard]] std::uint64_t Value() const noexcept
    {
        return hash_;
    }

private:
    std::uint64_t hash_{FingerprintOffsetBasis};
};

[[nodiscard]] ParticleInspectionError MakeError(
    const ParticleInspectionErrorCode code,
    std::string message)
{
    return ParticleInspectionError{
        .code = code,
        .message = std::move(message),
    };
}

[[nodiscard]] std::optional<ParticleInspectionError> ValidateBinding(
    const ParticleEmitterBinding& binding)
{
    if (binding.emitter == nullptr)
    {
        return MakeError(
            ParticleInspectionErrorCode::EmitterUnavailable,
            "No ParticleEmitter2D is bound for Agent inspection.");
    }
    if (!binding.emitter->IsPrepared() || binding.emitter->Effect() == nullptr)
    {
        return MakeError(
            ParticleInspectionErrorCode::EmitterNotPrepared,
            "The bound ParticleEmitter2D is not prepared with a CPU reference effect.");
    }
    return std::nullopt;
}

[[nodiscard]] ParticleSimulationSpaceSnapshot MakeSimulationSpace(
    const particles::ParticleSimulationSpace space) noexcept
{
    return space == particles::ParticleSimulationSpace::World
        ? ParticleSimulationSpaceSnapshot::World
        : ParticleSimulationSpaceSnapshot::Local;
}

[[nodiscard]] ParticleStateSnapshot MakeParticleSnapshot(
    const particles::ParticleReferenceParticle& particle) noexcept
{
    return ParticleStateSnapshot{
        .spawnOrdinal = particle.spawnOrdinal,
        .position = {particle.position.x, particle.position.y},
        .velocity = {particle.velocity.x, particle.velocity.y},
        .acceleration = {particle.acceleration.x, particle.acceleration.y},
        .ageFrames = particle.ageFrames,
        .lifetimeFrames = particle.lifetimeFrames,
        .initialSize = particle.initialSize,
        .size = particle.size,
        .rotationRadians = particle.rotationRadians,
        .angularVelocityRadiansPerFrame = particle.angularVelocityRadiansPerFrame,
        .initialColor = {
            particle.initialColor.r,
            particle.initialColor.g,
            particle.initialColor.b,
            particle.initialColor.a,
        },
        .color = {
            particle.color.r,
            particle.color.g,
            particle.color.b,
            particle.color.a,
        },
        .spriteIndex = particle.spriteIndex,
        .simulationSpace = MakeSimulationSpace(particle.simulationSpace),
    };
}

void AddVec2(FingerprintBuilder& builder, const particles::ParticleVec2 value) noexcept
{
    builder.AddFloat(value.x);
    builder.AddFloat(value.y);
}

void AddColor(FingerprintBuilder& builder, const particles::ParticleColor value) noexcept
{
    builder.AddFloat(value.r);
    builder.AddFloat(value.g);
    builder.AddFloat(value.b);
    builder.AddFloat(value.a);
}

void AddDefinition(
    FingerprintBuilder& builder,
    const particles::ParticleReferenceDefinition& definition) noexcept
{
    builder.AddUInt32(definition.maxParticles);
    builder.AddUInt64(definition.globalSeed);
    builder.AddUInt64(definition.emitterStableId);
    builder.AddUInt64(definition.periodicStartFrame);
    builder.AddUInt32(definition.periodicCount);
    builder.AddUInt32(definition.periodicEveryFrames);
    builder.AddByte(static_cast<std::uint8_t>(definition.spawnShape.type));
    AddVec2(builder, definition.spawnShape.offset);
    AddVec2(builder, definition.spawnShape.boxHalfExtents);
    builder.AddFloat(definition.spawnShape.circleRadius);
    builder.AddUInt32(definition.lifetimeFrames.minValue);
    builder.AddUInt32(definition.lifetimeFrames.maxValue);
    builder.AddFloat(definition.speed.minValue);
    builder.AddFloat(definition.speed.maxValue);
    builder.AddFloat(definition.angleRadians.minValue);
    builder.AddFloat(definition.angleRadians.maxValue);
    AddVec2(builder, definition.acceleration);
    builder.AddFloat(definition.initialSize.minValue);
    builder.AddFloat(definition.initialSize.maxValue);
    builder.AddFloat(definition.endSizeMultiplier);
    builder.AddFloat(definition.rotationRadians.minValue);
    builder.AddFloat(definition.rotationRadians.maxValue);
    builder.AddFloat(definition.angularVelocityRadiansPerFrame.minValue);
    builder.AddFloat(definition.angularVelocityRadiansPerFrame.maxValue);
    AddColor(builder, definition.initialColor.minValue);
    AddColor(builder, definition.initialColor.maxValue);
    AddColor(builder, definition.endColor);
    builder.AddUInt32(definition.spriteChoiceCount);
    builder.AddByte(static_cast<std::uint8_t>(definition.simulationSpace));
}

void AddParticle(
    FingerprintBuilder& builder,
    const particles::ParticleReferenceParticle& particle) noexcept
{
    builder.AddUInt64(particle.spawnOrdinal);
    AddVec2(builder, particle.position);
    AddVec2(builder, particle.velocity);
    AddVec2(builder, particle.acceleration);
    builder.AddUInt32(particle.ageFrames);
    builder.AddUInt32(particle.lifetimeFrames);
    builder.AddFloat(particle.initialSize);
    builder.AddFloat(particle.size);
    builder.AddFloat(particle.rotationRadians);
    builder.AddFloat(particle.angularVelocityRadiansPerFrame);
    AddColor(builder, particle.initialColor);
    AddColor(builder, particle.color);
    builder.AddUInt32(particle.spriteIndex);
    builder.AddByte(static_cast<std::uint8_t>(particle.simulationSpace));
}

[[nodiscard]] std::uint64_t ComputeStateFingerprint(
    const particles::ParticleEmitter2D& emitter) noexcept
{
    const particles::ParticleReferenceEmitter& reference = emitter.Reference();
    const particles::ParticleEffectAsset& effect = *emitter.Effect();
    const particles::ParticleReferenceDefinition& definition = reference.Definition();
    const particles::ParticleReferenceCounters& counters = reference.Counters();

    FingerprintBuilder builder{};
    builder.AddUInt32(ParticleFingerprintVersion);
    AddDefinition(builder, definition);
    builder.AddUInt32(static_cast<std::uint32_t>(effect.bursts.size()));
    for (const particles::ParticleBurst& burst : effect.bursts)
    {
        builder.AddUInt64(burst.frame);
        builder.AddUInt32(burst.count);
    }

    builder.AddUInt64(reference.NextFrameIndex());
    builder.AddUInt32(reference.AliveCount());
    builder.AddUInt64(reference.NextSpawnOrdinal());
    builder.AddUInt64(counters.spawnAttempts);
    builder.AddUInt64(counters.spawned);
    builder.AddUInt64(counters.updated);
    builder.AddUInt64(counters.expired);
    builder.AddUInt64(counters.dropped);
    builder.AddUInt32(counters.peakAlive);

    particles::ParticleReferenceParticle particle{};
    for (std::uint32_t index = 0U; index < reference.AliveCount(); ++index)
    {
        if (reference.TryGetParticle(index, particle))
        {
            AddParticle(builder, particle);
        }
    }
    return builder.Value();
}

[[nodiscard]] ParticleAssertionContext MakeAssertionContext(
    const ParticleEmitterBinding& binding)
{
    const particles::ParticleEmitter2D& emitter = *binding.emitter;
    const particles::ParticleReferenceEmitter& reference = emitter.Reference();
    const particles::ParticleReferenceDefinition& definition = reference.Definition();
    const particles::ParticleEffectAsset& effect = *emitter.Effect();

    return ParticleAssertionContext{
        .entitySemanticId = std::string{binding.entitySemanticId},
        .effectSemanticId = effect.semanticId,
        .effectAssetId = effect.id,
        .emitterStableId = definition.emitterStableId,
        .nextSimulationFrame = reference.NextFrameIndex(),
        .cycleFrame = emitter.CycleFrame(),
        .seed = definition.globalSeed,
        .aliveCount = reference.AliveCount(),
        .particleDetail = std::nullopt,
    };
}

[[nodiscard]] bool IsAggregateField(const ParticleAssertionField field) noexcept
{
    return field <= ParticleAssertionField::StateFingerprint;
}

[[nodiscard]] ParticleValue ReadAggregateValue(
    const particles::ParticleEmitter2D& emitter,
    const ParticleAssertionField field) noexcept
{
    const particles::ParticleReferenceEmitter& reference = emitter.Reference();
    const particles::ParticleReferenceCounters& counters = reference.Counters();

    switch (field)
    {
    case ParticleAssertionField::Prepared:
        return ParticleValue::Boolean(emitter.IsPrepared());
    case ParticleAssertionField::Playing:
        return ParticleValue::Boolean(emitter.IsPlaying());
    case ParticleAssertionField::CycleFrame:
        return ParticleValue::Unsigned(emitter.CycleFrame());
    case ParticleAssertionField::CompletedLoops:
        return ParticleValue::Unsigned(emitter.CompletedLoops());
    case ParticleAssertionField::NextSimulationFrame:
        return ParticleValue::Unsigned(reference.NextFrameIndex());
    case ParticleAssertionField::AliveCount:
        return ParticleValue::Unsigned(reference.AliveCount());
    case ParticleAssertionField::Capacity:
        return ParticleValue::Unsigned(reference.Definition().maxParticles);
    case ParticleAssertionField::SpawnAttemptsTotal:
        return ParticleValue::Unsigned(counters.spawnAttempts);
    case ParticleAssertionField::EmittedTotal:
        return ParticleValue::Unsigned(counters.spawned);
    case ParticleAssertionField::UpdatedTotal:
        return ParticleValue::Unsigned(counters.updated);
    case ParticleAssertionField::ExpiredTotal:
        return ParticleValue::Unsigned(counters.expired);
    case ParticleAssertionField::DroppedTotal:
        return ParticleValue::Unsigned(counters.dropped);
    case ParticleAssertionField::PeakAlive:
        return ParticleValue::Unsigned(counters.peakAlive);
    case ParticleAssertionField::StateFingerprint:
        return ParticleValue::Unsigned(ComputeStateFingerprint(emitter));
    default:
        return {};
    }
}

[[nodiscard]] ParticleValue ReadParticleValue(
    const ParticleStateSnapshot& particle,
    const ParticleAssertionField field)
{
    switch (field)
    {
    case ParticleAssertionField::SpawnOrdinal:
        return ParticleValue::Unsigned(particle.spawnOrdinal);
    case ParticleAssertionField::PositionX:
        return ParticleValue::Float(particle.position.x);
    case ParticleAssertionField::PositionY:
        return ParticleValue::Float(particle.position.y);
    case ParticleAssertionField::VelocityX:
        return ParticleValue::Float(particle.velocity.x);
    case ParticleAssertionField::VelocityY:
        return ParticleValue::Float(particle.velocity.y);
    case ParticleAssertionField::AccelerationX:
        return ParticleValue::Float(particle.acceleration.x);
    case ParticleAssertionField::AccelerationY:
        return ParticleValue::Float(particle.acceleration.y);
    case ParticleAssertionField::AgeFrames:
        return ParticleValue::Unsigned(particle.ageFrames);
    case ParticleAssertionField::LifetimeFrames:
        return ParticleValue::Unsigned(particle.lifetimeFrames);
    case ParticleAssertionField::InitialSize:
        return ParticleValue::Float(particle.initialSize);
    case ParticleAssertionField::Size:
        return ParticleValue::Float(particle.size);
    case ParticleAssertionField::RotationRadians:
        return ParticleValue::Float(particle.rotationRadians);
    case ParticleAssertionField::AngularVelocityRadiansPerFrame:
        return ParticleValue::Float(particle.angularVelocityRadiansPerFrame);
    case ParticleAssertionField::InitialColorR:
        return ParticleValue::Float(particle.initialColor.r);
    case ParticleAssertionField::InitialColorG:
        return ParticleValue::Float(particle.initialColor.g);
    case ParticleAssertionField::InitialColorB:
        return ParticleValue::Float(particle.initialColor.b);
    case ParticleAssertionField::InitialColorA:
        return ParticleValue::Float(particle.initialColor.a);
    case ParticleAssertionField::ColorR:
        return ParticleValue::Float(particle.color.r);
    case ParticleAssertionField::ColorG:
        return ParticleValue::Float(particle.color.g);
    case ParticleAssertionField::ColorB:
        return ParticleValue::Float(particle.color.b);
    case ParticleAssertionField::ColorA:
        return ParticleValue::Float(particle.color.a);
    case ParticleAssertionField::SpriteIndex:
        return ParticleValue::Unsigned(particle.spriteIndex);
    case ParticleAssertionField::SimulationSpace:
        return ParticleValue::String(std::string{ToString(particle.simulationSpace)});
    default:
        return {};
    }
}

[[nodiscard]] bool ValuesEqual(
    const ParticleValue& expected,
    const ParticleValue& observed) noexcept
{
    if (expected.kind != observed.kind) return false;

    switch (expected.kind)
    {
    case ParticleValueKind::Boolean:
        return expected.booleanValue == observed.booleanValue;
    case ParticleValueKind::UnsignedInteger:
        return expected.unsignedIntegerValue == observed.unsignedIntegerValue;
    case ParticleValueKind::Float:
        return expected.floatValue == observed.floatValue;
    case ParticleValueKind::String:
        return expected.stringValue == observed.stringValue;
    }
    return false;
}
} // namespace

std::string_view ToString(const ParticleInspectionErrorCode code) noexcept
{
    switch (code)
    {
    case ParticleInspectionErrorCode::EmitterUnavailable:
        return "emitter_unavailable";
    case ParticleInspectionErrorCode::EmitterNotPrepared:
        return "emitter_not_prepared";
    case ParticleInspectionErrorCode::InvalidRange:
        return "invalid_range";
    case ParticleInspectionErrorCode::ParticleNotFound:
        return "particle_not_found";
    case ParticleInspectionErrorCode::InvalidAssertion:
        return "invalid_assertion";
    case ParticleInspectionErrorCode::TypeMismatch:
        return "type_mismatch";
    case ParticleInspectionErrorCode::StateMismatch:
        return "state_mismatch";
    }
    return "unknown_particle_inspection_error";
}

std::string_view ToString(const ParticleValueKind kind) noexcept
{
    switch (kind)
    {
    case ParticleValueKind::Boolean:
        return "bool";
    case ParticleValueKind::UnsignedInteger:
        return "uint64";
    case ParticleValueKind::Float:
        return "float";
    case ParticleValueKind::String:
        return "string";
    }
    return "unknown";
}

ParticleValue ParticleValue::Boolean(const bool value) noexcept
{
    ParticleValue result{};
    result.kind = ParticleValueKind::Boolean;
    result.booleanValue = value;
    return result;
}

ParticleValue ParticleValue::Unsigned(const std::uint64_t value) noexcept
{
    ParticleValue result{};
    result.kind = ParticleValueKind::UnsignedInteger;
    result.unsignedIntegerValue = value;
    return result;
}

ParticleValue ParticleValue::Float(const float value) noexcept
{
    ParticleValue result{};
    result.kind = ParticleValueKind::Float;
    result.floatValue = value;
    return result;
}

ParticleValue ParticleValue::String(std::string value) noexcept
{
    ParticleValue result{};
    result.kind = ParticleValueKind::String;
    result.stringValue = std::move(value);
    return result;
}

std::string_view ToString(const ParticleSimulationSpaceSnapshot space) noexcept
{
    switch (space)
    {
    case ParticleSimulationSpaceSnapshot::Local:
        return "local";
    case ParticleSimulationSpaceSnapshot::World:
        return "world";
    }
    return "unknown";
}

std::string_view ToString(const ParticleAssertionField field) noexcept
{
    switch (field)
    {
    case ParticleAssertionField::Prepared: return "prepared";
    case ParticleAssertionField::Playing: return "playing";
    case ParticleAssertionField::CycleFrame: return "cycle_frame";
    case ParticleAssertionField::CompletedLoops: return "completed_loops";
    case ParticleAssertionField::NextSimulationFrame: return "next_simulation_frame";
    case ParticleAssertionField::AliveCount: return "alive_count";
    case ParticleAssertionField::Capacity: return "capacity";
    case ParticleAssertionField::SpawnAttemptsTotal: return "spawn_attempts_total";
    case ParticleAssertionField::EmittedTotal: return "emitted_total";
    case ParticleAssertionField::UpdatedTotal: return "updated_total";
    case ParticleAssertionField::ExpiredTotal: return "expired_total";
    case ParticleAssertionField::DroppedTotal: return "dropped_total";
    case ParticleAssertionField::PeakAlive: return "peak_alive";
    case ParticleAssertionField::StateFingerprint: return "state_fingerprint";
    case ParticleAssertionField::SpawnOrdinal: return "spawn_ordinal";
    case ParticleAssertionField::PositionX: return "position.x";
    case ParticleAssertionField::PositionY: return "position.y";
    case ParticleAssertionField::VelocityX: return "velocity.x";
    case ParticleAssertionField::VelocityY: return "velocity.y";
    case ParticleAssertionField::AccelerationX: return "acceleration.x";
    case ParticleAssertionField::AccelerationY: return "acceleration.y";
    case ParticleAssertionField::AgeFrames: return "age_frames";
    case ParticleAssertionField::LifetimeFrames: return "lifetime_frames";
    case ParticleAssertionField::InitialSize: return "initial_size";
    case ParticleAssertionField::Size: return "size";
    case ParticleAssertionField::RotationRadians: return "rotation_radians";
    case ParticleAssertionField::AngularVelocityRadiansPerFrame: return "angular_velocity_radians_per_frame";
    case ParticleAssertionField::InitialColorR: return "initial_color.r";
    case ParticleAssertionField::InitialColorG: return "initial_color.g";
    case ParticleAssertionField::InitialColorB: return "initial_color.b";
    case ParticleAssertionField::InitialColorA: return "initial_color.a";
    case ParticleAssertionField::ColorR: return "color.r";
    case ParticleAssertionField::ColorG: return "color.g";
    case ParticleAssertionField::ColorB: return "color.b";
    case ParticleAssertionField::ColorA: return "color.a";
    case ParticleAssertionField::SpriteIndex: return "sprite_index";
    case ParticleAssertionField::SimulationSpace: return "simulation_space";
    }
    return "unknown_particle_field";
}

ParticleEmitterInspectionResult AgentFacade::InspectParticleEmitter(
    const ParticleEmitterBinding& binding) const
{
    if (const std::optional<ParticleInspectionError> error = ValidateBinding(binding); error.has_value())
    {
        ParticleEmitterInspectionResult result{};
        result.error = *error;
        return result;
    }

    const particles::ParticleEmitter2D& emitter = *binding.emitter;
    const particles::ParticleReferenceEmitter& reference = emitter.Reference();
    const particles::ParticleReferenceDefinition& definition = reference.Definition();
    const particles::ParticleReferenceCounters& counters = reference.Counters();
    const particles::ParticleEffectAsset& effect = *emitter.Effect();

    ParticleEmitterInspectionResult result{};
    result.snapshot = ParticleEmitterSnapshot{
        .entitySemanticId = std::string{binding.entitySemanticId},
        .effectSemanticId = effect.semanticId,
        .effectAssetId = effect.id,
        .emitterStableId = definition.emitterStableId,
        .prepared = emitter.IsPrepared(),
        .playing = emitter.IsPlaying(),
        .cycleFrame = emitter.CycleFrame(),
        .completedLoops = emitter.CompletedLoops(),
        .nextSimulationFrame = reference.NextFrameIndex(),
        .aliveCount = reference.AliveCount(),
        .capacity = definition.maxParticles,
        .spawnAttemptsTotal = counters.spawnAttempts,
        .emittedTotal = counters.spawned,
        .updatedTotal = counters.updated,
        .expiredTotal = counters.expired,
        .droppedTotal = counters.dropped,
        .peakAlive = counters.peakAlive,
        .stateFingerprint = ComputeStateFingerprint(emitter),
    };
    return result;
}

ParticleDetailInspectionResult AgentFacade::InspectParticles(
    const ParticleEmitterBinding& binding,
    const std::uint32_t offset,
    const std::uint32_t limit) const
{
    if (const std::optional<ParticleInspectionError> error = ValidateBinding(binding); error.has_value())
    {
        ParticleDetailInspectionResult result{};
        result.error = *error;
        return result;
    }
    if (limit == 0U || limit > MaxParticleInspectionCount)
    {
        ParticleDetailInspectionResult result{};
        result.error = MakeError(
            ParticleInspectionErrorCode::InvalidRange,
            "Particle detail limit must be between 1 and MaxParticleInspectionCount.");
        return result;
    }

    const particles::ParticleEmitter2D& emitter = *binding.emitter;
    const particles::ParticleReferenceEmitter& reference = emitter.Reference();
    if (offset > reference.AliveCount())
    {
        ParticleDetailInspectionResult result{};
        result.error = MakeError(
            ParticleInspectionErrorCode::InvalidRange,
            "Particle detail offset exceeds the current alive particle count.");
        return result;
    }

    const std::uint32_t count = std::min(limit, reference.AliveCount() - offset);
    ParticleDetailSnapshot snapshot{};
    snapshot.entitySemanticId = std::string{binding.entitySemanticId};
    snapshot.effectSemanticId = emitter.Effect()->semanticId;
    snapshot.emitterStableId = reference.Definition().emitterStableId;
    snapshot.totalAlive = reference.AliveCount();
    snapshot.offset = offset;
    snapshot.requestedLimit = limit;
    snapshot.particles.reserve(count);

    particles::ParticleReferenceParticle particle{};
    for (std::uint32_t index = 0U; index < count; ++index)
    {
        if (!reference.TryGetParticle(offset + index, particle))
        {
            ParticleDetailInspectionResult result{};
            result.error = MakeError(
                ParticleInspectionErrorCode::ParticleNotFound,
                "A particle disappeared while reading a bounded detail range.");
            return result;
        }
        snapshot.particles.push_back(MakeParticleSnapshot(particle));
    }

    ParticleDetailInspectionResult result{};
    result.snapshot = std::move(snapshot);
    return result;
}

ParticleSingleInspectionResult AgentFacade::InspectParticle(
    const ParticleEmitterBinding& binding,
    const particles::ParticleSpawnOrdinal spawnOrdinal) const
{
    if (const std::optional<ParticleInspectionError> error = ValidateBinding(binding); error.has_value())
    {
        ParticleSingleInspectionResult result{};
        result.error = *error;
        return result;
    }

    particles::ParticleReferenceParticle particle{};
    if (!binding.emitter->Reference().TryGetParticleBySpawnOrdinal(spawnOrdinal, particle))
    {
        ParticleSingleInspectionResult result{};
        result.error = MakeError(
            ParticleInspectionErrorCode::ParticleNotFound,
            "No live particle has the requested spawn ordinal.");
        return result;
    }

    ParticleSingleInspectionResult result{};
    result.particle = MakeParticleSnapshot(particle);
    return result;
}

ParticleAssertionResult AgentFacade::AssertParticle(
    const ParticleEmitterBinding& binding,
    const ParticleAssertion& assertion) const
{
    ParticleAssertionResult result{};
    result.assertion = assertion;

    if (const std::optional<ParticleInspectionError> error = ValidateBinding(binding); error.has_value())
    {
        result.error = *error;
        return result;
    }

    result.context = MakeAssertionContext(binding);
    const bool aggregateField = IsAggregateField(assertion.field);
    if (aggregateField && assertion.spawnOrdinal.has_value())
    {
        result.error = MakeError(
            ParticleInspectionErrorCode::InvalidAssertion,
            "Aggregate particle assertions must not specify a spawn ordinal.");
        return result;
    }
    if (!aggregateField && !assertion.spawnOrdinal.has_value())
    {
        result.error = MakeError(
            ParticleInspectionErrorCode::InvalidAssertion,
            "Per-particle assertions require an explicit stable spawn ordinal.");
        return result;
    }

    if (aggregateField)
    {
        result.observed = ReadAggregateValue(*binding.emitter, assertion.field);
    }
    else
    {
        particles::ParticleReferenceParticle referenceParticle{};
        if (!binding.emitter->Reference().TryGetParticleBySpawnOrdinal(
                *assertion.spawnOrdinal,
                referenceParticle))
        {
            result.error = MakeError(
                ParticleInspectionErrorCode::ParticleNotFound,
                "No live particle has the assertion's spawn ordinal.");
            return result;
        }
        result.context.particleDetail = MakeParticleSnapshot(referenceParticle);
        result.observed = ReadParticleValue(*result.context.particleDetail, assertion.field);
    }

    if (result.observed->kind != assertion.expected.kind)
    {
        result.error = MakeError(
            ParticleInspectionErrorCode::TypeMismatch,
            "Particle assertion expected value kind does not match field '" +
                std::string{ToString(assertion.field)} + "'.");
        return result;
    }
    if (!ValuesEqual(assertion.expected, *result.observed))
    {
        result.error = MakeError(
            ParticleInspectionErrorCode::StateMismatch,
            "Particle assertion failed for field '" +
                std::string{ToString(assertion.field)} + "'.");
        return result;
    }
    return result;
}
} // namespace trace2d::agent
