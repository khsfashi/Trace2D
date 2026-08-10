#include <trace2d/particles/ParticleProgram.hpp>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <memory>
#include <utility>

namespace trace2d::particles
{
namespace
{
constexpr std::uint64_t FingerprintOffsetBasis = 14'695'981'039'346'656'037ULL;
constexpr std::uint64_t FingerprintPrime = 1'099'511'628'211ULL;
constexpr std::uint32_t ParticleProgramFingerprintVersion = 1U;
constexpr std::uint32_t ParticleGpuArtifactVersion = 1U;

class FingerprintBuilder final
{
public:
    void AddByte(const std::uint8_t value) noexcept
    {
        hash_ ^= value;
        hash_ *= FingerprintPrime;
    }

    void AddBool(const bool value) noexcept
    {
        AddByte(value ? 1U : 0U);
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

    void AddString(const std::string_view value) noexcept
    {
        AddUInt64(static_cast<std::uint64_t>(value.size()));
        for (const char character : value)
        {
            AddByte(static_cast<std::uint8_t>(static_cast<unsigned char>(character)));
        }
    }

    [[nodiscard]] std::uint64_t Value() const noexcept
    {
        return hash_;
    }

private:
    std::uint64_t hash_{FingerprintOffsetBasis};
};

[[nodiscard]] bool RangeVaries(const ParticleFloatRange& range) noexcept
{
    return range.minValue != range.maxValue;
}

[[nodiscard]] bool RangeVaries(const ParticleUIntRange& range) noexcept
{
    return range.minValue != range.maxValue;
}

[[nodiscard]] bool ColorRangeVaries(const ParticleColorRange& range) noexcept
{
    return range.minValue.r != range.maxValue.r ||
        range.minValue.g != range.maxValue.g ||
        range.minValue.b != range.maxValue.b ||
        range.minValue.a != range.maxValue.a;
}

[[nodiscard]] bool IsZero(const ParticleVec2& value) noexcept
{
    return value.x == 0.0F && value.y == 0.0F;
}

[[nodiscard]] bool RangeContainsNonZero(const ParticleFloatRange& range) noexcept
{
    return range.minValue != 0.0F || range.maxValue != 0.0F;
}

[[nodiscard]] bool ColorsEqual(const ParticleColor& left, const ParticleColor& right) noexcept
{
    return left == right;
}

void AddVec2(FingerprintBuilder& builder, const ParticleVec2 value) noexcept
{
    builder.AddFloat(value.x);
    builder.AddFloat(value.y);
}

void AddColor(FingerprintBuilder& builder, const ParticleColor value) noexcept
{
    builder.AddFloat(value.r);
    builder.AddFloat(value.g);
    builder.AddFloat(value.b);
    builder.AddFloat(value.a);
}

void AddDefinition(
    FingerprintBuilder& builder,
    const ParticleReferenceDefinition& definition) noexcept
{
    builder.AddUInt32(definition.maxParticles);
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

[[nodiscard]] std::uint64_t ComputeProgramFingerprint(const ParticleProgram& program) noexcept
{
    FingerprintBuilder builder{};
    builder.AddUInt32(ParticleProgramFingerprintVersion);
    builder.AddString(program.semanticId);
    builder.AddUInt32(program.lifecycle.durationFrames);
    builder.AddBool(program.lifecycle.loop);
    builder.AddBool(program.lifecycle.playOnLoad);
    AddDefinition(builder, program.definition);
    builder.AddUInt64(static_cast<std::uint64_t>(program.bursts.size()));
    for (const ParticleBurst& burst : program.bursts)
    {
        builder.AddUInt64(burst.frame);
        builder.AddUInt32(burst.count);
    }
    builder.AddByte(static_cast<std::uint8_t>(program.blendMode));
    builder.AddUInt64(static_cast<std::uint64_t>(program.spriteReferences.size()));
    for (const std::string& sprite : program.spriteReferences)
    {
        builder.AddString(sprite);
    }
    return builder.Value();
}

[[nodiscard]] ParticleProgramMask AllCpuAttributes() noexcept
{
    ParticleProgramMask mask = 0U;
    for (std::uint8_t index = 0U;
         index <= static_cast<std::uint8_t>(ParticleProgramAttribute::SpriteIndex);
         ++index)
    {
        mask |= ParticleProgramMask{1} << index;
    }
    return mask;
}

void AddFeature(ParticleProgram& program, const ParticleProgramFeature feature) noexcept
{
    program.featureMask |= ParticleProgramBit(feature);
}

void AddRandomChannel(ParticleProgram& program, const ParticleRandomChannel channel) noexcept
{
    if (program.requiredRandomChannelCount >= program.requiredRandomChannels.size())
    {
        return;
    }
    program.requiredRandomChannels[program.requiredRandomChannelCount] = channel;
    ++program.requiredRandomChannelCount;
}

void SetOperationCost(
    ParticleProgram& program,
    const ParticleProgramOperation operation,
    const std::uint32_t perSpawn,
    const std::uint32_t perUpdated,
    const std::uint32_t perSurvivor) noexcept
{
    const std::size_t index = static_cast<std::size_t>(operation);
    program.operationCosts[index] = ParticleProgramOperationCost{
        operation,
        perSpawn,
        perUpdated,
        perSurvivor,
    };
}

void AddGpuField(
    ParticleProgram& program,
    const ParticleGpuRuntimeFieldKind kind,
    const std::uint32_t sizeBytes) noexcept
{
    if (program.gpuFieldCount >= program.gpuFields.size())
    {
        return;
    }
    program.gpuFields[program.gpuFieldCount] = ParticleGpuRuntimeField{
        kind,
        program.gpuStrideBytes,
        sizeBytes,
    };
    ++program.gpuFieldCount;
    program.gpuStrideBytes += sizeBytes;
}

[[nodiscard]] std::uint64_t ComputeGpuPipelineVariantId(const ParticleProgram& program) noexcept
{
    FingerprintBuilder builder{};
    builder.AddUInt32(ParticleGpuArtifactVersion);
    builder.AddUInt64(program.featureMask);
    builder.AddByte(static_cast<std::uint8_t>(program.blendMode));
    builder.AddUInt32(program.gpuFieldCount);
    for (std::uint32_t index = 0U; index < program.gpuFieldCount; ++index)
    {
        const ParticleGpuRuntimeField& field = program.gpuFields[index];
        builder.AddByte(static_cast<std::uint8_t>(field.kind));
        builder.AddUInt32(field.offsetBytes);
        builder.AddUInt32(field.sizeBytes);
    }
    return builder.Value();
}

[[nodiscard]] std::uint64_t ComputeGpuArtifactFingerprint(
    const ParticleProgram& program) noexcept
{
    FingerprintBuilder builder{};
    builder.AddUInt32(ParticleGpuArtifactVersion);
    builder.AddUInt64(program.fingerprint);
    builder.AddUInt64(program.gpuPipelineVariantId);
    builder.AddUInt32(program.definition.maxParticles);
    builder.AddUInt32(program.gpuFieldCount);
    builder.AddUInt32(program.gpuStrideBytes);
    builder.AddUInt64(program.gpuBufferBytes);
    for (std::uint32_t index = 0U; index < program.gpuFieldCount; ++index)
    {
        const ParticleGpuRuntimeField& field = program.gpuFields[index];
        builder.AddByte(static_cast<std::uint8_t>(field.kind));
        builder.AddUInt32(field.offsetBytes);
        builder.AddUInt32(field.sizeBytes);
    }
    return builder.Value();
}

[[nodiscard]] std::uint64_t CounterDelta(
    const std::uint64_t current,
    const std::uint64_t previous) noexcept
{
    return current >= previous ? current - previous : current;
}
} // namespace

ParticleProgram CompileParticleProgram(const ParticleEffectAsset& effect)
{
    ParticleProgram program{};
    program.effectAssetId = effect.id;
    program.semanticId = effect.semanticId;
    program.selectedBackend = effect.backend;
    program.lifecycle = effect.lifecycle;
    program.definition = effect.definition;
    program.definition.globalSeed = 0U;
    program.definition.emitterStableId = 0U;
    program.bursts = effect.bursts;
    program.spriteReferences = effect.spriteReferences;
    program.blendMode = effect.blendMode;

    const ParticleReferenceDefinition& definition = program.definition;

    if (definition.periodicCount != 0U) AddFeature(program, ParticleProgramFeature::PeriodicEmission);
    if (!program.bursts.empty()) AddFeature(program, ParticleProgramFeature::Bursts);
    switch (definition.spawnShape.type)
    {
    case ParticleSpawnShapeType::Point: AddFeature(program, ParticleProgramFeature::SpawnPoint); break;
    case ParticleSpawnShapeType::Box: AddFeature(program, ParticleProgramFeature::SpawnBox); break;
    case ParticleSpawnShapeType::Circle: AddFeature(program, ParticleProgramFeature::SpawnCircle); break;
    }
    if (RangeVaries(definition.lifetimeFrames)) AddFeature(program, ParticleProgramFeature::VariableLifetime);
    if (RangeContainsNonZero(definition.speed)) AddFeature(program, ParticleProgramFeature::InitialMotion);
    if (!IsZero(definition.acceleration)) AddFeature(program, ParticleProgramFeature::Acceleration);
    if (RangeVaries(definition.initialSize)) AddFeature(program, ParticleProgramFeature::VariableInitialSize);
    if (definition.endSizeMultiplier != 1.0F) AddFeature(program, ParticleProgramFeature::SizeOverLife);
    if (RangeContainsNonZero(definition.rotationRadians)) AddFeature(program, ParticleProgramFeature::InitialRotation);
    if (RangeContainsNonZero(definition.angularVelocityRadiansPerFrame)) AddFeature(program, ParticleProgramFeature::AngularVelocity);
    if (ColorRangeVaries(definition.initialColor)) AddFeature(program, ParticleProgramFeature::VariableInitialColor);
    if (!ColorsEqual(definition.initialColor.minValue, definition.endColor) ||
        !ColorsEqual(definition.initialColor.maxValue, definition.endColor))
    {
        AddFeature(program, ParticleProgramFeature::ColorOverLife);
    }
    if (definition.spriteChoiceCount > 1U) AddFeature(program, ParticleProgramFeature::SpriteChoice);
    if (definition.simulationSpace == ParticleSimulationSpace::World) AddFeature(program, ParticleProgramFeature::WorldSpace);
    if (program.lifecycle.loop) AddFeature(program, ParticleProgramFeature::Looping);
    if (program.blendMode == ParticleBlendMode::Additive) AddFeature(program, ParticleProgramFeature::AdditiveBlend);

    program.cpuStoredAttributeMask = AllCpuAttributes();
    program.spawnAttributeMask = program.cpuStoredAttributeMask;
    program.updateReadAttributeMask =
        ParticleProgramBit(ParticleProgramAttribute::Position) |
        ParticleProgramBit(ParticleProgramAttribute::Velocity) |
        ParticleProgramBit(ParticleProgramAttribute::Acceleration) |
        ParticleProgramBit(ParticleProgramAttribute::AgeFrames) |
        ParticleProgramBit(ParticleProgramAttribute::LifetimeFrames) |
        ParticleProgramBit(ParticleProgramAttribute::InitialSize) |
        ParticleProgramBit(ParticleProgramAttribute::Rotation) |
        ParticleProgramBit(ParticleProgramAttribute::AngularVelocity) |
        ParticleProgramBit(ParticleProgramAttribute::InitialColor);
    program.updateWriteAttributeMask =
        ParticleProgramBit(ParticleProgramAttribute::Position) |
        ParticleProgramBit(ParticleProgramAttribute::Velocity) |
        ParticleProgramBit(ParticleProgramAttribute::AgeFrames) |
        ParticleProgramBit(ParticleProgramAttribute::Size) |
        ParticleProgramBit(ParticleProgramAttribute::Rotation) |
        ParticleProgramBit(ParticleProgramAttribute::Color);
    program.renderOnlyAttributeMask =
        ParticleProgramBit(ParticleProgramAttribute::Size) |
        ParticleProgramBit(ParticleProgramAttribute::Rotation) |
        ParticleProgramBit(ParticleProgramAttribute::Color) |
        ParticleProgramBit(ParticleProgramAttribute::SpriteIndex);

    program.constantAttributeMask |= ParticleProgramBit(ParticleProgramAttribute::Acceleration);
    if (!RangeVaries(definition.lifetimeFrames))
        program.constantAttributeMask |= ParticleProgramBit(ParticleProgramAttribute::LifetimeFrames);
    if (!RangeVaries(definition.initialSize))
        program.constantAttributeMask |= ParticleProgramBit(ParticleProgramAttribute::InitialSize);
    if (!RangeVaries(definition.rotationRadians))
        program.constantAttributeMask |= ParticleProgramBit(ParticleProgramAttribute::Rotation);
    if (!RangeVaries(definition.angularVelocityRadiansPerFrame))
        program.constantAttributeMask |= ParticleProgramBit(ParticleProgramAttribute::AngularVelocity);
    if (!ColorRangeVaries(definition.initialColor))
        program.constantAttributeMask |= ParticleProgramBit(ParticleProgramAttribute::InitialColor);
    if (definition.spriteChoiceCount == 1U)
        program.constantAttributeMask |= ParticleProgramBit(ParticleProgramAttribute::SpriteIndex);
    if (definition.spawnShape.type == ParticleSpawnShapeType::Point &&
        !RangeContainsNonZero(definition.speed) && IsZero(definition.acceleration))
    {
        program.constantAttributeMask |= ParticleProgramBit(ParticleProgramAttribute::Position);
    }
    if (!RangeContainsNonZero(definition.speed) && IsZero(definition.acceleration))
    {
        program.constantAttributeMask |= ParticleProgramBit(ParticleProgramAttribute::Velocity);
    }

    program.derivedGpuAttributeMask =
        ParticleProgramBit(ParticleProgramAttribute::Size) |
        ParticleProgramBit(ParticleProgramAttribute::Rotation) |
        ParticleProgramBit(ParticleProgramAttribute::Color);

    std::uint32_t spawnPositionRandoms = 0U;
    if (definition.spawnShape.type == ParticleSpawnShapeType::Box ||
        definition.spawnShape.type == ParticleSpawnShapeType::Circle)
    {
        AddRandomChannel(program, ParticleRandomChannel::SpawnPositionX);
        AddRandomChannel(program, ParticleRandomChannel::SpawnPositionY);
        spawnPositionRandoms = 2U;
    }
    if (RangeVaries(definition.lifetimeFrames)) AddRandomChannel(program, ParticleRandomChannel::Lifetime);
    if (RangeVaries(definition.speed)) AddRandomChannel(program, ParticleRandomChannel::Speed);
    if (RangeVaries(definition.angleRadians)) AddRandomChannel(program, ParticleRandomChannel::Angle);
    if (RangeVaries(definition.rotationRadians)) AddRandomChannel(program, ParticleRandomChannel::Rotation);
    if (RangeVaries(definition.angularVelocityRadiansPerFrame)) AddRandomChannel(program, ParticleRandomChannel::AngularVelocity);
    if (RangeVaries(definition.initialSize)) AddRandomChannel(program, ParticleRandomChannel::Size);

    std::uint32_t colorRandoms = 0U;
    if (definition.initialColor.minValue.r != definition.initialColor.maxValue.r)
    {
        AddRandomChannel(program, ParticleRandomChannel::ColorR);
        ++colorRandoms;
    }
    if (definition.initialColor.minValue.g != definition.initialColor.maxValue.g)
    {
        AddRandomChannel(program, ParticleRandomChannel::ColorG);
        ++colorRandoms;
    }
    if (definition.initialColor.minValue.b != definition.initialColor.maxValue.b)
    {
        AddRandomChannel(program, ParticleRandomChannel::ColorB);
        ++colorRandoms;
    }
    if (definition.initialColor.minValue.a != definition.initialColor.maxValue.a)
    {
        AddRandomChannel(program, ParticleRandomChannel::ColorA);
        ++colorRandoms;
    }
    if (definition.spriteChoiceCount > 1U) AddRandomChannel(program, ParticleRandomChannel::SpriteChoice);

    SetOperationCost(program, ParticleProgramOperation::SpawnPositionRandom, spawnPositionRandoms, 0U, 0U);
    SetOperationCost(program, ParticleProgramOperation::SpawnLifetimeRandom, RangeVaries(definition.lifetimeFrames) ? 1U : 0U, 0U, 0U);
    SetOperationCost(program, ParticleProgramOperation::SpawnSpeedRandom, RangeVaries(definition.speed) ? 1U : 0U, 0U, 0U);
    SetOperationCost(program, ParticleProgramOperation::SpawnAngleRandom, RangeVaries(definition.angleRadians) ? 1U : 0U, 0U, 0U);
    SetOperationCost(program, ParticleProgramOperation::SpawnSizeRandom, RangeVaries(definition.initialSize) ? 1U : 0U, 0U, 0U);
    SetOperationCost(program, ParticleProgramOperation::SpawnRotationRandom, RangeVaries(definition.rotationRadians) ? 1U : 0U, 0U, 0U);
    SetOperationCost(program, ParticleProgramOperation::SpawnAngularVelocityRandom, RangeVaries(definition.angularVelocityRadiansPerFrame) ? 1U : 0U, 0U, 0U);
    SetOperationCost(program, ParticleProgramOperation::SpawnColorRandom, colorRandoms, 0U, 0U);
    SetOperationCost(program, ParticleProgramOperation::SpawnSpriteRandom, definition.spriteChoiceCount > 1U ? 1U : 0U, 0U, 0U);
    SetOperationCost(program, ParticleProgramOperation::ApplyAcceleration, 0U, 1U, 0U);
    SetOperationCost(program, ParticleProgramOperation::IntegratePosition, 0U, 1U, 0U);
    SetOperationCost(program, ParticleProgramOperation::IntegrateRotation, 0U, 1U, 0U);
    SetOperationCost(program, ParticleProgramOperation::AdvanceLifetime, 0U, 1U, 0U);
    SetOperationCost(program, ParticleProgramOperation::EvaluateSizeOverLife, 0U, 0U, 1U);
    SetOperationCost(program, ParticleProgramOperation::EvaluateColorOverLife, 0U, 0U, 1U);

    const bool positionNeedsStorage =
        definition.spawnShape.type != ParticleSpawnShapeType::Point ||
        RangeContainsNonZero(definition.speed) ||
        !IsZero(definition.acceleration);
    const bool velocityNeedsStorage =
        RangeContainsNonZero(definition.speed) || !IsZero(definition.acceleration);

    if (positionNeedsStorage) AddGpuField(program, ParticleGpuRuntimeFieldKind::Position, 8U);
    if (velocityNeedsStorage) AddGpuField(program, ParticleGpuRuntimeFieldKind::Velocity, 8U);
    AddGpuField(program, ParticleGpuRuntimeFieldKind::AgeFrames, 4U);
    if (RangeVaries(definition.lifetimeFrames)) AddGpuField(program, ParticleGpuRuntimeFieldKind::LifetimeFrames, 4U);
    if (RangeVaries(definition.initialSize)) AddGpuField(program, ParticleGpuRuntimeFieldKind::InitialSize, 4U);
    if (RangeVaries(definition.rotationRadians)) AddGpuField(program, ParticleGpuRuntimeFieldKind::InitialRotation, 4U);
    if (RangeVaries(definition.angularVelocityRadiansPerFrame)) AddGpuField(program, ParticleGpuRuntimeFieldKind::AngularVelocity, 4U);
    if (ColorRangeVaries(definition.initialColor)) AddGpuField(program, ParticleGpuRuntimeFieldKind::InitialColor, 16U);
    if (definition.spriteChoiceCount > 1U) AddGpuField(program, ParticleGpuRuntimeFieldKind::SpriteIndex, 4U);

    program.gpuBufferBytes =
        static_cast<std::uint64_t>(program.gpuStrideBytes) * definition.maxParticles;
    program.gpuPipelineVariantId = ComputeGpuPipelineVariantId(program);
    program.fingerprint = ComputeProgramFingerprint(program);
    return program;
}

ParticleGpuCompileResult CompileParticleGpuArtifact(const ParticleProgram& program)
{
    if (program.selectedBackend != ParticleEffectBackend::Gpu)
    {
        return ParticleGpuCompileResult{
            ParticleGpuCompileError::BackendNotSelected,
            {},
            "GPU artifact compilation requires explicit authored backend = \"gpu\"; analysis never changes it.",
        };
    }

    ParticleGpuCompileArtifact artifact{};
    artifact.programFingerprint = program.fingerprint;
    artifact.pipelineVariantId = program.gpuPipelineVariantId;
    artifact.capacity = program.definition.maxParticles;
    artifact.fields = program.gpuFields;
    artifact.fieldCount = program.gpuFieldCount;
    artifact.strideBytes = program.gpuStrideBytes;
    artifact.bufferBytes = program.gpuBufferBytes;
    artifact.artifactFingerprint = ComputeGpuArtifactFingerprint(program);
    return ParticleGpuCompileResult{ParticleGpuCompileError::None, artifact, {}};
}

ParticleStructuralCostReport BuildParticleStructuralCostReport(
    const ParticleProgram& program,
    const ParticleCostObservation& observation) noexcept
{
    ParticleStructuralCostReport report{};
    report.programFingerprint = program.fingerprint;
    report.selectedBackend = program.selectedBackend;
    report.capacityPerEmitter = program.definition.maxParticles;
    report.emitterCount = observation.emitterCount;
    report.observedFrames = observation.observedFrames;
    report.currentAlive = observation.currentAlive;
    report.peakAlive = observation.peakAlive;
    report.counters = observation.counters;
    report.cpuStoredAttributeMask = program.cpuStoredAttributeMask;
    report.bytesPerParticlePayload = observation.memory.bytesPerParticlePayload;
    report.particleStorageBytes =
        observation.memory.particleStorageBytes * static_cast<std::size_t>(observation.emitterCount);
    report.preparedCpuStateBytes =
        observation.memory.preparedPayloadBytes * static_cast<std::size_t>(observation.emitterCount);
    report.steadyStateSimulationAllocations = observation.memory.steadyStateSimulationAllocations;
    report.particleUpdates = observation.counters.updated;
    report.survivingParticleUpdates = observation.counters.updated >= observation.counters.expired
        ? observation.counters.updated - observation.counters.expired
        : 0U;
    report.plannedGpuStrideBytes = program.gpuStrideBytes;
    report.plannedGpuBufferBytesPerEmitter = program.gpuBufferBytes;

    for (std::size_t index = 0U; index < program.operationCosts.size(); ++index)
    {
        const ParticleProgramOperationCost& cost = program.operationCosts[index];
        const std::uint64_t evaluations =
            (static_cast<std::uint64_t>(cost.perAdmittedSpawn) * observation.counters.spawned) +
            (static_cast<std::uint64_t>(cost.perUpdatedParticle) * report.particleUpdates) +
            (static_cast<std::uint64_t>(cost.perSurvivingUpdatedParticle) * report.survivingParticleUpdates);
        report.operationTotals[index] = ParticleOperationTotal{cost.operation, evaluations};

        if (cost.operation <= ParticleProgramOperation::SpawnSpriteRandom)
        {
            report.spawnRandomEvaluations += evaluations;
        }
    }
    return report;
}

void ParticleCostAccumulator::Reset(const ParticleEmitter2D& emitter) noexcept
{
    previous_ = emitter.Reference().Counters();
    totals_ = {};
    observedFrames_ = 0U;
    peakAlive_ = emitter.Reference().AliveCount();
    initialized_ = true;
}

void ParticleCostAccumulator::ObserveAfterStep(const ParticleEmitter2D& emitter) noexcept
{
    if (!initialized_)
    {
        Reset(emitter);
        return;
    }

    const ParticleReferenceCounters& current = emitter.Reference().Counters();
    totals_.spawnAttempts += CounterDelta(current.spawnAttempts, previous_.spawnAttempts);
    totals_.spawned += CounterDelta(current.spawned, previous_.spawned);
    totals_.updated += CounterDelta(current.updated, previous_.updated);
    totals_.expired += CounterDelta(current.expired, previous_.expired);
    totals_.dropped += CounterDelta(current.dropped, previous_.dropped);
    peakAlive_ = std::max(peakAlive_, emitter.Reference().AliveCount());
    peakAlive_ = std::max(peakAlive_, current.peakAlive);
    totals_.peakAlive = peakAlive_;
    previous_ = current;
    ++observedFrames_;
}

ParticleCostObservation ParticleCostAccumulator::Observation(
    const ParticleEmitter2D& emitter,
    const std::uint32_t emitterCount) const noexcept
{
    return ParticleCostObservation{
        emitterCount,
        observedFrames_,
        emitter.Reference().AliveCount(),
        peakAlive_,
        totals_,
        emitter.Reference().MemoryReport(),
    };
}

ParticleEmitter2DPrepareResult PrepareParticleProgramCpuEmitter(
    const ParticleProgram& program,
    const std::uint64_t globalSeed,
    const ParticleEmitterStableId stableId,
    ParticleEmitter2D& emitter,
    const ParticleReferenceLimits& limits)
{
    auto effect = std::make_shared<ParticleEffectAsset>();
    effect->id = program.effectAssetId;
    effect->semanticId = program.semanticId;
    effect->backend = ParticleEffectBackend::Cpu;
    effect->lifecycle = program.lifecycle;
    effect->definition = program.definition;
    effect->bursts = program.bursts;
    effect->spriteReferences = program.spriteReferences;
    effect->blendMode = program.blendMode;
    return emitter.Prepare(std::move(effect), globalSeed, stableId, limits);
}

std::string_view ToString(const ParticleEffectBackend backend) noexcept
{
    switch (backend)
    {
    case ParticleEffectBackend::Cpu: return "cpu";
    case ParticleEffectBackend::Gpu: return "gpu";
    }
    return "unknown";
}

std::string_view ToString(const ParticleProgramFeature feature) noexcept
{
    switch (feature)
    {
    case ParticleProgramFeature::PeriodicEmission: return "periodic_emission";
    case ParticleProgramFeature::Bursts: return "bursts";
    case ParticleProgramFeature::SpawnPoint: return "spawn_point";
    case ParticleProgramFeature::SpawnBox: return "spawn_box";
    case ParticleProgramFeature::SpawnCircle: return "spawn_circle";
    case ParticleProgramFeature::VariableLifetime: return "variable_lifetime";
    case ParticleProgramFeature::InitialMotion: return "initial_motion";
    case ParticleProgramFeature::Acceleration: return "acceleration";
    case ParticleProgramFeature::VariableInitialSize: return "variable_initial_size";
    case ParticleProgramFeature::SizeOverLife: return "size_over_life";
    case ParticleProgramFeature::InitialRotation: return "initial_rotation";
    case ParticleProgramFeature::AngularVelocity: return "angular_velocity";
    case ParticleProgramFeature::VariableInitialColor: return "variable_initial_color";
    case ParticleProgramFeature::ColorOverLife: return "color_over_life";
    case ParticleProgramFeature::SpriteChoice: return "sprite_choice";
    case ParticleProgramFeature::WorldSpace: return "world_space";
    case ParticleProgramFeature::Looping: return "looping";
    case ParticleProgramFeature::AdditiveBlend: return "additive_blend";
    }
    return "unknown";
}

std::string_view ToString(const ParticleProgramAttribute attribute) noexcept
{
    switch (attribute)
    {
    case ParticleProgramAttribute::SpawnOrdinal: return "spawn_ordinal";
    case ParticleProgramAttribute::Position: return "position";
    case ParticleProgramAttribute::Velocity: return "velocity";
    case ParticleProgramAttribute::Acceleration: return "acceleration";
    case ParticleProgramAttribute::AgeFrames: return "age_frames";
    case ParticleProgramAttribute::LifetimeFrames: return "lifetime_frames";
    case ParticleProgramAttribute::InitialSize: return "initial_size";
    case ParticleProgramAttribute::Size: return "size";
    case ParticleProgramAttribute::Rotation: return "rotation";
    case ParticleProgramAttribute::AngularVelocity: return "angular_velocity";
    case ParticleProgramAttribute::InitialColor: return "initial_color";
    case ParticleProgramAttribute::Color: return "color";
    case ParticleProgramAttribute::SpriteIndex: return "sprite_index";
    }
    return "unknown";
}

std::string_view ToString(const ParticleProgramOperation operation) noexcept
{
    switch (operation)
    {
    case ParticleProgramOperation::SpawnPositionRandom: return "spawn_position_random";
    case ParticleProgramOperation::SpawnLifetimeRandom: return "spawn_lifetime_random";
    case ParticleProgramOperation::SpawnSpeedRandom: return "spawn_speed_random";
    case ParticleProgramOperation::SpawnAngleRandom: return "spawn_angle_random";
    case ParticleProgramOperation::SpawnSizeRandom: return "spawn_size_random";
    case ParticleProgramOperation::SpawnRotationRandom: return "spawn_rotation_random";
    case ParticleProgramOperation::SpawnAngularVelocityRandom: return "spawn_angular_velocity_random";
    case ParticleProgramOperation::SpawnColorRandom: return "spawn_color_random";
    case ParticleProgramOperation::SpawnSpriteRandom: return "spawn_sprite_random";
    case ParticleProgramOperation::ApplyAcceleration: return "apply_acceleration";
    case ParticleProgramOperation::IntegratePosition: return "integrate_position";
    case ParticleProgramOperation::IntegrateRotation: return "integrate_rotation";
    case ParticleProgramOperation::AdvanceLifetime: return "advance_lifetime";
    case ParticleProgramOperation::EvaluateSizeOverLife: return "evaluate_size_over_life";
    case ParticleProgramOperation::EvaluateColorOverLife: return "evaluate_color_over_life";
    case ParticleProgramOperation::Count: break;
    }
    return "unknown";
}

std::string_view ToString(const ParticleGpuRuntimeFieldKind field) noexcept
{
    switch (field)
    {
    case ParticleGpuRuntimeFieldKind::Position: return "position";
    case ParticleGpuRuntimeFieldKind::Velocity: return "velocity";
    case ParticleGpuRuntimeFieldKind::AgeFrames: return "age_frames";
    case ParticleGpuRuntimeFieldKind::LifetimeFrames: return "lifetime_frames";
    case ParticleGpuRuntimeFieldKind::InitialSize: return "initial_size";
    case ParticleGpuRuntimeFieldKind::InitialRotation: return "initial_rotation";
    case ParticleGpuRuntimeFieldKind::AngularVelocity: return "angular_velocity";
    case ParticleGpuRuntimeFieldKind::InitialColor: return "initial_color";
    case ParticleGpuRuntimeFieldKind::SpriteIndex: return "sprite_index";
    }
    return "unknown";
}

std::string_view ToString(const ParticleRandomChannel channel) noexcept
{
    switch (channel)
    {
    case ParticleRandomChannel::SpawnPositionX: return "spawn_position_x";
    case ParticleRandomChannel::SpawnPositionY: return "spawn_position_y";
    case ParticleRandomChannel::Lifetime: return "lifetime";
    case ParticleRandomChannel::Speed: return "speed";
    case ParticleRandomChannel::Angle: return "angle";
    case ParticleRandomChannel::Rotation: return "rotation";
    case ParticleRandomChannel::AngularVelocity: return "angular_velocity";
    case ParticleRandomChannel::Size: return "size";
    case ParticleRandomChannel::ColorR: return "color_r";
    case ParticleRandomChannel::ColorG: return "color_g";
    case ParticleRandomChannel::ColorB: return "color_b";
    case ParticleRandomChannel::ColorA: return "color_a";
    case ParticleRandomChannel::SpriteChoice: return "sprite_choice";
    }
    return "unknown";
}

std::string_view ToString(const ParticleGpuCompileError error) noexcept
{
    switch (error)
    {
    case ParticleGpuCompileError::None: return "none";
    case ParticleGpuCompileError::BackendNotSelected: return "backend_not_selected";
    case ParticleGpuCompileError::UnsupportedFeature: return "unsupported_feature";
    }
    return "unknown";
}
} // namespace trace2d::particles
