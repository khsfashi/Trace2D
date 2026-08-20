#include "AudioOutput2DInternal.hpp"

#include <algorithm>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>

namespace trace2d::audio
{
bool AudioOutput2D::Impl::IsPrepared(const PhysicalVoice& voice) const noexcept
{
    if (voice.stream == nullptr)
    {
        return false;
    }
    if (voice.loadPolicy == assets::AudioClipLoadPolicy::Preload)
    {
        return voice.preloadAcquired && FindPreload(voice.clip) != nullptr;
    }
    return voice.decoder != nullptr;
}

AudioOutputResult2D AudioOutput2D::Impl::EnsurePrepared(PhysicalVoice& voice)
{
    const assets::AudioClipResource* const resource = resources_.Resolve(voice.clip);
    if (resource == nullptr)
    {
        diagnostic_ = "AudioOutput2D cannot prepare a stale or unavailable AudioClip resource.";
        return AudioOutputResult2D::ResourceUnavailable;
    }

    voice.loadPolicy = resource->loadPolicy;
    voice.sampleRateHz = resource->sampleRateHz;
    voice.channelCount = resource->channelCount;
    voice.frameCount = resource->frameCount;
    if (voice.frameCount == 0U || voice.channelCount == 0U || voice.sampleRateHz == 0U)
    {
        diagnostic_ = "AudioOutput2D resolved invalid canonical AudioClip timing metadata.";
        return AudioOutputResult2D::ResourceUnavailable;
    }

    if (voice.loadPolicy == assets::AudioClipLoadPolicy::Preload)
    {
        const AudioOutputResult2D acquire = AcquirePreload(voice);
        if (acquire != AudioOutputResult2D::Success)
        {
            return acquire;
        }
    }
    else if (voice.decoder == nullptr)
    {
        AudioClipStreamOpenResult2D opened = preparation_.OpenStream(voice.clip);
        if (!opened.Succeeded())
        {
            ++backendFailureCount_;
            diagnostic_ = opened.diagnostic.empty() ? "Streaming audio decoder open failed." : std::move(opened.diagnostic);
            return AudioOutputResult2D::StreamOpenFailed;
        }
        voice.decoder = std::move(opened.stream);
        voice.nextFrame = AudioOutputClampFrameIndex(voice.semanticPositionFrames, voice.frameCount);
        if (voice.nextFrame != 0U &&
            voice.decoder->Seek(voice.nextFrame) != AudioClipPreparationResult2D::Success)
        {
            ++backendFailureCount_;
            diagnostic_ = "Streaming audio decoder could not seek to the semantic start position.";
            return AudioOutputResult2D::StreamSeekFailed;
        }

        std::size_t refillSamples = 0U;
        if (!AudioOutputCheckedSampleCount(config_.refillChunkFrames, voice.channelCount, refillSamples))
        {
            diagnostic_ = "Streaming refill buffer size overflowed the bounded audio contract.";
            return AudioOutputResult2D::InvalidConfig;
        }
        const std::size_t refillBytes = refillSamples * sizeof(float);
        if (refillBytes > config_.refillBufferByteBudget ||
            CurrentRefillBytes() > config_.refillBufferByteBudget - refillBytes)
        {
            diagnostic_ = "Streaming refill-buffer byte budget is exhausted.";
            return AudioOutputResult2D::CapacityExceeded;
        }
        voice.refillBuffer = std::make_unique_for_overwrite<float[]>(refillSamples);
        voice.refillSampleCount = refillSamples;
    }
    else if (voice.stream == nullptr)
    {
        const std::uint64_t restartFrame = AudioOutputClampFrameIndex(
            voice.semanticPositionFrames,
            voice.frameCount);
        if (voice.decoder->Seek(restartFrame) != AudioClipPreparationResult2D::Success)
        {
            ++backendFailureCount_;
            diagnostic_ = "Streaming audio decoder could not seek while rebuilding its SDL stream.";
            return AudioOutputResult2D::StreamSeekFailed;
        }
        voice.nextFrame = restartFrame;
        voice.reachedEnd = false;
    }

    if (voice.stream == nullptr)
    {
        SDL_AudioSpec sourceSpec{};
        sourceSpec.format = SDL_AUDIO_F32;
        sourceSpec.channels = static_cast<int>(voice.channelCount);
        sourceSpec.freq = static_cast<int>(voice.sampleRateHz);
        voice.stream = SDL_CreateAudioStream(&sourceSpec, nullptr);
        if (voice.stream == nullptr)
        {
            ++backendFailureCount_;
            diagnostic_ = std::string{"SDL audio stream creation failed: "} + SDL_GetError();
            return AudioOutputResult2D::StreamCreateFailed;
        }
        ++streamCreateCount_;
        voice.nextFrame = AudioOutputClampFrameIndex(voice.semanticPositionFrames, voice.frameCount);
        voice.reachedEnd = false;
        return ApplyStreamState(voice);
    }
    return AudioOutputResult2D::Success;
}

AudioOutputPumpReport2D AudioOutput2D::Impl::Pump()
{
    AudioOutputPumpReport2D report{};
    if (state_ == AudioOutputState2D::Stopped || device_ == 0U)
    {
        report.result = AudioOutputResult2D::NotStarted;
        return report;
    }
    if (state_ == AudioOutputState2D::RecoveryPending)
    {
        report.result = AudioOutputResult2D::RecoveryFailed;
        diagnostic_ = "Audio device recovery is pending; call Recover() before refilling output.";
        return report;
    }

    ++refillCallCount_;
    for (PhysicalVoice& voice : voices_)
    {
        ++report.visitedVoiceCount;
        const bool wasPrepared = IsPrepared(voice);
        const AudioOutputResult2D prepare = EnsurePrepared(voice);
        if (prepare != AudioOutputResult2D::Success)
        {
            report.result = prepare;
            return report;
        }
        if (!wasPrepared)
        {
            ++report.preparedVoiceCount;
        }
        if (voice.semanticState == AudioPlaybackState2D::Paused || voice.reachedEnd)
        {
            continue;
        }

        const AudioOutputResult2D refill = RefillVoice(voice, report);
        if (refill != AudioOutputResult2D::Success)
        {
            report.result = refill;
            return report;
        }
    }
    return report;
}

AudioOutputResult2D AudioOutput2D::Impl::RefillVoice(
    PhysicalVoice& voice,
    AudioOutputPumpReport2D& report)
{
    if (voice.stream == nullptr)
    {
        return AudioOutputResult2D::StreamCreateFailed;
    }

    std::size_t targetBytes = 0U;
    if (!AudioOutputCheckedByteCount(config_.targetQueuedFrames, voice.channelCount, targetBytes))
    {
        diagnostic_ = "Audio output target queue size overflowed the bounded audio contract.";
        return AudioOutputResult2D::InvalidConfig;
    }
    const int initialQueued = SDL_GetAudioStreamQueued(voice.stream);
    if (initialQueued < 0)
    {
        ++backendFailureCount_;
        diagnostic_ = std::string{"SDL queued-audio query failed: "} + SDL_GetError();
        return AudioOutputResult2D::StreamUpdateFailed;
    }

    std::size_t queuedBytes = static_cast<std::size_t>(initialQueued);
    std::uint32_t chunks = 0U;
    while (queuedBytes < targetBytes && chunks < config_.maxRefillChunksPerPump && !voice.reachedEnd)
    {
        std::uint64_t framesWritten = 0U;
        const AudioOutputResult2D fill = voice.loadPolicy == assets::AudioClipLoadPolicy::Preload
            ? PutPreloadedChunk(voice, framesWritten)
            : PutStreamingChunk(voice, framesWritten);
        if (fill != AudioOutputResult2D::Success)
        {
            return fill;
        }
        if (framesWritten == 0U)
        {
            break;
        }

        std::size_t bytesWritten = 0U;
        if (!AudioOutputCheckedByteCount(framesWritten, voice.channelCount, bytesWritten))
        {
            diagnostic_ = "Audio refill byte accounting overflowed.";
            return AudioOutputResult2D::InvalidConfig;
        }
        queuedBytes = queuedBytes > std::numeric_limits<std::size_t>::max() - bytesWritten
            ? std::numeric_limits<std::size_t>::max()
            : queuedBytes + bytesWritten;
        ++chunks;
        ++report.refillChunkCount;
        report.refillFrameCount += framesWritten;
        refillFrameCount_ += framesWritten;
    }
    return AudioOutputResult2D::Success;
}

AudioOutputResult2D AudioOutput2D::Impl::PutPreloadedChunk(
    PhysicalVoice& voice,
    std::uint64_t& framesWritten)
{
    framesWritten = 0U;
    const PreloadCacheEntry* const cached = FindPreload(voice.clip);
    if (cached == nullptr)
    {
        diagnostic_ = "Prepared preload cache entry disappeared while a physical voice referenced it.";
        return AudioOutputResult2D::ClipPreparationFailed;
    }

    if (voice.nextFrame >= voice.frameCount)
    {
        if (!voice.loop)
        {
            voice.reachedEnd = true;
            return AudioOutputResult2D::Success;
        }
        voice.nextFrame = 0U;
    }

    const std::uint64_t chunkFrames = std::min<std::uint64_t>(
        config_.refillChunkFrames,
        voice.frameCount - voice.nextFrame);
    std::size_t byteCount = 0U;
    std::size_t sampleOffset = 0U;
    if (!AudioOutputCheckedByteCount(chunkFrames, voice.channelCount, byteCount) ||
        !AudioOutputCheckedSampleCount(voice.nextFrame, voice.channelCount, sampleOffset) ||
        byteCount > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        sampleOffset > cached->prepared.interleavedPcmF32.size())
    {
        diagnostic_ = "Prepared preload PCM bounds exceeded canonical AudioClip metadata.";
        return AudioOutputResult2D::ClipPreparationFailed;
    }

    const std::size_t sampleCount = byteCount / sizeof(float);
    if (sampleCount > cached->prepared.interleavedPcmF32.size() - sampleOffset)
    {
        diagnostic_ = "Prepared preload PCM is shorter than canonical AudioClip metadata.";
        return AudioOutputResult2D::ClipPreparationFailed;
    }
    if (!SDL_PutAudioStreamData(
            voice.stream,
            cached->prepared.interleavedPcmF32.data() + sampleOffset,
            static_cast<int>(byteCount)))
    {
        ++backendFailureCount_;
        diagnostic_ = std::string{"SDL preload queue write failed: "} + SDL_GetError();
        return AudioOutputResult2D::StreamUpdateFailed;
    }

    voice.nextFrame += chunkFrames;
    framesWritten = chunkFrames;
    return AudioOutputResult2D::Success;
}

AudioOutputResult2D AudioOutput2D::Impl::PutStreamingChunk(
    PhysicalVoice& voice,
    std::uint64_t& framesWritten)
{
    framesWritten = 0U;
    if (voice.decoder == nullptr || voice.refillBuffer == nullptr || voice.refillSampleCount == 0U)
    {
        return AudioOutputResult2D::StreamOpenFailed;
    }

    std::uint64_t decodedFrames = 0U;
    AudioClipPreparationResult2D read = voice.decoder->ReadFrames(
        std::span<float>{voice.refillBuffer.get(), voice.refillSampleCount},
        config_.refillChunkFrames,
        decodedFrames);
    if (read != AudioClipPreparationResult2D::Success)
    {
        ++backendFailureCount_;
        diagnostic_ = "Streaming decoder refill failed.";
        return AudioOutputResult2D::StreamUpdateFailed;
    }

    if (decodedFrames == 0U)
    {
        if (!voice.loop)
        {
            voice.reachedEnd = true;
            return AudioOutputResult2D::Success;
        }
        if (voice.decoder->Seek(0U) != AudioClipPreparationResult2D::Success)
        {
            ++backendFailureCount_;
            diagnostic_ = "Looping streaming decoder could not seek to frame zero.";
            return AudioOutputResult2D::StreamSeekFailed;
        }
        voice.nextFrame = 0U;
        read = voice.decoder->ReadFrames(
            std::span<float>{voice.refillBuffer.get(), voice.refillSampleCount},
            config_.refillChunkFrames,
            decodedFrames);
        if (read != AudioClipPreparationResult2D::Success || decodedFrames == 0U)
        {
            ++backendFailureCount_;
            diagnostic_ = "Looping streaming decoder produced no PCM after rewind.";
            return AudioOutputResult2D::StreamUpdateFailed;
        }
    }

    std::size_t byteCount = 0U;
    if (!AudioOutputCheckedByteCount(decodedFrames, voice.channelCount, byteCount) ||
        byteCount > voice.refillSampleCount * sizeof(float) ||
        byteCount > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        diagnostic_ = "Streaming decoder returned frames outside the retained refill buffer.";
        return AudioOutputResult2D::StreamUpdateFailed;
    }
    if (!SDL_PutAudioStreamData(voice.stream, voice.refillBuffer.get(), static_cast<int>(byteCount)))
    {
        ++backendFailureCount_;
        diagnostic_ = std::string{"SDL streaming queue write failed: "} + SDL_GetError();
        return AudioOutputResult2D::StreamUpdateFailed;
    }

    voice.nextFrame += decodedFrames;
    framesWritten = decodedFrames;
    return AudioOutputResult2D::Success;
}
} // namespace trace2d::audio
