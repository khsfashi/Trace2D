#include <trace2d/particles/ParticleReference.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>

namespace trace2d::particles
{
namespace
{
constexpr float TwoPi = 6.28318530717958647692F;
constexpr std::uint32_t ParticleStorageBlockCount = 13U;

[[nodiscard]] bool IsFinite(const float value) noexcept
{
    return std::isfinite(value);
}

[[nodiscard]] bool IsFinite(const ParticleVec2& value) noexcept
{
    return IsFinite(value.x) && IsFinite(value.y);
}

[[nodiscard]] bool IsFinite(const ParticleColor& value) noexcept
{
    return IsFinite(value.r) && IsFinite(value.g) && IsFinite(value.b) && IsFinite(value.a);
}

[[nodiscard]] bool IsOrderedFiniteRange(const ParticleFloatRange& range) noexcept
{
    return IsFinite(range.minValue) && IsFinite(range.maxValue) &&
        range.minValue <= range.maxValue;
}

[[nodiscard]] bool IsNormalizedColor(const ParticleColor& color) noexcept
{
    return IsFinite(color) &&
        color.r >= 0.0F && color.r <= 1.0F &&
        color.g >= 0.0F && color.g <= 1.0F &&
        color.b >= 0.0F && color.b <= 1.0F &&
        color.a >= 0.0F && color.a <= 1.0F;
}

[[nodiscard]] bool IsOrderedColorRange(const ParticleColorRange& range) noexcept
{
    return IsNormalizedColor(range.minValue) && IsNormalizedColor(range.maxValue) &&
        range.minValue.r <= range.maxValue.r &&
        range.minValue.g <= range.maxValue.g &&
        range.minValue.b <= range.maxValue.b &&
        range.minValue.a <= range.maxValue.a;
}

[[nodiscard]] float Lerp(const float from, const float to, const float t) noexcept
{
    return from + ((to - from) * t);
}

[[nodiscard]] ParticleColor Lerp(
    const ParticleColor& from,
    const ParticleColor& to,
    const float t) noexcept
{
    return ParticleColor{
        Lerp(from.r, to.r, t),
        Lerp(from.g, to.g, t),
        Lerp(from.b, to.b, t),
        Lerp(from.a, to.a, t),
    };
}

[[nodiscard]] ParticleReferenceError ValidateDefinition(
    const ParticleReferenceDefinition& definition,
    const std::span<const ParticleBurst> bursts,
    const ParticleReferenceLimits& limits) noexcept
{
    if (definition.maxParticles == 0U)
    {
        return ParticleReferenceError::ZeroCapacity;
    }
    if (definition.maxParticles > limits.maxParticlesPerEmitter)
    {
        return ParticleReferenceError::CapacityExceedsLimit;
    }
    if (bursts.size() > limits.maxBursts)
    {
        return ParticleReferenceError::TooManyBursts;
    }
    if (definition.periodicCount != 0U && definition.periodicEveryFrames == 0U)
    {
        return ParticleReferenceError::InvalidPeriodicEmission;
    }
    if (definition.periodicCount > limits.maxSpawnAttemptsPerFrame)
    {
        return ParticleReferenceError::SpawnAttemptsPerFrameExceedLimit;
    }
    if (definition.lifetimeFrames.minValue == 0U ||
        definition.lifetimeFrames.minValue > definition.lifetimeFrames.maxValue)
    {
        return ParticleReferenceError::InvalidLifetimeRange;
    }
    if (!IsOrderedFiniteRange(definition.speed) ||
        !IsOrderedFiniteRange(definition.angleRadians) ||
        !IsOrderedFiniteRange(definition.initialSize) ||
        !IsOrderedFiniteRange(definition.rotationRadians) ||
        !IsOrderedFiniteRange(definition.angularVelocityRadiansPerFrame) ||
        definition.initialSize.minValue < 0.0F ||
        !IsFinite(definition.endSizeMultiplier) ||
        definition.endSizeMultiplier < 0.0F ||
        !IsFinite(definition.acceleration))
    {
        return ParticleReferenceError::InvalidFloatRange;
    }
    if (!IsOrderedColorRange(definition.initialColor) ||
        !IsNormalizedColor(definition.endColor))
    {
        return ParticleReferenceError::InvalidColorRange;
    }
    if (definition.spriteChoiceCount == 0U)
    {
        return ParticleReferenceError::InvalidSpriteChoiceCount;
    }

    switch (definition.spawnShape.type)
    {
    case ParticleSpawnShapeType::Point:
        if (!IsFinite(definition.spawnShape.offset))
        {
            return ParticleReferenceError::InvalidSpawnShape;
        }
        break;
    case ParticleSpawnShapeType::Box:
        if (!IsFinite(definition.spawnShape.offset) ||
            !IsFinite(definition.spawnShape.boxHalfExtents) ||
            definition.spawnShape.boxHalfExtents.x < 0.0F ||
            definition.spawnShape.boxHalfExtents.y < 0.0F)
        {
            return ParticleReferenceError::InvalidSpawnShape;
        }
        break;
    case ParticleSpawnShapeType::Circle:
        if (!IsFinite(definition.spawnShape.offset) ||
            !IsFinite(definition.spawnShape.circleRadius) ||
            definition.spawnShape.circleRadius < 0.0F)
        {
            return ParticleReferenceError::InvalidSpawnShape;
        }
        break;
    default:
        return ParticleReferenceError::InvalidSpawnShape;
    }

    ParticleFrameIndex previousFrame = 0;
    bool hasPreviousFrame = false;
    std::size_t index = 0;
    while (index < bursts.size())
    {
        const ParticleFrameIndex frame = bursts[index].frame;
        if (hasPreviousFrame && frame < previousFrame)
        {
            return ParticleReferenceError::BurstsNotOrdered;
        }
        previousFrame = frame;
        hasPreviousFrame = true;

        std::uint64_t attemptsOnFrame = 0;
        while (index < bursts.size() && bursts[index].frame == frame)
        {
            attemptsOnFrame += bursts[index].count;
            ++index;
        }

        if (definition.periodicCount != 0U &&
            frame >= definition.periodicStartFrame &&
            ((frame - definition.periodicStartFrame) % definition.periodicEveryFrames) == 0U)
        {
            attemptsOnFrame += definition.periodicCount;
        }

        if (attemptsOnFrame > limits.maxSpawnAttemptsPerFrame)
        {
            return ParticleReferenceError::SpawnAttemptsPerFrameExceedLimit;
        }
    }

    return ParticleReferenceError::None;
}
} // namespace

ParticleReferencePrepareResult ParticleReferenceEmitter::Prepare(
    const ParticleReferenceDefinition& definition,
    const std::span<const ParticleBurst> bursts,
    const ParticleReferenceLimits& limits) noexcept
{
    const ParticleReferenceError validation = ValidateDefinition(definition, bursts, limits);
    if (validation != ParticleReferenceError::None)
    {
        return ParticleReferencePrepareResult{validation};
    }

    Storage newStorage{};
    std::unique_ptr<ParticleBurst[]> newBursts{};
    try
    {
        const std::size_t capacity = definition.maxParticles;
        newStorage.spawnOrdinals = std::make_unique<ParticleSpawnOrdinal[]>(capacity);
        newStorage.positions = std::make_unique<ParticleVec2[]>(capacity);
        newStorage.velocities = std::make_unique<ParticleVec2[]>(capacity);
        newStorage.accelerations = std::make_unique<ParticleVec2[]>(capacity);
        newStorage.ageFrames = std::make_unique<std::uint32_t[]>(capacity);
        newStorage.lifetimeFrames = std::make_unique<std::uint32_t[]>(capacity);
        newStorage.initialSizes = std::make_unique<float[]>(capacity);
        newStorage.sizes = std::make_unique<float[]>(capacity);
        newStorage.rotations = std::make_unique<float[]>(capacity);
        newStorage.angularVelocities = std::make_unique<float[]>(capacity);
        newStorage.initialColors = std::make_unique<ParticleColor[]>(capacity);
        newStorage.colors = std::make_unique<ParticleColor[]>(capacity);
        newStorage.spriteIndices = std::make_unique<std::uint32_t[]>(capacity);

        if (!bursts.empty())
        {
            newBursts = std::make_unique<ParticleBurst[]>(bursts.size());
            std::copy(bursts.begin(), bursts.end(), newBursts.get());
        }
    }
    catch (const std::bad_alloc&)
    {
        return ParticleReferencePrepareResult{ParticleReferenceError::AllocationFailed};
    }

    definition_ = definition;
    limits_ = limits;
    storage_ = std::move(newStorage);
    bursts_ = std::move(newBursts);
    burstCount_ = static_cast<std::uint32_t>(bursts.size());
    prepared_ = true;
    Reset();
    return ParticleReferencePrepareResult{};
}

void ParticleReferenceEmitter::Reset() noexcept
{
    nextBurstIndex_ = 0;
    aliveCount_ = 0;
    nextFrameIndex_ = 0;
    nextSpawnOrdinal_ = 0;
    counters_ = ParticleReferenceCounters{};
}

bool ParticleReferenceEmitter::Step() noexcept
{
    if (!prepared_)
    {
        return false;
    }

    UpdateExisting();
    EmitCurrentFrame();
    ++nextFrameIndex_;
    return true;
}

bool ParticleReferenceEmitter::IsPrepared() const noexcept
{
    return prepared_;
}

const ParticleReferenceDefinition& ParticleReferenceEmitter::Definition() const noexcept
{
    return definition_;
}

ParticleFrameIndex ParticleReferenceEmitter::NextFrameIndex() const noexcept
{
    return nextFrameIndex_;
}

std::uint32_t ParticleReferenceEmitter::AliveCount() const noexcept
{
    return aliveCount_;
}

ParticleSpawnOrdinal ParticleReferenceEmitter::NextSpawnOrdinal() const noexcept
{
    return nextSpawnOrdinal_;
}

const ParticleReferenceCounters& ParticleReferenceEmitter::Counters() const noexcept
{
    return counters_;
}

ParticleReferenceMemoryReport ParticleReferenceEmitter::MemoryReport() const noexcept
{
    if (!prepared_)
    {
        return ParticleReferenceMemoryReport{};
    }

    const std::size_t particleBytes =
        static_cast<std::size_t>(definition_.maxParticles) * BytesPerParticlePayload();
    const std::size_t burstBytes = static_cast<std::size_t>(burstCount_) * sizeof(ParticleBurst);
    return ParticleReferenceMemoryReport{
        definition_.maxParticles,
        BytesPerParticlePayload(),
        particleBytes,
        burstBytes,
        particleBytes + burstBytes,
        ParticleStorageBlockCount + (burstCount_ == 0U ? 0U : 1U),
        0U,
    };
}

bool ParticleReferenceEmitter::TryGetParticle(
    const std::uint32_t aliveIndex,
    ParticleReferenceParticle& particle) const noexcept
{
    if (!prepared_ || aliveIndex >= aliveCount_)
    {
        return false;
    }

    particle = ReadParticle(aliveIndex);
    return true;
}

bool ParticleReferenceEmitter::TryGetParticleBySpawnOrdinal(
    const ParticleSpawnOrdinal spawnOrdinal,
    ParticleReferenceParticle& particle) const noexcept
{
    if (!prepared_)
    {
        return false;
    }

    for (std::uint32_t index = 0; index < aliveCount_; ++index)
    {
        if (storage_.spawnOrdinals[index] == spawnOrdinal)
        {
            particle = ReadParticle(index);
            return true;
        }
    }
    return false;
}

ParticleRandomKey ParticleReferenceEmitter::MakeRandomKey(
    const ParticleSpawnOrdinal ordinal,
    const ParticleRandomChannel channel) const noexcept
{
    return ParticleRandomKey{
        definition_.globalSeed,
        definition_.emitterStableId,
        ordinal,
        channel,
    };
}

float ParticleReferenceEmitter::SampleFloat(
    const ParticleSpawnOrdinal ordinal,
    const ParticleRandomChannel channel,
    const ParticleFloatRange& range) const noexcept
{
    if (range.minValue == range.maxValue)
    {
        return range.minValue;
    }
    return ParticleRandomFloatRange(MakeRandomKey(ordinal, channel), range.minValue, range.maxValue);
}

std::uint32_t ParticleReferenceEmitter::SampleUIntInclusive(
    const ParticleSpawnOrdinal ordinal,
    const ParticleRandomChannel channel,
    const ParticleUIntRange& range) const noexcept
{
    if (range.minValue == range.maxValue)
    {
        return range.minValue;
    }

    const std::uint64_t width =
        static_cast<std::uint64_t>(range.maxValue) - range.minValue + 1ULL;
    const std::uint64_t randomValue = ParticleRandomU32(MakeRandomKey(ordinal, channel));
    const std::uint32_t offset = static_cast<std::uint32_t>((randomValue * width) >> 32U);
    return range.minValue + offset;
}

ParticleColor ParticleReferenceEmitter::SampleInitialColor(
    const ParticleSpawnOrdinal ordinal) const noexcept
{
    const ParticleColorRange& range = definition_.initialColor;
    const auto sampleComponent = [this, ordinal](
                                     const float minValue,
                                     const float maxValue,
                                     const ParticleRandomChannel channel) noexcept
    {
        if (minValue == maxValue)
        {
            return minValue;
        }
        return ParticleRandomFloatRange(MakeRandomKey(ordinal, channel), minValue, maxValue);
    };

    return ParticleColor{
        sampleComponent(range.minValue.r, range.maxValue.r, ParticleRandomChannel::ColorR),
        sampleComponent(range.minValue.g, range.maxValue.g, ParticleRandomChannel::ColorG),
        sampleComponent(range.minValue.b, range.maxValue.b, ParticleRandomChannel::ColorB),
        sampleComponent(range.minValue.a, range.maxValue.a, ParticleRandomChannel::ColorA),
    };
}

ParticleVec2 ParticleReferenceEmitter::SampleSpawnPosition(
    const ParticleSpawnOrdinal ordinal) const noexcept
{
    const ParticleSpawnShape& shape = definition_.spawnShape;
    switch (shape.type)
    {
    case ParticleSpawnShapeType::Point:
        return shape.offset;
    case ParticleSpawnShapeType::Box:
        return ParticleVec2{
            shape.offset.x + ParticleRandomFloatRange(
                MakeRandomKey(ordinal, ParticleRandomChannel::SpawnPositionX),
                -shape.boxHalfExtents.x,
                shape.boxHalfExtents.x),
            shape.offset.y + ParticleRandomFloatRange(
                MakeRandomKey(ordinal, ParticleRandomChannel::SpawnPositionY),
                -shape.boxHalfExtents.y,
                shape.boxHalfExtents.y),
        };
    case ParticleSpawnShapeType::Circle:
    {
        const float angle =
            ParticleRandomUnitFloat(MakeRandomKey(ordinal, ParticleRandomChannel::SpawnPositionX)) *
            TwoPi;
        const float radialUnit =
            ParticleRandomUnitFloat(MakeRandomKey(ordinal, ParticleRandomChannel::SpawnPositionY));
        const float radius = std::sqrt(radialUnit) * shape.circleRadius;
        return ParticleVec2{
            shape.offset.x + (std::cos(angle) * radius),
            shape.offset.y + (std::sin(angle) * radius),
        };
    }
    default:
        return shape.offset;
    }
}

ParticleVec2 ParticleReferenceEmitter::SampleInitialVelocity(
    const ParticleSpawnOrdinal ordinal) const noexcept
{
    const float speed = SampleFloat(ordinal, ParticleRandomChannel::Speed, definition_.speed);
    const float angle = SampleFloat(ordinal, ParticleRandomChannel::Angle, definition_.angleRadians);
    return ParticleVec2{std::cos(angle) * speed, std::sin(angle) * speed};
}

void ParticleReferenceEmitter::UpdateExisting() noexcept
{
    std::uint32_t writeIndex = 0;
    const std::uint32_t originalAliveCount = aliveCount_;
    for (std::uint32_t readIndex = 0; readIndex < originalAliveCount; ++readIndex)
    {
        ParticleVec2& velocity = storage_.velocities[readIndex];
        const ParticleVec2& acceleration = storage_.accelerations[readIndex];
        velocity.x += acceleration.x;
        velocity.y += acceleration.y;

        ParticleVec2& position = storage_.positions[readIndex];
        position.x += velocity.x;
        position.y += velocity.y;
        storage_.rotations[readIndex] += storage_.angularVelocities[readIndex];

        const ParticleLifetimeTransition lifetime = AdvanceExistingParticleLifetime(
            storage_.ageFrames[readIndex],
            storage_.lifetimeFrames[readIndex]);
        storage_.ageFrames[readIndex] = lifetime.ageAfterUpdate;
        ++counters_.updated;

        if (lifetime.expiresBeforeObservation)
        {
            ++counters_.expired;
            continue;
        }

        ApplyOverLife(readIndex);
        if (writeIndex != readIndex)
        {
            CopyParticle(readIndex, writeIndex);
        }
        ++writeIndex;
    }
    aliveCount_ = writeIndex;
}

void ParticleReferenceEmitter::EmitCurrentFrame() noexcept
{
    while (nextBurstIndex_ < burstCount_ && bursts_[nextBurstIndex_].frame == nextFrameIndex_)
    {
        const std::uint32_t count = bursts_[nextBurstIndex_].count;
        for (std::uint32_t attempt = 0; attempt < count; ++attempt)
        {
            AttemptSpawn();
        }
        ++nextBurstIndex_;
    }

    if (definition_.periodicCount != 0U &&
        nextFrameIndex_ >= definition_.periodicStartFrame &&
        ((nextFrameIndex_ - definition_.periodicStartFrame) % definition_.periodicEveryFrames) == 0U)
    {
        for (std::uint32_t attempt = 0; attempt < definition_.periodicCount; ++attempt)
        {
            AttemptSpawn();
        }
    }
}

void ParticleReferenceEmitter::AttemptSpawn() noexcept
{
    const ParticleSpawnOrdinal ordinal = nextSpawnOrdinal_;
    ++nextSpawnOrdinal_;
    ++counters_.spawnAttempts;

    if (aliveCount_ >= definition_.maxParticles)
    {
        ++counters_.dropped;
        return;
    }

    const std::uint32_t index = aliveCount_;
    storage_.spawnOrdinals[index] = ordinal;
    storage_.positions[index] = SampleSpawnPosition(ordinal);
    storage_.velocities[index] = SampleInitialVelocity(ordinal);
    storage_.accelerations[index] = definition_.acceleration;
    storage_.ageFrames[index] = 0U;
    storage_.lifetimeFrames[index] = SampleUIntInclusive(
        ordinal,
        ParticleRandomChannel::Lifetime,
        definition_.lifetimeFrames);
    storage_.initialSizes[index] = SampleFloat(
        ordinal,
        ParticleRandomChannel::Size,
        definition_.initialSize);
    storage_.sizes[index] = storage_.initialSizes[index];
    storage_.rotations[index] = SampleFloat(
        ordinal,
        ParticleRandomChannel::Rotation,
        definition_.rotationRadians);
    storage_.angularVelocities[index] = SampleFloat(
        ordinal,
        ParticleRandomChannel::AngularVelocity,
        definition_.angularVelocityRadiansPerFrame);
    storage_.initialColors[index] = SampleInitialColor(ordinal);
    storage_.colors[index] = storage_.initialColors[index];
    storage_.spriteIndices[index] = SampleUIntInclusive(
        ordinal,
        ParticleRandomChannel::SpriteChoice,
        ParticleUIntRange{0U, definition_.spriteChoiceCount - 1U});

    ++aliveCount_;
    ++counters_.spawned;
    counters_.peakAlive = std::max(counters_.peakAlive, aliveCount_);
}

void ParticleReferenceEmitter::ApplyOverLife(const std::uint32_t index) noexcept
{
    const std::uint32_t lifetime = storage_.lifetimeFrames[index];
    const std::uint32_t age = storage_.ageFrames[index];
    const float t = lifetime <= 1U
        ? 0.0F
        : static_cast<float>(age) / static_cast<float>(lifetime - 1U);

    const float sizeMultiplier = Lerp(1.0F, definition_.endSizeMultiplier, t);
    storage_.sizes[index] = storage_.initialSizes[index] * sizeMultiplier;
    storage_.colors[index] = Lerp(storage_.initialColors[index], definition_.endColor, t);
}

void ParticleReferenceEmitter::CopyParticle(
    const std::uint32_t fromIndex,
    const std::uint32_t toIndex) noexcept
{
    storage_.spawnOrdinals[toIndex] = storage_.spawnOrdinals[fromIndex];
    storage_.positions[toIndex] = storage_.positions[fromIndex];
    storage_.velocities[toIndex] = storage_.velocities[fromIndex];
    storage_.accelerations[toIndex] = storage_.accelerations[fromIndex];
    storage_.ageFrames[toIndex] = storage_.ageFrames[fromIndex];
    storage_.lifetimeFrames[toIndex] = storage_.lifetimeFrames[fromIndex];
    storage_.initialSizes[toIndex] = storage_.initialSizes[fromIndex];
    storage_.sizes[toIndex] = storage_.sizes[fromIndex];
    storage_.rotations[toIndex] = storage_.rotations[fromIndex];
    storage_.angularVelocities[toIndex] = storage_.angularVelocities[fromIndex];
    storage_.initialColors[toIndex] = storage_.initialColors[fromIndex];
    storage_.colors[toIndex] = storage_.colors[fromIndex];
    storage_.spriteIndices[toIndex] = storage_.spriteIndices[fromIndex];
}

ParticleReferenceParticle ParticleReferenceEmitter::ReadParticle(
    const std::uint32_t aliveIndex) const noexcept
{
    return ParticleReferenceParticle{
        storage_.spawnOrdinals[aliveIndex],
        storage_.positions[aliveIndex],
        storage_.velocities[aliveIndex],
        storage_.accelerations[aliveIndex],
        storage_.ageFrames[aliveIndex],
        storage_.lifetimeFrames[aliveIndex],
        storage_.initialSizes[aliveIndex],
        storage_.sizes[aliveIndex],
        storage_.rotations[aliveIndex],
        storage_.angularVelocities[aliveIndex],
        storage_.initialColors[aliveIndex],
        storage_.colors[aliveIndex],
        storage_.spriteIndices[aliveIndex],
        definition_.simulationSpace,
    };
}
} // namespace trace2d::particles
