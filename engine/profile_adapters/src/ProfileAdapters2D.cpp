#include <trace2d/profile_adapters/ProfileAdapters2D.hpp>

#include <limits>

namespace trace2d::profile_adapters
{
namespace
{
using Availability = profile::ProfileMetricAvailability2D;
using Kind = profile::StructuralProfileMetricKind2D;
using Result = profile::StructuralProfileResult2D;

[[nodiscard]] constexpr Availability AvailabilityOf(const bool measured) noexcept
{
    return measured ? Availability::Available : Availability::NotMeasured;
}

[[nodiscard]] constexpr std::uint64_t ToU64(const std::size_t value) noexcept
{
    static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
    return static_cast<std::uint64_t>(value);
}

[[nodiscard]] bool AddSizeChecked(std::uint64_t& total, const std::size_t value) noexcept
{
    const auto converted = ToU64(value);
    if (total > std::numeric_limits<std::uint64_t>::max() - converted)
    {
        return false;
    }

    total += converted;
    return true;
}
} // namespace

profile::StructuralProfileResult2D ComposeStructuralProfile2D(
    profile::StructuralProfileSnapshot2D& snapshot,
    const StructuralProfileInputs2D& inputs) noexcept
{
    if (!snapshot.Prepared())
    {
        return Result::NotPrepared;
    }
    if (snapshot.StorageMetrics().retainedMetricCapacity < StructuralProfileAdapterMetricCount2D)
    {
        return Result::MetricCapacityExceeded;
    }

    std::uint64_t retainedCpuBytes = 0U;
    std::uint64_t retainedContainerBytes = 0U;
    std::uint64_t rendererGpuBytes = 0U;
    if (inputs.resourceMemoryMeasured)
    {
        for (const auto& resource : inputs.resourceSnapshots)
        {
            if (!AddSizeChecked(retainedCpuBytes, resource.memory.knownRetainedCpuBytes) ||
                !AddSizeChecked(retainedContainerBytes, resource.memory.retainedContainerCapacityBytes) ||
                !AddSizeChecked(rendererGpuBytes, resource.memory.knownRendererGpuBytes))
            {
                return Result::ValueOverflow;
            }
        }
    }

    const auto clearResult = snapshot.Clear();
    if (clearResult != Result::Success)
    {
        return clearResult;
    }

    Result result = Result::Success;
    const auto append = [&snapshot, &result](
        const std::string_view name,
        const std::string_view unit,
        const Kind kind,
        const bool measured,
        const std::uint64_t value) noexcept
    {
        if (result == Result::Success)
        {
            result = snapshot.AddMetric(name, unit, kind, AvailabilityOf(measured), value);
        }
    };

    const auto* renderer = inputs.renderer;
    const bool renderMeasured = renderer != nullptr;
    append("render.frame.submitted", "{frame}", Kind::Counter, renderMeasured, renderer ? renderer->submittedFrames : 0U);
    append("render.frame.presented", "{frame}", Kind::Counter, renderMeasured, renderer ? renderer->presentedFrames : 0U);
    append("render.pass.count", "{pass}", Kind::Counter, renderMeasured, renderer ? renderer->renderPasses : 0U);
    append("render.draw.count", "{draw}", Kind::Counter, renderMeasured, renderer ? renderer->drawCalls : 0U);
    append("render.sprite.submitted", "{sprite}", Kind::Counter, renderMeasured, renderer ? renderer->submittedSprites : 0U);
    append("render.sprite.culled", "{sprite}", Kind::Counter, renderMeasured, renderer ? renderer->culledSprites : 0U);
    append("render.sprite.presentation.draw", "{draw}", Kind::Counter, renderMeasured, renderer ? renderer->spritePresentationDrawCalls : 0U);
    append("render.sprite.presentation.visible", "{sprite}", Kind::Counter, renderMeasured, renderer ? renderer->spritePresentationVisibleSprites : 0U);
    append("render.sprite.upload.vertex_bytes", "By", Kind::Counter, renderMeasured, renderer ? renderer->spritePresentationUploadedVertexBytes : 0U);
    append("render.material.pipeline.switch", "{switch}", Kind::Counter, renderMeasured, renderer ? renderer->materialPipelineSwitches : 0U);
    append("render.material.uniform.upload_bytes", "By", Kind::Counter, renderMeasured, renderer ? renderer->fragmentUniformUploadBytes : 0U);
    append("render.material.pipeline.bundle.count", "{bundle}", Kind::Gauge, renderMeasured, renderer ? renderer->materialPreparedPipelineBundles : 0U);
    append("render.material.pipeline.bundle.capacity", "{bundle}", Kind::Gauge, renderMeasured, renderer ? renderer->materialPreparedPipelineBundleCapacity : 0U);
    append("render.sprite.vertex.capacity", "{sprite}", Kind::Gauge, renderMeasured, renderer ? renderer->spriteVertexCapacitySprites : 0U);
    append("render.sprite.vertex.capacity_bytes", "By", Kind::Gauge, renderMeasured, renderer ? renderer->spriteVertexCapacityBytes : 0U);
    append("render.target.offscreen.count", "{target}", Kind::Gauge, renderMeasured, renderer ? renderer->retainedOffscreenColorTargetCount : 0U);
    append("render.target.offscreen.bytes", "By", Kind::Gauge, renderMeasured, renderer ? renderer->retainedOffscreenColorTargetBytes : 0U);
    append("render.target.width", "{pixel}", Kind::Gauge, renderMeasured, renderer ? renderer->lastTargetWidth : 0U);
    append("render.target.height", "{pixel}", Kind::Gauge, renderMeasured, renderer ? renderer->lastTargetHeight : 0U);
    append("render.gpu.readback.count", "{readback}", Kind::Counter, renderMeasured, renderer ? renderer->explicitGpuReadbacks : 0U);

    const auto* physics = inputs.physics;
    const bool physicsMeasured = physics != nullptr;
    append("physics.body.attached", "{body}", Kind::Gauge, physicsMeasured, physics ? ToU64(physics->attachedBodyCount) : 0U);
    append("physics.body.capacity", "{body}", Kind::Gauge, physicsMeasured, physics ? ToU64(physics->retainedBodyCapacity) : 0U);
    append("physics.ray.hit.capacity", "{hit}", Kind::Gauge, physicsMeasured, physics ? ToU64(physics->retainedRayHitCapacity) : 0U);
    append("physics.overlap.hit.capacity", "{hit}", Kind::Gauge, physicsMeasured, physics ? ToU64(physics->retainedOverlapHitCapacity) : 0U);
    append("physics.shape_cast.hit.capacity", "{hit}", Kind::Gauge, physicsMeasured, physics ? ToU64(physics->retainedShapeCastHitCapacity) : 0U);
    append("physics.contact.event.capacity", "{event}", Kind::Gauge, physicsMeasured, physics ? ToU64(physics->retainedContactEventCapacity) : 0U);
    append("physics.sensor.event.capacity", "{event}", Kind::Gauge, physicsMeasured, physics ? ToU64(physics->retainedSensorEventCapacity) : 0U);
    append("physics.contact.event.published", "{event}", Kind::Gauge, physicsMeasured, physics ? ToU64(physics->publishedContactEventCount) : 0U);
    append("physics.sensor.event.published", "{event}", Kind::Gauge, physicsMeasured, physics ? ToU64(physics->publishedSensorEventCount) : 0U);
    append("physics.step.fixed", "{step}", Kind::Counter, physicsMeasured, physics ? physics->fixedStepCount : 0U);
    append("physics.body.command", "{command}", Kind::Counter, physicsMeasured, physics ? physics->bodyCommandCount : 0U);
    append("physics.body.command.failure", "{failure}", Kind::Counter, physicsMeasured, physics ? physics->bodyCommandFailureCount : 0U);
    append("physics.ray.query", "{query}", Kind::Counter, physicsMeasured, physics ? physics->rayQueryCount : 0U);
    append("physics.ray.capacity_failure", "{failure}", Kind::Counter, physicsMeasured, physics ? physics->rayCapacityFailureCount : 0U);
    append("physics.overlap.query", "{query}", Kind::Counter, physicsMeasured, physics ? physics->overlapQueryCount : 0U);
    append("physics.overlap.capacity_failure", "{failure}", Kind::Counter, physicsMeasured, physics ? physics->overlapCapacityFailureCount : 0U);
    append("physics.shape_cast.query", "{query}", Kind::Counter, physicsMeasured, physics ? physics->shapeCastQueryCount : 0U);
    append("physics.shape_cast.capacity_failure", "{failure}", Kind::Counter, physicsMeasured, physics ? physics->shapeCastCapacityFailureCount : 0U);
    append("physics.event.capacity_failure", "{failure}", Kind::Counter, physicsMeasured, physics ? physics->eventCapacityFailureCount : 0U);

    const auto* audio = inputs.audio;
    const bool audioMeasured = audio != nullptr;
    append("audio.voice.capacity", "{voice}", Kind::Gauge, audioMeasured, audio ? ToU64(audio->retainedVoiceCapacity) : 0U);
    append("audio.event.capacity", "{event}", Kind::Gauge, audioMeasured, audio ? ToU64(audio->retainedEventCapacity) : 0U);
    append("audio.voice.active", "{voice}", Kind::Gauge, audioMeasured, audio ? ToU64(audio->activeVoiceCount) : 0U);
    append("audio.voice.high_watermark", "{voice}", Kind::Gauge, audioMeasured, audio ? ToU64(audio->voiceHighWatermark) : 0U);
    append("audio.event.published", "{event}", Kind::Gauge, audioMeasured, audio ? ToU64(audio->publishedEventCount) : 0U);
    append("audio.command.count", "{command}", Kind::Counter, audioMeasured, audio ? audio->commandCount : 0U);
    append("audio.command.failure", "{failure}", Kind::Counter, audioMeasured, audio ? audio->commandFailureCount : 0U);
    append("audio.step.count", "{step}", Kind::Counter, audioMeasured, audio ? audio->stepCount : 0U);
    append("audio.voice.stolen", "{voice}", Kind::Counter, audioMeasured, audio ? audio->stolenVoiceCount : 0U);
    append("audio.voice.limit_reject", "{voice}", Kind::Counter, audioMeasured, audio ? audio->voiceLimitRejectCount : 0U);
    append("audio.event.capacity_failure", "{failure}", Kind::Counter, audioMeasured, audio ? audio->eventCapacityFailureCount : 0U);
    append("audio.loop.event", "{event}", Kind::Counter, audioMeasured, audio ? audio->loopEventCount : 0U);
    append("audio.completion.event", "{event}", Kind::Counter, audioMeasured, audio ? audio->completionEventCount : 0U);
    append("audio.voice.detached", "{voice}", Kind::Counter, audioMeasured, audio ? audio->detachedVoiceCount : 0U);

    const auto* output = inputs.audioOutput;
    const bool outputMeasured = output != nullptr;
    append("audio.output.voice.capacity", "{voice}", Kind::Gauge, outputMeasured, output ? ToU64(output->configuredVoiceCapacity) : 0U);
    append("audio.output.voice.tracked", "{voice}", Kind::Gauge, outputMeasured, output ? ToU64(output->trackedVoiceCount) : 0U);
    append("audio.output.stream.active", "{stream}", Kind::Gauge, outputMeasured, output ? ToU64(output->activeStreamCount) : 0U);
    append("audio.output.voice.streaming", "{voice}", Kind::Gauge, outputMeasured, output ? ToU64(output->streamingVoiceCount) : 0U);
    append("audio.output.cache.preload.count", "{entry}", Kind::Gauge, outputMeasured, output ? ToU64(output->preloadCacheEntryCount) : 0U);
    append("audio.output.cache.preload.capacity", "{entry}", Kind::Gauge, outputMeasured, output ? ToU64(output->preloadCacheCapacity) : 0U);
    append("audio.output.memory.preload.bytes", "By", Kind::Gauge, outputMeasured, output ? ToU64(output->trace2dOwnedPreloadPcmBytes) : 0U);
    append("audio.output.memory.preload.capacity_bytes", "By", Kind::Gauge, outputMeasured, output ? ToU64(output->trace2dOwnedPreloadPcmCapacityBytes) : 0U);
    append("audio.output.memory.refill.bytes", "By", Kind::Gauge, outputMeasured, output ? ToU64(output->trace2dOwnedRefillBytes) : 0U);
    append("audio.output.memory.refill.capacity_bytes", "By", Kind::Gauge, outputMeasured, output ? ToU64(output->trace2dOwnedRefillCapacityBytes) : 0U);
    append("audio.output.queue.input_bytes", "By", Kind::Gauge, outputMeasured, output ? output->queuedInputBytes : 0U);
    append("audio.output.device.open", "{open}", Kind::Counter, outputMeasured, output ? output->deviceOpenCount : 0U);
    append("audio.output.device.loss", "{event}", Kind::Counter, outputMeasured, output ? output->deviceLossEventCount : 0U);
    append("audio.output.device.format_change", "{event}", Kind::Counter, outputMeasured, output ? output->deviceFormatChangeEventCount : 0U);
    append("audio.output.recovery", "{recovery}", Kind::Counter, outputMeasured, output ? output->recoveryCount : 0U);
    append("audio.output.stream.create", "{stream}", Kind::Counter, outputMeasured, output ? output->streamCreateCount : 0U);
    append("audio.output.stream.destroy", "{stream}", Kind::Counter, outputMeasured, output ? output->streamDestroyCount : 0U);
    append("audio.output.refill.call", "{call}", Kind::Counter, outputMeasured, output ? output->refillCallCount : 0U);
    append("audio.output.refill.frame", "{frame}", Kind::Counter, outputMeasured, output ? output->refillFrameCount : 0U);
    append("audio.output.backend.failure", "{failure}", Kind::Counter, outputMeasured, output ? output->backendFailureCount : 0U);

    const auto* resources = inputs.resources;
    const bool resourcesMeasured = resources != nullptr;
    append("resource.ready", "{resource}", Kind::Gauge, resourcesMeasured, resources ? ToU64(resources->readyResources) : 0U);
    append("resource.error", "{resource}", Kind::Gauge, resourcesMeasured, resources ? ToU64(resources->errorResources) : 0U);
    append("resource.canonicalization", "{call}", Kind::Counter, resourcesMeasured, resources ? resources->canonicalizationCalls : 0U);
    append("resource.duplicate_ready_load", "{load}", Kind::Counter, resourcesMeasured, resources ? resources->duplicateReadyLoads : 0U);
    append("resource.failed_load_record", "{record}", Kind::Counter, resourcesMeasured, resources ? resources->failedLoadRecords : 0U);
    append("resource.unload", "{resource}", Kind::Counter, resourcesMeasured, resources ? resources->unloads : 0U);
    append("resource.filesystem_query", "{query}", Kind::Counter, resourcesMeasured, resources ? resources->filesystemQueries : 0U);
    append("resource.memory.cpu.retained_bytes", "By", Kind::Gauge, inputs.resourceMemoryMeasured, retainedCpuBytes);
    append("resource.memory.container.capacity_bytes", "By", Kind::Gauge, inputs.resourceMemoryMeasured, retainedContainerBytes);
    append("resource.memory.renderer_gpu_bytes", "By", Kind::Gauge, inputs.resourceMemoryMeasured, rendererGpuBytes);
    append("resource.snapshot.count", "{resource}", Kind::Gauge, inputs.resourceMemoryMeasured, ToU64(inputs.resourceSnapshots.size()));

    const auto& particle = inputs.particleReference;
    const bool particleCounterMeasured = particle.measured && particle.counters != nullptr;
    const bool particleMemoryMeasured = particle.measured && particle.memory != nullptr;
    append("particle.reference.alive", "{particle}", Kind::Gauge, particle.measured, particle.aliveCount);
    append("particle.reference.spawn_attempt", "{particle}", Kind::Counter, particleCounterMeasured, particle.counters ? particle.counters->spawnAttempts : 0U);
    append("particle.reference.spawned", "{particle}", Kind::Counter, particleCounterMeasured, particle.counters ? particle.counters->spawned : 0U);
    append("particle.reference.updated", "{particle}", Kind::Counter, particleCounterMeasured, particle.counters ? particle.counters->updated : 0U);
    append("particle.reference.expired", "{particle}", Kind::Counter, particleCounterMeasured, particle.counters ? particle.counters->expired : 0U);
    append("particle.reference.dropped", "{particle}", Kind::Counter, particleCounterMeasured, particle.counters ? particle.counters->dropped : 0U);
    append("particle.reference.peak_alive", "{particle}", Kind::Gauge, particleCounterMeasured, particle.counters ? particle.counters->peakAlive : 0U);
    append("particle.reference.capacity", "{particle}", Kind::Gauge, particleMemoryMeasured, particle.memory ? particle.memory->capacity : 0U);
    append("particle.reference.memory.particle_storage_bytes", "By", Kind::Gauge, particleMemoryMeasured, particle.memory ? ToU64(particle.memory->particleStorageBytes) : 0U);
    append("particle.reference.memory.burst_schedule_bytes", "By", Kind::Gauge, particleMemoryMeasured, particle.memory ? ToU64(particle.memory->burstScheduleBytes) : 0U);
    append("particle.reference.memory.prepared_payload_bytes", "By", Kind::Gauge, particleMemoryMeasured, particle.memory ? ToU64(particle.memory->preparedPayloadBytes) : 0U);
    append("particle.reference.memory.bytes_per_particle", "By", Kind::Gauge, particleMemoryMeasured, particle.memory ? ToU64(particle.memory->bytesPerParticlePayload) : 0U);
    append("particle.reference.allocation.steady_state", "{allocation}", Kind::Gauge, particleMemoryMeasured, particle.memory ? particle.memory->steadyStateSimulationAllocations : 0U);

    if (result == Result::Success && snapshot.Metrics().size() != StructuralProfileAdapterMetricCount2D)
    {
        return Result::MetricCapacityExceeded;
    }

    return result;
}
} // namespace trace2d::profile_adapters
