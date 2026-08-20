#include "AudioOutput2DInternal.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace trace2d::audio
{
namespace
{
[[nodiscard]] bool IsTerminalEvent(const AudioEventType2D type) noexcept
{
    return type == AudioEventType2D::Stopped || type == AudioEventType2D::Stolen ||
        type == AudioEventType2D::Finished || type == AudioEventType2D::Detached;
}
}

std::optional<std::size_t> AudioOutput2D::Impl::FindVoice(const AudioVoiceHandle2D handle) const noexcept
{
    for (std::size_t index = 0U; index < voices_.size(); ++index)
    {
        if (voices_[index].handle == handle)
        {
            return index;
        }
    }
    return std::nullopt;
}

AudioOutput2D::Impl::PreloadCacheEntry* AudioOutput2D::Impl::FindPreload(
    const assets::ResourceHandle<assets::AudioClipResource> clip) noexcept
{
    const auto found = std::find_if(
        preloadCache_.begin(),
        preloadCache_.end(),
        [clip](const PreloadCacheEntry& entry) { return entry.clip == clip; });
    return found == preloadCache_.end() ? nullptr : &*found;
}

const AudioOutput2D::Impl::PreloadCacheEntry* AudioOutput2D::Impl::FindPreload(
    const assets::ResourceHandle<assets::AudioClipResource> clip) const noexcept
{
    const auto found = std::find_if(
        preloadCache_.begin(),
        preloadCache_.end(),
        [clip](const PreloadCacheEntry& entry) { return entry.clip == clip; });
    return found == preloadCache_.end() ? nullptr : &*found;
}

std::size_t AudioOutput2D::Impl::CurrentPreloadPcmCapacityBytes() const noexcept
{
    std::size_t bytes = 0U;
    for (const PreloadCacheEntry& entry : preloadCache_)
    {
        bytes += entry.prepared.interleavedPcmF32.capacity() * sizeof(float);
    }
    return bytes;
}

std::size_t AudioOutput2D::Impl::CurrentRefillBytes() const noexcept
{
    std::size_t bytes = 0U;
    for (const PhysicalVoice& voice : voices_)
    {
        bytes += voice.refillSampleCount * sizeof(float);
    }
    return bytes;
}

bool AudioOutput2D::Impl::EvictOneUnusedPreload() noexcept
{
    std::optional<std::size_t> candidate{};
    std::uint64_t oldestUse = std::numeric_limits<std::uint64_t>::max();
    for (std::size_t index = 0U; index < preloadCache_.size(); ++index)
    {
        const PreloadCacheEntry& entry = preloadCache_[index];
        if (entry.referenceCount == 0U &&
            (!candidate.has_value() || entry.lastUseOrder < oldestUse))
        {
            candidate = index;
            oldestUse = entry.lastUseOrder;
        }
    }
    if (!candidate.has_value())
    {
        return false;
    }
    preloadCache_.erase(preloadCache_.begin() + static_cast<std::ptrdiff_t>(*candidate));
    return true;
}

AudioOutputResult2D AudioOutput2D::Impl::AcquirePreload(PhysicalVoice& voice)
{
    if (voice.preloadAcquired)
    {
        return AudioOutputResult2D::Success;
    }
    if (PreloadCacheEntry* const existing = FindPreload(voice.clip); existing != nullptr)
    {
        ++existing->referenceCount;
        if (preloadUseOrder_ != std::numeric_limits<std::uint64_t>::max())
        {
            ++preloadUseOrder_;
        }
        existing->lastUseOrder = preloadUseOrder_;
        voice.preloadAcquired = true;
        return AudioOutputResult2D::Success;
    }

    const assets::AudioClipResource* const resource = resources_.Resolve(voice.clip);
    std::size_t expectedBytes = 0U;
    if (resource == nullptr ||
        !AudioOutputCheckedByteCount(resource->frameCount, resource->channelCount, expectedBytes))
    {
        diagnostic_ = "Preloaded PCM size overflowed canonical AudioClip metadata.";
        return AudioOutputResult2D::ResourceUnavailable;
    }
    if (expectedBytes > config_.preloadPcmByteBudget)
    {
        diagnostic_ = "One preloaded clip exceeds the configured retained PCM byte budget.";
        return AudioOutputResult2D::CapacityExceeded;
    }

    while (preloadCache_.size() >= config_.preloadCacheCapacity ||
           CurrentPreloadPcmCapacityBytes() > config_.preloadPcmByteBudget - expectedBytes)
    {
        if (!EvictOneUnusedPreload())
        {
            diagnostic_ = "Preloaded PCM cache budget is exhausted by active voice data.";
            return AudioOutputResult2D::CapacityExceeded;
        }
    }

    AudioClipPrepareResult2D prepared = preparation_.PreparePreloaded(voice.clip);
    if (!prepared.Succeeded())
    {
        ++backendFailureCount_;
        diagnostic_ = prepared.diagnostic.empty() ? "Preloaded audio preparation failed." : std::move(prepared.diagnostic);
        return AudioOutputResult2D::ClipPreparationFailed;
    }

    const std::size_t preparedCapacityBytes = prepared.prepared.interleavedPcmF32.capacity() * sizeof(float);
    if (preparedCapacityBytes > config_.preloadPcmByteBudget)
    {
        diagnostic_ = "Prepared preload PCM allocation exceeds the retained PCM byte budget.";
        return AudioOutputResult2D::CapacityExceeded;
    }
    while (CurrentPreloadPcmCapacityBytes() > config_.preloadPcmByteBudget - preparedCapacityBytes)
    {
        if (!EvictOneUnusedPreload())
        {
            diagnostic_ = "Prepared preload PCM cannot fit without evicting active voice data.";
            return AudioOutputResult2D::CapacityExceeded;
        }
    }

    PreloadCacheEntry entry{};
    entry.clip = voice.clip;
    entry.prepared = std::move(prepared.prepared);
    entry.referenceCount = 1U;
    if (preloadUseOrder_ != std::numeric_limits<std::uint64_t>::max())
    {
        ++preloadUseOrder_;
    }
    entry.lastUseOrder = preloadUseOrder_;
    preloadCache_.push_back(std::move(entry));
    voice.preloadAcquired = true;
    return AudioOutputResult2D::Success;
}

void AudioOutput2D::Impl::ReleaseVoicePreparation(PhysicalVoice& voice) noexcept
{
    if (voice.preloadAcquired)
    {
        if (PreloadCacheEntry* const cached = FindPreload(voice.clip);
            cached != nullptr && cached->referenceCount > 0U)
        {
            --cached->referenceCount;
        }
        voice.preloadAcquired = false;
    }
    voice.decoder.reset();
    voice.refillBuffer.reset();
    voice.refillSampleCount = 0U;
}

void AudioOutput2D::Impl::DestroyStream(PhysicalVoice& voice) noexcept
{
    if (voice.stream != nullptr)
    {
        SDL_DestroyAudioStream(voice.stream);
        voice.stream = nullptr;
        voice.bound = false;
        ++streamDestroyCount_;
    }
}

void AudioOutput2D::Impl::RemoveVoice(const std::size_t index) noexcept
{
    PhysicalVoice& voice = voices_[index];
    DestroyStream(voice);
    ReleaseVoicePreparation(voice);
    voices_.erase(voices_.begin() + static_cast<std::ptrdiff_t>(index));
}

void AudioOutput2D::Impl::UpdateFromSnapshot(
    PhysicalVoice& voice,
    const AudioVoiceSnapshot2D& snapshot) noexcept
{
    voice.clip = snapshot.clip;
    voice.semanticState = snapshot.state;
    voice.effectiveVolume = snapshot.effectiveVolume;
    voice.pitch = snapshot.pitch;
    voice.loop = snapshot.loop;
    voice.semanticPositionFrames = snapshot.positionFrames;
}

AudioOutputResult2D AudioOutput2D::Impl::ApplyStreamState(PhysicalVoice& voice)
{
    if (voice.stream == nullptr)
    {
        return AudioOutputResult2D::Success;
    }
    if (!SDL_SetAudioStreamGain(voice.stream, voice.effectiveVolume) ||
        !SDL_SetAudioStreamFrequencyRatio(voice.stream, voice.pitch))
    {
        ++backendFailureCount_;
        diagnostic_ = std::string{"SDL audio stream gain/pitch update failed: "} + SDL_GetError();
        return AudioOutputResult2D::StreamUpdateFailed;
    }

    const bool shouldBind = voice.semanticState == AudioPlaybackState2D::Playing &&
        device_ != 0U && state_ != AudioOutputState2D::RecoveryPending;
    if (shouldBind && !voice.bound)
    {
        if (!SDL_BindAudioStream(device_, voice.stream))
        {
            ++backendFailureCount_;
            diagnostic_ = std::string{"SDL audio stream bind failed: "} + SDL_GetError();
            return AudioOutputResult2D::StreamBindFailed;
        }
        voice.bound = true;
    }
    else if (!shouldBind && voice.bound)
    {
        SDL_UnbindAudioStream(voice.stream);
        voice.bound = false;
    }
    return AudioOutputResult2D::Success;
}

AudioOutputSyncReport2D AudioOutput2D::Impl::Sync(
    const AudioSystem2D& semantic,
    const std::span<const AudioEvent2D> events)
{
    AudioOutputSyncReport2D report{};
    for (const AudioEvent2D& event : events)
    {
        if (IsTerminalEvent(event.type))
        {
            if (const std::optional<std::size_t> index = FindVoice(event.voice); index.has_value())
            {
                RemoveVoice(*index);
                ++report.removedVoiceCount;
            }
            continue;
        }
        if (event.type != AudioEventType2D::Started || FindVoice(event.voice).has_value())
        {
            continue;
        }

        const std::optional<AudioVoiceSnapshot2D> snapshot = semantic.InspectVoice(event.voice);
        if (!snapshot.has_value())
        {
            continue;
        }
        if (voices_.size() >= config_.voiceCapacity)
        {
            report.result = AudioOutputResult2D::CapacityExceeded;
            diagnostic_ = "Physical voice capacity is smaller than semantic admission capacity.";
            return report;
        }
        PhysicalVoice voice{};
        voice.handle = event.voice;
        UpdateFromSnapshot(voice, *snapshot);
        voices_.push_back(std::move(voice));
        ++report.createdVoiceCount;
    }

    std::size_t index = 0U;
    while (index < voices_.size())
    {
        const std::optional<AudioVoiceSnapshot2D> snapshot = semantic.InspectVoice(voices_[index].handle);
        if (!snapshot.has_value())
        {
            RemoveVoice(index);
            ++report.removedVoiceCount;
            continue;
        }
        PhysicalVoice& voice = voices_[index];
        UpdateFromSnapshot(voice, *snapshot);
        if (voice.stream != nullptr)
        {
            const AudioOutputResult2D result = ApplyStreamState(voice);
            if (result != AudioOutputResult2D::Success)
            {
                report.result = result;
                return report;
            }
        }
        ++report.updatedVoiceCount;
        ++index;
    }
    return report;
}

AudioOutputMetrics2D AudioOutput2D::Impl::Metrics() const noexcept
{
    AudioOutputMetrics2D metrics{};
    metrics.state = state_;
    metrics.configuredVoiceCapacity = config_.voiceCapacity;
    metrics.trackedVoiceCount = voices_.size();
    metrics.preloadCacheEntryCount = preloadCache_.size();
    metrics.preloadCacheCapacity = config_.preloadCacheCapacity;
    metrics.preloadPcmByteBudget = config_.preloadPcmByteBudget;
    metrics.refillBufferByteBudget = config_.refillBufferByteBudget;
    metrics.deviceOpenCount = deviceOpenCount_;
    metrics.deviceSuspendCount = deviceSuspendCount_;
    metrics.deviceResumeCount = deviceResumeCount_;
    metrics.deviceLossEventCount = deviceLossEventCount_;
    metrics.deviceFormatChangeEventCount = deviceFormatChangeEventCount_;
    metrics.recoveryCount = recoveryCount_;
    metrics.streamCreateCount = streamCreateCount_;
    metrics.streamDestroyCount = streamDestroyCount_;
    metrics.refillCallCount = refillCallCount_;
    metrics.refillFrameCount = refillFrameCount_;
    metrics.backendFailureCount = backendFailureCount_;

    for (const PreloadCacheEntry& entry : preloadCache_)
    {
        metrics.trace2dOwnedPreloadPcmBytes += entry.prepared.interleavedPcmF32.size() * sizeof(float);
        metrics.trace2dOwnedPreloadPcmCapacityBytes += entry.prepared.interleavedPcmF32.capacity() * sizeof(float);
    }
    for (const PhysicalVoice& voice : voices_)
    {
        if (voice.stream != nullptr)
        {
            ++metrics.activeStreamCount;
            const int queued = SDL_GetAudioStreamQueued(voice.stream);
            if (queued > 0)
            {
                metrics.queuedInputBytes += static_cast<std::uint64_t>(queued);
            }
        }
        if (voice.decoder != nullptr)
        {
            ++metrics.streamingVoiceCount;
        }
        metrics.trace2dOwnedRefillBytes += voice.refillSampleCount * sizeof(float);
        metrics.trace2dOwnedRefillCapacityBytes += voice.refillSampleCount * sizeof(float);
    }
    return metrics;
}
} // namespace trace2d::audio
